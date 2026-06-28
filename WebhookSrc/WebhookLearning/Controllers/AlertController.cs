using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.AlertService;

namespace Webhook.API.Controllers
{
    public class AlertController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IAlertService _service;

        public AlertController(ILogger<AlertController> logger, WebhookDbContext context, IAlertService alertService) : base(logger)
        {
            _context = context;
            _service = alertService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<Alert>), 200)]
        public async Task<IActionResult> GetAlerts()
        {
            var alerts = await _service.GetAlertsAsync();
            return Ok(alerts);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(Alert), 200)]
        public async Task<IActionResult> GetAlertById(Guid id)
        {
            var alert = await _service.GetAlertAsync(id);

            if (alert == null)
            {
                return NotFound();
            }

            return Ok(alert);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateAlert([FromBody]Alert alert)
        {
            await _service.AddOrUpdateAlert(alert);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteAlert(Guid id)
        {
            await _service.DeleteAlert(id);
            return NoContent();
        }
    }
}
