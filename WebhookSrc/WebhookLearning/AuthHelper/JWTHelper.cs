using Microsoft.IdentityModel.Tokens;
using System.CodeDom.Compiler;
using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Security.Cryptography;
using System.Text;
using Webhook.Data.Models;

namespace Webhook.API.AuthHelper
{
    public class JWTHelper
    {
        private readonly string _rsaKey;

        public JWTHelper(IConfiguration configuration)
        {
            _rsaKey = configuration.GetConnectionString("RSAKey")!;
        }

        public string GenerateJwt(User user)
        {
            // 1. Initialize RSA and import the private key
            using var rsa = RSA.Create();
            rsa.ImportFromPem(_rsaKey);

            // 2. Define the Signing Credentials explicitly using RS256
            var credentials = new SigningCredentials(
                new RsaSecurityKey(rsa),
                SecurityAlgorithms.RsaSha256
            );

            // 3. Configure the Token Structure
            var tokenDescriptor = new SecurityTokenDescriptor
            {
                Subject = new ClaimsIdentity(new[]
                {
                    new Claim(JwtRegisteredClaimNames.Sub, user.UserId.ToString()),
                    new Claim(JwtRegisteredClaimNames.Jti, Guid.NewGuid().ToString())
                }),
                // Audience = endpointAudience, // Restricts the token's validity to this specific endpoint
                Issuer = "Webhook.Api",
                Expires = DateTime.UtcNow.AddHours(24), // Keep lifetimes short for security
                SigningCredentials = credentials
            };

            // 4. Generate and serialize the token
            var handler = new JwtSecurityTokenHandler();
            var token = handler.CreateToken(tokenDescriptor);

            return handler.WriteToken(token);
        }
    }
}
