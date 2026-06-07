#include "Config.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace restocker {
namespace {

// Helper: fetch optional nested value, leaving `out` untouched if absent/null.
template <typename T>
void getIf(const json& obj, const char* key, T& out) {
    auto it = obj.find(key);
    if (it != obj.end() && !it->is_null()) {
        out = it->get<T>();
    }
}

}  // namespace

Config Config::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("config file not found: " + path);
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error("failed to parse config " + path + ": " + e.what());
    }

    Config c;

    if (j.contains("constructor")) {
        const auto& s = j["constructor"];
        auto& d = c.constructor;
        getIf(s, "base_url", d.base_url);
        getIf(s, "key", d.key);
        getIf(s, "client_version", d.client_version);
        getIf(s, "search_terms", d.search_terms);
        getIf(s, "num_results_per_page", d.num_results_per_page);
        getIf(s, "max_pages", d.max_pages);
        getIf(s, "url_prefix_filter", d.url_prefix_filter);
        getIf(s, "page_delay_ms", d.page_delay_ms);
        getIf(s, "min_ratelimit_remaining", d.min_ratelimit_remaining);
    }

    if (j.contains("kmart")) {
        const auto& s = j["kmart"];
        auto& d = c.kmart;
        getIf(s, "graphql_url", d.graphql_url);
        getIf(s, "postcode", d.postcode);
        getIf(s, "country", d.country);
        getIf(s, "target_state", d.target_state);
        getIf(s, "batch_size", d.batch_size);
        getIf(s, "batch_delay_ms", d.batch_delay_ms);
        getIf(s, "instore_max", d.instore_max);
        getIf(s, "transport", d.transport);
    }

    if (j.contains("browser")) {
        const auto& s = j["browser"];
        auto& d = c.browser;
        getIf(s, "executable_path", d.executable_path);
        getIf(s, "headless", d.headless);
        getIf(s, "nav_url", d.nav_url);
        getIf(s, "page_settle_ms", d.page_settle_ms);
        getIf(s, "relaunch_every_cycles", d.relaunch_every_cycles);
        getIf(s, "cdp_timeout_ms", d.cdp_timeout_ms);
    }

    if (j.contains("intervals")) {
        const auto& s = j["intervals"];
        auto& d = c.intervals;
        getIf(s, "discovery_seconds", d.discovery_seconds);
        getIf(s, "discovery_jitter_seconds", d.discovery_jitter_seconds);
        getIf(s, "inventory_seconds", d.inventory_seconds);
        getIf(s, "inventory_jitter_seconds", d.inventory_jitter_seconds);
    }

    if (j.contains("notifiers")) {
        const auto& s = j["notifiers"];
        if (s.contains("discord")) {
            getIf(s["discord"], "enabled", c.notifiers.discord.enabled);
            getIf(s["discord"], "webhook_url", c.notifiers.discord.webhook_url);
        }
        if (s.contains("generic")) {
            getIf(s["generic"], "enabled", c.notifiers.generic.enabled);
            getIf(s["generic"], "url", c.notifiers.generic.url);
            getIf(s["generic"], "headers", c.notifiers.generic.headers);
        }
    }

    if (j.contains("http")) {
        const auto& s = j["http"];
        getIf(s, "user_agent", c.http.user_agent);
        getIf(s, "timeout_ms", c.http.timeout_ms);
        getIf(s, "proxy", c.http.proxy);
        getIf(s, "impersonate", c.http.impersonate);
        getIf(s, "impersonate_target", c.http.impersonate_target);
    }

    if (j.contains("database")) {
        getIf(j["database"], "path", c.database.path);
    }

    // Validation.
    if (c.constructor.key.empty()) {
        throw std::runtime_error("constructor.key must not be empty");
    }
    if (c.constructor.url_prefix_filter.empty()) {
        throw std::runtime_error("constructor.url_prefix_filter must not be empty");
    }
    if (c.kmart.postcode.empty()) {
        throw std::runtime_error("kmart.postcode must not be empty");
    }
    if (c.kmart.batch_size < 1) {
        throw std::runtime_error("kmart.batch_size must be >= 1");
    }
    if (c.intervals.inventory_seconds < 1 || c.intervals.discovery_seconds < 1) {
        throw std::runtime_error("interval seconds must be >= 1");
    }
    if (c.notifiers.discord.enabled && c.notifiers.discord.webhook_url.empty()) {
        throw std::runtime_error("discord notifier enabled but webhook_url is empty");
    }
    if (c.notifiers.generic.enabled && c.notifiers.generic.url.empty()) {
        throw std::runtime_error("generic notifier enabled but url is empty");
    }
    if (c.http.impersonate && c.http.impersonate_target.empty()) {
        throw std::runtime_error("http.impersonate is true but impersonate_target is empty");
    }
    if (c.kmart.transport != "browser" && c.kmart.transport != "http") {
        throw std::runtime_error("kmart.transport must be \"browser\" or \"http\"");
    }

    return c;
}

}  // namespace restocker
