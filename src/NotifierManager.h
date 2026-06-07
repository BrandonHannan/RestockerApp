#pragma once

#include <memory>
#include <vector>

#include "Config.h"
#include "HttpClient.h"
#include "Notifier.h"

namespace restocker {

// Owns the enabled notifiers and fans one event out to all of them. In dry-run
// mode it logs what would fire without delivering.
class NotifierManager {
public:
    NotifierManager(const NotifiersConfig& cfg, HttpClient& http, bool dry_run);

    // Deliver to every notifier. Failures are logged, never thrown.
    void notifyAll(const RestockEvent& event);

    bool empty() const { return notifiers_.empty(); }

private:
    std::vector<std::unique_ptr<INotifier>> notifiers_;
    bool dry_run_;
};

}  // namespace restocker
