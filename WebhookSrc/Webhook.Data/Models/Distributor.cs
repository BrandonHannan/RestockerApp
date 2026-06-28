using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class Distributor : BaseModel
    {
        public Guid DistributorID { get; set; }
        public string Name { get; set; }
        public string BaseUrl { get; set; }
        public string SitemapUrl { get; set; }
        public bool IsActive { get; set; } = true;
    }
}
