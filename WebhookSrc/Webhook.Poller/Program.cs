using Webhook.Poller;

var builder = Host.CreateDefaultBuilder(args);

builder.ConfigureServices((hostContext, services) =>
{
    services.AddHttpClient();

    services.AddTransient<IWebhookPollerHandler, KmartPoller>();

    services.AddHostedService<WebhookPollerWorker>();
});

var host = builder.Build();
host.Run();
