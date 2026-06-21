using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;

namespace Webhook.Services
{
    public class BaseService
    {
        protected readonly WebhookDbContext _dbContext;

        public BaseService(WebhookDbContext dbContext)
        {
            _dbContext = dbContext;
        }
    }
}
