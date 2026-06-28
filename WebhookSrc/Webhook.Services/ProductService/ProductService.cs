using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Util.Helpers;

namespace Webhook.Services.ProductService
{
    public class ProductService : BaseService, IProductService
    {
        public ProductService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<Product> GetProductAsync(Guid productId)
        {
            return await _dbContext.Products.FirstOrDefaultAsync(p => p.ProductID == productId && !p.IsDeleted);
        }

        public async Task<List<Product>> GetProductsAsync()
        {
            var products = await _dbContext.Products.Where(p => !p.IsDeleted).ToListAsync();
            return products;
        }

        public async Task AddOrUpdateProduct(Product product, bool checkValidity = true)
        {
            if (checkValidity)
            {
                IsValidProduct(product);
            }

            if (product.ProductID == Guid.Empty)
            {
                product.ProductID = Guid.NewGuid();
                product.Created = DateTime.Now;
                product.Updated = DateTime.Now;

                _dbContext.Products.Add(product);
            }
            else
            {
                var foundProduct = await _dbContext.Products.FirstOrDefaultAsync(p => p.ProductID == product.ProductID && !p.IsDeleted);

                if (foundProduct == null)
                {
                    throw new Exception("Product does not exist or has been deleted");
                }

                foundProduct.ReferenceID = product.ReferenceID;
                foundProduct.Name = product.Name;
                foundProduct.Description = product.Description;
                foundProduct.Price = product.Price;
                foundProduct.ProductUrl = product.ProductUrl;
                foundProduct.ProductImgUrl = product.ProductImgUrl;
                foundProduct.DistributorID = product.DistributorID;
                foundProduct.IsPreOrder = product.IsPreOrder;
                foundProduct.PreOrderDate = product.PreOrderDate;
                foundProduct.IsAvailable = product.IsAvailable;
                foundProduct.FufilmentChannelID = product.FufilmentChannelID;
                foundProduct.Updated = DateTime.Now;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteProduct(Guid productId)
        {
            var foundProduct = await _dbContext.Products.FirstOrDefaultAsync(p => p.ProductID == productId && !p.IsDeleted);

            if (foundProduct == null)
            {
                throw new Exception("Product does not exist or has already been deleted");
            }

            foundProduct.IsDeleted = true;
            foundProduct.Updated = DateTime.Now;

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidProduct(Product product)
        {
            if (product == null)
            {
                throw new ArgumentException("Invalid product");
            }

            if (string.IsNullOrEmpty(product.Name))
            {
                throw new ArgumentException("Invalid name for new product");
            }

            if (string.IsNullOrEmpty(product.ProductUrl) || !ValidUrl.IsValidUrl(product.ProductUrl))
            {
                throw new ArgumentException("Invalid product url for new product");
            }

            if (product.DistributorID == Guid.Empty)
            {
                throw new ArgumentException("Invalid distributor for new product");
            }

            if (product.FufilmentChannelID == Guid.Empty)
            {
                throw new ArgumentException("Invalid fufilment channel for new product");
            }

            if (product.Price < 0)
            {
                throw new ArgumentException("Invalid price for new product");
            }

            return true;
        }
    }
}
