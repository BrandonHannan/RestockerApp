using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.LocationService;

namespace Webhook.API.Controllers
{
    public class LocationController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly ILocationService _service;

        public LocationController(ILogger<LocationController> logger, WebhookDbContext context, ILocationService locationService) : base(logger)
        {
            _context = context;
            _service = locationService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<Location>), 200)]
        public async Task<IActionResult> GetLocations()
        {
            var locations = await _service.GetLocationsAsync();
            return Ok(locations);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(Location), 200)]
        public async Task<IActionResult> GetLocationById(Guid id)
        {
            var location = await _service.GetLocationAsync(id);

            if (location == null)
            {
                return NotFound();
            }

            return Ok(location);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateLocation([FromBody]Location location)
        {
            await _service.AddOrUpdateLocation(location);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteLocation(Guid id)
        {
            await _service.DeleteLocation(id);
            return NoContent();
        }
    }
}
