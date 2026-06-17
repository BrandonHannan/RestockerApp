#include "RestockEngine.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <optional>

#include "Database.h"
#include "NotifierManager.h"

namespace restocker {
namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Local-time "YYYY-MM-DD" for the pre-order release-date comparison.
std::string todayLocalDate() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
    return std::string(buffer);
}

}  // namespace

bool processRestock(const std::string& keycode, const std::vector<ChannelStock>& rows,
                    const RestockDeps& deps) {
    bool restocked = false;
    int bestDelta = 0;          // largest increase across channels (for the summary)
    const ChannelStock* trigger = nullptr;
    int triggerPrev = 0;
    int homeDelivery = -1;
    int clickCollectTotal = -1;
    // Numeric per-store units keyed by location_id, to merge into the store list.
    std::map<std::string, int> numericByLocation;

    for (const auto& s : rows) {
        std::optional<int> prev = deps.db.getStock(deps.distributor, s.keycode, s.channel,
                                                    s.location_id);
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

        deps.db.setStock(s);  // always persist the new baseline
    }

    if (!restocked) return false;

    RestockEvent e;
    e.keycode = keycode;
    e.distributor = deps.distributor;
    e.channel = trigger ? trigger->channel : "";
    e.location_id = trigger ? trigger->location_id : "";
    e.previous = triggerPrev;
    e.available = trigger ? trigger->available : 0;
    e.home_delivery = homeDelivery;
    e.click_collect_total = clickCollectTotal;
    e.timestamp = nowSeconds();
    int fulfilmentChannel = 0;
    if (auto p = deps.db.getProduct(deps.distributor, keycode)) {
        e.name = p->name;
        e.url = p->url;
        e.image_url = p->image_url;
        e.price = p->price;
        e.is_preorder = p->is_preorder;
        e.preorder_release_date = p->preorder_release_date;
        fulfilmentChannel = p->fulfilment_channel;
        e.fulfilment_channel = p->fulfilment_channel;
    }

    // FulfilmentChannel 2 is in-store/pickup only: suppress the alert unless a
    // nearby store actually holds stock. Channels 3/5 and unknown alert on the
    // online restock signal as usual. (Baselines are already persisted above.)
    if (fulfilmentChannel == 2 && numericByLocation.empty()) {
        return false;
    }

    if (e.is_preorder && e.preorder_release_date <= todayLocalDate()) {
        e.is_preorder = false;
    }

    // Build the nearby-store list from the numeric per-store availability, using
    // the enrichment callback only to add names/phone. Stores with no stock are
    // already excluded (numericByLocation only holds available > 0).
    std::vector<StoreStock> meta = deps.storeEnrich ? deps.storeEnrich(keycode)
                                                     : std::vector<StoreStock>{};
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
    if (deps.instore_max > 0 && static_cast<int>(stores.size()) > deps.instore_max) {
        stores.resize(deps.instore_max);
    }
    e.stores = std::move(stores);

    deps.notifiers.notifyAll(e);
    deps.db.recordAlert(e);
    return true;
}

}  // namespace restocker
