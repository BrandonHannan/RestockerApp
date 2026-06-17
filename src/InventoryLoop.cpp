#include "InventoryLoop.h"

#include <chrono>
#include <random>
#include <unordered_map>

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

InventoryLoop::InventoryLoop(const Config& cfg, KmartGraphQLClient& client, Database& db,
                             NotifierManager& notifiers, StopToken& stop)
    : cfg_(cfg), client_(client), db_(db), notifiers_(notifiers), stop_(stop) {}

int InventoryLoop::runOnce() {
    std::vector<std::string> keycodes =
        db_.getTrackedOOSKeycodes(static_cast<int>(Distributor::Kmart));
    if (keycodes.empty()) {
        spdlog::info("inventory pass: no tracked out-of-stock keycodes");
        return 0;
    }
    spdlog::info("inventory pass: checking {} keycodes", keycodes.size());

    int alerts = 0;
    const int batch = cfg_.kmart.batch_size;
    for (size_t i = 0; i < keycodes.size(); i += batch) {
        if (stop_.stopRequested()) break;

        std::vector<std::string> chunk(
            keycodes.begin() + i,
            keycodes.begin() + std::min(keycodes.size(), i + batch));

        auto res = client_.queryAvailability(chunk);
        if (!res.ok) {
            spdlog::warn("availability batch [{}..{}) failed: {}", i, i + chunk.size(),
                         res.error);
            continue;
        }

        // Group rows by keycode so we fire one consolidated alert per product.
        std::unordered_map<std::string, std::vector<ChannelStock>> byKeycode;
        for (auto& s : res.stocks) {
            byKeycode[s.keycode].push_back(s);
        }
        RestockDeps deps{db_, notifiers_, static_cast<int>(Distributor::Kmart),
                         cfg_.kmart.instore_max,
                         [this](const std::string& kc) { return client_.queryFindInStore(kc); }};
        for (auto& kv : byKeycode) {
            if (processRestock(kv.first, kv.second, deps)) ++alerts;
        }

        bool more = (i + batch) < keycodes.size();
        if (more && cfg_.kmart.batch_delay_ms > 0) {
            if (!stop_.sleepFor(std::chrono::milliseconds(cfg_.kmart.batch_delay_ms))) {
                break;
            }
        }
    }

    spdlog::info("inventory pass complete: {} restock alert(s)", alerts);
    return alerts;
}

void InventoryLoop::run() {
    spdlog::info("inventory loop started (every ~{}s, or on discovery trigger)",
                 cfg_.intervals.inventory_seconds);
    while (!stop_.stopRequested()) {
        try {
            runOnce();
        } catch (const std::exception& e) {
            spdlog::error("inventory pass threw: {}", e.what());
        }
        // Wait for the idle interval, but return early when discovery wakes us
        // (a browse cross-reference happened) so a restock is caught ASAP. The
        // wait restarts each iteration, so a wake rebases the idle timer.
        StopToken::Wait w = stop_.waitForOrWake(jitteredDelay(
            cfg_.intervals.inventory_seconds, cfg_.intervals.inventory_jitter_seconds));
        if (w == StopToken::Wait::Stopped) break;
        if (w == StopToken::Wait::Woken) {
            spdlog::info("inventory loop woken by discovery");
        }
    }
    spdlog::info("inventory loop stopped");
}

}  // namespace restocker
