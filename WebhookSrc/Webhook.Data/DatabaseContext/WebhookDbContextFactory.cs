using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Design;

namespace Webhook.Data;

public class WebhookDbContextFactory : IDesignTimeDbContextFactory<WebhookDbContext>
{
    public WebhookDbContext CreateDbContext(string[] args)
    {
        var environment = Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT")
                       ?? Environment.GetEnvironmentVariable("DOTNET_ENVIRONMENT")
                       ?? "Development";

        var configuration = new ConfigurationBuilder()
            .SetBasePath(Directory.GetCurrentDirectory())
            .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
            .AddJsonFile($"appsettings.{environment}.json", optional: true)
            .AddEnvironmentVariables()
            .Build();

        var connectionString = configuration.GetConnectionString("Database");

        if (string.IsNullOrWhiteSpace(connectionString))
        {
            throw new InvalidOperationException($"Connection string 'Database' not found in appsettings.{environment}.json or appsettings.json.");
        }

        var builder = new DbContextOptionsBuilder<WebhookDbContext>();
        builder.UseNpgsql(connectionString);

        return new WebhookDbContext(builder.Options);
    }
}