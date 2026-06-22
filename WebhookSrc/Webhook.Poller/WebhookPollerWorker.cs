using Webhook.Data;

namespace Webhook.Poller
{
    public class WebhookPollerWorker : BackgroundService
    {
        private readonly IEnumerable<IWebhookPollerHandler> _handlers;
        private readonly IConfiguration _configuration;
        private readonly WebhookDbContext _dbContext;
        private readonly ILogger<WebhookPollerWorker> _logger;

        public WebhookPollerWorker(IEnumerable<IWebhookPollerHandler> handlers, WebhookDbContext dbContext, IConfiguration configuration, ILogger<WebhookPollerWorker> logger)
        {
            _handlers = handlers;
            _dbContext = dbContext;
            _configuration = configuration;
            _logger = logger;
        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
            int interval = _configuration.GetValue<int>("WebhookSettings:DispatchIntervalSeconds");

            using PeriodicTimer timer = new PeriodicTimer(TimeSpan.FromSeconds(interval));

            _logger.LogInformation("Poller started. Firing every {Interval} seconds.", interval);

            while (await timer.WaitForNextTickAsync(stoppingToken))
            {
                _logger.LogInformation("Timer ticked! Polling webhooks concurrently...");
                var pollingTasks = _handlers.Select(handler =>
                {
                    _logger.LogInformation("Starting polling for {SubscriptionName}", handler.StoreName);
                    return handler.PollAsync(stoppingToken);
                });

                await Task.WhenAll(pollingTasks);

                _logger.LogInformation("All polling completed for this cycle.");
            }
        }
    }
}
