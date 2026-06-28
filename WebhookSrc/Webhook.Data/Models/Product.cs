using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class Product : BaseModel
    {
        public Guid ProductID { get; set; }
        public string Name { get; set; }
        public string Description { get; set; }
        public double Price { get; set; }
        public string ReferenceID { get; set; }
        public string ProductUrl { get; set; }
        public string ProductImgUrl { get; set; }
        public Guid DistributorID { get; set; }
        [ForeignKey("DistributorID")]
        public virtual Distributor Distributor { get; set; }
        public bool IsPreOrder { get; set; } = false;
        public string PreOrderDate { get; set; }
        public bool IsAvailable { get; set; } = true;
        public Guid FufilmentChannelID { get; set; }
        [ForeignKey("FufilmentChannelID")]
        public virtual FufilmentChannel FufilmentChannel { get; set; }
    }
}
