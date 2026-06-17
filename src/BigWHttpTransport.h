#pragma once

#include <functional>
#include <map>
#include <string>

#include "Config.h"
#include "HttpResponse.h"

namespace restocker {

class HttpClient;
class CdpClient;
class Database;

// BigW gateway transport. The availability and stores endpoints on
// api.bigw.com.au are fronted by Akamai Bot Manager, so — like KmartHttpTransport
// — it replays a captured _abck cookie jar over plain (non-impersonated) HTTP with
// the BigW app User-Agent, and self-heals: after `harvest_after_failures`
// consecutive failures it opens a real browser (CdpClient pointed at bigw.com.au)
// to harvest a fresh cookie + UA, persists them, and retries.
//
// Unlike the Kmart transport this is not an IGatewayTransport: BigW availability
// and stores are GETs (and search is a POST), so it exposes get()/post() directly.
class BigWHttpTransport {
public:
    // `harvester` may be null to disable browser re-harvest (static creds only).
    BigWHttpTransport(const BigWConfig& cfg, HttpClient& http, CdpClient* harvester,
                      Database& db);

    // GET `url` (availability / stores), replaying the current credential set.
    HttpResponse get(const std::string& url);
    // POST a JSON body to `url` (search), replaying the current credential set.
    HttpResponse post(const std::string& url, const std::string& body);

private:
    // Headers mirroring the captured request (cookie jar; UA sent separately).
    std::map<std::string, std::string> appHeaders() const;
    // Run one send (the GET or POST closure) with failure-driven cookie re-harvest.
    HttpResponse sendWithHarvest(const std::function<HttpResponse()>& doSend);
    // Harvest a fresh cookie + UA via the browser, adopt and persist them.
    bool harvestAndAdopt();

    HttpClient& http_;
    CdpClient* harvester_;
    Database& db_;

    std::string cookie_;
    std::string user_agent_;
    std::map<std::string, std::string> extra_headers_;

    int harvest_after_failures_;
    int consecutive_failures_ = 0;
};

}  // namespace restocker
