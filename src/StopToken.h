#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>

namespace restocker {

// Cooperative shutdown signal with an interruptible sleep. Shared by both
// worker loops so that SIGINT/SIGTERM wakes them immediately instead of
// waiting out a multi-minute sleep.
class StopToken {
public:
    // Request shutdown and wake every waiter.
    void requestStop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    bool stopRequested() const { return stop_.load(); }

    // Sleep for `dur` or until stop is requested. Returns false if woken by a
    // stop request (caller should exit), true if the full duration elapsed.
    bool sleepFor(std::chrono::milliseconds dur) {
        std::unique_lock<std::mutex> lock(mtx_);
        bool stopped = cv_.wait_for(lock, dur, [this] { return stop_.load(); });
        return !stopped;
    }

private:
    std::atomic<bool> stop_{false};
    std::mutex mtx_;
    std::condition_variable cv_;
};

}  // namespace restocker
