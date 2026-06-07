#pragma once

#include <string>
#include <vector>
#include <map>

namespace restocker {

struct ConstructorConfig {
    std::string base_url = "https://ac.cnstrc.com";
    std::string key = "key_GZTqlLr41FS2p7AY";
    std::string client_version = "ciojs-client-2.77.1";
    std::vector<std::string> search_terms{"pokemon cards"};
    int num_results_per_page = 60;
    int max_pages = 3;
    std::string url_prefix_filter = "/product/pokemon-trading-card-game:";
    int page_delay_ms = 750;
    int min_ratelimit_remaining = 20;
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
    // How to deliver the gateway POST: "browser" (real headless browser via CDP,
    // defeats Akamai) or "http" (curl-impersonate; blocked by Akamai on this
    // endpoint, kept as a fallback).
    std::string transport = "browser";
};

struct BrowserConfig {
    // Path to a Chromium-based browser (Chrome/Edge/Chromium). Empty = auto-detect.
    std::string executable_path;
    // Akamai serves "Access Denied" to headless Chrome, so default to a real
    // (headful) window. On a headless Linux host, run under a virtual display
    // (e.g. xvfb-run). Set true only if a future stealth setup evades detection.
    bool headless = false;
    std::string nav_url = "https://www.kmart.com.au/";  // origin to navigate before fetching
    int page_settle_ms = 4000;     // wait after load for Akamai sensor to validate cookies
    int relaunch_every_cycles = 0; // 0 = never proactively relaunch (only on error)
    int cdp_timeout_ms = 30000;    // per CDP command timeout
};

struct IntervalsConfig {
    int discovery_seconds = 2700;
    int discovery_jitter_seconds = 900;
    int inventory_seconds = 120;
    int inventory_jitter_seconds = 60;
};

struct DiscordConfig {
    bool enabled = false;
    std::string webhook_url;
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
