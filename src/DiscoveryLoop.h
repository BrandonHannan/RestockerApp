#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "Config.h"
#include "ConstructorClient.h"
#include "Database.h"
#include "SitemapClient.h"
#include "StopToken.h"

namespace restocker {

// Discovery loop: builds the Pokemon TCG product universe from the Kmart product
// sitemap (keycode -> URL, cached and refreshed periodically), enriches it with
// live status from the Constructor.io browse endpoint, and persists each product.
class DiscoveryLoop {
public:
    DiscoveryLoop(const Config& cfg, ConstructorClient& client, SitemapClient& sitemap,
                  Database& db, StopToken& stop);

    // Run one discovery pass (sitemap join + browse sweep). Returns number of
    // newly discovered products. Safe to call standalone for --once / --discovery-only.
    int runOnce();

    // Blocking loop until stop is requested.
    void run();

private:
    // Refresh the cached sitemap keycode->URL map if it is empty or stale. Keeps
    // the previous cache on a failed fetch.
    void refreshSitemapIfDue();

    // Sweep the Constructor browse group into a keycode -> status Product map.
    std::unordered_map<std::string, Product> sweepBrowseStatus();

    const Config& cfg_;
    ConstructorClient& client_;
    SitemapClient& sitemap_;
    Database& db_;
    StopToken& stop_;

    std::unordered_map<std::string, std::string> sitemap_cache_;  // keycode -> URL
    std::chrono::steady_clock::time_point last_sitemap_refresh_{};
    bool sitemap_loaded_ = false;
};

}  // namespace restocker
