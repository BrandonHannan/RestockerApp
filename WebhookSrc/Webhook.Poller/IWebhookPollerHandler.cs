using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Poller
{
    public interface IWebhookPollerHandler
    {
        string StoreName { get; }

        bool PollProductsPage { get; set; }

        public Task PollAsync(CancellationToken cancellationToken);

        public Task PollSiteMapAsync(Distributor distributor, CancellationToken cancellationToken);

        public Task PollProductsAsync(Distributor distributor, CancellationToken cancellationToken);

        public Task PollProductAvailabilityAsync(CancellationToken cancellationToken);

        public Task PollLocationsAsync(CancellationToken cancellationToken);
    }
}
