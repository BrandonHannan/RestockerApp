#pragma once

#include <map>
#include <string>

namespace restocker {

// Result of an HTTP request, produced by either transport (HttpClient or CdpClient).
struct HttpResponse {
    long status_code = 0;
    std::string text;
    std::map<std::string, std::string> headers;  // lower-cased keys
    std::string error;  // non-empty on transport failure

    bool ok() const { return error.empty() && status_code >= 200 && status_code < 300; }
};

}  // namespace restocker
