#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Models.h"

namespace restocker {

class Database;
class NotifierManager;

// Dependencies for the shared restock-decision logic. Lets both the Kmart and
// BigW inventory loops reuse one copy of the "available > previous -> alert"
// machinery while plugging in their own store-name enrichment and limits.
struct RestockDeps {
    Database& db;
    NotifierManager& notifiers;
    int distributor = 1;   // see Distributor enum
    int instore_max = 8;   // cap on the nearby-store list in an alert
    // Per-keycode store metadata lookup (name/phone), merged into the alert by
    // location_id. Kmart uses getFindInStore; BigW uses its cached store list.
    std::function<std::vector<StoreStock>(const std::string& keycode)> storeEnrich;
};

// Compare a batch of channel/location stock readings for one product against the
// stored baseline, persist the new baseline, and — when availability increased in
// any channel — build, send, and record one consolidated RestockEvent. Returns
// true when an alert was fired. Distributor-agnostic: works for Kmart numeric
// stock and BigW 0/1 availability alike.
bool processRestock(const std::string& keycode, const std::vector<ChannelStock>& rows,
                    const RestockDeps& deps);

}  // namespace restocker
