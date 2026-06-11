#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace restocker {

// A product discovered via the Constructor.io search API.
struct Product {
    std::string variation_id;        // numeric keycode as text, e.g. "43519781"
    std::string name;                // display "value"
    std::string url;                 // e.g. "/product/pokemon-trading-card-game:-..."
    std::string brand;
    std::string image_url;           // product image (absolute URL), may be empty
    double price = 0.0;
    bool is_preorder = false;
    std::string preorder_release_date;  // ISO date string, may be empty
    // False when target_state appears (out of stock) in the Constructor stateOOS
    // map; such items are inserted but not promoted to the polling tier.
    bool tracked = true;
    // Kmart FulfilmentChannel policy: 2 = in-store only, 3 = standard (HD+CnC),
    // 5 = online (+ possible in-store). 0 = unknown/absent (treated like 3/5).
    int fulfilment_channel = 0;
};

// One stock reading for a (keycode, channel, location) tuple from the GraphQL gateway.
struct ChannelStock {
    std::string keycode;
    std::string channel;       // "HOME_DELIVERY" | "CLICK_AND_COLLECT" | "IN_STORE"
    std::string location_id;   // "" for national HOME_DELIVERY / CnC total
    int available = 0;
};

// Per-store availability for the consolidated alert. The qualitative level + name
// + phone come from the getFindInStore query; `available` (numeric units) is merged
// in by location_id from getProductAvailability when known (-1 = unknown).
struct StoreStock {
    std::string location_id;
    std::string location_name;
    std::string stock_level;   // qualitative, e.g. "Low" / "High"
    std::string phone;
    int available = -1;
};

// Emitted when available stock for a product increases. Drives notifications.
// Carries one consolidated, enriched payload per restocked product.
struct RestockEvent {
    std::string keycode;
    std::string name;          // product display name (may be empty if unknown)
    std::string url;           // absolute Kmart URL
    std::string image_url;     // product image URL (may be empty)
    std::string channel;       // trigger summary: channel with the largest increase
    std::string location_id;
    int previous = 0;          // trigger summary: previous available for that channel
    int available = 0;         // trigger summary: new available for that channel
    bool is_preorder = false;
    std::string preorder_release_date;  // ISO date string, may be empty
    double price = 0.0;
    int home_delivery = -1;       // national Home Delivery units (-1 = unknown)
    int click_collect_total = -1; // Click & Collect total units (-1 = unknown)
    std::vector<StoreStock> stores;  // nearby store breakdown
    std::int64_t timestamp = 0; // epoch seconds
    // Routing: 2 = in-store, 3/5 = online/pre-order, 0 = unknown. Selects which
    // Discord webhook the alert is delivered to.
    int fulfilment_channel = 0;
};

}  // namespace restocker
