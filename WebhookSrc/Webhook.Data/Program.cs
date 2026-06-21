using Microsoft.EntityFrameworkCore;
using Webhook.Data;

var builder = Host.CreateDefaultBuilder(args);
builder.ConfigureServices((hostContext, services) =>
{
    services.AddDbContext<WebhookDbContext>(options =>
        options.UseNpgsql(hostContext.Configuration.GetConnectionString("Database")));

    services.AddHostedService<MigrationWorker>();
});

var host = builder.Build();
host.Run();
