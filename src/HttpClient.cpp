#include "HttpClient.h"

#include <algorithm>
#include <cctype>

#include <curl/curl.h>

// curl-impersonate (lexiforest) extends libcurl with this entry point. The
// bundled Windows headers declare it; the Linux prebuilt ships no headers and
// falls back to system libcurl headers that don't. Re-declare with CURL_EXTERN
// so the linkage matches the bundled declaration (no C4273 on MSVC) and the
// Linux/system-header build still compiles. An identical redeclaration is legal.
extern "C" {
CURL_EXTERN CURLcode curl_easy_impersonate(CURL* curl, const char* target,
                                           int default_headers);
}

namespace restocker {
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}

// Accumulates the response body.
size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Captures one response header line, storing lower-cased keys.
size_t writeHeader(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    size_t len = size * nitems;
    std::string line(buffer, len);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = toLower(line.substr(0, colon));
        std::string val = line.substr(colon + 1);
        // trim surrounding whitespace and trailing CRLF.
        auto trim = [](std::string& s) {
            size_t b = s.find_first_not_of(" \t\r\n");
            size_t e = s.find_last_not_of(" \t\r\n");
            if (b == std::string::npos) {
                s.clear();
            } else {
                s = s.substr(b, e - b + 1);
            }
        };
        trim(key);
        trim(val);
        if (!key.empty()) (*headers)[key] = val;
    }
    return len;
}

// RAII wrapper for a curl_slist header list.
struct SList {
    curl_slist* list = nullptr;
    ~SList() {
        if (list) curl_slist_free_all(list);
    }
    void add(const std::string& header) { list = curl_slist_append(list, header.c_str()); }
};

}  // namespace

HttpClient::HttpClient(HttpConfig cfg) : cfg_(std::move(cfg)) {}

std::map<std::string, std::string> HttpClient::baseHeaders() const {
    // The Kmart GraphQL gateway is fronted by Akamai Bot Manager; the TLS/JA3 +
    // HTTP-2 fingerprint (from curl-impersonate) is what gets us past it, but we
    // still mirror a real browser's request headers. sec-fetch-site defaults to
    // "same-site" (correct for the gateway); the cross-site Constructor call
    // overrides it.
    return {
        {"Accept", "*/*"},
        {"Accept-Language", "en-US,en;q=0.9"},
        {"Origin", "https://www.kmart.com.au"},
        {"Referer", "https://www.kmart.com.au/"},
        {"sec-ch-ua",
         "\"Google Chrome\";v=\"149\", \"Chromium\";v=\"149\", \"Not)A;Brand\";v=\"24\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-site"},
        {"priority", "u=1, i"},
    };
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& extra_headers) {
    return perform(url, /*post=*/false, /*head=*/false, /*body=*/"", extra_headers, SendOpts{});
}

HttpResponse HttpClient::head(const std::string& url,
                              const std::map<std::string, std::string>& extra_headers) {
    return perform(url, /*post=*/false, /*head=*/true, /*body=*/"", extra_headers, SendOpts{});
}

HttpResponse HttpClient::postJson(const std::string& url, const std::string& body,
                                  const std::map<std::string, std::string>& extra_headers) {
    auto merged = extra_headers;
    merged.emplace("Content-Type", "application/json");  // don't override caller's
    return perform(url, /*post=*/true, /*head=*/false, body, merged, SendOpts{});
}

HttpResponse HttpClient::postJsonRaw(const std::string& url, const std::string& body,
                                     const std::map<std::string, std::string>& headers,
                                     const std::string& user_agent) {
    auto merged = headers;
    merged.emplace("Content-Type", "application/json");  // don't override caller's
    SendOpts opts;
    opts.include_base_headers = false;  // send ONLY the caller's headers
    opts.impersonate = false;           // plain curl TLS (matches the captured request)
    opts.user_agent = user_agent;
    return perform(url, /*post=*/true, /*head=*/false, body, merged, opts);
}

HttpResponse HttpClient::perform(const std::string& url, bool post, bool head,
                                 const std::string& body,
                                 const std::map<std::string, std::string>& extra_headers,
                                 const SendOpts& opts) {
    HttpResponse out;

    CURL* curl = curl_easy_init();
    if (!curl) {
        out.error = "curl_easy_init failed";
        return out;
    }

    // Apply the Chrome TLS/JA3 + HTTP-2 fingerprint. default_headers=0 keeps our
    // own header set (proven sufficient) and only sets the transport fingerprint.
    if (opts.impersonate) {
        CURLcode ic = curl_easy_impersonate(curl, cfg_.impersonate_target.c_str(),
                                            /*default_headers=*/0);
        if (ic != CURLE_OK) {
            out.error = std::string("curl_easy_impersonate('") + cfg_.impersonate_target +
                        "') failed: " + curl_easy_strerror(ic);
            curl_easy_cleanup(curl);
            return out;
        }
    }

    std::map<std::string, std::string> merged;
    if (opts.include_base_headers) merged = baseHeaders();
    for (const auto& kv : extra_headers) merged[kv.first] = kv.second;

    SList headers;
    for (const auto& kv : merged) {
        headers.add(kv.first + ": " + kv.second);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.text);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out.headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(cfg_.timeout_ms));
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // all supported + auto-decompress
    // curl-impersonate uses BoringSSL, which has no default CA store on Windows.
    // Load the OS-native trust store so certificate verification works.
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));

    // Only force a UA when not impersonating; otherwise the impersonate profile
    // governs it. A per-request UA (raw path) wins over the configured default.
    if (!opts.impersonate) {
        const std::string& ua = !opts.user_agent.empty() ? opts.user_agent : cfg_.user_agent;
        if (!ua.empty()) curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
    }

    if (!cfg_.proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, cfg_.proxy.c_str());
    }

    if (post) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (head) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD: headers only, no body
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        out.error = curl_easy_strerror(rc);
    } else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        out.status_code = code;
    }

    curl_easy_cleanup(curl);
    return out;
}

}  // namespace restocker
