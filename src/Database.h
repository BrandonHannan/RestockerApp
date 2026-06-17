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

    // Insert a product discovered from the catalogue with only its base fields
    // (distributor, product_id, url, name); enrichment columns keep their defaults.
    // Returns true only when a row was actually inserted (i.e. a genuinely new
    // (distributor, product_id)) — this drives "new product" detection in the
    // discovery loop. Existing rows are left untouched.
    bool insertProductIfAbsent(int distributor, const std::string& product_id,
                               const std::string& url, const std::string& name);

    // Update the browse-sourced status columns for an existing product (brand,
    // image, price, pre-order, tracked, fulfilment channel, and name when the
    // browse value is non-empty). Never touches `url` (the canonical sitemap URL)
    // and never inserts. No-op if the keycode is not present.
    void updateProductStatus(const Product& p);

    // Tracked products for one distributor that are currently out of stock
    // everywhere we know about (no inventory rows yet, or max known available
    // across channels == 0).
    std::vector<std::string> getTrackedOOSKeycodes(int distributor);

    // All tracked product ids for one distributor, regardless of known stock.
    // Used where discovery only surfaces in-stock items (BigW), so the inventory
    // loop must keep polling in-stock products to observe them going out of stock
    // (re-arming a future restock alert).
    std::vector<std::string> getTrackedKeycodes(int distributor);

    // Look up product display fields for an alert (name + absolute-friendly url).
    std::optional<Product> getProduct(int distributor, const std::string& product_id);

    // Previous stored availability for a tuple; std::nullopt if never seen.
    std::optional<int> getStock(int distributor, const std::string& keycode,
                                const std::string& channel, const std::string& location_id);

    void setStock(const ChannelStock& s);

    void recordAlert(const RestockEvent& e);

    // Persistent anon-session helpers for the Constructor API.
    std::string getOrCreateSessionId();
    long nextSessionSeq();  // monotonically increasing per call

    // Last Akamai cookie jar (and the browser User-Agent that produced it)
    // harvested for the "http" gateway transport, persisted across restarts.
    // Empty string if never harvested.
    std::string getKmartCookie();
    void setKmartCookie(const std::string& cookie);
    std::string getKmartUserAgent();
    void setKmartUserAgent(const std::string& user_agent);

    // Same as above, but for the BigW gateway transport (separate Akamai origin).
    std::string getBigWCookie();
    void setBigWCookie(const std::string& cookie);
    std::string getBigWUserAgent();
    void setBigWUserAgent(const std::string& user_agent);

private:
    std::string getMeta(const std::string& key);
    void setMeta(const std::string& key, const std::string& value);
    void initSchema();
    void migrateToV1();  // rename variation_id->product_id + add distributor dimension

    std::unique_ptr<SQLite::Database> db_;
    std::mutex mtx_;
};

}  // namespace restocker
