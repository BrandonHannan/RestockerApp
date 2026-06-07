#pragma once

#include <string>

#include "HttpResponse.h"

namespace restocker {

// Abstraction over "POST this JSON to the Kmart GraphQL gateway and give me the
// response". Implemented by HttpClient (curl-impersonate transport) and by
// CdpClient (real headless-browser transport). Lets KmartGraphQLClient stay
// agnostic about how the request actually reaches Akamai.
class IGatewayTransport {
public:
    virtual ~IGatewayTransport() = default;

    // POST a JSON body to `url`; return status/text/error in an HttpResponse.
    virtual HttpResponse postGraphQL(const std::string& url, const std::string& jsonBody) = 0;
};

}  // namespace restocker
