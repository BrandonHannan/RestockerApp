#include "KmartHttpTransport.h"

#include <spdlog/spdlog.h>

#include "CdpClient.h"
#include "Database.h"
#include "HttpClient.h"

namespace restocker {

namespace {
// Attempts of (harvest -> replay) when self-healing; re-harvesting re-navigates the
// browser, giving Akamai a beat to validate a freshly issued _abck.
constexpr int kHarvestRetries = 3;
}  // namespace

KmartHttpTransport::KmartHttpTransport(const KmartConfig& cfg, HttpClient& http,
                                       CdpClient* harvester, Database& db)
    : http_(http),
      harvester_(harvester),
      db_(db),
      extra_headers_(cfg.extra_headers),
      harvest_after_failures_(cfg.harvest_after_failures > 0 ? cfg.harvest_after_failures : 3) {
    // Prefer the last harvested set (latest known-good; survives restarts). Falling
    // back to the config seed. (To force a freshly pasted config cookie, clear the
    // app_meta kmart_cookie row.)
    std::string persisted = db_.getKmartCookie();
    if (!persisted.empty()) {
        cookie_ = persisted;
        std::string ua = db_.getKmartUserAgent();
        user_agent_ = !ua.empty() ? ua : cfg.user_agent;
        token_.clear();  // harvested credentials carry no bearer token
    } else {
        cookie_ = cfg.cookie;
        user_agent_ = cfg.user_agent;
        token_ = cfg.auth_token;
    }
}

std::map<std::string, std::string> KmartHttpTransport::appHeaders() const {
    // Mirrors the captured Kmart request shape. No sec-ch-ua* (the proven request
    // sends none); the UA is supplied separately via postJsonRaw.
    std::map<std::string, std::string> h = {
        {"accept", "*/*"},
        {"accept-language", "en-AU,en;q=0.9"},
        {"origin", "https://www.kmart.com.au"},
        {"referer", "https://www.kmart.com.au/"},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-site"},
        {"priority", "u=3, i"},
    };
    if (!cookie_.empty()) h["cookie"] = cookie_;
    if (!token_.empty()) h["authorization"] = "Bearer " + token_;
    // Caller-configured extras (e.g. newrelic/traceparent) override the defaults.
    for (const auto& kv : extra_headers_) h[kv.first] = kv.second;
    return h;
}

HttpResponse KmartHttpTransport::doSend(const std::string& url, const std::string& body) const {
    return http_.postJsonRaw(url, body, appHeaders(), user_agent_);
}

bool KmartHttpTransport::harvestAndAdopt() {
    if (!harvester_) return false;
    CdpClient::HarvestedCookies h = harvester_->harvestCookies();
    if (h.cookie.empty()) return false;
    cookie_ = h.cookie;
    if (!h.user_agent.empty()) user_agent_ = h.user_agent;
    token_.clear();  // a browser harvest yields cookies only, no bearer token
    db_.setKmartCookie(cookie_);
    db_.setKmartUserAgent(user_agent_);
    return true;
}

HttpResponse KmartHttpTransport::postGraphQL(const std::string& url, const std::string& jsonBody) {
    // No cookie at all (first run, nothing in config or DB): harvest one up front so
    // we don't burn failures on an empty jar.
    if (cookie_.empty() && harvester_) {
        spdlog::info("KmartHttpTransport: no seed cookie, harvesting via browser");
        harvestAndAdopt();
    }

    HttpResponse resp = doSend(url, jsonBody);
    if (resp.ok()) {
        consecutive_failures_ = 0;
        return resp;
    }

    ++consecutive_failures_;
    spdlog::warn("KmartHttpTransport: gateway failure {} (status={}, err='{}')",
                 consecutive_failures_, resp.status_code, resp.error);

    if (consecutive_failures_ >= harvest_after_failures_ && harvester_) {
        spdlog::warn("KmartHttpTransport: {} consecutive failures, re-harvesting cookies",
                     consecutive_failures_);
        consecutive_failures_ = 0;
        for (int attempt = 0; attempt < kHarvestRetries; ++attempt) {
            if (!harvestAndAdopt()) break;
            resp = doSend(url, jsonBody);
            if (resp.ok()) {
                spdlog::info("KmartHttpTransport: recovered after cookie re-harvest "
                             "(attempt {})", attempt + 1);
                return resp;
            }
            spdlog::warn("KmartHttpTransport: replay after harvest still failing "
                         "(attempt {}, status={})", attempt + 1, resp.status_code);
        }
        spdlog::error("KmartHttpTransport: cookie re-harvest did not recover the gateway");
    }

    return resp;
}

}  // namespace restocker
