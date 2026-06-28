using Webhook.Data;

namespace Webhook.Poller
{
    public class WebhookPollerWorker : BackgroundService
    {
        private readonly IConfiguration _configuration;
        private readonly IServiceScopeFactory _scopeFactory;
        private readonly ILogger<WebhookPollerWorker> _logger;

        public WebhookPollerWorker(IServiceScopeFactory scopeFactory, IConfiguration configuration, ILogger<WebhookPollerWorker> logger)
        {
            _scopeFactory = scopeFactory;
            _configuration = configuration;
            _logger = logger;
        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
            int interval = _configuration.GetValue<int>("WebhookSettings:DispatchIntervalSeconds");
            if (interval <= 0) interval = 60;

            using PeriodicTimer timer = new PeriodicTimer(TimeSpan.FromSeconds(interval));

            _logger.LogInformation("Poller started. Firing every {Interval} seconds.", interval);

            List<Type> registeredHandlerTypes;
            using (var initScope = _scopeFactory.CreateScope())
            {
                registeredHandlerTypes = initScope.ServiceProvider
                    .GetServices<IWebhookPollerHandler>()
                    .Select(h => h.GetType())
                    .ToList();
            }

            while (await timer.WaitForNextTickAsync(stoppingToken))
            {
                var pollingTasks = registeredHandlerTypes.Select(async handlerType =>
                {
                    // Create a strictly isolated scope for THIS specific task
                    using var taskScope = _scopeFactory.CreateScope();

                    // Resolve only the specific handler needed for this thread
                    var handler = (IWebhookPollerHandler)taskScope.ServiceProvider.GetRequiredService(handlerType);

                    _logger.LogInformation("Starting concurrent polling for {StoreName}", handler.StoreName);

                    try
                    {
                        // Execute the work. The DbContext here is completely isolated to this thread.
                        await handler.PollAsync(stoppingToken);
                    }
                    catch (Exception ex)
                    {
                        // Catch exceptions inside the task so one failing poller doesn't crash the others
                        _logger.LogError(ex, "An error occurred while polling for {StoreName}", handler.StoreName);
                    }
                });

                await Task.WhenAll(pollingTasks);

                _logger.LogInformation("All polling completed for this cycle.");
            }
        }
    }
}
