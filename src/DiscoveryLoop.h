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

// Fast discovery loop (~30s): sweeps the Kmart product sitemap (authoritative
// list of TCG products), inserts newly-seen keycodes, cross-references new
// keycodes (and, every browse_refresh_seconds, all rows) against the Constructor
// browse endpoint for status, and wakes the inventory loop whenever it learns
// something changed.
class DiscoveryLoop {
public:
    DiscoveryLoop(const Config& cfg, ConstructorClient& client, SitemapClient& sitemap,
                  Database& db, StopToken& stop);

    // Run one discovery pass. Returns the number of newly discovered products.
    // Safe to call standalone for --once / --discovery-only.
    int runOnce();

    // Blocking loop until stop is requested.
    void run();

private:
    // Sweep the Constructor browse group into a keycode -> status Product map.
    std::unordered_map<std::string, Product> sweepBrowseStatus();

    const Config& cfg_;
    ConstructorClient& client_;
    SitemapClient& sitemap_;
    Database& db_;
    StopToken& stop_;

    std::chrono::steady_clock::time_point last_browse_refresh_{};
    bool browse_refreshed_ = false;  // forces a full browse refresh on the first pass
};

}  // namespace restocker
