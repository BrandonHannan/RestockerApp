using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Mvc;
using System.Security.Claims;

namespace Webhook.API.Controllers
{
    [Authorize]
    [ApiController]
    [Route("api/[controller]")]
    public abstract class BaseController : ControllerBase
    {
        protected readonly ILogger _logger;

        protected BaseController(ILogger logger)
        {
            _logger = logger;
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
    }
}
