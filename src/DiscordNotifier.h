#pragma once

#include <string>

#include "HttpClient.h"
#include "Notifier.h"

namespace restocker {

// Posts a Discord embed to a channel webhook URL. This is the standard way for
// a "bot" to deliver restock alerts to a Discord channel.
class DiscordNotifier : public INotifier {
public:
    DiscordNotifier(std::string webhook_url, HttpClient& http);

    bool notify(const RestockEvent& event) override;
    const char* name() const override { return "discord"; }

    // Exposed for testing: build the Discord webhook JSON body.
    static std::string buildBody(const RestockEvent& event);

private:
    std::string webhook_url_;
    HttpClient& http_;
};

}  // namespace restocker
