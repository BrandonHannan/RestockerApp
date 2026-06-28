using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class Location : BaseModel
    {
        public Guid LocationID { get; set; }
        public string Name { get; set; }
        public string ReferenceID { get; set; }
        public Guid DistributorID { get; set; }
        [ForeignKey("DistributorID")]
        public virtual Distributor Distributor { get; set; }
    }
}
