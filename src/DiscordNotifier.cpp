#include "DiscordNotifier.h"

#include <ctime>
#include <string>

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

// Numeric stock as a short, friendly string. -1 means "unknown".
std::string countLabel(int n) {
    if (n < 0) return "—";
    if (n == 0) return "❌ Out";
    return "✅ " + std::to_string(n);
}

// ISO-8601 UTC timestamp for the embed footer/timestamp field.
std::string isoTime(std::int64_t epoch) {
    if (epoch <= 0) return {};
    std::time_t t = static_cast<std::time_t>(epoch);
    std::tm tmv{};
#if defined(_WIN32)
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &tmv);
    return buf;
}

// One store line, e.g. "**Broadway** — 4 left · (02) 9282 6600". Stock is the
// real per-store unit count from getProductAvailability; the qualitative
// descriptor is intentionally omitted.
std::string storeLine(const StoreStock& s) {
    std::string line = "**" + (s.location_name.empty() ? s.location_id : s.location_name) + "**";
    bool first = true;
    auto sep = [&]() -> std::string { return first ? (first = false, " — ") : " · "; };
    if (s.available >= 0) line += sep() + std::to_string(s.available) + " left";
    if (!s.phone.empty()) line += sep() + s.phone;
    return line;
}

}  // namespace

DiscordNotifier::DiscordNotifier(const DiscordConfig& cfg, HttpClient& http)
    : webhook_url_(cfg.webhook_url),
      webhook_instore_(cfg.webhook_instore),
      webhook_online_(cfg.webhook_online),
      webhook_preorder_(cfg.webhook_preorder),
      http_(http) {}

const std::string& DiscordNotifier::webhookFor(const RestockEvent& event) const {
    // Each category falls back to the default webhook when left unconfigured.
    auto pick = [&](const std::string& specific) -> const std::string& {
        return specific.empty() ? webhook_url_ : specific;
    };
    if (event.fulfilment_channel == 2) return pick(webhook_instore_);
    if (event.fulfilment_channel == 3 || event.fulfilment_channel == 5) {
        return event.is_preorder ? pick(webhook_preorder_) : pick(webhook_online_);
    }
    return webhook_url_;  // 0 / unknown -> default
}

std::string DiscordNotifier::buildBody(const RestockEvent& event) {
    std::string title = event.name.empty() ? ("Keycode " + event.keycode) : event.name;
    std::string url = absoluteUrl(event.url);

    // Header line: pre-order banner with the release date, or a restock banner.
    std::string description;
    int color = 3066993;  // green
    if (event.is_preorder) {
        color = 15844367;  // gold
        description = "🔜 **Pre-order**";
        if (!event.preorder_release_date.empty()) {
            description += " — available " + event.preorder_release_date;
        }
    } else {
        description = "✅ **Back in stock**";
    }

    json fields = json::array();
    fields.push_back({{"name", "🚚 Home Delivery"},
                      {"value", countLabel(event.home_delivery)},
                      {"inline", true}});
    fields.push_back({{"name", "🛒 Click & Collect"},
                      {"value", countLabel(event.click_collect_total)},
                      {"inline", true}});
    if (event.price > 0.0) {
        char price[32];
        std::snprintf(price, sizeof(price), "$%.2f", event.price);
        fields.push_back({{"name", "💲 Price"}, {"value", price}, {"inline", true}});
    }

    // Nearby store list (Discord caps a field value at 1024 chars).
    if (!event.stores.empty()) {
        std::string list;
        for (const auto& s : event.stores) {
            std::string line = storeLine(s);
            if (list.size() + line.size() + 1 > 1000) break;
            if (!list.empty()) list += "\n";
            list += line;
        }
        if (!list.empty()) {
            fields.push_back({{"name", "🏬 Nearby stores"}, {"value", list}, {"inline", false}});
        }
    }

    fields.push_back({{"name", "🔑 Keycode"}, {"value", event.keycode}, {"inline", true}});

    json embed = {
        {"title", title},
        {"description", description},
        {"color", color},
        {"fields", fields},
        {"footer", {{"text", "Kmart Restocker"}}},
    };
    if (!url.empty()) embed["url"] = url;
    if (!event.image_url.empty()) embed["image"] = {{"url", event.image_url}};
    std::string ts = isoTime(event.timestamp);
    if (!ts.empty()) embed["timestamp"] = ts;

    json body = {
        {"username", "Kmart Restocker"},
        {"content", event.is_preorder ? "🔜 Pre-order live!" : "🎉 Restock detected!"},
        {"embeds", json::array({embed})},
    };
    return body.dump();
}

bool DiscordNotifier::notify(const RestockEvent& event) {
    HttpResponse resp = http_.postJson(webhookFor(event), buildBody(event));
    // Discord returns 204 No Content on success.
    if (resp.error.empty() && resp.status_code >= 200 && resp.status_code < 300) {
        return true;
    }
    spdlog::warn("discord notify failed: status={} err={} body={}", resp.status_code,
                 resp.error, resp.text);
    return false;
}

}  // namespace restocker
