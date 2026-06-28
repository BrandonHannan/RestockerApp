using Microsoft.AspNetCore.Http.HttpResults;
using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Util.Helpers;

namespace Webhook.Services.DistributorService
{
    public class DistributorService : BaseService, IDistributorService
    {
        public DistributorService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<Distributor> GetDistributorAsync(Guid distributorId)
        {
            return await _dbContext.Distributors.FirstOrDefaultAsync(d => d.DistributorID == distributorId && !d.IsDeleted);
        }

        public async Task<List<Distributor>> GetDistributorsAsync()
        {
            var distributors = await _dbContext.Distributors.Where(d => !d.IsDeleted).ToListAsync();
            return distributors;
        }

        public async Task AddOrUpdateDistributor(Distributor distributor)
        {
            IsValidDistributor(distributor);

            if (distributor.DistributorID == Guid.Empty)
            {
                distributor.DistributorID = Guid.NewGuid();
                distributor.Created = DateTime.Now;
                distributor.Updated = DateTime.Now;

                _dbContext.Distributors.Add(distributor);
            }
            else
            {
                var foundDistributor = await _dbContext.Distributors.FirstOrDefaultAsync(d => d.DistributorID == distributor.DistributorID && !d.IsDeleted);

                if (foundDistributor == null)
                {
                    throw new Exception("Distributor does not exist or has been deleted");
                }

                foundDistributor.Name = distributor.Name;
                foundDistributor.BaseUrl = distributor.BaseUrl;
                foundDistributor.SitemapUrl = distributor.SitemapUrl;
                foundDistributor.IsActive = distributor.IsActive;
                foundDistributor.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteDistributor(Guid distributorId)
        {
            var foundDistributor = await _dbContext.Distributors.FirstOrDefaultAsync(d => d.DistributorID == distributorId && !d.IsDeleted);

            if (foundDistributor == null)
            {
                throw new Exception("Distributor does not exist or has already been deleted");
            }

            foundDistributor.IsDeleted = true;
            foundDistributor.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidDistributor(Distributor distributor)
        {
            if (distributor == null)
            {
                throw new ArgumentException("Invalid distributor");
            }

            if (string.IsNullOrEmpty(distributor.Name))
            {
                throw new ArgumentException("Invalid name for new distributor");
            }

            if (string.IsNullOrEmpty(distributor.BaseUrl) || !ValidUrl.IsValidUrl(distributor.BaseUrl))
            {
                throw new ArgumentException("Invalid base url for new distributor");
            }

            if (string.IsNullOrEmpty(distributor.SitemapUrl) || !ValidUrl.IsValidUrl(distributor.SitemapUrl))
            {
                throw new ArgumentException("Invalid sitemap url for new distributor");
            }

            return true;
        }
    }
}
