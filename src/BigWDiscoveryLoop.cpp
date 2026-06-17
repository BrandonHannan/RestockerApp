#include "BigWDiscoveryLoop.h"

#include <chrono>
#include <random>

#include <spdlog/spdlog.h>

namespace restocker {
namespace {

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

BigWDiscoveryLoop::BigWDiscoveryLoop(const Config& cfg, BigWSearchClient& search,
                                     BigWSitemapResolver& resolver, Database& db, StopToken& stop)
    : cfg_(cfg), search_(search), resolver_(resolver), db_(db), stop_(stop) {}

int BigWDiscoveryLoop::runOnce() {
    const int kBigW = static_cast<int>(Distributor::BigW);
    int newly = 0;
    int enriched = 0;
    int resolved = 0;
    int seen = 0;

    for (int page = 0; page < cfg_.bigw.max_pages; ++page) {
        if (stop_.stopRequested()) break;

        auto res = search_.fetchPage(page);
        if (!res.ok) {
            spdlog::warn("bigw search page {} failed: {}", page, res.error);
            break;
        }

        for (auto& p : res.products) {
            // Resolve the product URL from the sitemap index (cached/HEAD-diffed).
            if (auto url = resolver_.resolve(p.product_id)) {
                p.url = *url;
                ++resolved;
            } else {
                spdlog::debug("bigw: no sitemap URL for articleId {}", p.product_id);
            }

            ++seen;
            // Insert base row if new; updateProductStatus enriches (no-op if absent,
            // but we just inserted, so it always applies).
            if (db_.insertProductIfAbsent(kBigW, p.product_id, p.url, p.name)) {
                ++newly;
            }
            db_.updateProductStatus(p);
            ++enriched;
        }

        // Last page when the retailer returned fewer than a full page.
        if (res.result_count < cfg_.bigw.per_page) break;

        if (cfg_.bigw.per_product_delay_ms > 0) {
            if (!stop_.sleepFor(std::chrono::milliseconds(cfg_.bigw.per_product_delay_ms))) break;
        }
    }

    spdlog::info("bigw discovery: {} seen, {} new, {} enriched, {} URLs resolved (index {})",
                 seen, newly, enriched, resolved, resolver_.indexSize());

    // Let the BigW inventory loop poll stock now that the catalogue is refreshed.
    stop_.wake();
    return newly;
}

void BigWDiscoveryLoop::run() {
    spdlog::info("bigw discovery loop started (search every ~{}s)", cfg_.bigw.search_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("bigw discovery pass threw: {}", e.what());
        }
        if (!stop_.sleepFor(jitteredDelay(cfg_.bigw.search_seconds,
                                          cfg_.bigw.search_jitter_seconds))) {
            break;
        }
    }
    spdlog::info("bigw discovery loop stopped");
}

}  // namespace restocker
