#include "DiscoveryLoop.h"

#include <chrono>
#include <random>

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

DiscoveryLoop::DiscoveryLoop(const Config& cfg, ConstructorClient& client, Database& db,
                             StopToken& stop)
    : cfg_(cfg), client_(client), db_(db), stop_(stop) {}

int DiscoveryLoop::runOnce() {
    int newly = 0;
    int seen = 0;
    const std::string session_id = db_.getOrCreateSessionId();

    for (const auto& term : cfg_.constructor.search_terms) {
        for (int page = 1; page <= cfg_.constructor.max_pages; ++page) {
            if (stop_.stopRequested()) return newly;

            auto res = client_.fetchPage(term, page, session_id, db_.nextSessionSeq(),
                                         epochMs());
            if (!res.ok) {
                spdlog::warn("discovery '{}' page {} failed: {}", term, page, res.error);
                break;  // move on to next term
            }

            for (const auto& p : res.products) {
                ++seen;
                if (db_.upsertProduct(p)) {
                    ++newly;
                    spdlog::info("discovered new product keycode={} '{}'{}", p.variation_id,
                                 p.name, p.is_preorder ? " [pre-order]" : "");
                }
            }

            spdlog::debug("discovery '{}' page {}: {} matched, total_results={}, ratelimit_remaining={}",
                          term, page, res.products.size(), res.total_results,
                          res.ratelimit_remaining);

            // Stop paging this term once we run past the result set.
            if (res.total_results > 0 &&
                page * cfg_.constructor.num_results_per_page >= res.total_results) {
                break;
            }
            // Back off if we're close to the rate limit.
            if (res.ratelimit_remaining >= 0 &&
                res.ratelimit_remaining < cfg_.constructor.min_ratelimit_remaining) {
                spdlog::warn("constructor rate limit low ({}); ending discovery pass early",
                             res.ratelimit_remaining);
                return newly;
            }
            if (cfg_.constructor.page_delay_ms > 0) {
                if (!stop_.sleepFor(
                        std::chrono::milliseconds(cfg_.constructor.page_delay_ms))) {
                    return newly;
                }
            }
        }
    }

    spdlog::info("discovery pass complete: {} matched, {} new", seen, newly);
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
