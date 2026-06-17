#include "BigWAvailabilityClient.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "BigWHttpTransport.h"

using json = nlohmann::json;

namespace restocker {
namespace {

// Is the named channel object available at `store_id`? Reads only the boolean —
// the BigW quantity field is documented as unreliable.
bool channelAvailable(const json& product, const char* channel, const std::string& store_id) {
    auto it = product.find(channel);
    if (it == product.end() || !it->is_object()) return false;
    auto store = it->find(store_id);
    if (store == it->end() || !store->is_object()) return false;
    return store->value("available", false);
}

// Any delivery method (keyed by display name) available?
bool deliveryAvailable(const json& product) {
    auto it = product.find("delivery");
    if (it == product.end() || !it->is_object()) return false;
    for (const auto& method : it->items()) {
        if (method.value().is_object() && method.value().value("available", false)) return true;
    }
    return false;
}

ChannelStock makeStock(const std::string& keycode, const std::string& channel,
                       const std::string& location_id, bool available) {
    ChannelStock s;
    s.keycode = keycode;
    s.distributor = static_cast<int>(Distributor::BigW);
    s.channel = channel;
    s.location_id = location_id;
    s.available = available ? 1 : 0;
    return s;
}

}  // namespace

std::vector<ChannelStock> parseBigWAvailability(const std::string& body,
                                                const std::string& product_id,
                                                const std::string& store_id) {
    std::vector<ChannelStock> out;
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return out;

    auto products = j.find("products");
    if (products == j.end() || !products->is_object()) return out;
    auto pit = products->find(product_id);
    if (pit == products->end() || !pit->is_object()) return out;
    const json& product = *pit;

    bool instore = channelAvailable(product, "instore", store_id);
    bool pickup = channelAvailable(product, "pickup", store_id);
    bool delivery = deliveryAvailable(product);

    // In-store: per-store only (mirrors Kmart IN_STORE).
    out.push_back(makeStock(product_id, "IN_STORE", store_id, instore));
    // Click & collect: a national total plus the per-store row (mirrors Kmart).
    out.push_back(makeStock(product_id, "CLICK_AND_COLLECT", "", pickup));
    out.push_back(makeStock(product_id, "CLICK_AND_COLLECT", store_id, pickup));
    // Home delivery: national.
    out.push_back(makeStock(product_id, "HOME_DELIVERY", "", delivery));
    return out;
}

BigWAvailabilityClient::BigWAvailabilityClient(BigWConfig cfg, BigWHttpTransport& transport)
    : cfg_(std::move(cfg)), transport_(transport) {}

std::string BigWAvailabilityClient::buildUrl(const std::string& product_id) const {
    return cfg_.availability_url + product_id + "?storeId=" + cfg_.store_id +
           "&deliveryPostcode=" + cfg_.delivery_postcode +
           "&deliverySuburb=" + cfg_.delivery_suburb;
}

BigWAvailabilityClient::Result BigWAvailabilityClient::queryAvailability(
    const std::string& product_id) {
    Result res;
    HttpResponse resp = transport_.get(buildUrl(product_id));
    if (!resp.ok()) {
        res.error = resp.error.empty() ? ("status " + std::to_string(resp.status_code))
                                       : resp.error;
        return res;
    }
    res.stocks = parseBigWAvailability(resp.text, product_id, cfg_.store_id);
    res.ok = true;
    return res;
}

}  // namespace restocker
