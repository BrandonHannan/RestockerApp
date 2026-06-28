using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.StockService
{
    public class StockService : BaseService, IStockService
    {
        public StockService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<Stock> GetStockAsync(Guid stockId)
        {
            return await _dbContext.Stock.FirstOrDefaultAsync(s => s.StockID == stockId && !s.IsDeleted);
        }

        public async Task<List<Stock>> GetStockListAsync()
        {
            var stockList = await _dbContext.Stock.Where(s => !s.IsDeleted).ToListAsync();
            return stockList;
        }

        public async Task AddOrUpdateStock(Stock stock)
        {
            IsValidStock(stock);

            if (stock.StockID == Guid.Empty)
            {
                stock.StockID = Guid.NewGuid();
                stock.Created = DateTime.Now;
                stock.Updated = DateTime.Now;

                _dbContext.Stock.Add(stock);
            }
            else
            {
                var foundStock = await _dbContext.Stock.FirstOrDefaultAsync(s => s.StockID == stock.StockID && !s.IsDeleted);

                if (foundStock == null)
                {
                    throw new Exception("Stock does not exist or has been deleted");
                }

                foundStock.StockAvailable = stock.StockAvailable;
                foundStock.StockTypeID = stock.StockTypeID;
                foundStock.ProductID = stock.ProductID;
                foundStock.LocationID = stock.LocationID;
                foundStock.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteStock(Guid stockId)
        {
            var foundStock = await _dbContext.Stock.FirstOrDefaultAsync(s => s.StockID == stockId && !s.IsDeleted);

            if (foundStock == null)
            {
                throw new Exception("Stock does not exist or has already been deleted");
            }

            foundStock.IsDeleted = true;
            foundStock.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidStock(Stock stock)
        {
            if (stock == null)
            {
                throw new ArgumentException("Invalid stock");
            }

            if (stock.StockAvailable < 0)
            {
                throw new ArgumentException("Invalid stock available for new stock");
            }

            if (stock.StockTypeID == Guid.Empty)
            {
                throw new ArgumentException("Invalid stock type for new stock");
            }

            if (stock.ProductID == Guid.Empty)
            {
                throw new ArgumentException("Invalid product for new stock");
            }

            return true;
        }
    }
}
