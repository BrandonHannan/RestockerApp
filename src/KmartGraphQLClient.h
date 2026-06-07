#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "GatewayTransport.h"
#include "Models.h"

namespace restocker {

// Inventory client for the Kmart GraphQL gateway. Builds the
// getProductAvailability payload for a batch of keycodes and parses the
// per-channel availability response. The actual delivery is delegated to an
// IGatewayTransport (curl-impersonate HTTP, or the headless-browser CDP path).
class KmartGraphQLClient {
public:
    KmartGraphQLClient(KmartConfig cfg, IGatewayTransport& transport);

    struct BatchResult {
        std::vector<ChannelStock> stocks;
        bool ok = false;
        std::string error;
    };

    // Query availability for up to batch_size keycodes in one POST.
    BatchResult queryAvailability(const std::vector<std::string>& keycodes);

    // Enrichment: per-store stock (name, qualitative level, phone) for one
    // keycode via the getFindInStore query. Returns empty on any failure.
    std::vector<StoreStock> queryFindInStore(const std::string& keycode);

    // Exposed for testing: build the JSON request body for a keycode batch.
    std::string buildPayload(const std::vector<std::string>& keycodes) const;

    // Exposed for testing: build the getFindInStore request body.
    std::string buildFindInStorePayload(const std::string& keycode) const;

private:
    KmartConfig cfg_;
    IGatewayTransport& transport_;
};

// Exposed for testing: parse a getProductAvailability response body into
// per-(keycode,channel,location) stock rows.
std::vector<ChannelStock> parseAvailability(const std::string& body);

// Exposed for testing: parse a getFindInStore response body into per-store rows.
std::vector<StoreStock> parseFindInStore(const std::string& body);

}  // namespace restocker
