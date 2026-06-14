#pragma once

#include <map>
#include <string>

#include "Config.h"
#include "GatewayTransport.h"
#include "HttpResponse.h"

namespace restocker {

class HttpClient;
class CdpClient;
class Database;

// Default inventory transport. It passes Akamai Bot Manager by replaying a valid
// cookie jar over plain HTTP (no impersonation), with a User-Agent matching the
// client that produced the cookie and no `sec-ch-ua*` headers — the shape proven to
// return 200. A bearer token is sent only if configured (cookies alone suffice).
//
// The credential set {cookie, user_agent, token} is swappable: it is seeded from a
// previously harvested set (DB) or config, and self-heals — after
// `harvest_after_failures` consecutive failures it opens a real browser (CdpClient)
// to harvest a fresh cookie + its UA, persists them, and retries.
class KmartHttpTransport : public IGatewayTransport {
public:
    // `harvester` may be null to disable browser re-harvest (static creds only).
    KmartHttpTransport(const KmartConfig& cfg, HttpClient& http, CdpClient* harvester,
                       Database& db);

    HttpResponse postGraphQL(const std::string& url, const std::string& jsonBody) override;

private:
    // Replay the current credential set as a plain (non-impersonated) POST.
    HttpResponse doSend(const std::string& url, const std::string& body) const;
    // Headers mirroring the captured request (no sec-ch-ua*; UA sent separately).
    std::map<std::string, std::string> appHeaders() const;
    // Harvest a fresh cookie + UA via the browser, adopt and persist them. The
    // harvested set carries no bearer token. Returns true on success.
    bool harvestAndAdopt();

    HttpClient& http_;
    CdpClient* harvester_;
    Database& db_;

    std::string cookie_;
    std::string user_agent_;
    std::string token_;  // optional; empty for harvested credentials
    std::map<std::string, std::string> extra_headers_;

    int harvest_after_failures_;
    int consecutive_failures_ = 0;
};

}  // namespace restocker
