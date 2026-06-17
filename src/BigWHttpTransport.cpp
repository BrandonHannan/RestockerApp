#include "BigWHttpTransport.h"

#include <spdlog/spdlog.h>

#include "CdpClient.h"
#include "Database.h"
#include "HttpClient.h"

namespace restocker {

namespace {
// Attempts of (harvest -> replay) when self-healing.
constexpr int kHarvestRetries = 3;
}  // namespace

BigWHttpTransport::BigWHttpTransport(const BigWConfig& cfg, HttpClient& http,
                                     CdpClient* harvester, Database& db)
    : http_(http),
      harvester_(harvester),
      db_(db),
      extra_headers_(cfg.extra_headers),
      harvest_after_failures_(cfg.harvest_after_failures > 0 ? cfg.harvest_after_failures : 3) {
    // Prefer the last harvested set (latest known-good; survives restarts), falling
    // back to the config seed. (To force a freshly pasted config cookie, clear the
    // app_meta bigw_cookie row.)
    std::string persisted = db_.getBigWCookie();
    if (!persisted.empty()) {
        cookie_ = persisted;
        std::string ua = db_.getBigWUserAgent();
        user_agent_ = !ua.empty() ? ua : cfg.user_agent;
    } else {
        cookie_ = cfg.cookie;
        user_agent_ = cfg.user_agent;
    }
}

std::map<std::string, std::string> BigWHttpTransport::appHeaders() const {
    std::map<std::string, std::string> h = {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "en-AU,en;q=0.9"},
        {"referer", "BigwApp"},
    };
    if (!cookie_.empty()) h["cookie"] = cookie_;
    // Caller-configured extras override the defaults.
    for (const auto& kv : extra_headers_) h[kv.first] = kv.second;
    return h;
}

bool BigWHttpTransport::harvestAndAdopt() {
    if (!harvester_) return false;
    CdpClient::HarvestedCookies h = harvester_->harvestCookies();
    if (h.cookie.empty()) return false;
    cookie_ = h.cookie;
    if (!h.user_agent.empty()) user_agent_ = h.user_agent;
    db_.setBigWCookie(cookie_);
    db_.setBigWUserAgent(user_agent_);
    return true;
}

HttpResponse BigWHttpTransport::sendWithHarvest(const std::function<HttpResponse()>& doSend) {
    // No cookie at all (first run, nothing in config or DB): harvest one up front so
    // we don't burn failures on an empty jar.
    if (cookie_.empty() && harvester_) {
        spdlog::info("BigWHttpTransport: no seed cookie, harvesting via browser");
        harvestAndAdopt();
    }

    HttpResponse resp = doSend();
    if (resp.ok()) {
        consecutive_failures_ = 0;
        return resp;
    }

    ++consecutive_failures_;
    spdlog::warn("BigWHttpTransport: gateway failure {} (status={}, err='{}')",
                 consecutive_failures_, resp.status_code, resp.error);

    if (consecutive_failures_ >= harvest_after_failures_ && harvester_) {
        spdlog::warn("BigWHttpTransport: {} consecutive failures, re-harvesting cookies",
                     consecutive_failures_);
        consecutive_failures_ = 0;
        for (int attempt = 0; attempt < kHarvestRetries; ++attempt) {
            if (!harvestAndAdopt()) break;
            resp = doSend();
            if (resp.ok()) {
                spdlog::info("BigWHttpTransport: recovered after cookie re-harvest "
                             "(attempt {})", attempt + 1);
                return resp;
            }
            spdlog::warn("BigWHttpTransport: replay after harvest still failing "
                         "(attempt {}, status={})", attempt + 1, resp.status_code);
        }
        spdlog::error("BigWHttpTransport: cookie re-harvest did not recover the gateway");
    }

    return resp;
}

HttpResponse BigWHttpTransport::get(const std::string& url) {
    return sendWithHarvest([&] { return http_.getRaw(url, appHeaders(), user_agent_); });
}

HttpResponse BigWHttpTransport::post(const std::string& url, const std::string& body) {
    return sendWithHarvest(
        [&] { return http_.postJsonRaw(url, body, appHeaders(), user_agent_); });
}

}  // namespace restocker
