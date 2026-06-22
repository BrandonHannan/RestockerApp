using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Poller
{
    public interface IWebhookPollerHandler
    {
        string StoreName { get; }

        Task PollAsync(CancellationToken cancellationToken);
    }
}
