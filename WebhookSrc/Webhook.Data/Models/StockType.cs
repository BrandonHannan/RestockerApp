using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class StockType : BaseModel
    {
        public Guid StockTypeID { get; set; }
        public string Name { get; set; }
        public string ReferenceID { get; set; }
    }
}
