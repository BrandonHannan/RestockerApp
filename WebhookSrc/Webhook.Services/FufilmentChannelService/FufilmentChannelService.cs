using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.FufilmentChannelService
{
    public class FufilmentChannelService : BaseService, IFufilmentChannelService
    {
        public FufilmentChannelService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<FufilmentChannel> GetFufilmentChannelAsync(Guid fufilmentChannelId)
        {
            return await _dbContext.fufilmentChannels.FirstOrDefaultAsync(f => f.FufilmentChannelID == fufilmentChannelId && !f.IsDeleted);
        }

        public async Task<List<FufilmentChannel>> GetFufilmentChannelsAsync()
        {
            var fufilmentChannels = await _dbContext.fufilmentChannels.Where(f => !f.IsDeleted).ToListAsync();
            return fufilmentChannels;
        }

        public async Task AddOrUpdateFufilmentChannel(FufilmentChannel fufilmentChannel)
        {
            IsValidFufilmentChannel(fufilmentChannel);

            if (fufilmentChannel.FufilmentChannelID == Guid.Empty)
            {
                fufilmentChannel.FufilmentChannelID = Guid.NewGuid();
                fufilmentChannel.Created = DateTime.Now;
                fufilmentChannel.Updated = DateTime.Now;

                _dbContext.fufilmentChannels.Add(fufilmentChannel);
            }
            else
            {
                var foundFufilmentChannel = await _dbContext.fufilmentChannels.FirstOrDefaultAsync(f => f.FufilmentChannelID == fufilmentChannel.FufilmentChannelID && !f.IsDeleted);

                if (foundFufilmentChannel == null)
                {
                    throw new Exception("Fufilment channel does not exist or has been deleted");
                }

                foundFufilmentChannel.Name = fufilmentChannel.Name;
                foundFufilmentChannel.ReferenceID = fufilmentChannel.ReferenceID;
                foundFufilmentChannel.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteFufilmentChannel(Guid fufilmentChannelId)
        {
            var foundFufilmentChannel = await _dbContext.fufilmentChannels.FirstOrDefaultAsync(f => f.FufilmentChannelID == fufilmentChannelId && !f.IsDeleted);

            if (foundFufilmentChannel == null)
            {
                throw new Exception("Fufilment channel does not exist or has already been deleted");
            }

            foundFufilmentChannel.IsDeleted = true;
            foundFufilmentChannel.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidFufilmentChannel(FufilmentChannel fufilmentChannel)
        {
            if (fufilmentChannel == null)
            {
                throw new ArgumentException("Invalid fufilment channel");
            }

            if (string.IsNullOrEmpty(fufilmentChannel.Name))
            {
                throw new ArgumentException("Invalid name for new fufilment channel");
            }

            return true;
        }
    }
}
