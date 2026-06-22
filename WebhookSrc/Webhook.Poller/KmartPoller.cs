using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;

namespace Webhook.Poller
{
    public class KmartPoller : IWebhookPollerHandler
    {
        private readonly WebhookDbContext _dbContext;
        public string StoreName => "Kmart";

        public KmartPoller(WebhookDbContext dbContext)
        {
            _dbContext = dbContext;
        }

        public async Task PollAsync(CancellationToken cancellationToken)
        {
            // Custom Polling Logic
        }
    }
}
