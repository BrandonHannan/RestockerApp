using Microsoft.EntityFrameworkCore;
using Microsoft.Playwright;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Xml.Serialization;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Poller.ResponseModels.Kmart;
using Webhook.Services;
using Webhook.Services.ProductService;

namespace Webhook.Poller
{
    public class KmartPoller : IWebhookPollerHandler
    {
        private static DateTime _lastForcedPollTime = DateTime.MinValue;

        private readonly ILogger<KmartPoller> _logger;
        private readonly WebhookDbContext _dbContext;
        private readonly IHttpClientFactory _httpClientFactory;
        private readonly INotificationService _notificationService;
        private readonly IConfiguration _configuration;
        private readonly IProductService _productService;
        public string StoreName => "Kmart";
        public bool PollProductsPage { get; set; }

        public KmartPoller(
            WebhookDbContext dbContext, 
            IProductService productService, 
            IHttpClientFactory httpClientFactory, 
            INotificationService notificationService,
            IConfiguration configuration, 
            ILogger<KmartPoller> logger)
        {
            _dbContext = dbContext;
            _httpClientFactory = httpClientFactory;
            _notificationService = notificationService;
            _productService = productService;
            _configuration = configuration;
            _logger = logger;
        }

        public async Task PollAsync(CancellationToken cancellationToken)
        {
            var distributor = await _dbContext.Distributors
                .FirstOrDefaultAsync(d => d.Name == "Kmart" && !d.IsDeleted, cancellationToken);

            if (distributor == null)
            {
                _logger.LogWarning("Kmart distributor not found.");
                return;
            }

            await PollSiteMapAsync(distributor, cancellationToken);

            // Pull the override interval from appsettings, defaulting to 30 seconds if missing
            int forceIntervalSeconds = _configuration.GetValue<int>("WebhookSettings:ForcePollProductsIntervalSeconds", 30);

            // Check if enough time has passed since the last poll
            bool timeToForcePoll = (DateTime.UtcNow - _lastForcedPollTime).TotalSeconds >= forceIntervalSeconds;

            if (PollProductsPage || timeToForcePoll)
            {
                await PollProductsAsync(distributor, cancellationToken);
                _lastForcedPollTime = DateTime.UtcNow;
            }

            await PollProductAvailabilityAsync(cancellationToken);

            await PollLocationsAsync(cancellationToken);
        }

        public async Task PollSiteMapAsync(Distributor distributor, CancellationToken cancellationToken)
        {
            var client = _httpClientFactory.CreateClient();

            var siteMapUrl = distributor.SitemapUrl;

            using var request = new HttpRequestMessage(HttpMethod.Get, siteMapUrl);

            var response = await client.SendAsync(request, cancellationToken);

            if (response.IsSuccessStatusCode)
            {
                var content = await response.Content.ReadAsStringAsync(cancellationToken);

                _logger.LogInformation($"GET to {siteMapUrl} produced a response: {content}");

                var serializer = new XmlSerializer(typeof(SiteMapXML.sitemapindex));

                using var stringReader = new StringReader(content);

                try
                {
                    var sitemapData = (SiteMapXML.sitemapindex)serializer.Deserialize(stringReader);

                    if (sitemapData?.sitemap != null)
                    {
                        var targetSitemaps = sitemapData.sitemap
                            .Where(s => s.loc != null && s.loc.Contains("product-sitemap"))
                            .ToList();

                        foreach (var sitemap in targetSitemaps)
                        {
                            try
                            {
                                using var productSiteMapRequest = new HttpRequestMessage(HttpMethod.Get, sitemap.loc);

                                var productSiteMapRespone = await client.SendAsync(productSiteMapRequest, cancellationToken);

                                if (productSiteMapRespone.IsSuccessStatusCode)
                                {
                                    var productContent = await productSiteMapRespone.Content.ReadAsStringAsync(cancellationToken);

                                    _logger.LogInformation($"GET to {sitemap.loc} produced a response: {productContent}");

                                    var productSiteMapSerializer = new XmlSerializer(typeof(ProductSiteMapXML.urlset));

                                    using var productSiteMapStringReader = new StringReader(productContent);

                                    try
                                    {
                                        var productSiteMapData = (ProductSiteMapXML.urlset)productSiteMapSerializer.Deserialize(productSiteMapStringReader);

                                        if (productSiteMapData?.url != null)
                                        {
                                            var targetProducts = productSiteMapData.url
                                                .Where(product => product.loc != null && product.loc.Contains("/product/pokemon-trading-card-game:"))
                                                .ToList();

                                            foreach (var targetProduct in targetProducts)
                                            {
                                                var product = new Product
                                                {
                                                    ProductUrl = targetProduct.loc
                                                };

                                                var foundProduct = await _dbContext.Products.FirstOrDefaultAsync(p => p.ProductUrl == targetProduct.loc && !p.IsDeleted);

                                                if (foundProduct == null)
                                                {
                                                    PollProductsPage = true;
                                                    await _productService.AddOrUpdateProduct(product, false);
                                                }
                                            }
                                        }
                                    }
                                    catch (Exception ex)
                                    {
                                        _logger.LogError(ex, "Failed to deserialize the product sitempa XML. Check namespaces and structure.");
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                _logger.LogError(ex, "Failed to download sitemap: {loc}", sitemap.loc);
                            }
                        }
                    }
                }
                catch (InvalidOperationException ex)
                {
                    _logger.LogError(ex, "Failed to deserialize the sitemap XML. Check namespaces and structure.");
                }
            }
            else
            {
                _logger.LogError($"GET Request to {siteMapUrl}, failed with status: {response.StatusCode}");
            }
        }

        public async Task PollProductsAsync(Distributor distributor, CancellationToken cancellationToken)
        {
            var client = _httpClientFactory.CreateClient();

            var baseUrl = "https://ac.cnstrc.com/browse/group_id/abfdf5b2d48e682ca75bfe87a0ecba17";
            var queryParameters = "?c=ciojs-client-2.77.1&key=key_GZTqlLr41FS2p7AY&i=e2f502b2-dad5-44f5-abf5-d8edf95214e8&s=1&page=1&num_results_per_page=200&sort_by=relevance&sort_order=descending&_dt=1781776740584";
            var requestUri = baseUrl + queryParameters;

            using var request = new HttpRequestMessage(HttpMethod.Get, requestUri);

            request.Headers.Add("accept", "*/*");
            request.Headers.Add("accept-language", "en-US,en;q=0.9");
            request.Headers.Add("origin", "https://www.kmart.com.au");
            request.Headers.Add("priority", "u=1, i");
            request.Headers.Add("referer", "https://www.kmart.com.au/");
            request.Headers.Add("sec-ch-ua", "\"Google Chrome\";v=\"149\", \"Chromium\";v=\"149\", \"Not)A;Brand\";v=\"24\"");
            request.Headers.Add("sec-ch-ua-mobile", "?0");
            request.Headers.Add("sec-ch-ua-platform", "\"Windows\"");
            request.Headers.Add("sec-fetch-dest", "empty");
            request.Headers.Add("sec-fetch-mode", "cors");
            request.Headers.Add("sec-fetch-site", "cross-site");
            request.Headers.Add("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");

            var response = await client.SendAsync(request, cancellationToken);

            if (response.IsSuccessStatusCode)
            {
                var content = await response.Content.ReadAsStringAsync(cancellationToken);

                _logger.LogInformation($"GET to {requestUri} produced a response: {content}");

                var options = new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                };

                var jsonResponse = JsonSerializer.Deserialize<ProductCategoryResponse.Rootobject>(content, options);

                if (jsonResponse != null && jsonResponse.response != null)
                {

                    Dictionary<string, Guid> fufilmentChannels = await _dbContext.fufilmentChannels.ToDictionaryAsync(fC => fC.ReferenceID, fC => fC.FufilmentChannelID);
                    // Check if results are not empty
                    if (jsonResponse.response.results != null && jsonResponse.response.results.Count() > 0)
                    {
                        foreach (var result in jsonResponse.response.results)
                        {
                            try
                            {
                                if (result.data == null) continue;

                                // Check each product and hydrate product table fields

                                string productUrl = result.data.url;

                                string productId = result.data.variation_id;

                                string productName = result.value;
                                string productDescription = result.data.Brand; // For this distributor, Kmart's product description is just the brand
                                string productImageUrl = result.data.image_url;

                                double productPrice = result.data.price;

                                bool productPreOrderStatus = result.data.isPreOrderActive;
                                string productPreOrderReleaseDate = result.data.preOrderReleaseDate;

                                // Links back to existing fufilment channels
                                int productFufillmentChannel = result.data.FulfilmentChannel;
                                var fufilmentChannel = fufilmentChannels.GetValueOrDefault(productFufillmentChannel.ToString(), Guid.Empty);

                                bool productIsAvailable = !(result.data.stateOOS.QLD != null && result.data.stateOOS.QLD == "6");

                                var product = new Product
                                {
                                    Name = productName,
                                    Description = productDescription,
                                    Price = productPrice,
                                    ReferenceID = productId,
                                    ProductUrl = productUrl,
                                    ProductImgUrl = productImageUrl,
                                    IsPreOrder = productPreOrderStatus,
                                    PreOrderDate = productPreOrderReleaseDate,
                                    FufilmentChannelID = fufilmentChannel,
                                    IsAvailable = productIsAvailable,
                                    DistributorID = distributor.DistributorID
                                };

                                bool notifyProduct = false;

                                // Checks if the product already exists
                                // Checking by ProductUrl as it is unique for each product and products found in the sitemap will not have a ReferenceID until the product page is polled
                                var findProduct = await _dbContext.Products
                                    .FirstOrDefaultAsync(p => p.ProductUrl == product.ProductUrl && !p.IsDeleted);

                                if (findProduct != null)
                                {
                                    product.ProductID = findProduct.ProductID;
                                }

                                // Check if the product exists from the sitemap
                                if (findProduct != null && findProduct.Name == null)
                                {
                                    notifyProduct = true;
                                }

                                // Adds or Updates the found product
                                await _productService.AddOrUpdateProduct(product);
                            }
                            catch
                            {
                                _logger.LogError("Failed to process product {ProductID}", result?.data?.variation_id);
                            }
                        }
                    }
                }
            }
            else
            {
                _logger.LogError($"GET Request to {requestUri}, failed with status: {response.StatusCode}");
            }
        }

        public async Task PollProductAvailabilityAsync(CancellationToken cancellationToken)
        {
            // Use a raw string literal for the exact JSON payload from your curl
            string payload = """
                {"operationName":"getProductAvailability","variables":{"input":{"country":"AU","postcode":"2000","products":[{"keycode":"43788767","quantity":1,"isNationalInventory":false,"isClickAndCollectOnly":false}],"fulfilmentMethods":["HOME_DELIVERY","CLICK_AND_COLLECT"],"amendNearestInStockCnc":true,"limit":3}},"query":"query getProductAvailability($input: ProductAvailabilityQueryInput!) {\n  getProductAvailability(input: $input) {\n    postcode\n    country\n    region\n    availability {\n      HOME_DELIVERY {\n        keycode\n        poolName\n        stock {\n          available\n          __typename\n        }\n        __typename\n      }\n      CLICK_AND_COLLECT {\n        keycode\n        stock {\n          totalAvailable\n          __typename\n        }\n        locations {\n          fulfilment {\n            isBuddyLocation\n            locationId\n            stock {\n              available\n              __typename\n            }\n            __typename\n          }\n          location {\n            locationId\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n      IN_STORE {\n        keycode\n        locations {\n          fulfilment {\n            stock {\n              available\n              __typename\n            }\n            __typename\n          }\n          location {\n            locationId\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n      __typename\n    }\n    __typename\n  }\n}\n"}
                """;

            // api.kmart.com.au is behind Akamai Bot Manager, which gates on a valid Akamai cookie set
            // (_abck / bm_* / ak_bmsc) plus browser-like headers — NOT on the client's TLS fingerprint
            // (a plain HttpClient replay succeeds with the right cookies). The cookie set is harvested
            // from a real browser session and supplied via configuration (it expires within hours, so
            // it must be refreshed). See "Kmart:AvailabilityCookie".
            var cookie = _configuration["Kmart:AvailabilityCookie"];
            if (string.IsNullOrWhiteSpace(cookie))
            {
                _logger.LogError("GraphQL Request Failed: {Status} {Text}", 0,
                    "No Kmart:AvailabilityCookie configured — supply a fresh Akamai cookie set.");
                return;
            }

            var client = _httpClientFactory.CreateClient();

            using var request = new HttpRequestMessage(HttpMethod.Post, "https://api.kmart.com.au/gateway/graphql")
            {
                Content = new StringContent(payload, Encoding.UTF8, "application/json")
            };

            request.Headers.TryAddWithoutValidation("accept", "*/*");
            request.Headers.TryAddWithoutValidation("accept-language", "en-US,en;q=0.9");
            request.Headers.TryAddWithoutValidation("origin", "https://www.kmart.com.au");
            request.Headers.TryAddWithoutValidation("priority", "u=1, i");
            request.Headers.TryAddWithoutValidation("referer", "https://www.kmart.com.au/");
            request.Headers.TryAddWithoutValidation("sec-ch-ua", "\"Not;A=Brand\";v=\"8\", \"Chromium\";v=\"150\", \"Google Chrome\";v=\"150\"");
            request.Headers.TryAddWithoutValidation("sec-ch-ua-mobile", "?0");
            request.Headers.TryAddWithoutValidation("sec-ch-ua-platform", "\"Windows\"");
            request.Headers.TryAddWithoutValidation("sec-fetch-dest", "empty");
            request.Headers.TryAddWithoutValidation("sec-fetch-mode", "cors");
            request.Headers.TryAddWithoutValidation("sec-fetch-site", "same-site");
            request.Headers.TryAddWithoutValidation("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36");
            request.Headers.TryAddWithoutValidation("cookie", cookie);

            var response = await client.SendAsync(request, cancellationToken);
            var body = await response.Content.ReadAsStringAsync(cancellationToken);

            if ((int)response.StatusCode == 200)
            {
                _logger.LogInformation("Availability Data ({Status}): {Data}", (int)response.StatusCode, body);
                // Deserialize and process...
            }
            else
            {
                _logger.LogError("GraphQL Request Failed: {Status} {Text}", (int)response.StatusCode, body);
            }
        }

        public async Task PollLocationsAsync(CancellationToken cancellationToken)
        {
            //
        }
    }
}
