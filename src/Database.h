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

    // Insert a newly discovered product or refresh an existing one. Returns
    // true if this product was not previously in the table (i.e. newly found).
    bool upsertProduct(const Product& p);

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
