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
        getIf(s, "browse_group_id", d.browse_group_id);
        getIf(s, "browse_sort_by", d.browse_sort_by);
        getIf(s, "browse_sort_order", d.browse_sort_order);
        getIf(s, "sitemap_index_url", d.sitemap_index_url);
        getIf(s, "product_sitemap_filter", d.product_sitemap_filter);
        getIf(s, "sitemap_max_concurrency", d.sitemap_max_concurrency);
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
        getIf(s, "cookie", d.cookie);
        getIf(s, "auth_token", d.auth_token);
        getIf(s, "user_agent", d.user_agent);
        getIf(s, "harvest_after_failures", d.harvest_after_failures);
        getIf(s, "extra_headers", d.extra_headers);
    }

    if (j.contains("bigw")) {
        const auto& s = j["bigw"];
        auto& d = c.bigw;
        getIf(s, "enabled", d.enabled);
        getIf(s, "search_url", d.search_url);
        getIf(s, "availability_url", d.availability_url);
        getIf(s, "stores_url", d.stores_url);
        getIf(s, "sitemap_index_url", d.sitemap_index_url);
        getIf(s, "product_sitemap_filter", d.product_sitemap_filter);
        getIf(s, "search_text", d.search_text);
        getIf(s, "store_id", d.store_id);
        getIf(s, "state", d.state);
        getIf(s, "zone", d.zone);
        getIf(s, "per_page", d.per_page);
        getIf(s, "max_pages", d.max_pages);
        getIf(s, "client_id", d.client_id);
        getIf(s, "delivery_postcode", d.delivery_postcode);
        getIf(s, "delivery_suburb", d.delivery_suburb);
        getIf(s, "required_spec_name", d.required_spec_name);
        getIf(s, "required_spec_value", d.required_spec_value);
        getIf(s, "user_agent", d.user_agent);
        getIf(s, "cookie", d.cookie);
        getIf(s, "transport", d.transport);
        getIf(s, "harvest_after_failures", d.harvest_after_failures);
        getIf(s, "extra_headers", d.extra_headers);
        getIf(s, "search_seconds", d.search_seconds);
        getIf(s, "search_jitter_seconds", d.search_jitter_seconds);
        getIf(s, "availability_seconds", d.availability_seconds);
        getIf(s, "availability_jitter_seconds", d.availability_jitter_seconds);
        getIf(s, "stores_refresh_seconds", d.stores_refresh_seconds);
        getIf(s, "per_product_delay_ms", d.per_product_delay_ms);
        getIf(s, "instore_max", d.instore_max);
    }

    if (j.contains("browser")) {
        const auto& s = j["browser"];
        auto& d = c.browser;
        getIf(s, "executable_path", d.executable_path);
        getIf(s, "headless", d.headless);
        getIf(s, "nav_url", d.nav_url);
        getIf(s, "cookie_domain", d.cookie_domain);
        getIf(s, "page_settle_ms", d.page_settle_ms);
        getIf(s, "relaunch_every_cycles", d.relaunch_every_cycles);
        getIf(s, "cdp_timeout_ms", d.cdp_timeout_ms);
    }

    if (j.contains("intervals")) {
        const auto& s = j["intervals"];
        auto& d = c.intervals;
        getIf(s, "sitemap_seconds", d.sitemap_seconds);
        getIf(s, "sitemap_jitter_seconds", d.sitemap_jitter_seconds);
        getIf(s, "browse_refresh_seconds", d.browse_refresh_seconds);
        getIf(s, "inventory_seconds", d.inventory_seconds);
        getIf(s, "inventory_jitter_seconds", d.inventory_jitter_seconds);
    }

    if (j.contains("notifiers")) {
        const auto& s = j["notifiers"];
        if (s.contains("discord")) {
            getIf(s["discord"], "enabled", c.notifiers.discord.enabled);
            getIf(s["discord"], "webhook_url", c.notifiers.discord.webhook_url);
            getIf(s["discord"], "webhook_instore", c.notifiers.discord.webhook_instore);
            getIf(s["discord"], "webhook_online", c.notifiers.discord.webhook_online);
            getIf(s["discord"], "webhook_preorder", c.notifiers.discord.webhook_preorder);
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
    if (c.constructor.browse_group_id.empty()) {
        throw std::runtime_error("constructor.browse_group_id must not be empty");
    }
    if (c.constructor.sitemap_index_url.empty()) {
        throw std::runtime_error("constructor.sitemap_index_url must not be empty");
    }
    if (c.constructor.sitemap_max_concurrency < 1) {
        throw std::runtime_error("constructor.sitemap_max_concurrency must be >= 1");
    }
    if (c.kmart.postcode.empty()) {
        throw std::runtime_error("kmart.postcode must not be empty");
    }
    if (c.kmart.batch_size < 1) {
        throw std::runtime_error("kmart.batch_size must be >= 1");
    }
    if (c.intervals.inventory_seconds < 1 || c.intervals.sitemap_seconds < 1 ||
        c.intervals.browse_refresh_seconds < 1) {
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
    if (c.bigw.enabled) {
        if (c.bigw.transport != "browser" && c.bigw.transport != "http") {
            throw std::runtime_error("bigw.transport must be \"browser\" or \"http\"");
        }
        if (c.bigw.store_id.empty()) {
            throw std::runtime_error("bigw.store_id must not be empty");
        }
        if (c.bigw.per_page < 1) {
            throw std::runtime_error("bigw.per_page must be >= 1");
        }
        if (c.bigw.search_seconds < 1 || c.bigw.availability_seconds < 1) {
            throw std::runtime_error("bigw interval seconds must be >= 1");
        }
    }

    return c;
}

}  // namespace restocker
