using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.UserServices
{
    public class UserService : BaseService
    {
        public UserService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<User> GetUserByIdAsync(Guid userId)
        {
            return await _dbContext.User.FirstOrDefaultAsync(user => user.UserId == userId && !user.IsDeleted);
        }
    }
}
