#include "BigWInventoryLoop.h"

#include <chrono>
#include <random>

#include <spdlog/spdlog.h>

#include "Models.h"
#include "RestockEngine.h"

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

BigWInventoryLoop::BigWInventoryLoop(const Config& cfg, BigWAvailabilityClient& client,
                                     BigWStoresClient& stores, Database& db,
                                     NotifierManager& notifiers, StopToken& stop)
    : cfg_(cfg), client_(client), stores_(stores), db_(db), notifiers_(notifiers), stop_(stop) {}

int BigWInventoryLoop::runOnce() {
    const int kBigW = static_cast<int>(Distributor::BigW);
    std::vector<std::string> keycodes = db_.getTrackedKeycodes(kBigW);
    if (keycodes.empty()) {
        spdlog::info("bigw inventory pass: no tracked products");
        return 0;
    }
    spdlog::info("bigw inventory pass: checking {} products", keycodes.size());

    // Store-name enrichment shared across this pass (cached, refreshed if stale).
    RestockDeps deps{db_, notifiers_, kBigW, cfg_.bigw.instore_max,
                     [this](const std::string&) { return stores_.stores(); }};

    int alerts = 0;
    for (size_t i = 0; i < keycodes.size(); ++i) {
        if (stop_.stopRequested()) break;

        auto res = client_.queryAvailability(keycodes[i]);
        if (!res.ok) {
            spdlog::warn("bigw availability for {} failed: {}", keycodes[i], res.error);
            continue;
        }
        if (processRestock(keycodes[i], res.stocks, deps)) ++alerts;

        bool more = (i + 1) < keycodes.size();
        if (more && cfg_.bigw.per_product_delay_ms > 0) {
            if (!stop_.sleepFor(std::chrono::milliseconds(cfg_.bigw.per_product_delay_ms))) break;
        }
    }

    spdlog::info("bigw inventory pass complete: {} restock alert(s)", alerts);
    return alerts;
}

void BigWInventoryLoop::run() {
    spdlog::info("bigw inventory loop started (every ~{}s, or on discovery trigger)",
                 cfg_.bigw.availability_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("bigw inventory pass threw: {}", e.what());
        }
        StopToken::Wait w = stop_.waitForOrWake(jitteredDelay(
            cfg_.bigw.availability_seconds, cfg_.bigw.availability_jitter_seconds));
        if (w == StopToken::Wait::Stopped) break;
        if (w == StopToken::Wait::Woken) {
            spdlog::info("bigw inventory loop woken by discovery");
        }
    }
    spdlog::info("bigw inventory loop stopped");
}

}  // namespace restocker
