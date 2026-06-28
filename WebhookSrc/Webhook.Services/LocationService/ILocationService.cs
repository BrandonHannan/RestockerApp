using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.LocationService
{
    public interface ILocationService
    {
        public Task<Location> GetLocationAsync(Guid locationId);
        public Task<List<Location>> GetLocationsAsync();
        public Task AddOrUpdateLocation(Location location);
        public Task DeleteLocation(Guid locationId);
    }
}
