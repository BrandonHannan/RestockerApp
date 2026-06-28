using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using Microsoft.IdentityModel.Tokens;
using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.Linq;
using System.Net;
using System.Net.Mail;
using System.Security.Claims;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Webhook.API.ExceptionHandler;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.UserServices
{
    public class UserService : BaseService, IUserService
    {
        private readonly IConfiguration _config;

        public UserService(WebhookDbContext dbContext, IConfiguration config) : base(dbContext)
        {
            _config = config;
        }

        public async Task<User> GetUserByIdAsync(Guid userId)
        {
            return await _dbContext.Users.FirstOrDefaultAsync(user => user.UserId == userId && !user.IsDeleted);
        }

        public string GenerateJwtToken(User user)
        {
            // Grab the secret key from your appsettings.json
            var securityKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(_config["Jwt:Key"]));
            var credentials = new SigningCredentials(securityKey, SecurityAlgorithms.HmacSha256);

            // Add claims (data embedded in the token)
            var claims = new[]
            {
                new Claim(ClaimTypes.NameIdentifier, user.UserId.ToString()),
                new Claim(ClaimTypes.Email, user.Email),
                new Claim(ClaimTypes.Name, user.Username)
                // Add Role claims here later if needed: new Claim(ClaimTypes.Role, "Admin")
            };

            var token = new JwtSecurityToken(
                issuer: _config["Jwt:Issuer"],
                audience: _config["Jwt:Audience"],
                claims: claims,
                expires: DateTime.Now.AddHours(2), // Token expires in 2 hours
                signingCredentials: credentials);

            return new JwtSecurityTokenHandler().WriteToken(token);
        }

        public async Task CreateUser(User user)
        {
            string hashedPassword = BCrypt.Net.BCrypt.HashPassword(user.Password);
            user.Password = hashedPassword;

            user.Created = DateTime.Now;
            user.Updated = DateTime.Now;

            _dbContext.Users.Add(user);

            await _dbContext.SaveChangesAsync();
        }

        public bool IsValidEmail(string email)
        {
            if (string.IsNullOrWhiteSpace(email))
            {
                return false;
            }

            try
            {
                var mailAddress = new MailAddress(email);

                // Extra check: MailAddress accepts strings like "Name <email@domain.com>".
                // To ensure the input string is strictly the address itself, compare them.
                return mailAddress.Address == email;
            }
            catch (FormatException)
            {
                return false;
            }
        }
    }
}

