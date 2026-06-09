#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "HttpClient.h"
#include "Models.h"

namespace restocker {

// Discovery client for the Constructor.io search API. Builds the search URL
// (sorted newest-first), fetches a page, and parses + filters results down to
// Pokemon TCG products matching the configured URL prefix.
class ConstructorClient {
public:
    // `target_state` (e.g. "QLD") gates the polling tier via the Constructor
    // stateOOS map; empty disables the gate. See parseConstructorResults.
    ConstructorClient(ConstructorConfig cfg, HttpClient& http,
                      std::string target_state = "");

    struct PageResult {
        std::vector<Product> products;   // already prefix-filtered
        int total_results = 0;
        long ratelimit_remaining = -1;   // -1 if header absent
        bool ok = false;
        std::string error;
    };

    // Fetch one search page. `session_id` is the persistent anon UUID, `seq`
    // the per-request counter, `dt_ms` the epoch-ms cache-buster.
    // Legacy term-search discovery; superseded by fetchBrowsePage + the sitemap.
    PageResult fetchPage(const std::string& term, int page, const std::string& session_id,
                         long seq, long long dt_ms);

    // Fetch one page of the Constructor.io *browse* endpoint for a group id
    // (/browse/group_id/<group_id>). Same response shape as search, so the parsed
    // products carry preorder / fulfilment / stateOOS status. Used by discovery to
    // enrich the keycodes found in the sitemap.
    PageResult fetchBrowsePage(const std::string& group_id, int page,
                               const std::string& session_id, long seq, long long dt_ms);

private:
    ConstructorConfig cfg_;
    HttpClient& http_;
    std::string target_state_;
};

// Exposed for unit reasoning/tests: parse a raw Constructor JSON body, keeping
// only products whose url begins with `url_prefix`. When `target_state` is
// non-empty, a product whose stateOOS map contains that state key is marked
// not-tracked (out of stock in our region).
std::vector<Product> parseConstructorResults(const std::string& body,
                                             const std::string& url_prefix,
                                             int* total_results_out,
                                             const std::string& target_state = "");

// Percent-encode a query term for use in the URL path.
std::string urlEncode(const std::string& s);

}  // namespace restocker
