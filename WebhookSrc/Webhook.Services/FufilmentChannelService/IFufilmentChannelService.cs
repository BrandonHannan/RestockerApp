using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.FufilmentChannelService
{
    public interface IFufilmentChannelService
    {
        public Task<FufilmentChannel> GetFufilmentChannelAsync(Guid fufilmentChannelId);
        public Task<List<FufilmentChannel>> GetFufilmentChannelsAsync();
        public Task AddOrUpdateFufilmentChannel(FufilmentChannel fufilmentChannel);
        public Task DeleteFufilmentChannel(Guid fufilmentChannelId);
    }
}
