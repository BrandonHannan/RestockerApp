#include "InventoryLoop.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <random>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace restocker {
namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::chrono::milliseconds jitteredDelay(int base_s, int jitter_s) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    int j = 0;
    if (jitter_s > 0) {
        std::uniform_int_distribution<int> d(-jitter_s, jitter_s);
        j = d(rng);
    }
    int secs = base_s + j;
    if (secs < 1) secs = 1;
    return std::chrono::milliseconds(static_cast<long long>(secs) * 1000);
}

}  // namespace

InventoryLoop::InventoryLoop(const Config& cfg, KmartGraphQLClient& client, Database& db,
                             NotifierManager& notifiers, StopToken& stop)
    : cfg_(cfg), client_(client), db_(db), notifiers_(notifiers), stop_(stop) {}

bool InventoryLoop::processProduct(const std::string& keycode,
                                   const std::vector<ChannelStock>& rows) {
    bool restocked = false;
    int bestDelta = 0;          // largest increase across channels (for the summary)
    const ChannelStock* trigger = nullptr;
    int triggerPrev = 0;
    int homeDelivery = -1;
    int clickCollectTotal = -1;
    // Numeric per-store units keyed by location_id, to merge into the store list.
    std::map<std::string, int> numericByLocation;

    for (const auto& s : rows) {
        std::optional<int> prev = db_.getStock(s.keycode, s.channel, s.location_id);
        int prevVal = prev.value_or(0);

        // Restock = availability increased (covers the 0 -> positive case). A
        // first sighting with positive stock also counts as a restock signal.
        if (s.available > prevVal) {
            restocked = true;
            int delta = s.available - prevVal;
            if (delta > bestDelta || trigger == nullptr) {
                bestDelta = delta;
                trigger = &s;
                triggerPrev = prevVal;
            }
        }

        if (s.location_id.empty()) {
            if (s.channel == "HOME_DELIVERY") homeDelivery = s.available;
            else if (s.channel == "CLICK_AND_COLLECT") clickCollectTotal = s.available;
        } else if (s.available > 0 && (s.channel == "CLICK_AND_COLLECT" || s.channel == "IN_STORE")) {
            // A store can appear under both CLICK_AND_COLLECT and IN_STORE; keep
            // the larger reading for that location.
            int& cur = numericByLocation[s.location_id];
            if (s.available > cur) cur = s.available;
        }

        db_.setStock(s);  // always persist the new baseline
    }

    if (!restocked) return false;

    RestockEvent e;
    e.keycode = keycode;
    e.channel = trigger ? trigger->channel : "";
    e.location_id = trigger ? trigger->location_id : "";
    e.previous = triggerPrev;
    e.available = trigger ? trigger->available : 0;
    e.home_delivery = homeDelivery;
    e.click_collect_total = clickCollectTotal;
    e.timestamp = nowSeconds();
    int fulfilmentChannel = 0;
    if (auto p = db_.getProduct(keycode)) {
        e.name = p->name;
        e.url = p->url;
        e.image_url = p->image_url;
        e.price = p->price;
        e.is_preorder = p->is_preorder;
        e.preorder_release_date = p->preorder_release_date;
        fulfilmentChannel = p->fulfilment_channel;
    }

    // FulfilmentChannel 2 is in-store only: suppress the alert unless a nearby
    // store actually holds stock. Channels 3/5 and unknown alert on the online
    // restock signal as usual. (Baselines are already persisted above.)
    if (fulfilmentChannel == 2 && numericByLocation.empty()) {
        return false;
    }

    auto now = std::chrono::system_clock::to_time_t(
    std::chrono::system_clock::now());

    std::tm tm{};
    localtime_s(&tm, &now);

    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);

    std::string dateStr(buffer);
    if (e.is_preorder && e.preorder_release_date < dateStr){
        e.is_preorder = !e.is_preorder;
    }

    // Build the nearby-store list from the numeric per-store availability, using
    // getFindInStore only to enrich names/phone. Stores with no stock are already
    // excluded (numericByLocation only holds available > 0).
    std::vector<StoreStock> meta = client_.queryFindInStore(keycode);
    std::map<std::string, const StoreStock*> metaById;
    for (const auto& m : meta) metaById[m.location_id] = &m;

    std::vector<StoreStock> stores;
    stores.reserve(numericByLocation.size());
    for (const auto& kv : numericByLocation) {
        StoreStock st;
        st.location_id = kv.first;
        st.available = kv.second;
        auto it = metaById.find(kv.first);
        if (it != metaById.end()) {
            st.location_name = it->second->location_name;
            st.phone = it->second->phone;
        }
        stores.push_back(std::move(st));
    }
    // Most stock first, then cap to the configured maximum.
    std::sort(stores.begin(), stores.end(),
              [](const StoreStock& a, const StoreStock& b) {
                  return a.available > b.available;
              });
    if (cfg_.kmart.instore_max > 0 &&
        static_cast<int>(stores.size()) > cfg_.kmart.instore_max) {
        stores.resize(cfg_.kmart.instore_max);
    }
    e.stores = std::move(stores);

    notifiers_.notifyAll(e);
    db_.recordAlert(e);
    return true;
}

int InventoryLoop::runOnce() {
    std::vector<std::string> keycodes = db_.getTrackedOOSKeycodes();
    if (keycodes.empty()) {
        spdlog::info("inventory pass: no tracked out-of-stock keycodes");
        return 0;
    }
    spdlog::info("inventory pass: checking {} keycodes", keycodes.size());

    int alerts = 0;
    const int batch = cfg_.kmart.batch_size;
    for (size_t i = 0; i < keycodes.size(); i += batch) {
        if (stop_.stopRequested()) break;

        std::vector<std::string> chunk(
            keycodes.begin() + i,
            keycodes.begin() + std::min(keycodes.size(), i + batch));

        auto res = client_.queryAvailability(chunk);
        if (!res.ok) {
            spdlog::warn("availability batch [{}..{}) failed: {}", i, i + chunk.size(),
                         res.error);
            continue;
        }

        // Group rows by keycode so we fire one consolidated alert per product.
        std::unordered_map<std::string, std::vector<ChannelStock>> byKeycode;
        for (auto& s : res.stocks) {
            byKeycode[s.keycode].push_back(s);
        }
        for (auto& kv : byKeycode) {
            if (processProduct(kv.first, kv.second)) ++alerts;
        }

        bool more = (i + batch) < keycodes.size();
        if (more && cfg_.kmart.batch_delay_ms > 0) {
            if (!stop_.sleepFor(std::chrono::milliseconds(cfg_.kmart.batch_delay_ms))) {
                break;
            }
        }
    }

    spdlog::info("inventory pass complete: {} restock alert(s)", alerts);
    return alerts;
}

void InventoryLoop::run() {
    spdlog::info("inventory loop started (every ~{}s)", cfg_.intervals.inventory_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("inventory pass threw: {}", e.what());
        }
        if (!stop_.sleepFor(jitteredDelay(cfg_.intervals.inventory_seconds,
                                          cfg_.intervals.inventory_jitter_seconds))) {
            break;
        }
    }
    spdlog::info("inventory loop stopped");
}

}  // namespace restocker
