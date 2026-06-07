#pragma once

#include "Config.h"
#include "Database.h"
#include "KmartGraphQLClient.h"
#include "NotifierManager.h"
#include "StopToken.h"

namespace restocker {

// Fast loop: polls live availability for tracked, out-of-stock keycodes and
// fires notifications when stock increases.
class InventoryLoop {
public:
    InventoryLoop(const Config& cfg, KmartGraphQLClient& client, Database& db,
                  NotifierManager& notifiers, StopToken& stop);

    // Run one inventory pass over all qualifying keycodes. Returns number of
    // restock alerts fired. Safe to call standalone for --once / --inventory-only.
    int runOnce();

    void run();

private:
    // Process all freshly fetched rows for one product: persist every row, and
    // if any channel/location increased, build one consolidated, enriched alert
    // and fire it. Returns true if an alert fired.
    bool processProduct(const std::string& keycode, const std::vector<ChannelStock>& rows);

    const Config& cfg_;
    KmartGraphQLClient& client_;
    Database& db_;
    NotifierManager& notifiers_;
    StopToken& stop_;
};

}  // namespace restocker
