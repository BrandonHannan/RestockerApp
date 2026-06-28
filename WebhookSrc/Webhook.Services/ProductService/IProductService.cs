using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.ProductService
{
    public interface IProductService
    {
        public Task<Product> GetProductAsync(Guid productId);
        public Task<List<Product>> GetProductsAsync();
        public Task AddOrUpdateProduct(Product product, bool checkValidity = true);
        public Task DeleteProduct(Guid productId);
    }
}
