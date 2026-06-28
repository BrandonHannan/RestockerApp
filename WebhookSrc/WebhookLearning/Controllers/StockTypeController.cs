using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.StockTypeService;

namespace Webhook.API.Controllers
{
    public class StockTypeController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IStockTypeService _service;

        public StockTypeController(ILogger<StockTypeController> logger, WebhookDbContext context, IStockTypeService stockTypeService) : base(logger)
        {
            _context = context;
            _service = stockTypeService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<StockType>), 200)]
        public async Task<IActionResult> GetStockTypes()
        {
            var stockTypes = await _service.GetStockTypesAsync();
            return Ok(stockTypes);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(StockType), 200)]
        public async Task<IActionResult> GetStockTypeById(Guid id)
        {
            var stockType = await _service.GetStockTypeAsync(id);

            if (stockType == null)
            {
                return NotFound();
            }

            return Ok(stockType);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateStockType([FromBody]StockType stockType)
        {
            await _service.AddOrUpdateStockType(stockType);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteStockType(Guid id)
        {
            await _service.DeleteStockType(id);
            return NoContent();
        }
    }
}
