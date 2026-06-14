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

    // Result of a cookie harvest: the kmart.com.au cookie jar plus the browser's
    // own User-Agent (so an HTTP replay can match the client that produced them).
    struct HarvestedCookies {
        std::string cookie;      // "name=value; name=value; ..." ("" on failure)
        std::string user_agent;  // navigator.userAgent of the harvest browser
    };

    // Re-navigate the browser (re-triggering Akamai's sensor) and return a fresh
    // kmart.com.au cookie jar + the browser UA. Used by the HTTP transport to
    // re-seed cookies after repeated failures. `cookie` is empty on failure.
    HarvestedCookies harvestCookies();

    // Close the browser and remove the temp profile. Called by the destructor too.
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace restocker
