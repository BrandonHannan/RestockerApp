using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.StockTypeService
{
    public class StockTypeService : BaseService, IStockTypeService
    {
        public StockTypeService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<StockType> GetStockTypeAsync(Guid stockTypeId)
        {
            return await _dbContext.StockTypes.FirstOrDefaultAsync(s => s.StockTypeID == stockTypeId && !s.IsDeleted);
        }

        public async Task<List<StockType>> GetStockTypesAsync()
        {
            var stockTypes = await _dbContext.StockTypes.Where(s => !s.IsDeleted).ToListAsync();
            return stockTypes;
        }

        public async Task AddOrUpdateStockType(StockType stockType)
        {
            IsValidStockType(stockType);

            if (stockType.StockTypeID == Guid.Empty)
            {
                stockType.StockTypeID = Guid.NewGuid();
                stockType.Created = DateTime.Now;
                stockType.Updated = DateTime.Now;

                _dbContext.StockTypes.Add(stockType);
            }
            else
            {
                var foundStockType = await _dbContext.StockTypes.FirstOrDefaultAsync(s => s.StockTypeID == stockType.StockTypeID && !s.IsDeleted);

                if (foundStockType == null)
                {
                    throw new Exception("Stock type does not exist or has been deleted");
                }

                foundStockType.Name = stockType.Name;
                foundStockType.ReferenceID = stockType.ReferenceID;
                foundStockType.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteStockType(Guid stockTypeId)
        {
            var foundStockType = await _dbContext.StockTypes.FirstOrDefaultAsync(s => s.StockTypeID == stockTypeId && !s.IsDeleted);

            if (foundStockType == null)
            {
                throw new Exception("Stock type does not exist or has already been deleted");
            }

            foundStockType.IsDeleted = true;
            foundStockType.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidStockType(StockType stockType)
        {
            if (stockType == null)
            {
                throw new ArgumentException("Invalid stock type");
            }

            if (string.IsNullOrEmpty(stockType.Name))
            {
                throw new ArgumentException("Invalid name for new stock type");
            }

            return true;
        }
    }
}
