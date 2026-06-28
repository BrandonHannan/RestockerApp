using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.LocationService
{
    public class LocationService : BaseService, ILocationService
    {
        public LocationService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<Location> GetLocationAsync(Guid locationId)
        {
            return await _dbContext.Locations.FirstOrDefaultAsync(l => l.LocationID == locationId && !l.IsDeleted);
        }

        public async Task<List<Location>> GetLocationsAsync()
        {
            var locations = await _dbContext.Locations.Where(l => !l.IsDeleted).ToListAsync();
            return locations;
        }

        public async Task AddOrUpdateLocation(Location location)
        {
            IsValidLocation(location);

            if (location.LocationID == Guid.Empty)
            {
                location.LocationID = Guid.NewGuid();
                location.Created = DateTime.Now;
                location.Updated = DateTime.Now;

                _dbContext.Locations.Add(location);
            }
            else
            {
                var foundLocation = await _dbContext.Locations.FirstOrDefaultAsync(l => l.LocationID == location.LocationID && !l.IsDeleted);

                if (foundLocation == null)
                {
                    throw new Exception("Location does not exist or has been deleted");
                }

                foundLocation.Name = location.Name;
                foundLocation.ReferenceID = location.ReferenceID;
                foundLocation.DistributorID = location.DistributorID;
                foundLocation.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteLocation(Guid locationId)
        {
            var foundLocation = await _dbContext.Locations.FirstOrDefaultAsync(l => l.LocationID == locationId && !l.IsDeleted);

            if (foundLocation == null)
            {
                throw new Exception("Location does not exist or has already been deleted");
            }

            foundLocation.IsDeleted = true;
            foundLocation.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidLocation(Location location)
        {
            if (location == null)
            {
                throw new ArgumentException("Invalid location");
            }

            if (string.IsNullOrEmpty(location.Name))
            {
                throw new ArgumentException("Invalid name for new location");
            }

            if (location.DistributorID == Guid.Empty)
            {
                throw new ArgumentException("Invalid distributor for new location");
            }

            return true;
        }
    }
}
