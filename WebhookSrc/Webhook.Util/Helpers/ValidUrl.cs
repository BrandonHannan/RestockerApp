using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Util.Helpers
{
    public class ValidUrl
    {
        public static bool IsValidUrl(string url)
        {
            // Try to parse the URL as an Absolute URI
            if (Uri.TryCreate(url, UriKind.Absolute, out Uri? uriResult))
            {
                // Ensure the scheme is HTTP or HTTPS
                return uriResult.Scheme == Uri.UriSchemeHttp || uriResult.Scheme == Uri.UriSchemeHttps;
            }

            return false;
        }
    }
}
