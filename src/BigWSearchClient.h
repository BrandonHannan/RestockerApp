#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "Models.h"

namespace restocker {

class BigWHttpTransport;

// Discovery client for the BigW search API. Builds the search POST body for a
// page, fetches it through the BigW transport, and parses + filters results down
// to Pokemon TCG products carrying the configured "ages 6+" specification.
class BigWSearchClient {
public:
    BigWSearchClient(BigWConfig cfg, BigWHttpTransport& transport);

    struct PageResult {
        std::vector<Product> products;  // already filtered; distributor = BigW
        int result_count = 0;           // raw organic.results size (before filter)
        bool ok = false;
        std::string error;
    };

    // Fetch one search page (0-based). URLs are NOT populated here — the search
    // response has no product URL; the discovery loop resolves them via sitemap.
    PageResult fetchPage(int page);

    // Exposed for testing: build the JSON request body for a page.
    std::string buildPayload(int page) const;

private:
    BigWConfig cfg_;
    BigWHttpTransport& transport_;
};

// Exposed for testing: parse a BigW search response body into Product rows,
// keeping only results whose specifications contain {required_spec_name:
// [required_spec_value]}. `result_count_out` (if non-null) receives the raw
// organic.results count. `price_state` selects which prices.<STATE> to read.
std::vector<Product> parseBigWSearch(const std::string& body, const std::string& price_state,
                                     const std::string& required_spec_name,
                                     const std::string& required_spec_value,
                                     int* result_count_out);

}  // namespace restocker
