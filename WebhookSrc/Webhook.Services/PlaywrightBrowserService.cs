using Microsoft.Playwright;

namespace Webhook.Services
{
    public class PlaywrightBrowserService : IAsyncDisposable
    {
        private IPlaywright? _playwright;
        private IBrowser? _browser;
        private IBrowserContext? _context;
        private readonly SemaphoreSlim _initLock = new(1, 1);

        public async Task<IBrowserContext> GetContextAsync()
        {
            if (_context != null) return _context;

            await _initLock.WaitAsync();
            try
            {
                if (_context != null) return _context; // Double-check lock

                _playwright = await Playwright.CreateAsync();
                _browser = await _playwright.Chromium.LaunchAsync(new BrowserTypeLaunchOptions
                {
                    Headless = false,
                    // Remove the most obvious automation fingerprints so Akamai's bot manager will
                    // validate the _abck cookie instead of permanently keeping it in its ~-1~ state.
                    Args = new[]
                    {
                        "--disable-blink-features=AutomationControlled",
                        "--disable-features=IsolateOrigins,site-per-process",
                        "--disable-web-security",
                        "--disable-site-isolation-trials"
                    },
                    IgnoreDefaultArgs = new[] { "--enable-automation" }
                });

                _context = await _browser.NewContextAsync(new BrowserNewContextOptions
                {
                    UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36",
                    // Disable the page's Content-Security-Policy so an injected fetch to the
                    // api.kmart.com.au subdomain isn't blocked by the homepage's connect-src rules.
                    BypassCSP = true,
                    Locale = "en-AU",
                    TimezoneId = "Australia/Sydney",
                    ViewportSize = new ViewportSize { Width = 1280, Height = 800 }
                });

                // Mask the residual automation signals (navigator.webdriver, headless chrome quirks)
                // that bot managers fingerprint, before any page script runs.
                await _context.AddInitScriptAsync(@"
                    Object.defineProperty(navigator, 'webdriver', { get: () => undefined });
                    window.chrome = window.chrome || { runtime: {} };
                    Object.defineProperty(navigator, 'languages', { get: () => ['en-AU', 'en'] });
                    Object.defineProperty(navigator, 'plugins', { get: () => [1, 2, 3, 4, 5] });
                ");
                // Cookie/sensor priming is performed in GetPrimedPageAsync, which the poller
                // calls before issuing the availability request.
            }
            finally
            {
                _initLock.Release();
            }

            return _context;
        }

        /// <summary>
        /// Returns an open page that has navigated to the Kmart site and waited for Akamai's
        /// Bot Manager sensor to validate the <c>_abck</c> cookie. Requests issued from this page
        /// (e.g. via <c>page.EvaluateAsync</c> + <c>fetch</c>) inherit the validated cookies and
        /// the browser's real headers/fingerprint, which is what gets past the 403 bot block.
        /// The caller is responsible for closing the returned page.
        /// </summary>
        public async Task<IPage> GetPrimedPageAsync(string url = "https://www.kmart.com.au/")
        {
            var context = await GetContextAsync();

            var page = await context.NewPageAsync();

            // Akamai blocks a cold first-hit to a deep product URL ("Access Denied"). Always warm up
            // on the homepage first to obtain and validate _abck, then navigate to the target URL
            // (now a trusted session) and re-validate, since each navigation can refresh _abck.
            await page.GotoAsync("https://www.kmart.com.au/", new PageGotoOptions
            {
                WaitUntil = WaitUntilState.DOMContentLoaded,
                Timeout = 60000
            });
            Console.WriteLine($"[PRIME] homepage: {page.Url} title='{await page.TitleAsync()}'");
            await WaitForAbckValidationAsync(context, page);

            if (url != "https://www.kmart.com.au/")
            {
                await page.GotoAsync(url, new PageGotoOptions
                {
                    WaitUntil = WaitUntilState.DOMContentLoaded,
                    Timeout = 60000
                });
                Console.WriteLine($"[PRIME] target: {page.Url} title='{await page.TitleAsync()}'");
                await WaitForAbckValidationAsync(context, page);
            }

            return page;
        }

        /// <summary>
        /// Akamai sets an unvalidated _abck (~-1~) on load, then its sensor JS collects behavioural
        /// data and POSTs it; the cookie flips to validated (~0~) on a later request. Feed the sensor
        /// realistic mouse/scroll entropy and reload once so the submission lands, polling until _abck
        /// validates (or timing out and letting the eventual request surface its status).
        /// </summary>
        private static async Task WaitForAbckValidationAsync(IBrowserContext context, IPage page)
        {
            var deadline = DateTime.UtcNow.AddSeconds(45);
            bool reloaded = false;
            int i = 0;
            while (DateTime.UtcNow < deadline)
            {
                try
                {
                    // Vary the interaction each iteration so the sensor sees human-like entropy.
                    await page.Mouse.MoveAsync(150 + (i % 5) * 90, 200 + (i % 4) * 70, new MouseMoveOptions { Steps = 8 });
                    await page.Mouse.WheelAsync(0, 350);
                    if (i % 3 == 2) await page.Mouse.WheelAsync(0, -200);
                }
                catch
                {
                    // Page interaction is best-effort; ignore transient errors while priming.
                }

                if (await IsAbckValidatedAsync(context))
                {
                    Console.WriteLine($"[PRIME] _abck validated after ~{i + 1}s");
                    return;
                }

                // Around the 12s mark, reload so the sensor's collected payload is submitted on a
                // fresh request — Akamai commonly validates _abck on the navigation after the POST.
                if (!reloaded && DateTime.UtcNow > deadline.AddSeconds(-33))
                {
                    reloaded = true;
                    try
                    {
                        await page.ReloadAsync(new PageReloadOptions { WaitUntil = WaitUntilState.DOMContentLoaded, Timeout = 60000 });
                    }
                    catch { /* best-effort */ }
                }

                await page.WaitForTimeoutAsync(1000);
                i++;
            }

            Console.WriteLine("[PRIME] _abck did not validate within the priming window.");
        }

        /// <summary>
        /// Akamai's <c>_abck</c> cookie is unvalidated while its second '~'-delimited segment is
        /// "-1"; once the sensor payload is accepted the segment becomes "0".
        /// </summary>
        private static async Task<bool> IsAbckValidatedAsync(IBrowserContext context)
        {
            var cookies = await context.CookiesAsync();
            var abck = cookies.FirstOrDefault(c => c.Name == "_abck");

            if (abck == null) return false;

            var segments = abck.Value.Split('~');
            return segments.Length > 1 && segments[1] == "0";
        }

        public async ValueTask DisposeAsync()
        {
            if (_browser != null) await _browser.CloseAsync();
            _playwright?.Dispose();
        }
    }
}