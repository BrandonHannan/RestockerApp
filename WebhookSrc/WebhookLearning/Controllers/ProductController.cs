using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.ProductService;

namespace Webhook.API.Controllers
{
    public class ProductController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IProductService _service;

        public ProductController(ILogger<ProductController> logger, WebhookDbContext context, IProductService productService) : base(logger)
        {
            _context = context;
            _service = productService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<Product>), 200)]
        public async Task<IActionResult> GetProducts()
        {
            var products = await _service.GetProductsAsync();
            return Ok(products);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(Product), 200)]
        public async Task<IActionResult> GetProductById(Guid id)
        {
            var product = await _service.GetProductAsync(id);

            if (product == null)
            {
                return NotFound();
            }

            return Ok(product);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateProduct([FromBody]Product product)
        {
            await _service.AddOrUpdateProduct(product);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteProduct(Guid id)
        {
            await _service.DeleteProduct(id);
            return NoContent();
        }
    }
}
