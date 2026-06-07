#include "NotifierManager.h"

#include <spdlog/spdlog.h>

#include "DiscordNotifier.h"
#include "GenericWebhookNotifier.h"

namespace restocker {

NotifierManager::NotifierManager(const NotifiersConfig& cfg, HttpClient& http, bool dry_run)
    : dry_run_(dry_run) {
    if (cfg.discord.enabled) {
        notifiers_.push_back(
            std::make_unique<DiscordNotifier>(cfg.discord.webhook_url, http));
        spdlog::info("notifier enabled: discord");
    }
    if (cfg.generic.enabled) {
        notifiers_.push_back(std::make_unique<GenericWebhookNotifier>(
            cfg.generic.url, cfg.generic.headers, http));
        spdlog::info("notifier enabled: generic");
    }
    if (notifiers_.empty()) {
        spdlog::warn("no notifiers enabled — restocks will only be logged");
    }
}

void NotifierManager::notifyAll(const RestockEvent& event) {
    spdlog::info("RESTOCK keycode={} '{}' channel={} location={} {}->{}", event.keycode,
                 event.name, event.channel,
                 event.location_id.empty() ? "-" : event.location_id, event.previous,
                 event.available);

    if (dry_run_) {
        spdlog::info("[dry-run] suppressing {} notifier delivery", notifiers_.size());
        return;
    }

    for (auto& n : notifiers_) {
        bool ok = n->notify(event);
        spdlog::info("notifier {} -> {}", n->name(), ok ? "ok" : "FAILED");
    }
}

}  // namespace restocker
