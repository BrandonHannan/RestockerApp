#include "DiscoveryLoop.h"

#include <chrono>
#include <random>
#include <utility>

#include <spdlog/spdlog.h>

namespace restocker {
namespace {

long long epochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// base ± [0, jitter] seconds, never below 1s.
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

DiscoveryLoop::DiscoveryLoop(const Config& cfg, ConstructorClient& client, SitemapClient& sitemap,
                             Database& db, StopToken& stop)
    : cfg_(cfg), client_(client), sitemap_(sitemap), db_(db), stop_(stop) {}

void DiscoveryLoop::refreshSitemapIfDue() {
    const auto now = std::chrono::steady_clock::now();
    const bool stale =
        !sitemap_loaded_ ||
        (now - last_sitemap_refresh_) >=
            std::chrono::seconds(cfg_.constructor.sitemap_refresh_seconds);
    if (!stale) return;

    auto fresh = sitemap_.sweep();
    if (!fresh.empty()) {
        sitemap_cache_ = std::move(fresh);
        last_sitemap_refresh_ = now;
        sitemap_loaded_ = true;
    } else if (sitemap_loaded_) {
        spdlog::warn("sitemap sweep failed; keeping {} cached keycodes", sitemap_cache_.size());
    } else {
        spdlog::warn("sitemap sweep returned no products; will retry next pass");
    }
}

std::unordered_map<std::string, Product> DiscoveryLoop::sweepBrowseStatus() {
    std::unordered_map<std::string, Product> map;
    const std::string session_id = db_.getOrCreateSessionId();
    const auto& cc = cfg_.constructor;

    for (int page = 1; page <= cc.max_pages; ++page) {
        if (stop_.stopRequested()) break;

        auto res = client_.fetchBrowsePage(cc.browse_group_id, page, session_id,
                                           db_.nextSessionSeq(), epochMs());
        if (!res.ok) {
            spdlog::warn("browse page {} failed: {}", page, res.error);
            break;
        }

        const size_t got = res.products.size();
        for (auto& p : res.products) {
            std::string kc = p.variation_id;
            map.emplace(std::move(kc), std::move(p));
        }
        spdlog::debug("browse page {}: {} products, total_results={}, ratelimit_remaining={}",
                      page, got, res.total_results, res.ratelimit_remaining);

        // Stop once we've paged past the group's result set.
        if (res.total_results > 0 && page * cc.num_results_per_page >= res.total_results) {
            break;
        }
        // Back off if we're close to the rate limit.
        if (res.ratelimit_remaining >= 0 && res.ratelimit_remaining < cc.min_ratelimit_remaining) {
            spdlog::warn("constructor rate limit low ({}); ending browse sweep early",
                         res.ratelimit_remaining);
            break;
        }
        if (cc.page_delay_ms > 0) {
            if (!stop_.sleepFor(std::chrono::milliseconds(cc.page_delay_ms))) break;
        }
    }
    return map;
}

int DiscoveryLoop::runOnce() {
    refreshSitemapIfDue();
    if (sitemap_cache_.empty()) {
        spdlog::warn("discovery pass: sitemap cache empty, nothing to join");
        return 0;
    }

    // Cheap per-cycle status feed for the whole TCG browse group.
    std::unordered_map<std::string, Product> browse = sweepBrowseStatus();
    spdlog::info("discovery: {} sitemap keycodes, {} browse status entries",
                 sitemap_cache_.size(), browse.size());

    int newly = 0;
    int enriched = 0;
    for (const auto& kv : sitemap_cache_) {
        if (stop_.stopRequested()) return newly;
        const std::string& keycode = kv.first;
        const std::string& url = kv.second;

        // Sitemap is authoritative for "which products exist"; browse enriches
        // status. Missing browse status -> conservative defaults (tracked=true).
        Product p;
        auto it = browse.find(keycode);
        if (it != browse.end()) {
            p = it->second;
            ++enriched;
        }
        p.variation_id = keycode;
        p.url = url;  // canonical absolute URL straight from the sitemap

        if (db_.upsertProduct(p)) {
            ++newly;
            spdlog::info("discovered new product keycode={} '{}'{}", p.variation_id, p.name,
                         p.is_preorder ? " [pre-order]" : "");
        }
    }

    spdlog::info("discovery pass complete: {} keycodes, {} enriched, {} new",
                 sitemap_cache_.size(), enriched, newly);
    return newly;
}

void DiscoveryLoop::run() {
    spdlog::info("discovery loop started (every ~{}s)", cfg_.intervals.discovery_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("discovery pass threw: {}", e.what());
        }
        if (!stop_.sleepFor(jitteredDelay(cfg_.intervals.discovery_seconds,
                                          cfg_.intervals.discovery_jitter_seconds))) {
            break;
        }
    }
    spdlog::info("discovery loop stopped");
}

}  // namespace restocker
