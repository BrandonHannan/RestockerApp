#include "ConstructorClient.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace restocker {

std::string urlEncode(const std::string& s) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%' << static_cast<int>(c >> 4) << static_cast<int>(c & 0xF);
        }
    }
    return out.str();
}

namespace {

// Tolerantly read a string field that may be absent or non-string.
std::string strField(const json& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return {};
    if (it->is_string()) return it->get<std::string>();
    return it->dump();
}

// Tolerantly read an integer field that the catalogue may encode as a number or
// a numeric string. Returns `fallback` if absent/null/unparseable.
int intField(const json& obj, const char* key, int fallback = 0) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return fallback;
    if (it->is_number()) return it->get<int>();
    if (it->is_string()) {
        const std::string& s = it->get_ref<const std::string&>();
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        if (end != s.c_str()) return static_cast<int>(v);
    }
    return fallback;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

std::vector<Product> parseConstructorResults(const std::string& body,
                                             const std::string& url_prefix,
                                             int* total_results_out,
                                             const std::string& target_state) {
    std::vector<Product> out;
    json j = json::parse(body);  // throws on bad JSON; caller handles

    const auto& resp = j.at("response");
    if (total_results_out && resp.contains("total_num_results") &&
        resp["total_num_results"].is_number()) {
        *total_results_out = resp["total_num_results"].get<int>();
    }

    if (!resp.contains("results") || !resp["results"].is_array()) return out;

    for (const auto& r : resp["results"]) {
        if (!r.contains("data") || !r["data"].is_object()) continue;
        const auto& d = r["data"];

        std::string url = strField(d, "url");
        if (url.empty()) url = strField(d, "uri");
        if (!startsWith(url, url_prefix)) continue;  // strict prefix filter

        Product p;
        p.variation_id = strField(d, "variation_id");
        if (p.variation_id.empty()) continue;  // need a keycode to be useful
        p.name = strField(r, "value");
        p.url = url;
        p.brand = strField(d, "Brand");
        // Image key varies across Constructor catalogues; try the common spellings.
        p.image_url = strField(d, "image_url");
        if (p.image_url.empty()) p.image_url = strField(d, "imageUrl");
        if (p.image_url.empty()) p.image_url = strField(d, "image");
        if (d.contains("price") && d["price"].is_number()) {
            p.price = d["price"].get<double>();
        }
        if (d.contains("isPreOrderActive") && d["isPreOrderActive"].is_boolean()) {
            p.is_preorder = d["isPreOrderActive"].get<bool>();
        }
        p.preorder_release_date = strField(d, "preOrderReleaseDate");

        // Fulfilment policy channel (gates notifications downstream).
        p.fulfilment_channel = intField(d, "FulfilmentChannel", 0);

        // Regional gate: if the target state is flagged out-of-stock in the
        // stateOOS cache, store the product but do not promote it to polling.
        if (!target_state.empty()) {
            auto it = d.find("stateOOS");
            if (it != d.end() && it->is_object() && it->contains(target_state)) {
                p.tracked = false;
            }
        }
        out.push_back(std::move(p));
    }
    return out;
}

ConstructorClient::ConstructorClient(ConstructorConfig cfg, HttpClient& http,
                                     std::string target_state)
    : cfg_(std::move(cfg)), http_(http), target_state_(std::move(target_state)) {}

ConstructorClient::PageResult ConstructorClient::fetchPage(const std::string& term, int page,
                                                           const std::string& session_id,
                                                           long seq, long long dt_ms) {
    PageResult result;

    std::ostringstream url;
    url << cfg_.base_url << "/search/" << urlEncode(term)
        << "?c=" << urlEncode(cfg_.client_version)
        << "&key=" << urlEncode(cfg_.key)
        << "&i=" << urlEncode(session_id)
        << "&s=" << seq
        << "&page=" << page
        << "&num_results_per_page=" << cfg_.num_results_per_page
        << "&sort_by=numberOfDaysSinceStartDate&sort_order=ascending"
        << "&_dt=" << dt_ms;

    // Constructor.io is a cross-site (CORS) request from the browser's POV.
    HttpResponse resp = http_.get(url.str(), {{"sec-fetch-site", "cross-site"}});
    if (!resp.ok()) {
        result.error = resp.error.empty()
                           ? ("HTTP " + std::to_string(resp.status_code))
                           : resp.error;
        return result;
    }

    auto it = resp.headers.find("x-ratelimit-remaining");
    if (it != resp.headers.end()) {
        result.ratelimit_remaining = std::strtol(it->second.c_str(), nullptr, 10);
    }

    try {
        result.products = parseConstructorResults(resp.text, cfg_.url_prefix_filter,
                                                  &result.total_results, target_state_);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("parse error: ") + e.what();
    }
    return result;
}

}  // namespace restocker
