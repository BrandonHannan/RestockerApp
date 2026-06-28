using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.StockService;

namespace Webhook.API.Controllers
{
    public class StockController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IStockService _service;

        public StockController(ILogger<StockController> logger, WebhookDbContext context, IStockService stockService) : base(logger)
        {
            _context = context;
            _service = stockService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<Stock>), 200)]
        public async Task<IActionResult> GetStockList()
        {
            var stockList = await _service.GetStockListAsync();
            return Ok(stockList);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(Stock), 200)]
        public async Task<IActionResult> GetStockById(Guid id)
        {
            var stock = await _service.GetStockAsync(id);

            if (stock == null)
            {
                return NotFound();
            }

            return Ok(stock);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateStock([FromBody]Stock stock)
        {
            await _service.AddOrUpdateStock(stock);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteStock(Guid id)
        {
            await _service.DeleteStock(id);
            return NoContent();
        }
    }
}
