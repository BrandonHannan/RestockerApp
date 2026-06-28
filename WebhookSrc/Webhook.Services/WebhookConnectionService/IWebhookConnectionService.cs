using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.WebhookConnectionService
{
    public interface IWebhookConnectionService
    {
        public Task<WebhookConnection> GetWebhookConnectionAsync(Guid webhookConnectionId);
        public Task<List<WebhookConnection>> GetWebhookConnectionsAsync();
        public Task AddOrUpdateWebhookConnection(WebhookConnection webhookConnection);
        public Task DeleteWebhookConnection(Guid webhookConnectionId);
    }
}
