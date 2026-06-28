using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.WebhookConnectionService;

namespace Webhook.API.Controllers
{
    public class WebhookConnectionController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IWebhookConnectionService _service;

        public WebhookConnectionController(ILogger<WebhookConnectionController> logger, WebhookDbContext context, IWebhookConnectionService webhookConnectionService) : base(logger)
        {
            _context = context;
            _service = webhookConnectionService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<WebhookConnection>), 200)]
        public async Task<IActionResult> GetWebhookConnections()
        {
            var webhookConnections = await _service.GetWebhookConnectionsAsync();
            return Ok(webhookConnections);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(WebhookConnection), 200)]
        public async Task<IActionResult> GetWebhookConnectionById(Guid id)
        {
            var webhookConnection = await _service.GetWebhookConnectionAsync(id);

            if (webhookConnection == null)
            {
                return NotFound();
            }

            return Ok(webhookConnection);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateWebhookConnection([FromBody]WebhookConnection webhookConnection)
        {
            await _service.AddOrUpdateWebhookConnection(webhookConnection);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteWebhookConnection(Guid id)
        {
            await _service.DeleteWebhookConnection(id);
            return NoContent();
        }
    }
}
