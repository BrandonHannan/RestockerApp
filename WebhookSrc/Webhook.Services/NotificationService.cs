using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;

namespace Webhook.Services
{
    public class NotificationService : BaseService, INotificationService
    {
        private readonly ILogger<NotificationService> _logger;
        private readonly IHttpClientFactory _httpClientFactory;

        public NotificationService(IHttpClientFactory httpClientFactory, ILogger<NotificationService> logger, WebhookDbContext context) : base(context)
        {
            _httpClientFactory = httpClientFactory;
            _logger = logger;
        }
        public async Task SendNotificationAsync(string message)
        {
            // Implement your notification logic here
            // For example, send an email, push notification, etc.
            await Task.CompletedTask;
        }
    }
}
