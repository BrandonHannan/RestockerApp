#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <thread>

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include "CdpClient.h"
#include "Config.h"
#include "ConstructorClient.h"
#include "Database.h"
#include "GatewayTransport.h"
#include "DiscoveryLoop.h"
#include "HttpClient.h"
#include "InventoryLoop.h"
#include "KmartGraphQLClient.h"
#include "NotifierManager.h"
#include "StopToken.h"

using namespace restocker;

namespace {

StopToken* g_stop = nullptr;

void handleSignal(int) {
    if (g_stop) g_stop->requestStop();
}

struct Args {
    std::string config_path = "config.json";
    bool once = false;
    bool discovery_only = false;
    bool inventory_only = false;
    bool dry_run = false;
    bool test_notify = false;
    bool help = false;
};

void printUsage(const char* exe) {
    std::printf(
        "Usage: %s [options]\n"
        "  --config <path>     config file (default config.json)\n"
        "  --once              run one discovery + one inventory pass, then exit\n"
        "  --discovery-only    only run the discovery loop/pass\n"
        "  --inventory-only    only run the inventory loop/pass\n"
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
    StopToken stop;
    g_stop = &stop;
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    ConstructorClient constructor(cfg.constructor, http, cfg.kmart.target_state);

    // Inventory transport: real headless browser (CDP) to defeat Akamai, or the
    // curl-impersonate HTTP path as a fallback.
    std::unique_ptr<CdpClient> cdp;
    IGatewayTransport* gateway = &http;
    if (cfg.kmart.transport == "browser") {
        cdp = std::make_unique<CdpClient>(cfg.browser);
        gateway = cdp.get();
        spdlog::info("inventory transport: browser (CDP)");
    } else {
        spdlog::info("inventory transport: http (curl-impersonate)");
    }

    KmartGraphQLClient kmart(cfg.kmart, *gateway);
    NotifierManager notifiers(cfg.notifiers, http, args.dry_run);

    DiscoveryLoop discovery(cfg, constructor, db, stop);
    InventoryLoop inventory(cfg, kmart, db, notifiers, stop);

    const bool run_discovery = !args.inventory_only;
    const bool run_inventory = !args.discovery_only;

    if (args.once) {
        if (run_discovery) discovery.runOnce();
        if (run_inventory) inventory.runOnce();
        return 0;
    }

    spdlog::info("RestockerApp starting (postcode={}, dry_run={})", cfg.kmart.postcode,
                 args.dry_run);

    std::thread t_discovery, t_inventory;
    if (run_discovery) t_discovery = std::thread([&] { discovery.run(); });
    if (run_inventory) t_inventory = std::thread([&] { inventory.run(); });

    if (t_discovery.joinable()) t_discovery.join();
    if (t_inventory.joinable()) t_inventory.join();

    spdlog::info("RestockerApp shut down cleanly");
    return 0;
}
