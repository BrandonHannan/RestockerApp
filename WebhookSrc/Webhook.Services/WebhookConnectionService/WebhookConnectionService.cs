using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Util.Helpers;

namespace Webhook.Services.WebhookConnectionService
{
    public class WebhookConnectionService : BaseService, IWebhookConnectionService
    {
        public WebhookConnectionService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<WebhookConnection> GetWebhookConnectionAsync(Guid webhookConnectionId)
        {
            return await _dbContext.WebhookConnections.FirstOrDefaultAsync(w => w.WebhookConnectionID == webhookConnectionId && !w.IsDeleted);
        }

        public async Task<List<WebhookConnection>> GetWebhookConnectionsAsync()
        {
            var webhookConnections = await _dbContext.WebhookConnections.Where(w => !w.IsDeleted).ToListAsync();
            return webhookConnections;
        }

        public async Task AddOrUpdateWebhookConnection(WebhookConnection webhookConnection)
        {
            IsValidWebhookConnection(webhookConnection);

            if (webhookConnection.WebhookConnectionID == Guid.Empty)
            {
                webhookConnection.WebhookConnectionID = Guid.NewGuid();
                webhookConnection.Created = DateTime.Now;
                webhookConnection.Updated = DateTime.Now;

                _dbContext.WebhookConnections.Add(webhookConnection);
            }
            else
            {
                var foundWebhookConnection = await _dbContext.WebhookConnections.FirstOrDefaultAsync(w => w.WebhookConnectionID == webhookConnection.WebhookConnectionID && !w.IsDeleted);

                if (foundWebhookConnection == null)
                {
                    throw new Exception("Webhook connection does not exist or has been deleted");
                }

                foundWebhookConnection.Url = webhookConnection.Url;
                foundWebhookConnection.DistributorID = webhookConnection.DistributorID;
                foundWebhookConnection.StockTypeID = webhookConnection.StockTypeID;
                foundWebhookConnection.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteWebhookConnection(Guid webhookConnectionId)
        {
            var foundWebhookConnection = await _dbContext.WebhookConnections.FirstOrDefaultAsync(w => w.WebhookConnectionID == webhookConnectionId && !w.IsDeleted);

            if (foundWebhookConnection == null)
            {
                throw new Exception("Webhook connection does not exist or has already been deleted");
            }

            foundWebhookConnection.IsDeleted = true;
            foundWebhookConnection.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidWebhookConnection(WebhookConnection webhookConnection)
        {
            if (webhookConnection == null)
            {
                throw new ArgumentException("Invalid webhook connection");
            }

            if (string.IsNullOrEmpty(webhookConnection.Url) || !ValidUrl.IsValidUrl(webhookConnection.Url))
            {
                throw new ArgumentException("Invalid url for new webhook connection");
            }

            if (webhookConnection.DistributorID == Guid.Empty)
            {
                throw new ArgumentException("Invalid distributor for new webhook connection");
            }

            if (webhookConnection.StockTypeID == Guid.Empty)
            {
                throw new ArgumentException("Invalid stock type for new webhook connection");
            }

            return true;
        }
    }
}
