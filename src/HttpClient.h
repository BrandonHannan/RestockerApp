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

    // POST a JSON body with ONLY the caller-supplied headers (no desktop base
    // headers) and impersonation disabled, sending `user_agent` verbatim. Used to
    // faithfully replay a captured request (e.g. the Kmart mobile-app gateway call)
    // rather than the desktop-Chrome fingerprint postJson applies.
    HttpResponse postJsonRaw(const std::string& url, const std::string& body,
                             const std::map<std::string, std::string>& headers,
                             const std::string& user_agent);

    // IGatewayTransport: POST GraphQL via the curl-impersonate HTTP path.
    HttpResponse postGraphQL(const std::string& url, const std::string& jsonBody) override {
        return postJson(url, jsonBody);
    }

private:
    std::map<std::string, std::string> baseHeaders() const;

    // Knobs controlling how a single request is built/sent.
    struct SendOpts {
        bool include_base_headers = true;  // merge the desktop browser header set
        bool impersonate = true;           // apply curl-impersonate TLS fingerprint
        std::string user_agent;            // when impersonate is off, force this UA
    };
    HttpResponse perform(const std::string& url, bool post, bool head, const std::string& body,
                         const std::map<std::string, std::string>& extra_headers,
                         const SendOpts& opts);

    HttpConfig cfg_;
};

}  // namespace restocker
