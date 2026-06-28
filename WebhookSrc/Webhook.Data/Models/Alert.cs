using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class Alert
    {
        public Guid AlertID { get; set; }
        public Guid StockID { get; set; }
        [ForeignKey("StockID")]
        public virtual Stock Stock { get; set; }
        public int StockChange { get; set; }
        public DateTime Created { get; set; }
    }
}
