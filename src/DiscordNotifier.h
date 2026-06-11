#pragma once

#include <string>

#include "Config.h"
#include "HttpClient.h"
#include "Notifier.h"

namespace restocker {

// Posts a Discord embed to a channel webhook URL. Routes each alert to a
// category-specific webhook based on the product's fulfilment channel and
// pre-order status, falling back to the default webhook for unmatched cases.
class DiscordNotifier : public INotifier {
public:
    DiscordNotifier(const DiscordConfig& cfg, HttpClient& http);

    bool notify(const RestockEvent& event) override;
    const char* name() const override { return "discord"; }

    // Exposed for testing: build the Discord webhook JSON body.
    static std::string buildBody(const RestockEvent& event);

    // Exposed for testing: pick the destination webhook for an event.
    const std::string& webhookFor(const RestockEvent& event) const;

private:
    std::string webhook_url_;       // default / fallback
    std::string webhook_instore_;   // fulfilment_channel == 2
    std::string webhook_online_;    // fulfilment_channel 3/5, not pre-order
    std::string webhook_preorder_;  // fulfilment_channel 3/5, pre-order
    HttpClient& http_;
};

}  // namespace restocker
