using System.Net;

namespace Webhook.API.ExceptionHandler
{
    public class WebhookException : Exception
    {
        public HttpStatusCode _statusCode { get; }

        public WebhookException(string message, HttpStatusCode statusCode) : base(message)
        {
            _statusCode = statusCode;
        }
    }
}
