using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class WebhookConnection : BaseModel
    {
        public Guid WebhookConnectionID { get; set; }
        public string Url { get; set; }
        public Guid DistributorID { get; set; }
        [ForeignKey("DistributorID")]
        public virtual Distributor Distributor { get; set; }
        public Guid StockTypeID { get; set; }
        [ForeignKey("StockTypeID")]
        public virtual StockType StockType { get; set; }
    }
}
