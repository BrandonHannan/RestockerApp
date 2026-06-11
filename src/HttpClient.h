#pragma once

#include <map>
#include <string>

#include "Config.h"
#include "GatewayTransport.h"
#include "HttpResponse.h"

namespace restocker {

// libcurl(-impersonate) transport: applies the shared browser header set,
// Chrome TLS/JA3 fingerprint, timeout and optional proxy from HttpConfig.
// Header lookups on the response are case-insensitive (keys are lower-cased).
class HttpClient : public IGatewayTransport {
public:
    explicit HttpClient(HttpConfig cfg);

    // GET with default browser headers; `extra_headers` override/extend them.
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& extra_headers = {});

    // HEAD request: returns status + response headers with no body (cheap). Used
    // to diff a resource's ETag/Last-Modified before deciding to download it.
    HttpResponse head(const std::string& url,
                      const std::map<std::string, std::string>& extra_headers = {});

    // POST a JSON body (Content-Type: application/json) with browser headers.
    HttpResponse postJson(const std::string& url, const std::string& body,
                          const std::map<std::string, std::string>& extra_headers = {});

    // IGatewayTransport: POST GraphQL via the curl-impersonate HTTP path.
    HttpResponse postGraphQL(const std::string& url, const std::string& jsonBody) override {
        return postJson(url, jsonBody);
    }

private:
    std::map<std::string, std::string> baseHeaders() const;
    HttpResponse perform(const std::string& url, bool post, bool head, const std::string& body,
                         const std::map<std::string, std::string>& extra_headers);

    HttpConfig cfg_;
};

}  // namespace restocker
