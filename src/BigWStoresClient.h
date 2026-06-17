#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "Config.h"
#include "Models.h"

namespace restocker {

class BigWHttpTransport;

// Fetches the BigW store directory (GET /api/stores/v0/list) and caches it, used
// to enrich restock alerts with store names/phone (BigW analog of Kmart's
// getFindInStore). The list changes rarely, so it is refreshed at most once per
// `stores_refresh_seconds`.
class BigWStoresClient {
public:
    BigWStoresClient(BigWConfig cfg, BigWHttpTransport& transport);

    // Cached store list (location_id, name, phone), refreshing it if stale or
    // never loaded. Returns the last good list on a failed refresh.
    const std::vector<StoreStock>& stores();

private:
    BigWConfig cfg_;
    BigWHttpTransport& transport_;

    std::vector<StoreStock> cache_;
    bool loaded_ = false;
    std::chrono::steady_clock::time_point last_refresh_{};
};

// Exposed for testing: parse a BigW stores-list response body into StoreStock rows
// (location_id = store id, location_name = name, phone = phoneNumber).
std::vector<StoreStock> parseBigWStores(const std::string& body);

}  // namespace restocker
