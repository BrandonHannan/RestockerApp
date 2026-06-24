using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Webhook.API.ExceptionHandler;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.UserServices
{
    public class UserService : BaseService
    {
        private const int SaltSize = 16;
        private const int KeySize = 32;
        private const int Iterations = 100_000;
        private static readonly HashAlgorithmName HashAlgorithm = HashAlgorithmName.SHA256;

        public UserService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<User> GetUserByIdAsync(Guid userId)
        {
            return await _dbContext.User.FirstOrDefaultAsync(user => user.UserId == userId && !user.IsDeleted);
        }

        public async Task<User> ValidateUserAsync(Guid userId, string password)
        {
            var user = await GetUserByIdAsync(userId);

            if (user == null)
            {
                throw new WebhookException("User does not exist", HttpStatusCode.NotFound);
            }

            byte[] secretBytes = Encoding.UTF8.GetBytes(password);

        }
    }
}

//using System;
//using System.Security.Cryptography;
//using System.Text;

//public class PasswordHasher
//{
//    // Define secure defaults
//    private const int SaltSize = 16; // 128-bit salt
//    private const int KeySize = 32;  // 256-bit hash
//    private const int Iterations = 100_000; // High work factor
//    private static readonly HashAlgorithmName HashAlgorithm = HashAlgorithmName.SHA256;

//    /// <summary>
//    /// Generates a secure hash and salt combo.
//    /// </summary>
//    public static (string Hash, string Salt) HashString(string input)
//    {
//        // 1. Generate a secure random salt
//        byte[] saltBytes = RandomNumberGenerator.GetBytes(SaltSize);

//        // 2. Derive the hash bytes using PBKDF2
//        byte[] hashBytes = Rfc2898DeriveBytes.GetBytes(
//            Encoding.UTF8.GetBytes(input),
//            saltBytes,
//            Iterations,
//            HashAlgorithm,
//            KeySize
//        );

//        // 3. Return both as Base64 strings to store in your database
//        return (Convert.ToBase64String(hashBytes), Convert.ToBase64String(saltBytes));
//    }

//    /// <summary>
//    /// Verifies the input matches the stored hash using the stored salt.
//    /// </summary>
//    public static bool VerifyString(string input, string storedHash, string storedSalt)
//    {
//        byte[] saltBytes = Convert.FromBase64String(storedSalt);
//        byte[] expectedHashBytes = Convert.FromBase64String(storedHash);

//        // Compute the hash of the input using the existing salt parameters
//        byte[] testHashBytes = Rfc2898DeriveBytes.GetBytes(
//            Encoding.UTF8.GetBytes(input),
//            saltBytes,
//            Iterations,
//            HashAlgorithm,
//            KeySize
//        );

//        // Protects against timing attacks by comparing bytes in fixed time
//        return CryptographicOperations.FixedTimeEquals(expectedHashBytes, testHashBytes);
//    }
//}

