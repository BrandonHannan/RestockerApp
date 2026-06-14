#include "KmartGraphQLClient.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace restocker {
namespace {

// The exact getProductAvailability query string used by kmart.com.au.
const char* kAvailabilityQuery =
    "query getProductAvailability($input: ProductAvailabilityQueryInput!) {\n"
    "  getProductAvailability(input: $input) {\n"
    "    postcode\n    country\n    region\n    availability {\n"
    "      HOME_DELIVERY {\n        keycode\n        poolName\n"
    "        stock {\n          available\n          __typename\n        }\n        __typename\n      }\n"
    "      CLICK_AND_COLLECT {\n        keycode\n"
    "        stock {\n          totalAvailable\n          __typename\n        }\n"
    "        locations {\n          fulfilment {\n            isBuddyLocation\n            locationId\n"
    "            stock {\n              available\n              __typename\n            }\n            __typename\n          }\n"
    "          location {\n            locationId\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n"
    "      IN_STORE {\n        keycode\n        locations {\n          fulfilment {\n"
    "            stock {\n              available\n              __typename\n            }\n            __typename\n          }\n"
    "          location {\n            locationId\n            __typename\n          }\n          __typename\n        }\n        __typename\n      }\n"
    "      __typename\n    }\n    __typename\n  }\n}\n";

// The exact getFindInStore query string used by kmart.com.au.
const char* kFindInStoreQuery =
    "query getFindInStore($input: FindInStoresQueryInput!) {\n"
    "  findInStores(input: $input) {\n"
    "    keycode\n    inventory {\n"
    "      locationName\n      locationId\n      stockLevel\n      phoneNumber\n      __typename\n    }\n"
    "    __typename\n  }\n}\n";

int readAvailable(const json& stock, const char* field) {
    if (stock.is_object()) {
        auto it = stock.find(field);
        if (it != stock.end() && it->is_number()) return it->get<int>();
    }
    return 0;
}

// Read a field that the gateway may return as either a string or a number
// (locationId, for example, is a string in getProductAvailability but a number
// in getFindInStore). Returns "" if absent/null.
std::string readIdString(const json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return {};
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number_integer()) return std::to_string(it->get<long long>());
    if (it->is_number()) return std::to_string(it->get<long long>());
    return {};
}

}  // namespace

KmartGraphQLClient::KmartGraphQLClient(KmartConfig cfg, IGatewayTransport& transport)
    : cfg_(std::move(cfg)), transport_(transport) {}

std::string KmartGraphQLClient::buildPayload(const std::vector<std::string>& keycodes) const {
    json products = json::array();
    for (const auto& kc : keycodes) {
        products.push_back({{"keycode", kc},
                            {"quantity", 1},
                            {"isNationalInventory", true},
                            {"isClickAndCollectOnly", false}});
    }

    json input = {
        {"country", cfg_.country},
        {"postcode", cfg_.postcode},
        {"products", products},
        {"fulfilmentMethods",
         json::array({"HOME_DELIVERY", "CLICK_AND_COLLECT", "IN_STORE"})},
        {"amendNearestInStockCnc", true},
        {"limit", cfg_.instore_max > 0 ? cfg_.instore_max : 3},
    };

    json body = {
        {"operationName", "getProductAvailability"},
        {"variables", {{"input", input}}},
        {"query", kAvailabilityQuery},
    };
    return body.dump();
}

std::vector<ChannelStock> parseAvailability(const std::string& body) {
    std::vector<ChannelStock> out;
    json j = json::parse(body);

    if (!j.contains("data") || j["data"].is_null()) return out;
    const auto& gpa = j["data"].value("getProductAvailability", json{});
    if (!gpa.is_object()) return out;
    const auto& avail = gpa.value("availability", json{});
    if (!avail.is_object()) return out;

    // HOME_DELIVERY: national, one entry per keycode.
    if (avail.contains("HOME_DELIVERY") && avail["HOME_DELIVERY"].is_array()) {
        for (const auto& hd : avail["HOME_DELIVERY"]) {
            ChannelStock s;
            s.keycode = hd.value("keycode", "");
            s.channel = "HOME_DELIVERY";
            s.location_id = "";
            s.available = readAvailable(hd.value("stock", json{}), "available");
            if (!s.keycode.empty()) out.push_back(std::move(s));
        }
    }

    // CLICK_AND_COLLECT: a total plus per-store locations.
    if (avail.contains("CLICK_AND_COLLECT") && avail["CLICK_AND_COLLECT"].is_array()) {
        for (const auto& cc : avail["CLICK_AND_COLLECT"]) {
            std::string keycode = cc.value("keycode", "");
            if (keycode.empty()) continue;

            ChannelStock total;
            total.keycode = keycode;
            total.channel = "CLICK_AND_COLLECT";
            total.location_id = "";
            total.available = readAvailable(cc.value("stock", json{}), "totalAvailable");
            out.push_back(std::move(total));

            if (cc.contains("locations") && cc["locations"].is_array()) {
                for (const auto& loc : cc["locations"]) {
                    const auto& ful = loc.value("fulfilment", json{});
                    if (!ful.is_object()) continue;
                    ChannelStock s;
                    s.keycode = keycode;
                    s.channel = "CLICK_AND_COLLECT";
                    s.location_id = ful.value("locationId", "");
                    s.available = readAvailable(ful.value("stock", json{}), "available");
                    if (!s.location_id.empty()) out.push_back(std::move(s));
                }
            }
        }
    }

    // IN_STORE: may be null; per-store only.
    if (avail.contains("IN_STORE") && avail["IN_STORE"].is_array()) {
        for (const auto& is : avail["IN_STORE"]) {
            std::string keycode = is.value("keycode", "");
            if (keycode.empty()) continue;
            if (is.contains("locations") && is["locations"].is_array()) {
                for (const auto& loc : is["locations"]) {
                    const auto& ful = loc.value("fulfilment", json{});
                    const auto& location = loc.value("location", json{});
                    if (!ful.is_object()) continue;
                    ChannelStock s;
                    s.keycode = keycode;
                    s.channel = "IN_STORE";
                    s.location_id = location.is_object() ? location.value("locationId", "") : "";
                    s.available = readAvailable(ful.value("stock", json{}), "available");
                    if (!s.location_id.empty()) out.push_back(std::move(s));
                }
            }
        }
    }

    return out;
}

KmartGraphQLClient::BatchResult KmartGraphQLClient::queryAvailability(
    const std::vector<std::string>& keycodes) {
    BatchResult result;
    if (keycodes.empty()) {
        result.ok = true;
        return result;
    }

    std::string payload = buildPayload(keycodes);
    HttpResponse resp = transport_.postGraphQL(cfg_.graphql_url, payload);
    if (!resp.ok()) {
        if (resp.status_code == 403) {
            // Akamai Bot Manager block — the replayed cookie jar is stale or
            // missing. On the "http" transport this self-heals: after
            // kmart.harvest_after_failures consecutive failures it re-harvests
            // cookies via the browser and retries. Persistent 403s mean no
            // valid cookie could be obtained (set kmart.cookie or check the
            // browser harvester).
            result.error = "HTTP 403 (Akamai bot block — stale/missing cookies)";
        } else {
            result.error = resp.error.empty()
                               ? ("HTTP " + std::to_string(resp.status_code))
                               : resp.error;
        }
        return result;
    }

    try {
        result.stocks = parseAvailability(resp.text);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("parse error: ") + e.what();
    }
    return result;
}

std::string KmartGraphQLClient::buildFindInStorePayload(const std::string& keycode) const {
    json input = {
        {"postcode", cfg_.postcode},
        {"country", cfg_.country},
        {"keycodes", json::array({keycode})},
    };
    json body = {
        {"operationName", "getFindInStore"},
        {"variables", {{"input", input}}},
        {"query", kFindInStoreQuery},
    };
    return body.dump();
}

std::vector<StoreStock> parseFindInStore(const std::string& body) {
    std::vector<StoreStock> out;
    json j = json::parse(body);

    if (!j.contains("data") || j["data"].is_null()) return out;
    const auto& stores = j["data"].value("findInStores", json{});
    if (!stores.is_array()) return out;

    for (const auto& entry : stores) {
        const auto& inv = entry.value("inventory", json{});
        if (!inv.is_array()) continue;
        for (const auto& loc : inv) {
            if (!loc.is_object()) continue;
            StoreStock s;
            s.location_id = readIdString(loc, "locationId");
            s.location_name = loc.value("locationName", "");
            s.stock_level = loc.value("stockLevel", "");
            s.phone = loc.value("phoneNumber", "");
            if (!s.location_name.empty() || !s.location_id.empty()) {
                out.push_back(std::move(s));
            }
        }
    }
    return out;
}

std::vector<StoreStock> KmartGraphQLClient::queryFindInStore(const std::string& keycode) {
    if (keycode.empty()) return {};
    HttpResponse resp = transport_.postGraphQL(cfg_.graphql_url, buildFindInStorePayload(keycode));
    if (!resp.ok()) return {};
    try {
        return parseFindInStore(resp.text);
    } catch (const std::exception&) {
        return {};
    }
}

}  // namespace restocker
