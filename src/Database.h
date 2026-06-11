#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Models.h"

namespace SQLite {
class Database;
}

namespace restocker {

// SQLite-backed persistence. All public methods are internally synchronized so
// the discovery and inventory threads can share one instance safely.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Insert a product discovered from the sitemap with only its base fields
    // (keycode, url, name); enrichment columns keep their defaults. Returns true
    // only when a row was actually inserted (i.e. a genuinely new keycode) —
    // this drives "new product" detection in the discovery loop. Existing rows
    // are left untouched.
    bool insertProductIfAbsent(const std::string& keycode, const std::string& url,
                               const std::string& name);

    // Update the browse-sourced status columns for an existing product (brand,
    // image, price, pre-order, tracked, fulfilment channel, and name when the
    // browse value is non-empty). Never touches `url` (the canonical sitemap URL)
    // and never inserts. No-op if the keycode is not present.
    void updateProductStatus(const Product& p);

    // Tracked products that are currently out of stock everywhere we know about
    // (no inventory rows yet, or max known available across channels == 0).
    std::vector<std::string> getTrackedOOSKeycodes();

    // Look up product display fields for an alert (name + absolute-friendly url).
    std::optional<Product> getProduct(const std::string& keycode);

    // Previous stored availability for a tuple; std::nullopt if never seen.
    std::optional<int> getStock(const std::string& keycode, const std::string& channel,
                                const std::string& location_id);

    void setStock(const ChannelStock& s);

    void recordAlert(const RestockEvent& e);

    // Persistent anon-session helpers for the Constructor API.
    std::string getOrCreateSessionId();
    long nextSessionSeq();  // monotonically increasing per call

private:
    std::string getMeta(const std::string& key);
    void setMeta(const std::string& key, const std::string& value);
    void initSchema();

    std::unique_ptr<SQLite::Database> db_;
    std::mutex mtx_;
};

}  // namespace restocker
