#include "SitemapClient.h"

#include <algorithm>
#include <cctype>
#include <future>

#include <spdlog/spdlog.h>

namespace restocker {

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

SitemapClient::SitemapClient(ConstructorConfig cfg, HttpClient& http)
    : cfg_(std::move(cfg)), http_(http) {}

std::vector<std::string> SitemapClient::scanProductSitemap(const std::string& sitemap_url) {
    std::vector<std::string> matches;
    HttpResponse r = http_.get(sitemap_url);
    if (!r.ok()) {
        spdlog::warn("sitemap fetch failed: {} ({})", sitemap_url,
                     r.error.empty() ? ("HTTP " + std::to_string(r.status_code)) : r.error);
        return matches;
    }
    for (const auto& url : extractLocUrls(r.text)) {
        if (url.find(cfg_.url_prefix_filter) != std::string::npos) {
            matches.push_back(url);
        }
    }
    return matches;
}

std::unordered_map<std::string, std::string> SitemapClient::sweep() {
    std::unordered_map<std::string, std::string> out;

    HttpResponse r = http_.get(cfg_.sitemap_index_url);
    if (!r.ok()) {
        spdlog::error("sitemap index fetch failed: {} ({})", cfg_.sitemap_index_url,
                      r.error.empty() ? ("HTTP " + std::to_string(r.status_code)) : r.error);
        return out;
    }

    std::vector<std::string> product_sitemaps;
    for (const auto& u : extractLocUrls(r.text)) {
        if (u.find(cfg_.product_sitemap_filter) != std::string::npos) {
            product_sitemaps.push_back(u);
        }
    }
    spdlog::info("sitemap index: {} product sitemaps to sweep", product_sitemaps.size());

    // Bounded concurrency: process the product sitemaps in chunks so we never
    // launch more than sitemap_max_concurrency parallel fetches at once.
    const size_t conc = static_cast<size_t>(std::max(1, cfg_.sitemap_max_concurrency));
    for (size_t i = 0; i < product_sitemaps.size(); i += conc) {
        const size_t end = std::min(product_sitemaps.size(), i + conc);
        std::vector<std::future<std::vector<std::string>>> futures;
        for (size_t k = i; k < end; ++k) {
            futures.push_back(std::async(std::launch::async, &SitemapClient::scanProductSitemap,
                                         this, product_sitemaps[k]));
        }
        for (auto& f : futures) {
            for (const auto& url : f.get()) {
                std::string kc = extractKeycodeFromUrl(url);
                if (!kc.empty()) out.emplace(std::move(kc), url);
            }
        }
    }

    spdlog::info("sitemap sweep complete: {} TCG product keycodes", out.size());
    return out;
}

}  // namespace restocker
