#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "Models.h"

namespace restocker {

class BigWHttpTransport;

// Inventory client for the BigW availability API. Polls one product at a time
// (GET /api/availability/v0/product/{id}) and maps the per-channel availability
// booleans onto ChannelStock rows the shared restock engine understands.
class BigWAvailabilityClient {
public:
    BigWAvailabilityClient(BigWConfig cfg, BigWHttpTransport& transport);

    struct Result {
        std::vector<ChannelStock> stocks;  // distributor = BigW
        bool ok = false;
        std::string error;
    };

    // Query availability for a single product id at the configured store.
    Result queryAvailability(const std::string& product_id);

    // Exposed for testing: build the availability request URL for a product id.
    std::string buildUrl(const std::string& product_id) const;

private:
    BigWConfig cfg_;
    BigWHttpTransport& transport_;
};

// Exposed for testing: parse a BigW availability response body for one product id
// at `store_id` into ChannelStock rows. Availability is taken from the per-channel
// `available` boolean (quantity is unreliable): 1 when available, else 0. Channels:
// instore -> IN_STORE (store_id), pickup -> CLICK_AND_COLLECT (national + store_id),
// any delivery method available -> HOME_DELIVERY (national).
std::vector<ChannelStock> parseBigWAvailability(const std::string& body,
                                                const std::string& product_id,
                                                const std::string& store_id);

}  // namespace restocker
