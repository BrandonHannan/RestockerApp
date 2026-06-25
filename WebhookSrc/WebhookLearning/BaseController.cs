using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Mvc;
using System.Security.Claims;
using WebhookLearning.Controllers;

namespace Webhook.API
{
    [Authorize]
    [ApiController]
    [Route("api/v1/[controller]")]
    public abstract class BaseController : ControllerBase
    {
        protected readonly ILogger<BaseController> _logger;

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
