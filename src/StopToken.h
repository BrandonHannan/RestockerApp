#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>

namespace restocker {

// Cooperative shutdown signal with an interruptible sleep, plus an on-demand
// "wake" used to make a periodic loop event-driven. Shared by both worker loops
// so that SIGINT/SIGTERM wakes them immediately instead of waiting out a
// multi-minute sleep, and so discovery can poke the inventory loop to run now.
class StopToken {
public:
    enum class Wait { Timeout, Woken, Stopped };

    // Request shutdown and wake every waiter.
    void requestStop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    bool stopRequested() const { return stop_.load(); }

    // Signal a one-shot wake: a thread blocked in waitForOrWake returns Woken.
    // Harmless to sleepFor waiters (their predicate only checks stop_).
    void wake() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            wake_ = true;
        }
        cv_.notify_all();
    }

    // Sleep for `dur` or until stop is requested. Returns false if woken by a
    // stop request (caller should exit), true if the full duration elapsed.
    bool sleepFor(std::chrono::milliseconds dur) {
        std::unique_lock<std::mutex> lock(mtx_);
        bool stopped = cv_.wait_for(lock, dur, [this] { return stop_.load(); });
        return !stopped;
    }

    // Wait up to `dur`, returning early on stop or wake. Consumes a pending wake.
    Wait waitForOrWake(std::chrono::milliseconds dur) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, dur, [this] { return stop_.load() || wake_; });
        if (stop_.load()) return Wait::Stopped;
        if (wake_) {
            wake_ = false;
            return Wait::Woken;
        }
        return Wait::Timeout;
    }

private:
    std::atomic<bool> stop_{false};
    bool wake_ = false;  // guarded by mtx_
    std::mutex mtx_;
    std::condition_variable cv_;
};

}  // namespace restocker
