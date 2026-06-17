#include "BigWSearchClient.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "BigWHttpTransport.h"

using json = nlohmann::json;

namespace restocker {
namespace {

// Does this result's specifications array contain {name == spec_name} with
// spec_value among its values? This is the "For ages 6+" gate that keeps the
// search to genuine TCG product (and drops accessories/unrelated hits).
bool hasSpec(const json& information, const std::string& spec_name,
            const std::string& spec_value) {
    auto it = information.find("specifications");
    if (it == information.end() || !it->is_array()) return false;
    for (const auto& spec : *it) {
        if (spec.value("name", std::string()) != spec_name) continue;
        auto v = spec.find("values");
        if (v == spec.end() || !v->is_array()) continue;
        for (const auto& val : *v) {
            if (val.is_string() && val.get<std::string>() == spec_value) return true;
        }
    }
    return false;
}

// True if any home-delivery method (standard/express/priority) is offered.
bool hasDelivery(const json& fulfilment) {
    auto d = fulfilment.find("delivery");
    if (d == fulfilment.end() || !d->is_object()) return false;
    return d->value("standard", false) || d->value("express", false) ||
           d->value("priority", false);
}

bool hasPickup(const json& fulfilment) {
    auto c = fulfilment.find("collection");
    if (c == fulfilment.end() || !c->is_object()) return false;
    return c->value("pickup", false);
}

// BigW fulfilment policy mapped onto the shared 2/3/5 enum used for Discord
// routing: 2 = pickup only, 3 = pickup + delivery, 5 = delivery only, 0 = neither.
int fulfilmentChannel(const json& fulfilment) {
    bool pickup = hasPickup(fulfilment);
    bool delivery = hasDelivery(fulfilment);
    if (pickup && delivery) return 3;
    if (pickup) return 2;
    if (delivery) return 5;
    return 0;
}

// Best-effort pre-order detection. BigW exposes no single documented flag in the
// search payload, so we look for a truthy preorder marker on the fulfilment block
// (and the lifecycle status as a fallback). Verify against a live pre-order SKU.
bool isPreorder(const json& fulfilment, const json& attributes) {
    for (const char* key : {"preorder", "preOrder", "preorderable"}) {
        if (fulfilment.value(key, false)) return true;
    }
    std::string lifecycle = attributes.value("lifecycleStatus", std::string());
    return lifecycle == "PR" || lifecycle == "PREORDER";
}

}  // namespace

std::vector<Product> parseBigWSearch(const std::string& body, const std::string& price_state,
                                     const std::string& required_spec_name,
                                     const std::string& required_spec_value,
                                     int* result_count_out) {
    std::vector<Product> out;
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return out;

    const auto& organic = j.value("organic", json::object());
    const auto& results = organic.value("results", json::array());
    if (!results.is_array()) return out;
    if (result_count_out) *result_count_out = static_cast<int>(results.size());

    for (const auto& r : results) {
        const auto& information = r.value("information", json::object());
        if (!information.is_object()) continue;

        // Filter: keep only the configured specification (e.g. ages 6+).
        if (!hasSpec(information, required_spec_name, required_spec_value)) continue;

        const auto& identifiers = r.value("identifiers", json::object());
        std::string article_id = identifiers.value("articleId", std::string());
        if (article_id.empty()) continue;  // need an id to track

        Product p;
        p.distributor = static_cast<int>(Distributor::BigW);
        p.product_id = article_id;
        p.name = information.value("name", std::string());
        if (information.contains("brand") && information["brand"].is_object()) {
            p.brand = information["brand"].value("name", std::string());
        }

        // Price: prices.<STATE>.price.cents / 100.
        const auto& prices = r.value("prices", json::object());
        if (prices.contains(price_state) && prices[price_state].is_object()) {
            const auto& price = prices[price_state].value("price", json::object());
            if (price.contains("cents") && price["cents"].is_number()) {
                p.price = price["cents"].get<double>() / 100.0;
            }
        }

        const auto& fulfilment = r.value("fulfilment", json::object());
        const auto& attributes = r.value("attributes", json::object());
        p.fulfilment_channel = fulfilmentChannel(fulfilment);
        p.is_preorder = isPreorder(fulfilment, attributes);

        // url is left empty; the discovery loop resolves it from the sitemap.
        out.push_back(std::move(p));
    }
    return out;
}

BigWSearchClient::BigWSearchClient(BigWConfig cfg, BigWHttpTransport& transport)
    : cfg_(std::move(cfg)), transport_(transport) {}

std::string BigWSearchClient::buildPayload(int page) const {
    json body = {
        {"text", cfg_.search_text},
        {"sort", "relevance"},
        {"filter", {{"inStock", true}, {"soldBy", json::array({"BIG W"})}}},
        {"storeId", cfg_.store_id},
        {"state", cfg_.state},
        {"zone", cfg_.zone},
        {"page", page},
        {"perPage", cfg_.per_page},
        {"format", "1"},
        {"clientId", cfg_.client_id},
    };
    return body.dump();
}

BigWSearchClient::PageResult BigWSearchClient::fetchPage(int page) {
    PageResult res;
    HttpResponse resp = transport_.post(cfg_.search_url, buildPayload(page));
    if (!resp.ok()) {
        res.error = resp.error.empty() ? ("status " + std::to_string(resp.status_code))
                                       : resp.error;
        return res;
    }
    res.products = parseBigWSearch(resp.text, cfg_.state, cfg_.required_spec_name,
                                   cfg_.required_spec_value, &res.result_count);
    res.ok = true;
    return res;
}

}  // namespace restocker
