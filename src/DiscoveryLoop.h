#pragma once

#include "Config.h"
#include "ConstructorClient.h"
#include "Database.h"
#include "StopToken.h"

namespace restocker {

// Slow loop: periodically discovers new Pokemon TCG products via Constructor.io
// and persists them as tracked.
class DiscoveryLoop {
public:
    DiscoveryLoop(const Config& cfg, ConstructorClient& client, Database& db, StopToken& stop);

    // Run one discovery pass (all terms, all pages). Returns number of newly
    // discovered products. Safe to call standalone for --once / --discovery-only.
    int runOnce();

    // Blocking loop until stop is requested.
    void run();

private:
    const Config& cfg_;
    ConstructorClient& client_;
    Database& db_;
    StopToken& stop_;
};

}  // namespace restocker
