#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Config.h"
#include "HttpClient.h"

namespace restocker {

// Extract every URL inside <loc>...</loc> tags from a sitemap XML body.
std::vector<std::string> extractLocUrls(const std::string& xml_content);

// Extract the trailing-digit keycode from a Kmart product URL: the run of digits
// at the very end of the path, ignoring any trailing '/'. Returns "" when the URL
// does not end in digits. This keycode equals Constructor's `variation_id`, which
// is what the discovery join keys on.
std::string extractKeycodeFromUrl(const std::string& url);

// Discovery source: sweeps the Kmart product sitemap(s) and returns the Pokemon
// TCG product universe as a keycode -> product-URL map. Reuses the shared
// HttpClient (gzip on, per-call thread-safe) and fans the product-sitemap fetches
// out with bounded concurrency.
class SitemapClient {
public:
    SitemapClient(ConstructorConfig cfg, HttpClient& http);

    // One full sweep: index -> product sitemaps (filtered) -> keyword-filtered
    // product URLs -> keycode map. Returns an empty map if the index fetch fails
    // (the caller keeps its previous cache in that case).
    std::unordered_map<std::string, std::string> sweep();

private:
    // Fetch one product sitemap and return URLs containing the TCG keyword.
    std::vector<std::string> scanProductSitemap(const std::string& sitemap_url);

    ConstructorConfig cfg_;
    HttpClient& http_;
};

}  // namespace restocker
