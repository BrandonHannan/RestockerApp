using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.StockTypeService
{
    public interface IStockTypeService
    {
        public Task<StockType> GetStockTypeAsync(Guid stockTypeId);
        public Task<List<StockType>> GetStockTypesAsync();
        public Task AddOrUpdateStockType(StockType stockType);
        public Task DeleteStockType(Guid stockTypeId);
    }
}
