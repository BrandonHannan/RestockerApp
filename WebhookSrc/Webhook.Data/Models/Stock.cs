using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class Stock : BaseModel
    {
        public Guid StockID { get; set; }
        public int StockAvailable { get; set; }
        public Guid StockTypeID { get; set; }
        [ForeignKey("StockTypeID")]
        public virtual StockType StockType { get; set; }
        public Guid ProductID { get; set; }
        [ForeignKey("ProductID")]
        public virtual Product Product { get; set; }
        public Guid? LocationID { get; set; }
    }
}
