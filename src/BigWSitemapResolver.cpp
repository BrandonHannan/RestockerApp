#include "BigWSitemapResolver.h"

#include <spdlog/spdlog.h>

#include "SitemapClient.h"  // extractLocUrls

namespace restocker {
namespace {

// Change-detection key for a sitemap: prefer ETag, fall back to Last-Modified.
std::string validatorOf(const HttpResponse& r) {
    auto et = r.headers.find("etag");
    if (et != r.headers.end() && !et->second.empty()) return et->second;
    auto lm = r.headers.find("last-modified");
    if (lm != r.headers.end()) return lm->second;
    return {};
}

}  // namespace

std::string extractBigWProductId(const std::string& url) {
    // Find the last "/p/" segment and read the id that follows it.
    size_t p = url.rfind("/p/");
    if (p == std::string::npos) return {};
    size_t start = p + 3;  // length of "/p/"
    size_t end = start;
    while (end < url.size() && url[end] != '/' && url[end] != '?' && url[end] != '#') ++end;
    return url.substr(start, end - start);
}

BigWSitemapResolver::BigWSitemapResolver(BigWConfig cfg, HttpClient& http)
    : cfg_(std::move(cfg)), http_(http) {}

bool BigWSitemapResolver::fetchSitemap(const std::string& url) {
    HttpResponse head = http_.head(url);
    std::string validator = validatorOf(head);

    auto it = cache_.find(url);
    if (it != cache_.end() && !validator.empty() && it->second.validator == validator) {
        return true;  // unchanged; keep cached entries
    }

    HttpResponse resp = http_.get(url);
    if (!resp.ok()) {
        spdlog::warn("BigW sitemap GET failed ({}): status={} err='{}'", url,
                     resp.status_code, resp.error);
        return false;
    }

    CacheEntry entry;
    entry.validator = validator;
    for (const auto& loc : extractLocUrls(resp.text)) {
        std::string id = extractBigWProductId(loc);
        if (!id.empty()) entry.entries.emplace_back(std::move(id), loc);
    }
    cache_[url] = std::move(entry);
    return true;
}

bool BigWSitemapResolver::refresh() {
    HttpResponse idx = http_.get(cfg_.sitemap_index_url);
    if (!idx.ok()) {
        spdlog::warn("BigW sitemap index GET failed: status={} err='{}'", idx.status_code,
                     idx.error);
        return false;
    }

    int fetched = 0;
    for (const auto& loc : extractLocUrls(idx.text)) {
        if (loc.find(cfg_.product_sitemap_filter) == std::string::npos) continue;
        if (fetchSitemap(loc)) ++fetched;
    }

    // Rebuild the flat id -> url index from every cached sitemap.
    index_.clear();
    for (const auto& kv : cache_) {
        for (const auto& e : kv.second.entries) {
            index_.emplace(e.first, e.second);
        }
    }
    built_ = true;
    spdlog::info("BigW sitemap index: {} product sitemaps, {} product URLs", fetched,
                 index_.size());
    return true;
}

std::optional<std::string> BigWSitemapResolver::resolve(const std::string& article_id) {
    if (!built_) refresh();
    auto it = index_.find(article_id);
    if (it == index_.end()) return std::nullopt;
    return it->second;
}

}  // namespace restocker
