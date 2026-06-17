#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Config.h"
#include "HttpClient.h"

namespace restocker {

// Extract the trailing "/p/<id>" id from a BigW product URL, e.g.
// ".../product/<slug>/p/9904047936" -> "9904047936". Returns "" when absent.
std::string extractBigWProductId(const std::string& url);

// Resolves a BigW article id to its product URL using the BigW product sitemaps.
//
// On first use it fetches the sitemap index, then each product sitemap, building
// one in-memory "/p/ id -> product URL" map. Like SitemapClient it diffs each
// sitemap's ETag/Last-Modified validator (cheap HEAD) so a refresh only re-GETs
// the sitemaps that actually changed.
//
// NOTE: the example BigW search articleId (7 digits) and the example sitemap
// "/p/<id>" (10 digits) differ, so resolve() tries the article id against the
// "/p/" id directly and reports the hit-rate via stats so the matching field can
// be confirmed against live data before relying on it.
class BigWSitemapResolver {
public:
    BigWSitemapResolver(BigWConfig cfg, HttpClient& http);

    // Ensure the index is built/refreshed (HEAD-diff; only changed sitemaps are
    // re-downloaded). Returns false on a hard failure to fetch the index.
    bool refresh();

    // Look up the product URL for an article id; std::nullopt if not indexed.
    std::optional<std::string> resolve(const std::string& article_id);

    size_t indexSize() const { return index_.size(); }

private:
    struct CacheEntry {
        std::string validator;  // ETag (preferred) or Last-Modified
        std::vector<std::pair<std::string, std::string>> entries;  // (/p/ id, url)
    };

    // HEAD the sitemap, diff its validator, full-GET + re-extract only on change.
    // Returns the (id,url) entries for this sitemap (cached when unchanged).
    bool fetchSitemap(const std::string& url);

    BigWConfig cfg_;
    HttpClient& http_;

    bool built_ = false;
    std::unordered_map<std::string, CacheEntry> cache_;       // keyed by sitemap URL
    std::unordered_map<std::string, std::string> index_;      // /p/ id -> product URL
};

}  // namespace restocker
