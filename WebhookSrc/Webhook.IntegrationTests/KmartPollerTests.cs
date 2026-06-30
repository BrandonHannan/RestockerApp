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

            // Real browser service -> real headless Chromium + live Kmart API.
            // This is what makes the test an integration test rather than a unit test.
            services.AddSingleton<PlaywrightBrowserService>();

            // The following deps are required by the constructor but are not used
            // by PollProductAvailabilityAsync; they only need to be resolvable.
            services.AddHttpClient();                                   // IHttpClientFactory
            services.AddSingleton<IConfiguration>(
                new ConfigurationBuilder().Build());                    // empty IConfiguration
            services.AddDbContext<WebhookDbContext>(o =>
                o.UseInMemoryDatabase("KmartPollerTests"));             // no-op DbContext
            services.AddSingleton(Mock.Of<IProductService>());          // stub

            services.AddTransient<KmartPoller>();
            await using var provider = services.BuildServiceProvider();

            var poller = provider.GetRequiredService<KmartPoller>();

            // Act: launches real headless Chromium, primes Akamai cookies on
            // www.kmart.com.au, then POSTs the GraphQL query via an in-page fetch.
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
