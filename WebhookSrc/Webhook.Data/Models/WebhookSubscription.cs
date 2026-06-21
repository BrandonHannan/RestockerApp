

namespace Webhook.Data.Models
{
    public class WebhookSubscription
    {
        public Guid WebhookSubscriptionId { get; set; }
        public string Name { get; set; }
        public string Description { get; set; }
        public string Url { get; set; }
        public bool IsActive { get; set; }
        public bool IsDeleted { get; set; }
        public DateTime Created { get; set; }
        public DateTime Updated { get; set; }
    }
}
