using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.WebhookServices
{
    public class WebhookSubscriptionService : BaseService
    {
        public WebhookSubscriptionService(WebhookDbContext dbContext) : base(dbContext) { }
        public async Task<WebhookSubscription> GetWebhookSubscriptionByIdAsync(Guid webhookId)
        {
            return await _dbContext.WebhookSubscription.FirstOrDefaultAsync(wb => wb.WebhookSubscriptionId == webhookId && !wb.IsDeleted);
        }
    }
}
