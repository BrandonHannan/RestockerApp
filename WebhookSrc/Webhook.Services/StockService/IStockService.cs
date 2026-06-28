using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.StockService
{
    public interface IStockService
    {
        public Task<Stock> GetStockAsync(Guid stockId);
        public Task<List<Stock>> GetStockListAsync();
        public Task AddOrUpdateStock(Stock stock);
        public Task DeleteStock(Guid stockId);
    }
}
