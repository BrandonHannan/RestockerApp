#pragma once

#include "BigWSearchClient.h"
#include "BigWSitemapResolver.h"
#include "Config.h"
#include "Database.h"
#include "StopToken.h"

namespace restocker {

// Discovery loop for BigW: paginates the search API for Pokemon TCG products,
// resolves each product's URL from the BigW sitemaps, upserts them into the
// shared products table (distributor = BigW), and wakes the BigW inventory loop
// when something changed.
class BigWDiscoveryLoop {
public:
    BigWDiscoveryLoop(const Config& cfg, BigWSearchClient& search,
                      BigWSitemapResolver& resolver, Database& db, StopToken& stop);

    // Run one discovery pass. Returns the number of newly discovered products.
    int runOnce();

    void run();

private:
    const Config& cfg_;
    BigWSearchClient& search_;
    BigWSitemapResolver& resolver_;
    Database& db_;
    StopToken& stop_;
};

}  // namespace restocker
