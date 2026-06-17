#include "DiscoveryLoop.h"

#include <chrono>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

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
            std::string kc = p.product_id;
            map.emplace(std::move(kc), std::move(p));
        }
        spdlog::debug("browse page {}: {} products, total_results={}, ratelimit_remaining={}",
                      page, got, res.total_results, res.ratelimit_remaining);

        if (res.total_results > 0 && page * cc.num_results_per_page >= res.total_results) {
            break;
        }
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
    int newly = 0;
    std::unordered_set<std::string> newKeycodes;

    // 1. Sitemap sweep (cheap; nullopt when nothing changed since last pass).
    auto cur = sitemap_.sweep();
    if (cur) {
        for (const auto& kv : *cur) {
            if (stop_.stopRequested()) return newly;
            const std::string& keycode = kv.first;
            const std::string& url = kv.second;
            if (db_.insertProductIfAbsent(
                    static_cast<int>(Distributor::Kmart), keycode, url,
                    productNameFromUrl(url, cfg_.constructor.url_prefix_filter))) {
                newKeycodes.insert(keycode);
                ++newly;
            }
        }
        if (newly > 0) spdlog::info("discovery: {} new product(s) from sitemap", newly);
    }

    // 2. Decide if we need to hit the browse endpoint this pass.
    const auto now = std::chrono::steady_clock::now();
    const bool refreshDue =
        !browse_refreshed_ ||
        (now - last_browse_refresh_) >=
            std::chrono::seconds(cfg_.intervals.browse_refresh_seconds);
    const bool browseNeeded = !newKeycodes.empty() || refreshDue;

    // 3. One browse sweep serves both the new-product and refresh-all paths.
    if (browseNeeded && !stop_.stopRequested()) {
        auto browse = sweepBrowseStatus();
        int enriched = 0;
        for (auto& kv : browse) {
            // Enrich newly-found rows always; on a refresh, enrich every existing
            // row (updateProductStatus is a no-op when the keycode is absent).
            if (refreshDue || newKeycodes.count(kv.first)) {
                db_.updateProductStatus(kv.second);
                ++enriched;
            }
        }
        if (refreshDue) {
            last_browse_refresh_ = now;
            browse_refreshed_ = true;
        }
        spdlog::info("discovery: browse sweep {} entries, {} enriched ({})", browse.size(),
                     enriched, refreshDue ? "full refresh" : "new only");

        // 4. Something was (re)checked — let the inventory loop poll stock now.
        stop_.wake();
    }

    return newly;
}

void DiscoveryLoop::run() {
    spdlog::info("discovery loop started (sitemap every ~{}s, browse refresh ~{}s)",
                 cfg_.intervals.sitemap_seconds, cfg_.intervals.browse_refresh_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("discovery pass threw: {}", e.what());
        }
        if (!stop_.sleepFor(jitteredDelay(cfg_.intervals.sitemap_seconds,
                                          cfg_.intervals.sitemap_jitter_seconds))) {
            break;
        }
    }
    spdlog::info("discovery loop stopped");
}

}  // namespace restocker
