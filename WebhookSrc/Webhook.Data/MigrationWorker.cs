using Microsoft.EntityFrameworkCore;

namespace Webhook.Data;

public class MigrationWorker : BackgroundService
{
    private readonly IServiceProvider _serviceProvider;
    private readonly ILogger<MigrationWorker> _logger;

    public MigrationWorker(IServiceProvider serviceProvider, ILogger<MigrationWorker> logger)
    {
        _serviceProvider = serviceProvider;
        _logger = logger;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Starting database migration process...");

        // Create a dedicated scope to resolve the scoped AppDbContext
        using var scope = _serviceProvider.CreateScope();
        var dbContext = scope.ServiceProvider.GetRequiredService<WebhookDbContext>();

        try
        {
            // Applies any pending migrations. If the DB doesn't exist, it creates it.
            await dbContext.Database.MigrateAsync(stoppingToken);
            _logger.LogInformation("Database migrations applied successfully.");
        }
        catch (Exception ex)
        {
            _logger.LogCritical(ex, "Failed to apply database migrations.");
            throw;
        }
    }
}