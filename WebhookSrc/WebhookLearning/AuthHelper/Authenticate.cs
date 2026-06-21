using Microsoft.IdentityModel.Tokens;
using System.IdentityModel.Tokens.Jwt;
using System.Text;
using Webhook.Services.UserServices;

namespace Webhook.API.AuthHelper
{
    public class Authenticate
    {
        private readonly RequestDelegate _next;
        private readonly string _rsaKey;

        public Authenticate(RequestDelegate next, IConfiguration configuration)
        {
            _rsaKey = configuration.GetConnectionString("RSAKey")!;
            _next = next;
        }

        public async Task InvokeAsync(HttpContext context)
        {
            // Extract token from the Authorization header (Format: Bearer <token>)
            var token = context.Request.Headers["Authorization"].FirstOrDefault()?.Split(" ").Last();

            if (token != null)
            {
                try
                {
                    var tokenHandler = new JwtSecurityTokenHandler();
                    var key = Encoding.ASCII.GetBytes(_rsaKey);

                    tokenHandler.ValidateToken(token, new TokenValidationParameters
                    {
                        ValidateIssuerSigningKey = true,
                        IssuerSigningKey = new SymmetricSecurityKey(key),
                        ValidateIssuer = false,
                        ValidateAudience = false,
                        ClockSkew = TimeSpan.Zero // Immediate expiration check
                    }, out SecurityToken validatedToken);

                    var jwtToken = (JwtSecurityToken)validatedToken;

                    // Attach user claims to the HttpContext so controllers can read them via context.User
                    context.Items["User"] = jwtToken.Claims.FirstOrDefault(x => x.Type == "id")?.Value;
                }
                catch
                {
                    // Token validation failed; treat request as unauthenticated.
                    // Do not throw an exception here unless you want to block all anonymous endpoints.
                    throw new Exception("Unauthenticated user");
                }
            }

            await _next(context);
        }
    }
}
