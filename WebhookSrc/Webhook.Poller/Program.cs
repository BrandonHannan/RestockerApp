using Webhook.Poller;
using Webhook.Services;
using Webhook.Services.ProductService;

var builder = Host.CreateDefaultBuilder(args);

builder.ConfigureServices((hostContext, services) =>
{
    services.AddHttpClient();

    services.AddScoped<IProductService, ProductService>();

    services.AddTransient<IWebhookPollerHandler, KmartPoller>();
    services.AddTransient<KmartPoller>();

    services.AddSingleton<PlaywrightBrowserService>();

    services.AddHostedService<WebhookPollerWorker>();
});

var host = builder.Build();
host.Run();
