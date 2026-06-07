#pragma once

#include <map>
#include <string>

#include "HttpClient.h"
#include "Notifier.h"

namespace restocker {

// Posts a plain, predictable JSON payload to an arbitrary HTTP endpoint with
// optional custom headers. Easy to wire into any downstream consumer.
class GenericWebhookNotifier : public INotifier {
public:
    GenericWebhookNotifier(std::string url, std::map<std::string, std::string> headers,
                           HttpClient& http);

    bool notify(const RestockEvent& event) override;
    const char* name() const override { return "generic"; }

    static std::string buildBody(const RestockEvent& event);

private:
    std::string url_;
    std::map<std::string, std::string> headers_;
    HttpClient& http_;
};

}  // namespace restocker
