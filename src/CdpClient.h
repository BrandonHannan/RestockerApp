#pragma once

#include <memory>
#include <string>

#include "Config.h"
#include "GatewayTransport.h"
#include "HttpResponse.h"

namespace restocker {

// Delivers the Kmart GraphQL request from inside a real headless browser via the
// Chrome DevTools Protocol. It launches Chrome/Edge, navigates to kmart.com.au
// (acquiring Akamai cookies + the correct origin), and runs the GraphQL fetch()
// in the page context — so the request carries a genuine browser TLS fingerprint
// and passes Akamai Bot Manager.
//
// All platform / libcurl-websocket details live behind a PIMPL.
class CdpClient : public IGatewayTransport {
public:
    explicit CdpClient(BrowserConfig cfg);
    ~CdpClient() override;

    CdpClient(const CdpClient&) = delete;
    CdpClient& operator=(const CdpClient&) = delete;

    // Launch the browser + establish a navigated page session if not already up.
    // Safe to call repeatedly; recovers a dead browser. Returns false on failure.
    bool ensureReady();

    // IGatewayTransport: run the POST as an in-page fetch and return status+body.
    HttpResponse postGraphQL(const std::string& url, const std::string& jsonBody) override;

    // Close the browser and remove the temp profile. Called by the destructor too.
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace restocker
