using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using System.Security.Claims;
using Webhook.API.Filtering;
using Webhook.API.Models;
using Webhook.Data;
using Webhook.Data.Models;
using Webhook.Services.UserServices;

namespace Webhook.API.Controllers
{
    [ApiController]
    [Route("api/[controller]")]
    [AllowedIP("192.168.1.100", "127.0.0.1")]
    public class UserController : ControllerBase
    {

        protected readonly ILogger<UserController> _logger;
        private readonly WebhookDbContext _context;
        private readonly IUserService _userService;

        public UserController(ILogger<UserController> logger, WebhookDbContext context, IUserService userService)
        {
            _logger = logger;
            _context = context;
            _userService = userService;
        }

        protected string CurrentUserId
        {
            get
            {
                return User.FindFirst(ClaimTypes.NameIdentifier)?.Value;
            }
        }

        // Extracts the Email from the token claims
        protected string CurrentUserEmail
        {
            get
            {
                return User.FindFirst(ClaimTypes.Email)?.Value;
            }
        }

        // Example: Check if the user has a specific role
        // Not implemented yet
        protected bool IsAdmin
        {
            get
            {
                return User.IsInRole("Admin");
            }
        }

        [HttpPost("login")]
        public async Task<IActionResult> Login([FromBody] LoginRequest request)
        {
            var user = await _context.Users.FirstOrDefaultAsync(u => u.Email == request.Email && !u.IsDeleted);

            if (user == null)
            {
                // Always return generic error messages for security (don't reveal if the email exists)
                return Unauthorized("Invalid email or password.");
            }

            bool isPasswordValid = BCrypt.Net.BCrypt.Verify(request.Password, user.Password);

            if (!isPasswordValid)
            {
                return Unauthorized("Invalid email or password.");
            }

            var token = _userService.GenerateJwtToken(user);

            return Ok(new { Token = token });
        }

        [HttpPost("signup")]
        public async Task<IActionResult> SignUp([FromBody] SignUpRequest request)
        {
            if (!_userService.IsValidEmail(request.Email)){
                return BadRequest("Invalid email provided");
            }

            var user = new User
            {
                UserId = Guid.NewGuid(),
                Email = request.Email,
                Username = request.Username,
                Password = request.Password
            };

            await _userService.CreateUser(user);

            return Ok();
        }
    }
}
