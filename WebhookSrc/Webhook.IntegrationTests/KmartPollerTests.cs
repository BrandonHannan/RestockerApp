using System.Collections.Concurrent;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Moq;
using Webhook.Data;
using Webhook.Poller;
using Webhook.Services;
using Webhook.Services.ProductService;
using Xunit;

namespace Webhook.IntegrationTests
{
    public class KmartPollerTests
    {
        [Fact]
        public async Task Test_PollProductAvailabilityAsync_Returns200()
        {
            // Arrange: build an isolated service provider satisfying every
            // KmartPoller constructor dependency.
            var capturing = new CapturingLoggerProvider();

            var services = new ServiceCollection();
            services.AddLogging(c =>
            {
                c.AddConsole();
                c.AddProvider(capturing);
            });

            // Injected by the constructor but not exercised by this method.
            services.AddSingleton<PlaywrightBrowserService>();

            // PollProductAvailabilityAsync POSTs to the Akamai-protected Kmart API using a valid
            // cookie set supplied via Kmart:AvailabilityCookie. These cookies expire within hours;
            // provide a fresh set via the KMART_AVAILABILITY_COOKIE env var or the kmart.cookie.txt
            // file (copied next to the test binary). See README / plan for how to refresh.
            var cookie = Environment.GetEnvironmentVariable("KMART_AVAILABILITY_COOKIE");
            if (string.IsNullOrWhiteSpace(cookie))
            {
                var cookieFile = Path.Combine(AppContext.BaseDirectory, "kmart.cookie.txt");
                if (File.Exists(cookieFile))
                {
                    cookie = File.ReadAllText(cookieFile).Trim();
                }
            }

            services.AddHttpClient();                                   // IHttpClientFactory
            services.AddSingleton<IConfiguration>(new ConfigurationBuilder()
                .AddInMemoryCollection(new Dictionary<string, string?>
                {
                    ["Kmart:AvailabilityCookie"] = cookie
                })
                .Build());
            services.AddDbContext<WebhookDbContext>(o =>
                o.UseInMemoryDatabase("KmartPollerTests"));             // no-op DbContext
            services.AddSingleton(Mock.Of<IProductService>());          // stub

            services.AddTransient<KmartPoller>();
            await using var provider = services.BuildServiceProvider();

            var poller = provider.GetRequiredService<KmartPoller>();

            // Act: POSTs the getProductAvailability query to the live Akamai-protected Kmart API
            // using the configured cookie set.
            await poller.PollProductAvailabilityAsync(CancellationToken.None);

            // Assert: the request returned HTTP 200 (logged as "Availability Data (200)")
            // and the Akamai 403 path ("GraphQL Request Failed") was not taken.
            Assert.DoesNotContain(capturing.Entries,
                e => e.Level == LogLevel.Error && e.Message.Contains("GraphQL Request Failed"));

            Assert.Contains(capturing.Entries,
                e => e.Level == LogLevel.Information && e.Message.Contains("Availability Data (200)"));
        }
    }

    /// <summary>
    /// Minimal in-memory <see cref="ILoggerProvider"/> that records every log entry so tests
    /// can assert on what the code under test logged.
    /// </summary>
    internal sealed class CapturingLoggerProvider : ILoggerProvider
    {
        public ConcurrentQueue<(LogLevel Level, string Message)> Entries { get; } = new();

        public ILogger CreateLogger(string categoryName) => new CapturingLogger(Entries);

        public void Dispose() { }

        private sealed class CapturingLogger : ILogger
        {
            private readonly ConcurrentQueue<(LogLevel, string)> _entries;

            public CapturingLogger(ConcurrentQueue<(LogLevel, string)> entries) => _entries = entries;

            public IDisposable? BeginScope<TState>(TState state) where TState : notnull => null;

            public bool IsEnabled(LogLevel logLevel) => true;

            public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception,
                Func<TState, Exception?, string> formatter)
            {
                _entries.Enqueue((logLevel, formatter(state, exception)));
            }
        }
    }
}
