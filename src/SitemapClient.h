#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Config.h"
#include "HttpClient.h"

namespace restocker {

// Extract every URL inside <loc>...</loc> tags from a sitemap (or sitemap-index)
// XML body.
std::vector<std::string> extractLocUrls(const std::string& xml_content);

// Extract the trailing-digit keycode from a Kmart product URL: the run of digits
// at the very end of the path, ignoring any trailing '/'. Returns "" when the URL
// does not end in digits. This keycode equals Constructor's `variation_id`
// (stored as the product's `product_id`).
std::string extractKeycodeFromUrl(const std::string& url);

// Derive a human-readable product name from the URL slug: the path segment after
// `keyword` (falling back to after "/product/"), with the trailing "-<keycode>"
// removed and '-'/':' turned into spaces, title-cased. Browse enrichment later
// upgrades this to Constructor's clean display name.
std::string productNameFromUrl(const std::string& url, const std::string& keyword);

// Discovery source: sweeps the Kmart product sitemap(s) and returns the Pokemon
// TCG product universe as a keycode -> product-URL map.
//
// Efficiency: the Kmart product sitemaps are large (~4.5 MB each, 13 of them) and
// ignore conditional GET (Cache-Control: no-store -> If-None-Match/If-Modified-
// Since are not honoured). So each sweep issues a cheap HEAD per sitemap, diffs
// the ETag/Last-Modified against an in-memory cache, and only full-GETs the
// sitemaps that actually changed. Steady state is ~14 bodyless HEAD requests.
class SitemapClient {
public:
    SitemapClient(ConstructorConfig cfg, HttpClient& http);

    // Returns std::nullopt when nothing changed since the last call (every sitemap
    // HEAD matched its cached validator and the index list is unchanged).
    // Otherwise the full current keycode->url map, having full-GET re-scanned only
    // the changed sitemaps.
    std::optional<std::unordered_map<std::string, std::string>> sweep();

private:
    struct CacheEntry {
        std::string validator;  // ETag (preferred) or Last-Modified
        std::vector<std::pair<std::string, std::string>> keycodes;  // (keycode, url)
    };

    struct FetchResult {
        std::string loc;
        bool ok = false;       // HEAD (and any follow-up GET) succeeded
        bool changed = false;  // validator differed -> re-scanned
        std::string validator;
        std::vector<std::pair<std::string, std::string>> keycodes;
    };

    // HEAD the sitemap, diff its validator against `cached_validator`, and full-GET
    // + re-extract only when it changed.
    FetchResult fetchProductSitemap(const std::string& url, const std::string& cached_validator);

    ConstructorConfig cfg_;
    HttpClient& http_;

    // Per product-sitemap cache, keyed by sitemap URL.
    std::unordered_map<std::string, CacheEntry> cache_;
};

}  // namespace restocker
