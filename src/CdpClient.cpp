#include "CdpClient.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <windows.h>
#else
#  include <csignal>
#  include <sys/select.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace restocker {
namespace {

std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// First existing path from a candidate list (auto-detect).
std::string firstExisting(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        std::error_code ec;
        if (!p.empty() && fs::exists(p, ec)) return p;
    }
    return {};
}

std::string detectBrowser() {
#if defined(_WIN32)
    std::vector<std::string> c;
    const char* pf = std::getenv("ProgramFiles");
    const char* pf86 = std::getenv("ProgramFiles(x86)");
    const char* lad = std::getenv("LOCALAPPDATA");
    auto add = [&](const char* base, const char* rel) {
        if (base) c.push_back(std::string(base) + rel);
    };
    add(pf, "\\Google\\Chrome\\Application\\chrome.exe");
    add(pf86, "\\Google\\Chrome\\Application\\chrome.exe");
    add(lad, "\\Google\\Chrome\\Application\\chrome.exe");
    add(pf, "\\Microsoft\\Edge\\Application\\msedge.exe");
    add(pf86, "\\Microsoft\\Edge\\Application\\msedge.exe");
    return firstExisting(c);
#elif defined(__APPLE__)
    return firstExisting({
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
    });
#else
    // Search PATH for common names.
    const char* path = std::getenv("PATH");
    std::vector<std::string> names{"google-chrome", "google-chrome-stable", "chromium",
                                   "chromium-browser", "microsoft-edge"};
    if (path) {
        std::string p(path);
        size_t start = 0;
        while (start <= p.size()) {
            size_t end = p.find(':', start);
            std::string dir = p.substr(start, end == std::string::npos ? std::string::npos
                                                                       : end - start);
            for (const auto& n : names) {
                std::error_code ec;
                std::string full = dir + "/" + n;
                if (!dir.empty() && fs::exists(full, ec)) return full;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return firstExisting({"/usr/bin/google-chrome", "/usr/bin/chromium",
                          "/usr/bin/chromium-browser", "/usr/bin/microsoft-edge"});
#endif
}

}  // namespace

struct CdpClient::Impl {
    BrowserConfig cfg;
    std::string exe;
    fs::path profile_dir;
    CURL* ws = nullptr;
    int next_id = 0;
    std::string session_id;
    bool ready = false;

#if defined(_WIN32)
    HANDLE proc = nullptr;
#else
    pid_t pid = -1;
#endif

    explicit Impl(BrowserConfig c) : cfg(std::move(c)) {}

    // ---- process management ------------------------------------------------

    bool processAlive() {
#if defined(_WIN32)
        if (!proc) return false;
        return WaitForSingleObject(proc, 0) == WAIT_TIMEOUT;
#else
        if (pid <= 0) return false;
        int st = 0;
        pid_t r = waitpid(pid, &st, WNOHANG);
        return r == 0;  // 0 => still running
#endif
    }

    void killProcess() {
#if defined(_WIN32)
        if (proc) {
            TerminateProcess(proc, 0);
            WaitForSingleObject(proc, 3000);
            CloseHandle(proc);
            proc = nullptr;
        }
#else
        if (pid > 0) {
            ::kill(pid, SIGTERM);
            int st = 0;
            for (int i = 0; i < 30 && waitpid(pid, &st, WNOHANG) == 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (waitpid(pid, &st, WNOHANG) == 0) ::kill(pid, SIGKILL);
            pid = -1;
        }
#endif
    }

    std::vector<std::string> browserArgs() const {
        std::vector<std::string> a;
        if (cfg.headless) a.push_back("--headless=new");
        a.push_back("--disable-gpu");
        a.push_back("--no-first-run");
        a.push_back("--no-default-browser-check");
        a.push_back("--disable-extensions");
        a.push_back("--remote-allow-origins=*");  // required for CDP WS on Chrome >= 111
        a.push_back("--user-data-dir=" + profile_dir.string());
        a.push_back("--remote-debugging-port=0");  // ephemeral; written to DevToolsActivePort
        return a;
    }

    bool launch() {
        std::error_code ec;
        fs::create_directories(profile_dir, ec);
        fs::remove(profile_dir / "DevToolsActivePort", ec);

        auto args = browserArgs();
#if defined(_WIN32)
        std::string cmd = "\"" + exe + "\"";
        for (const auto& a : args) cmd += " " + a;
        std::wstring wcmd;
        int n = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
        wcmd.resize(n > 0 ? n - 1 : 0);
        if (n > 0) MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], n);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> mutableCmd(wcmd.begin(), wcmd.end());
        mutableCmd.push_back(L'\0');
        BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                                 CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!ok) {
            spdlog::error("CdpClient: CreateProcess failed ({}) for {}", GetLastError(), exe);
            return false;
        }
        CloseHandle(pi.hThread);
        proc = pi.hProcess;
        return true;
#else
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        pid_t p = fork();
        if (p < 0) {
            spdlog::error("CdpClient: fork failed");
            return false;
        }
        if (p == 0) {
            execv(exe.c_str(), argv.data());
            _exit(127);
        }
        pid = p;
        return true;
#endif
    }

    // Read DevToolsActivePort (line 1 = port, line 2 = browser ws path).
    bool readDevToolsEndpoint(std::string& port, std::string& path, int timeout_ms) {
        fs::path f = profile_dir / "DevToolsActivePort";
        std::int64_t deadline = nowMs() + timeout_ms;
        while (nowMs() < deadline) {
            std::ifstream in(f);
            if (in) {
                std::string l1, l2;
                std::getline(in, l1);
                std::getline(in, l2);
                if (!l1.empty()) {
                    port = l1;
                    path = l2.empty() ? "/devtools/browser" : l2;
                    return true;
                }
            }
            if (!processAlive()) {
                spdlog::error("CdpClient: browser exited before opening debug port");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    // ---- websocket I/O -----------------------------------------------------

    bool waitReadable(int timeout_ms) {
        curl_socket_t sock = CURL_SOCKET_BAD;
        if (curl_easy_getinfo(ws, CURLINFO_ACTIVESOCKET, &sock) != CURLE_OK ||
            sock == CURL_SOCKET_BAD) {
            return false;
        }
        fd_set rfd;
        FD_ZERO(&rfd);
        FD_SET(sock, &rfd);
        timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int r = select(static_cast<int>(sock) + 1, &rfd, nullptr, nullptr, &tv);
        return r > 0;
    }

    bool wsSend(const std::string& text) {
        size_t sent = 0;
        size_t off = 0;
        while (off < text.size()) {
            CURLcode rc = curl_ws_send(ws, text.data() + off, text.size() - off, &sent, 0,
                                       CURLWS_TEXT);
            if (rc == CURLE_AGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (rc != CURLE_OK) return false;
            off += sent;
        }
        return true;
    }

    // Receive one complete text message into `out`.
    bool wsRecv(std::string& out, int timeout_ms) {
        out.clear();
        std::int64_t deadline = nowMs() + timeout_ms;
        for (;;) {
            char buf[16384];
            size_t rlen = 0;
            const struct curl_ws_frame* meta = nullptr;
            CURLcode rc = curl_ws_recv(ws, buf, sizeof(buf), &rlen, &meta);
            if (rc == CURLE_OK) {
                out.append(buf, rlen);
                if (meta && meta->bytesleft == 0) return true;  // frame complete
            } else if (rc == CURLE_AGAIN) {
                int remaining = static_cast<int>(deadline - nowMs());
                if (remaining <= 0 || !waitReadable(remaining)) return false;
            } else {
                return false;
            }
        }
    }

    // ---- CDP JSON-RPC ------------------------------------------------------

    // Send a command and wait for the matching response (ignoring events / other
    // ids). Returns the full response object, or a null json on failure.
    json command(const std::string& method, const json& params, bool with_session,
                 int timeout_ms) {
        int id = ++next_id;
        json msg = {{"id", id}, {"method", method}, {"params", params}};
        if (with_session && !session_id.empty()) msg["sessionId"] = session_id;
        if (!wsSend(msg.dump())) {
            spdlog::warn("CdpClient: ws send failed for {}", method);
            return json();
        }
        std::int64_t deadline = nowMs() + timeout_ms;
        while (nowMs() < deadline) {
            std::string raw;
            if (!wsRecv(raw, static_cast<int>(deadline - nowMs()))) break;
            json obj = json::parse(raw, nullptr, /*allow_exceptions=*/false);
            if (obj.is_discarded()) continue;
            if (obj.contains("id") && obj["id"].is_number() && obj["id"].get<int>() == id) {
                return obj;
            }
            // otherwise it's an event or another id: ignore.
        }
        spdlog::warn("CdpClient: timeout waiting for response to {}", method);
        return json();
    }

    void teardownConnection() {
        if (ws) {
            curl_easy_cleanup(ws);
            ws = nullptr;
        }
        session_id.clear();
        ready = false;
    }

    bool connectAndPrepare() {
        std::string port, path;
        if (!readDevToolsEndpoint(port, path, 15000)) return false;

        std::string url = "ws://127.0.0.1:" + port + path;
        ws = curl_easy_init();
        if (!ws) return false;
        curl_easy_setopt(ws, CURLOPT_URL, url.c_str());
        curl_easy_setopt(ws, CURLOPT_CONNECT_ONLY, 2L);  // websocket
        curl_easy_setopt(ws, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
        CURLcode rc = curl_easy_perform(ws);
        if (rc != CURLE_OK) {
            spdlog::error("CdpClient: ws connect failed: {}", curl_easy_strerror(rc));
            teardownConnection();
            return false;
        }

        // Create a page target and attach (flatten => sessionId on each message).
        json ct = command("Target.createTarget", {{"url", "about:blank"}}, false, cfg.cdp_timeout_ms);
        if (!ct.contains("result") || !ct["result"].contains("targetId")) {
            spdlog::error("CdpClient: Target.createTarget failed");
            teardownConnection();
            return false;
        }
        std::string target_id = ct["result"]["targetId"].get<std::string>();

        json at = command("Target.attachToTarget",
                          {{"targetId", target_id}, {"flatten", true}}, false, cfg.cdp_timeout_ms);
        if (!at.contains("result") || !at["result"].contains("sessionId")) {
            spdlog::error("CdpClient: Target.attachToTarget failed");
            teardownConnection();
            return false;
        }
        session_id = at["result"]["sessionId"].get<std::string>();

        command("Page.enable", json::object(), true, cfg.cdp_timeout_ms);
        if (!navigate()) return false;

        ready = true;
        spdlog::info("CdpClient: browser session ready ({} on port {})", exe, port);
        return true;
    }

    // Navigate to nav_url and let Akamai's sensor JS settle (sets/validates cookies).
    bool navigate() {
        json nav = command("Page.navigate", {{"url", cfg.nav_url}}, true, cfg.cdp_timeout_ms);
        if (!nav.contains("result")) {
            spdlog::warn("CdpClient: Page.navigate to {} returned no result", cfg.nav_url);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.page_settle_ms));
        return true;
    }
};

CdpClient::CdpClient(BrowserConfig cfg) : impl_(std::make_unique<Impl>(std::move(cfg))) {
    impl_->exe = impl_->cfg.executable_path.empty() ? detectBrowser()
                                                    : impl_->cfg.executable_path;
    // Unique per-process profile directory.
#if defined(_WIN32)
    unsigned long pidv = GetCurrentProcessId();
#else
    unsigned long pidv = static_cast<unsigned long>(getpid());
#endif
    impl_->profile_dir =
        fs::temp_directory_path() / ("restocker_cdp_" + std::to_string(pidv));
}

CdpClient::~CdpClient() { shutdown(); }

bool CdpClient::ensureReady() {
    if (impl_->ready && impl_->processAlive() && impl_->ws) return true;

    // Clean up any half-dead state, then (re)launch.
    impl_->teardownConnection();
    if (!impl_->processAlive()) {
        impl_->killProcess();
        if (impl_->exe.empty()) {
            spdlog::error("CdpClient: no Chrome/Edge found; set browser.executable_path");
            return false;
        }
        spdlog::info("CdpClient: launching {}", impl_->exe);
        if (!impl_->launch()) return false;
    }
    return impl_->connectAndPrepare();
}

HttpResponse CdpClient::postGraphQL(const std::string& url, const std::string& jsonBody) {
    HttpResponse out;
    if (!ensureReady()) {
        out.error = "browser transport not ready (CDP launch/connect failed)";
        return out;
    }

    // Convert the POST body into a GET query. A GET with no custom headers is a
    // CORS-"simple" request, so it triggers NO preflight OPTIONS — which Akamai
    // intermittently blocks. The gateway answers getProductAvailability over GET.
    json body = json::parse(jsonBody, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        out.error = "CdpClient: could not parse GraphQL payload";
        return out;
    }
    std::string op = body.value("operationName", std::string());
    std::string query = body.value("query", std::string());
    std::string varsLit = body.contains("variables") ? body["variables"].dump() : "{}";

    // Run in-page inside a try/catch so network/CORS failures return a value
    // (with diagnostics) rather than an opaque CDP exception.
    std::string expr =
        "(async()=>{try{const u=" + json(url).dump() +
        "+'?operationName='+encodeURIComponent(" + json(op).dump() +
        ")+'&variables='+encodeURIComponent(JSON.stringify(" + varsLit +
        "))+'&query='+encodeURIComponent(" + json(query).dump() +
        ");const r=await fetch(u,{credentials:'include'});const t=await r.text();"
        "return {s:r.status,b:t};}"
        "catch(e){return {s:-1,b:String(e),loc:location.href,rs:document.readyState,"
        "t:document.title,bl:(document.body?document.body.innerText.length:0)};}})()";

    json params = {{"expression", expr}, {"awaitPromise", true}, {"returnByValue", true}};

    // "Failed to fetch" usually means Akamai hasn't validated the _abck cookie
    // yet. Retry the same call a few times with short delays (validation happens
    // a beat after load); a full re-navigate only as a mid-point fallback. Once
    // one call succeeds the cookie is valid and subsequent batches sail through.
    constexpr int kMaxAttempts = 6;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        json resp = impl_->command("Runtime.evaluate", params, true, impl_->cfg.cdp_timeout_ms);
        if (resp.is_null() || !resp.contains("result")) {
            impl_->ready = false;  // force relaunch next cycle
            out.error = "CDP Runtime.evaluate failed (no response)";
            return out;
        }
        const json& r = resp["result"];
        if (r.contains("exceptionDetails")) {
            out.error = "in-page evaluate threw: " + r["exceptionDetails"].dump();
            return out;
        }
        if (!r.contains("result") || !r["result"].contains("value")) {
            out.error = "CDP evaluate returned no value";
            return out;
        }
        const json& val = r["result"]["value"];
        long status = val.value("s", 0);
        if (status == -1) {
            out.error = "in-page fetch failed: " + val.value("b", std::string("?")) +
                        " [loc=" + val.value("loc", std::string("?")) +
                        " rs=" + val.value("rs", std::string("?")) +
                        " title='" + val.value("t", std::string("?")) +
                        "' bodyLen=" + std::to_string(val.value("bl", 0)) + "]";
            if (attempt + 1 >= kMaxAttempts) return out;
            if (attempt == 2) {
                spdlog::warn("CdpClient: still blocked after retries, re-navigating");
                impl_->navigate();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            continue;
        }
        out.status_code = status;
        if (val.contains("b") && val["b"].is_string()) out.text = val["b"].get<std::string>();
        out.error.clear();
        return out;
    }
    return out;
}

CdpClient::HarvestedCookies CdpClient::harvestCookies() {
    HarvestedCookies out;
    if (!ensureReady()) {
        spdlog::error("CdpClient: harvestCookies failed (CDP launch/connect failed)");
        return out;
    }

    // Re-navigate so Akamai's sensor re-runs and (re)issues a fresh cookie jar;
    // ensureReady() only navigates on the initial connect, so repeated harvests
    // would otherwise return the same cookies.
    impl_->navigate();

    // Network.getAllCookies returns the whole jar; enable the domain first so the
    // command is available across Chrome versions.
    impl_->command("Network.enable", json::object(), true, impl_->cfg.cdp_timeout_ms);
    json resp = impl_->command("Network.getAllCookies", json::object(), true,
                               impl_->cfg.cdp_timeout_ms);
    if (resp.is_null() || !resp.contains("result") ||
        !resp["result"].contains("cookies") || !resp["result"]["cookies"].is_array()) {
        spdlog::error("CdpClient: Network.getAllCookies returned no cookies");
        return out;
    }

    const std::string& cookie_domain = impl_->cfg.cookie_domain;
    for (const auto& c : resp["result"]["cookies"]) {
        std::string domain = c.value("domain", std::string());
        if (domain.find(cookie_domain) == std::string::npos) continue;
        std::string name = c.value("name", std::string());
        if (name.empty()) continue;
        if (!out.cookie.empty()) out.cookie += "; ";
        out.cookie += name + "=" + c.value("value", std::string());
    }

    // Capture the browser's UA so the HTTP replay matches the harvest client.
    json ua = impl_->command("Runtime.evaluate",
                             {{"expression", "navigator.userAgent"}, {"returnByValue", true}},
                             true, impl_->cfg.cdp_timeout_ms);
    if (ua.contains("result") && ua["result"].contains("result") &&
        ua["result"]["result"].contains("value") &&
        ua["result"]["result"]["value"].is_string()) {
        out.user_agent = ua["result"]["result"]["value"].get<std::string>();
    }

    if (out.cookie.empty()) {
        spdlog::warn("CdpClient: harvested no {} cookies", cookie_domain);
    } else {
        spdlog::info("CdpClient: harvested {} bytes of {} cookies (ua='{}')",
                     out.cookie.size(), cookie_domain, out.user_agent);
    }
    return out;
}

void CdpClient::shutdown() {
    if (impl_->ws) {
        // Best-effort graceful close.
        impl_->command("Browser.close", json::object(), false, 2000);
        impl_->teardownConnection();
    }
    impl_->killProcess();
    std::error_code ec;
    if (!impl_->profile_dir.empty()) fs::remove_all(impl_->profile_dir, ec);
}

}  // namespace restocker
