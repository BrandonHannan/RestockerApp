#pragma once

#include "Models.h"

namespace restocker {

// A sink for restock alerts. Implementations must not throw; report failures
// via the return value so one bad notifier never takes down the loop.
class INotifier {
public:
    virtual ~INotifier() = default;

    // Deliver one restock event. Returns true on success.
    virtual bool notify(const RestockEvent& event) = 0;

    // Human-readable name for logging.
    virtual const char* name() const = 0;
};

}  // namespace restocker
