using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.DistributorService;

namespace Webhook.API.Controllers
{
    public class DistributorController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IDistributorService _service;

        public DistributorController(ILogger<DistributorController> logger, WebhookDbContext context, IDistributorService distributorService) : base(logger)
        {
            _context = context;
            _service = distributorService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<Distributor>), 200)]
        public async Task<IActionResult> GetDistributors()
        {
            var distributors = await _service.GetDistributorsAsync();
            return Ok(distributors);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(Distributor), 200)]
        public async Task<IActionResult> GetDistributorById(Guid id)
        {
            var distributor = await _service.GetDistributorAsync(id);

            if (distributor == null)
            {
                return NotFound();
            }

            return Ok(distributor);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateDistributor([FromBody]Distributor distributor)
        {
            await _service.AddOrUpdateDistributor(distributor);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteDistributor(Guid id)
        {
            await _service.DeleteDistributor(id);
            return NoContent();
        }
    }
}
