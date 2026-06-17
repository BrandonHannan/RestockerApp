#pragma once

#include <string>
#include <vector>
#include <map>

namespace restocker {

struct ConstructorConfig {
    std::string base_url = "https://ac.cnstrc.com";
    std::string key = "key_GZTqlLr41FS2p7AY";
    std::string client_version = "ciojs-client-2.77.1";
    std::vector<std::string> search_terms{"pokemon cards"};  // legacy search discovery (unused)
    int num_results_per_page = 60;
    int max_pages = 3;
    std::string url_prefix_filter = "/product/pokemon-trading-card-game:";
    int page_delay_ms = 750;
    int min_ratelimit_remaining = 20;

    // Discovery now joins the Kmart product sitemap (authoritative list of TCG
    // product URLs / keycodes) against the Constructor.io *browse* endpoint
    // (status enrichment: preorder, fulfilment channel, regional stateOOS).
    // The browse group id is the Pokemon TCG category in Constructor's catalogue.
    std::string browse_group_id = "abfdf5b2d48e682ca75bfe87a0ecba17";
    std::string browse_sort_by = "relevance";
    std::string browse_sort_order = "descending";

    // Sitemap sweep: fetch the index, keep <loc>s containing product_sitemap_filter,
    // scan each product sitemap for url_prefix_filter, extract the trailing-digit
    // keycode. Driven by conditional GETs (ETag/Last-Modified + <lastmod> diffing)
    // so the ~30s poll only refetches sitemaps that actually changed.
    std::string sitemap_index_url = "https://www.kmart.com.au/sitemap-index.xml";
    std::string product_sitemap_filter = "product-sitemap";
    int sitemap_max_concurrency = 6;
};

struct KmartConfig {
    std::string graphql_url = "https://api.kmart.com.au/gateway/graphql";
    std::string postcode = "4221";
    std::string country = "AU";
    // State whose regional availability gates the polling tier. If this state is
    // flagged out-of-stock in a product's Constructor stateOOS map, the product is
    // stored but not polled. Empty disables the gate (everything is polled).
    std::string target_state = "QLD";
    int batch_size = 30;
    int batch_delay_ms = 1000;
    // Max nearby stores to list in an alert (from the getFindInStore enrichment).
    int instore_max = 8;
    // How to deliver the gateway POST: "http" (cookie-replay over curl-impersonate;
    // default) or "browser" (real headless browser via CDP, kept as a fallback).
    std::string transport = "http";
    // Captured Akamai cookie jar ("name=value; name=value; ...") replayed on the
    // gateway POST. Seeds the "http" transport; refreshed automatically by a browser
    // re-harvest after repeated failures (the freshest value is persisted to the DB).
    std::string cookie;
    // Optional OAuth bearer token. When non-empty it is sent as "Authorization:
    // Bearer <auth_token>". Leave empty if the cookies alone suffice.
    std::string auth_token;
    // User-Agent for the "http" transport's app-mode replay. Must match the client
    // that produced `cookie`/`auth_token` (the Kmart mobile app, by default) so the
    // Akamai cookies validate.
    std::string user_agent =
        "Mozilla/5.0 (iPhone; CPU iPhone OS 18_7 like Mac OS X) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Mobile/15E148 KmartApp/3.4.3";
    // Consecutive gateway failures on the "http" transport before opening a browser
    // to harvest fresh cookies and retry.
    int harvest_after_failures = 3;
    // Optional extra headers added to the gateway POST (e.g. a custom user-agent).
    // Most captured tracing headers (newrelic/traceparent) are not required.
    std::map<std::string, std::string> extra_headers;
};

struct BrowserConfig {
    // Path to a Chromium-based browser (Chrome/Edge/Chromium). Empty = auto-detect.
    std::string executable_path;
    // Akamai serves "Access Denied" to headless Chrome, so default to a real
    // (headful) window. On a headless Linux host, run under a virtual display
    // (e.g. xvfb-run). Set true only if a future stealth setup evades detection.
    bool headless = false;
    std::string nav_url = "https://www.kmart.com.au/";  // origin to navigate before fetching
    // Substring a harvested cookie's domain must contain to be kept. Lets the same
    // CdpClient harvest for different origins (e.g. "bigw.com.au" for BigW).
    std::string cookie_domain = "kmart.com.au";
    int page_settle_ms = 4000;     // wait after load for Akamai sensor to validate cookies
    int relaunch_every_cycles = 0; // 0 = never proactively relaunch (only on error)
    int cdp_timeout_ms = 30000;    // per CDP command timeout
};

struct BigWConfig {
    bool enabled = false;  // gate the whole BigW pipeline
    // Endpoints.
    std::string search_url = "https://api.bigw.com.au/search/v1/search";
    std::string availability_url = "https://api.bigw.com.au/api/availability/v0/product/";  // + {id}?...
    std::string stores_url = "https://api.bigw.com.au/api/stores/v0/list";
    std::string sitemap_index_url = "https://www.bigw.com.au/sitemap.xml";
    std::string product_sitemap_filter = "product-en-aud";  // matches product-en-aud-N.xml
    // Search request params.
    std::string search_text = "pokemon tcg";
    std::string store_id = "0284";
    std::string state = "QLD";
    std::string zone = "DDBURLEIGHHEADS";
    int per_page = 48;
    int max_pages = 10;
    std::string client_id = "mobile";
    // Availability request params.
    std::string delivery_postcode = "4221";
    std::string delivery_suburb = "ELANORA";
    // Discovery filter: keep only results carrying this specification entry.
    std::string required_spec_name = "EssentialSafety";
    std::string required_spec_value = "For ages 6+";
    // Akamai transport (mirrors KmartConfig). Availability/stores GETs replay the
    // captured _abck cookie jar; refreshed by a browser re-harvest on failure.
    std::string user_agent = "BigwApp/4.45.4 (ios - 26.5)";
    std::string cookie;
    std::string transport = "http";  // "http" | "browser"
    int harvest_after_failures = 3;
    std::map<std::string, std::string> extra_headers;
    // Cadence. BigW polls availability one product at a time, so it gets its own
    // (slower) intervals and a per-product throttle.
    int search_seconds = 120;
    int search_jitter_seconds = 15;
    int availability_seconds = 300;
    int availability_jitter_seconds = 30;
    int stores_refresh_seconds = 86400;  // store list changes rarely
    int per_product_delay_ms = 400;      // throttle the one-by-one availability GETs
    int instore_max = 8;
};

struct IntervalsConfig {
    // Fast discovery loop: poll the product sitemap. Cheap thanks to conditional
    // GETs (304 in steady state), so it runs often to catch new SKUs in seconds.
    int sitemap_seconds = 30;
    int sitemap_jitter_seconds = 5;
    // How often to re-sweep the Constructor browse group to refresh the status of
    // ALL existing product rows (preorder/fulfilment/tracked/price).
    int browse_refresh_seconds = 300;
    // Inventory idle cadence; usually preempted by the discovery wake trigger.
    int inventory_seconds = 300;
    int inventory_jitter_seconds = 30;
};

struct DiscordConfig {
    bool enabled = false;
    std::string webhook_url;        // default / fallback (unknown fulfilment channel)
    std::string webhook_instore;    // fulfilment_channel == 2
    std::string webhook_online;     // fulfilment_channel 3/5, not pre-order
    std::string webhook_preorder;   // fulfilment_channel 3/5, pre-order
};

struct GenericNotifierConfig {
    bool enabled = false;
    std::string url;
    std::map<std::string, std::string> headers;
};

struct NotifiersConfig {
    DiscordConfig discord;
    GenericNotifierConfig generic;
};

struct HttpConfig {
    std::string user_agent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    int timeout_ms = 15000;
    std::string proxy;
    // When true, apply curl-impersonate's Chrome TLS/JA3 + HTTP-2 fingerprint to
    // every request (needed to pass Akamai on the Kmart GraphQL gateway). The
    // target is a curl-impersonate browser profile, e.g. "chrome131", "chrome136".
    bool impersonate = true;
    std::string impersonate_target = "chrome131";
};

struct DatabaseConfig {
    std::string path = "restocker.db";
};

struct Config {
    ConstructorConfig constructor;
    KmartConfig kmart;
    BigWConfig bigw;
    BrowserConfig browser;
    IntervalsConfig intervals;
    NotifiersConfig notifiers;
    HttpConfig http;
    DatabaseConfig database;

    // Loads and validates config from a JSON file. Throws std::runtime_error
    // on missing file, parse failure, or invalid values.
    static Config loadFromFile(const std::string& path);
};

}  // namespace restocker
