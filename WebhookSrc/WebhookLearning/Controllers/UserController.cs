using Microsoft.AspNetCore.Mvc;

namespace Webhook.API.Controllers
{
    [Route("login")]
    public class UserController : BaseController
    {
        [HttpPost]
        public async Task<IActionResult> Login()
        {

        }
    }
}
