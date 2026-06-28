using Microsoft.AspNetCore.Mvc;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.FufilmentChannelService;

namespace Webhook.API.Controllers
{
    public class FufilmentChannelController : BaseController
    {
        private readonly WebhookDbContext _context;
        private readonly IFufilmentChannelService _service;

        public FufilmentChannelController(ILogger<FufilmentChannelController> logger, WebhookDbContext context, IFufilmentChannelService fufilmentChannelService) : base(logger)
        {
            _context = context;
            _service = fufilmentChannelService;
        }

        [HttpGet]
        [ProducesResponseType(typeof(List<FufilmentChannel>), 200)]
        public async Task<IActionResult> GetFufilmentChannels()
        {
            var fufilmentChannels = await _service.GetFufilmentChannelsAsync();
            return Ok(fufilmentChannels);
        }

        [HttpGet("{id:guid}")]
        [ProducesResponseType(typeof(FufilmentChannel), 200)]
        public async Task<IActionResult> GetFufilmentChannelById(Guid id)
        {
            var fufilmentChannel = await _service.GetFufilmentChannelAsync(id);

            if (fufilmentChannel == null)
            {
                return NotFound();
            }

            return Ok(fufilmentChannel);
        }

        [HttpPost]
        public async Task<IActionResult> AddOrUpdateFufilmentChannel([FromBody]FufilmentChannel fufilmentChannel)
        {
            await _service.AddOrUpdateFufilmentChannel(fufilmentChannel);
            return Ok();
        }

        [HttpDelete("{id:guid}")]
        public async Task<IActionResult> DeleteFufilmentChannel(Guid id)
        {
            await _service.DeleteFufilmentChannel(id);
            return NoContent();
        }
    }
}
