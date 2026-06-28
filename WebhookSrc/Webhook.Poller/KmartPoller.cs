using Microsoft.EntityFrameworkCore;
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
using Webhook.Services.ProductService;

namespace Webhook.Poller
{
    public class KmartPoller : IWebhookPollerHandler
    {
        private readonly ILogger<KmartPoller> _logger;
        private readonly WebhookDbContext _dbContext;
        private readonly IHttpClientFactory _httpClientFactory;
        private readonly IProductService _productService;
        public string StoreName => "Kmart";
        public bool PollProductsPage { get; set; }

        public KmartPoller(WebhookDbContext dbContext, IProductService productService, IHttpClientFactory httpClientFactory, ILogger<KmartPoller> logger)
        {
            _dbContext = dbContext;
            _httpClientFactory = httpClientFactory;
            _productService = productService;
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

            if (PollProductsPage)
            {
                await PollProductsAsync(distributor, cancellationToken);
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

                                // Checks if the product already exists
                                var findProduct = await _dbContext.Products
                                    .FirstOrDefaultAsync(p => p.ReferenceID == product.ReferenceID
                                    && p.Name == product.Name
                                    && !p.IsDeleted);

                                if (findProduct != null)
                                {
                                    product.ProductID = findProduct.ProductID;
                                }

                                // Adds or Updates the found product
                                await _productService.AddOrUpdateProduct(product);
                            }
                            catch
                            {
                                _logger.LogError(ex, "Failed to process product {ProductID}", result?.data?.variation_id);
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
            //
        }

        public async Task PollLocationsAsync(CancellationToken cancellationToken)
        {
            //
        }
    }
}
