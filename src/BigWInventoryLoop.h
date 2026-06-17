#pragma once

#include "BigWAvailabilityClient.h"
#include "BigWStoresClient.h"
#include "Config.h"
#include "Database.h"
#include "NotifierManager.h"
#include "StopToken.h"

namespace restocker {

// Inventory loop for BigW. Polls availability for every tracked BigW product one
// at a time (the API is per-product), persists each per-channel reading, and fires
// a restock alert when availability increases — reusing the shared restock engine.
//
// Unlike Kmart it polls ALL tracked products (not just out-of-stock ones): BigW
// discovery only surfaces in-stock items, so polling in-stock products is how we
// observe them going out of stock and thereby re-arm the next restock alert.
class BigWInventoryLoop {
public:
    BigWInventoryLoop(const Config& cfg, BigWAvailabilityClient& client, BigWStoresClient& stores,
                      Database& db, NotifierManager& notifiers, StopToken& stop);

    int runOnce();
    void run();

private:
    const Config& cfg_;
    BigWAvailabilityClient& client_;
    BigWStoresClient& stores_;
    Database& db_;
    NotifierManager& notifiers_;
    StopToken& stop_;
};

}  // namespace restocker
