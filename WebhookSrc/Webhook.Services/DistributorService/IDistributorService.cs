using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.DistributorService
{
    public interface IDistributorService
    {
        public Task<Distributor> GetDistributorAsync(Guid distributorId);
        public Task<List<Distributor>> GetDistributorsAsync();
        public Task AddOrUpdateDistributor(Distributor distributor);
        public Task DeleteDistributor(Guid distributorId);
    }
}
