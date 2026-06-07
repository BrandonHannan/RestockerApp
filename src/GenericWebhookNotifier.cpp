#include "GenericWebhookNotifier.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace restocker {
namespace {

std::string absoluteUrl(const std::string& url) {
    if (url.rfind("http", 0) == 0) return url;
    if (!url.empty() && url.front() == '/') return "https://www.kmart.com.au" + url;
    return url;
}

}  // namespace

GenericWebhookNotifier::GenericWebhookNotifier(std::string url,
                                               std::map<std::string, std::string> headers,
                                               HttpClient& http)
    : url_(std::move(url)), headers_(std::move(headers)), http_(http) {}

std::string GenericWebhookNotifier::buildBody(const RestockEvent& event) {
    json stores = json::array();
    for (const auto& s : event.stores) {
        stores.push_back({
            {"locationId", s.location_id},
            {"locationName", s.location_name},
            {"stockLevel", s.stock_level},
            {"phone", s.phone},
            {"available", s.available},
        });
    }

    json body = {
        {"keycode", event.keycode},
        {"name", event.name},
        {"url", absoluteUrl(event.url)},
        {"imageUrl", event.image_url},
        {"channel", event.channel},
        {"locationId", event.location_id},
        {"previous", event.previous},
        {"available", event.available},
        {"isPreorder", event.is_preorder},
        {"preorderReleaseDate", event.preorder_release_date},
        {"price", event.price},
        {"homeDelivery", event.home_delivery},
        {"clickCollectTotal", event.click_collect_total},
        {"stores", stores},
        {"timestamp", event.timestamp},
    };
    return body.dump();
}

bool GenericWebhookNotifier::notify(const RestockEvent& event) {
    HttpResponse resp = http_.postJson(url_, buildBody(event), headers_);
    if (resp.ok()) return true;
    spdlog::warn("generic notify failed: status={} err={} body={}", resp.status_code,
                 resp.error, resp.text);
    return false;
}

}  // namespace restocker
