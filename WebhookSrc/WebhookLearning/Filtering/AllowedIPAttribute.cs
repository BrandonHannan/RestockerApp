using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.Filters;
using System.Net;

namespace Webhook.API.Filtering
{
    public class AllowedIPAttribute : ActionFilterAttribute
    {
        private readonly string[] _allowedIPs;

        public AllowedIPAttribute(params string[] allowedIPs)
        {
            _allowedIPs = allowedIPs;
        }

        public override void OnActionExecuting(ActionExecutingContext context)
        {
            var remoteIp = context.HttpContext.Connection.RemoteIpAddress;
            var isAllowed = false;

            if (remoteIp != null)
            {
                // Handle IPv4 mapped to IPv6 addresses (common in local environments)
                if (remoteIp.IsIPv4MappedToIPv6)
                {
                    remoteIp = remoteIp.MapToIPv4();
                }

                foreach (var address in _allowedIPs)
                {
                    if (IPAddress.TryParse(address, out var allowedIp) && allowedIp.Equals(remoteIp))
                    {
                        isAllowed = true;
                        break;
                    }
                }
            }

            if (!isAllowed)
            {
                // Return 403 Forbidden if the IP doesn't match
                context.Result = new StatusCodeResult(StatusCodes.Status403Forbidden);
                return;
            }

            base.OnActionExecuting(context);
        }
    }
}
