using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.UserServices
{
    public interface IUserService
    {
        public Task<User> GetUserByIdAsync(Guid userId);

        public string GenerateJwtToken(User user);

        public Task CreateUser(User user);

        public bool IsValidEmail(string email);
    }
}
