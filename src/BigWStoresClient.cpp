#include "BigWStoresClient.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "BigWHttpTransport.h"

using json = nlohmann::json;

namespace restocker {

std::vector<StoreStock> parseBigWStores(const std::string& body) {
    std::vector<StoreStock> out;
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return out;

    // The response is a top-level map of storeId -> store object.
    for (const auto& kv : j.items()) {
        const json& s = kv.value();
        if (!s.is_object()) continue;
        StoreStock st;
        st.location_id = s.value("id", kv.key());
        st.location_name = s.value("name", std::string());
        st.phone = s.value("phoneNumber", std::string());
        if (st.location_id.empty()) continue;
        out.push_back(std::move(st));
    }
    return out;
}

BigWStoresClient::BigWStoresClient(BigWConfig cfg, BigWHttpTransport& transport)
    : cfg_(std::move(cfg)), transport_(transport) {}

const std::vector<StoreStock>& BigWStoresClient::stores() {
    auto now = std::chrono::steady_clock::now();
    bool stale = !loaded_ ||
                 (now - last_refresh_) >= std::chrono::seconds(cfg_.stores_refresh_seconds);
    if (!stale) return cache_;

    HttpResponse resp = transport_.get(cfg_.stores_url);
    if (!resp.ok()) {
        spdlog::warn("BigW stores list fetch failed: status={} err='{}'", resp.status_code,
                     resp.error);
        return cache_;  // keep the last good list (possibly empty)
    }

    std::vector<StoreStock> parsed = parseBigWStores(resp.text);
    if (!parsed.empty()) {
        cache_ = std::move(parsed);
        loaded_ = true;
        last_refresh_ = now;
        spdlog::info("BigW stores list: {} stores", cache_.size());
    }
    return cache_;
}

}  // namespace restocker
