#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include "BigWAvailabilityClient.h"
#include "BigWDiscoveryLoop.h"
#include "BigWHttpTransport.h"
#include "BigWInventoryLoop.h"
#include "BigWSearchClient.h"
#include "BigWSitemapResolver.h"
#include "BigWStoresClient.h"
#include "CdpClient.h"
#include "Config.h"
#include "ConstructorClient.h"
#include "Database.h"
#include "GatewayTransport.h"
#include "DiscoveryLoop.h"
#include "HttpClient.h"
#include "InventoryLoop.h"
#include "KmartGraphQLClient.h"
#include "KmartHttpTransport.h"
#include "NotifierManager.h"
#include "SitemapClient.h"
#include "StopToken.h"

using namespace restocker;

namespace {

std::vector<StopToken*> g_stops;

void handleSignal(int) {
    for (auto* s : g_stops) {
        if (s) s->requestStop();
    }
}

struct Args {
    std::string config_path = "config.json";
    bool once = false;
    bool discovery_only = false;
    bool inventory_only = false;
    bool dry_run = false;
    bool test_notify = false;
    bool help = false;
    std::string distributor = "all";  // "all" | "kmart" | "bigw"
};

void printUsage(const char* exe) {
    std::printf(
        "Usage: %s [options]\n"
        "  --config <path>     config file (default config.json)\n"
        "  --once              run one discovery + one inventory pass, then exit\n"
        "  --discovery-only    only run the discovery loop/pass\n"
        "  --inventory-only    only run the inventory loop/pass\n"
        "  --distributor <d>   which pipeline(s) to run: all (default) | kmart | bigw\n"
        "  --dry-run           detect restocks but do not POST to notifiers\n"
        "  --test-notify       send a synthetic restock event to all notifiers and exit\n"
        "  --help              show this help\n",
        exe);
}

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            a.config_path = argv[++i];
        } else if (arg == "--once") {
            a.once = true;
        } else if (arg == "--discovery-only") {
            a.discovery_only = true;
        } else if (arg == "--inventory-only") {
            a.inventory_only = true;
        } else if (arg == "--distributor" && i + 1 < argc) {
            a.distributor = argv[++i];
        } else if (arg == "--dry-run") {
            a.dry_run = true;
        } else if (arg == "--test-notify") {
            a.test_notify = true;
        } else if (arg == "--help" || arg == "-h") {
            a.help = true;
        } else {
            spdlog::warn("ignoring unknown argument: {}", arg);
        }
    }
    return a;
}

}  // namespace

// Initializes libcurl once for the process and cleans up on scope exit.
struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

int main(int argc, char** argv) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    CurlGlobal curl_global;

    Args args = parseArgs(argc, argv);
    if (args.help) {
        printUsage(argv[0]);
        return 0;
    }

    Config cfg;
    try {
        cfg = Config::loadFromFile(args.config_path);
    } catch (const std::exception& e) {
        spdlog::error("config error: {}", e.what());
        return 1;
    }

    HttpClient http(cfg.http);

    // --test-notify: short-circuit before touching the DB / network APIs.
    if (args.test_notify) {
        NotifierManager notifiers(cfg.notifiers, http, /*dry_run=*/false);
        RestockEvent e;
        e.distributor = (args.distributor == "bigw") ? static_cast<int>(Distributor::BigW)
                                                      : static_cast<int>(Distributor::Kmart);
        e.keycode = "43519781";
        e.name = "Pokemon TCG: Surging Sparks Blister Pack (TEST)";
        e.url = "/product/pokemon-trading-card-game:-scarlet-and-violet-surging-sparks-blister-pack-assorted-43519781/";
        e.image_url = "https://www.kmart.com.au/wcsstore/Kmart/images/products/43519781_1.jpg";
        e.channel = "HOME_DELIVERY";
        e.previous = 0;
        e.available = 5;
        e.price = 9.50;
        e.is_preorder = true;
        e.preorder_release_date = "2026-07-18";
        e.home_delivery = 5;
        e.click_collect_total = 12;
        e.stores = {
            {"1155", "Broadway", "Low", "(02) 9282 6600", 4},
            {"1084", "Bondi Eastgate", "High", "(02) 8305 7400", -1},
            {"1079", "Chatswood", "Low", "(02) 9934 7400", 2},
        };
        e.timestamp = static_cast<std::int64_t>(std::time(nullptr));
        notifiers.notifyAll(e);
        return 0;
    }

    Database db(cfg.database.path);
    // Separate stop tokens per pipeline so a Kmart discovery "wake" only nudges the
    // Kmart inventory loop (and vice-versa); SIGINT/SIGTERM stops both.
    StopToken stop;
    StopToken bigw_stop;
    g_stops = {&stop, &bigw_stop};
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    ConstructorClient constructor(cfg.constructor, http, cfg.kmart.target_state);
    SitemapClient sitemap(cfg.constructor, http);

    // Inventory transport. Default "http": replay captured Akamai cookies (+ optional
    // bearer) over curl-impersonate, with a browser cookie re-harvest on repeated
    // failure. "browser": route every call through a real headless browser (CDP).
    std::unique_ptr<CdpClient> cdp;
    std::unique_ptr<KmartHttpTransport> kmart_http;
    IGatewayTransport* gateway = nullptr;
    if (cfg.kmart.transport == "browser") {
        cdp = std::make_unique<CdpClient>(cfg.browser);
        gateway = cdp.get();
        spdlog::info("inventory transport: browser (CDP)");
    } else {
        // The CdpClient here is the lazy cookie harvester — it only spawns a browser
        // when the HTTP path fails repeatedly (or has no seed cookie).
        cdp = std::make_unique<CdpClient>(cfg.browser);
        kmart_http = std::make_unique<KmartHttpTransport>(cfg.kmart, http, cdp.get(), db);
        gateway = kmart_http.get();
        spdlog::info("inventory transport: http (cookie-replay, browser re-harvest on failure)");
    }

    KmartGraphQLClient kmart(cfg.kmart, *gateway);
    NotifierManager notifiers(cfg.notifiers, http, args.dry_run);

    DiscoveryLoop discovery(cfg, constructor, sitemap, db, stop);
    InventoryLoop inventory(cfg, kmart, db, notifiers, stop);

    // BigW pipeline (parallel to Kmart, sharing db + notifiers). Constructed only
    // when enabled and selected. Its Akamai harvester navigates bigw.com.au and
    // filters cookies on that domain.
    const bool want_kmart = (args.distributor == "all" || args.distributor == "kmart");
    const bool want_bigw =
        (args.distributor == "all" || args.distributor == "bigw") && cfg.bigw.enabled;

    std::unique_ptr<CdpClient> bigw_cdp;
    std::unique_ptr<BigWHttpTransport> bigw_transport;
    std::unique_ptr<BigWSearchClient> bigw_search;
    std::unique_ptr<BigWSitemapResolver> bigw_resolver;
    std::unique_ptr<BigWAvailabilityClient> bigw_avail;
    std::unique_ptr<BigWStoresClient> bigw_stores;
    std::unique_ptr<BigWDiscoveryLoop> bigw_discovery;
    std::unique_ptr<BigWInventoryLoop> bigw_inventory;
    if (want_bigw) {
        BrowserConfig bigw_browser = cfg.browser;
        bigw_browser.nav_url = "https://www.bigw.com.au/";
        bigw_browser.cookie_domain = "bigw.com.au";
        bigw_cdp = std::make_unique<CdpClient>(bigw_browser);
        bigw_transport = std::make_unique<BigWHttpTransport>(cfg.bigw, http, bigw_cdp.get(), db);
        bigw_search = std::make_unique<BigWSearchClient>(cfg.bigw, *bigw_transport);
        bigw_resolver = std::make_unique<BigWSitemapResolver>(cfg.bigw, http);
        bigw_avail = std::make_unique<BigWAvailabilityClient>(cfg.bigw, *bigw_transport);
        bigw_stores = std::make_unique<BigWStoresClient>(cfg.bigw, *bigw_transport);
        bigw_discovery =
            std::make_unique<BigWDiscoveryLoop>(cfg, *bigw_search, *bigw_resolver, db, bigw_stop);
        bigw_inventory = std::make_unique<BigWInventoryLoop>(cfg, *bigw_avail, *bigw_stores, db,
                                                             notifiers, bigw_stop);
        spdlog::info("bigw pipeline enabled (store={}, state={})", cfg.bigw.store_id,
                     cfg.bigw.state);
    }

    const bool run_discovery = !args.inventory_only;
    const bool run_inventory = !args.discovery_only;

    if (args.once) {
        if (want_kmart && run_discovery) discovery.runOnce();
        if (want_kmart && run_inventory) inventory.runOnce();
        if (want_bigw && run_discovery) bigw_discovery->runOnce();
        if (want_bigw && run_inventory) bigw_inventory->runOnce();
        return 0;
    }

    spdlog::info("RestockerApp starting (postcode={}, dry_run={})", cfg.kmart.postcode,
                 args.dry_run);

    std::thread t_discovery, t_inventory, t_bigw_discovery, t_bigw_inventory;
    if (want_kmart && run_discovery) t_discovery = std::thread([&] { discovery.run(); });
    if (want_kmart && run_inventory) t_inventory = std::thread([&] { inventory.run(); });
    if (want_bigw && run_discovery)
        t_bigw_discovery = std::thread([&] { bigw_discovery->run(); });
    if (want_bigw && run_inventory)
        t_bigw_inventory = std::thread([&] { bigw_inventory->run(); });

    if (t_discovery.joinable()) t_discovery.join();
    if (t_inventory.joinable()) t_inventory.join();
    if (t_bigw_discovery.joinable()) t_bigw_discovery.join();
    if (t_bigw_inventory.joinable()) t_bigw_inventory.join();

    spdlog::info("RestockerApp shut down cleanly");
    return 0;
}
