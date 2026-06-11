#include "SitemapClient.h"

#include <algorithm>
#include <cctype>
#include <future>
#include <unordered_set>

#include <spdlog/spdlog.h>

namespace restocker {
namespace {

// The change-detection key for a sitemap: prefer ETag, fall back to Last-Modified.
std::string validatorOf(const HttpResponse& r) {
    auto et = r.headers.find("etag");
    if (et != r.headers.end() && !et->second.empty()) return et->second;
    auto lm = r.headers.find("last-modified");
    if (lm != r.headers.end()) return lm->second;
    return {};
}

}  // namespace

std::vector<std::string> extractLocUrls(const std::string& xml_content) {
    std::vector<std::string> urls;
    size_t pos = 0;
    while ((pos = xml_content.find("<loc>", pos)) != std::string::npos) {
        pos += 5;  // length of "<loc>"
        size_t end_pos = xml_content.find("</loc>", pos);
        if (end_pos == std::string::npos) break;
        urls.push_back(xml_content.substr(pos, end_pos - pos));
        pos = end_pos + 6;
    }
    return urls;
}

std::string extractKeycodeFromUrl(const std::string& url) {
    size_t end = url.size();
    while (end > 0 && url[end - 1] == '/') --end;  // strip trailing slashes
    size_t start = end;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(url[start - 1]))) --start;
    if (start == end) return {};  // no trailing digits
    return url.substr(start, end - start);
}

std::string productNameFromUrl(const std::string& url, const std::string& keyword) {
    // Locate the slug: prefer the part after `keyword`, else after "/product/".
    size_t start = std::string::npos;
    if (!keyword.empty()) {
        size_t k = url.find(keyword);
        if (k != std::string::npos) start = k + keyword.size();
    }
    if (start == std::string::npos) {
        size_t p = url.find("/product/");
        if (p != std::string::npos) start = p + 9;  // length of "/product/"
        else start = 0;
    }

    // End of slug: strip a trailing slash, then a trailing "-<digits>" keycode.
    size_t end = url.size();
    while (end > start && url[end - 1] == '/') --end;
    size_t digits = end;
    while (digits > start && std::isdigit(static_cast<unsigned char>(url[digits - 1]))) --digits;
    if (digits > start && digits < end && url[digits - 1] == '-') {
        end = digits - 1;  // drop "-<keycode>"
    }
    if (end <= start) return {};

    std::string slug = url.substr(start, end - start);

    // Turn separators into spaces, collapse runs, trim, title-case each word.
    std::string name;
    name.reserve(slug.size());
    bool prev_space = true;  // also trims leading spaces
    bool start_word = true;
    for (char ch : slug) {
        const bool sep = (ch == '-' || ch == ':' || ch == '_');
        if (sep) {
            if (!prev_space) name.push_back(' ');
            prev_space = true;
            start_word = true;
        } else {
            if (start_word) {
                name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            } else {
                name.push_back(ch);
            }
            prev_space = false;
            start_word = false;
        }
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

SitemapClient::SitemapClient(ConstructorConfig cfg, HttpClient& http)
    : cfg_(std::move(cfg)), http_(http) {}

SitemapClient::FetchResult SitemapClient::fetchProductSitemap(const std::string& url,
                                                              const std::string& cached_validator) {
    FetchResult fr;
    fr.loc = url;

    // Cheap HEAD to read the current validator (no body).
    HttpResponse h = http_.head(url);
    if (!h.ok()) {
        spdlog::warn("sitemap HEAD failed: {} ({})", url,
                     h.error.empty() ? ("HTTP " + std::to_string(h.status_code)) : h.error);
        return fr;  // ok=false -> keep any prior cache
    }
    fr.validator = validatorOf(h);

    // Unchanged since last sweep: reuse cached keycodes, no download.
    if (!fr.validator.empty() && fr.validator == cached_validator) {
        fr.ok = true;
        fr.changed = false;
        return fr;
    }

    // Changed (or first time / no validator): full GET + re-extract.
    HttpResponse r = http_.get(url);
    if (!r.ok()) {
        spdlog::warn("sitemap GET failed: {} ({})", url,
                     r.error.empty() ? ("HTTP " + std::to_string(r.status_code)) : r.error);
        return fr;  // ok=false
    }
    if (fr.validator.empty()) fr.validator = validatorOf(r);
    for (const auto& u : extractLocUrls(r.text)) {
        if (u.find(cfg_.url_prefix_filter) != std::string::npos) {
            std::string kc = extractKeycodeFromUrl(u);
            if (!kc.empty()) fr.keycodes.emplace_back(std::move(kc), u);
        }
    }
    fr.ok = true;
    fr.changed = true;
    return fr;
}

std::optional<std::unordered_map<std::string, std::string>> SitemapClient::sweep() {
    // 1. Fetch the (tiny) index every cycle to learn the current sitemap list.
    HttpResponse idx = http_.get(cfg_.sitemap_index_url);
    if (!idx.ok()) {
        spdlog::error("sitemap index fetch failed: {} ({})", cfg_.sitemap_index_url,
                      idx.error.empty() ? ("HTTP " + std::to_string(idx.status_code)) : idx.error);
        return std::nullopt;  // keep prior cache
    }

    std::vector<std::string> product_sitemaps;
    for (const auto& u : extractLocUrls(idx.text)) {
        if (u.find(cfg_.product_sitemap_filter) != std::string::npos) {
            product_sitemaps.push_back(u);
        }
    }

    // 2. HEAD-diff each product sitemap; full-GET only the changed ones. Bounded
    //    concurrency so we never run more than sitemap_max_concurrency at once.
    bool any_changed = false;
    const size_t conc = static_cast<size_t>(std::max(1, cfg_.sitemap_max_concurrency));
    for (size_t i = 0; i < product_sitemaps.size(); i += conc) {
        const size_t end = std::min(product_sitemaps.size(), i + conc);
        std::vector<std::future<FetchResult>> futures;
        for (size_t k = i; k < end; ++k) {
            const std::string& url = product_sitemaps[k];
            std::string cached;
            auto it = cache_.find(url);
            if (it != cache_.end()) cached = it->second.validator;
            futures.push_back(std::async(std::launch::async, &SitemapClient::fetchProductSitemap,
                                         this, url, cached));
        }
        for (auto& f : futures) {
            FetchResult fr = f.get();
            if (!fr.ok) continue;  // keep any prior cache entry
            CacheEntry& ce = cache_[fr.loc];
            if (fr.changed) {
                any_changed = true;
                ce.validator = fr.validator;
                ce.keycodes = std::move(fr.keycodes);
            } else if (!fr.validator.empty()) {
                ce.validator = fr.validator;  // refresh (e.g. ETag -> same value)
            }
        }
    }

    // 3. Drop cache entries for sitemaps no longer listed in the index.
    {
        std::unordered_set<std::string> live(product_sitemaps.begin(), product_sitemaps.end());
        for (auto it = cache_.begin(); it != cache_.end();) {
            if (live.find(it->first) == live.end()) {
                it = cache_.erase(it);
                any_changed = true;
            } else {
                ++it;
            }
        }
    }

    if (!any_changed) {
        return std::nullopt;  // steady state: nothing to do
    }

    // 4. Aggregate the full current universe from the cache.
    std::unordered_map<std::string, std::string> current;
    for (const auto& kv : cache_) {
        for (const auto& pair : kv.second.keycodes) {
            current.emplace(pair.first, pair.second);
        }
    }
    spdlog::info("sitemap sweep: changed; {} TCG product keycodes", current.size());
    return current;
}

}  // namespace restocker
