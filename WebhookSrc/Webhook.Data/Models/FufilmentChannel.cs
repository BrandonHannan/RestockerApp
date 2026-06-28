using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Data.Models
{
    public class FufilmentChannel : BaseModel
    {
        public Guid FufilmentChannelID { get; set; }
        public string Name { get; set; }
        public string ReferenceID { get; set; }
    }
}
