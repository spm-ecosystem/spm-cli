#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>

namespace veneer {

class ComponentSchemaRegistry {
public:
    static const std::unordered_map<std::string, std::unordered_set<std::string>>& getSchemaMap() {
        static const std::unordered_map<std::string, std::unordered_set<std::string>> schemas = {
            {"UiNavHeader", {"siteName", "logoUrl", "logoHref", "primaryLinks", "secondaryLinks", "items", "layout", "hideOnMobile", "mobileBreakpoint", "sticky", "className", "style"}},
            {"UiHeroLanding", {"siteName", "logoUrl", "logoHref", "tagline", "subtext", "ctaLabel", "ctaUrl", "searchPlaceholder", "searchSubmitUrl", "searchParamName", "primaryLinks"}},
            {"UiTableListPage", {"pageTitle", "tableRows", "columns", "pageLinks", "height", "className", "style"}},
            {"UiModernGridPage", {"pageTitle", "items", "tagGroups", "pageLinks", "showSearch", "searchPlaceholder", "searchSubmitUrl", "sidebarHtml", "className", "style"}},
            {"UiCommentListPage", {"pageTitle", "threads", "showThumbnail", "className", "style"}},
            {"UiDashboardPage", {"pageTitle", "widgets", "className", "style"}},
            {"UiImageCard", {"title", "imageUrl", "url", "tags", "aspectRatio", "fallbackUrl", "lazy", "className", "style"}},
            {"UiImageViewer", {"src", "imageUrl", "alt", "title", "fit", "className", "style"}},
            {"UiPaginationBar", {"pageLinks", "paramName", "className", "style"}},
            {"UiPostDetails", {"title", "subhead", "imageUrl", "author", "date", "tags", "bodyHtml", "primaryLinks", "className", "style"}},
            {"UiScrollPanel", {"tags", "buttons", "statisticsHtml", "showSearch", "searchPlaceholder", "searchSubmitUrl", "searchParamName", "width", "className", "style"}},
            {"UiSearchBar", {"placeholder", "submitUrl", "paramName", "hiddenFields", "className", "style"}},
            {"UiSplitLayout", {"imageSlot", "tags", "buttons", "statisticsHtml", "sidebarWidth", "sidebarSide", "imageFit", "height", "splitButtons", "showSearch", "searchPlaceholder", "searchSubmitUrl", "mainHtml", "className", "style"}},
            {"UiStatsDashboard", {"title", "stats", "className", "style"}},
            {"UiTable", {"columns", "data", "sortKey", "sortDirection", "className", "style"}},
            {"UiTagBadge", {"label", "count", "href", "addUrl", "removeUrl", "variant", "className", "style"}},
            {"UiToast", {"message", "type", "visible", "duration", "className", "style"}}
        };
        return schemas;
    }

    static bool isKnownComponent(const std::string& compName) {
        const auto& schemas = getSchemaMap();
        return schemas.find(compName) != schemas.end();
    }

    static bool isValidProp(const std::string& compName, const std::string& propKey) {
        const auto& schemas = getSchemaMap();
        auto it = schemas.find(compName);
        if (it == schemas.end()) return true; // Unrecognized components pass gracefully
        return it->second.find(propKey) != it->second.end();
    }

    static std::string getDidYouMean(const std::string& compName, const std::string& invalidKey) {
        const auto& schemas = getSchemaMap();
        auto it = schemas.find(compName);
        if (it == schemas.end()) return "";

        std::string bestMatch = "";
        size_t minDistance = 999;

        for (const auto& validProp : it->second) {
            size_t dist = levenshteinDistance(invalidKey, validProp);
            if (dist < minDistance && dist <= 3) {
                minDistance = dist;
                bestMatch = validProp;
            }
        }
        return bestMatch;
    }

private:
    static size_t levenshteinDistance(const std::string& s1, const std::string& s2) {
        const size_t m = s1.length();
        const size_t n = s2.length();
        if (m == 0) return n;
        if (n == 0) return m;

        std::vector<size_t> costs(n + 1);
        for (size_t j = 0; j <= n; ++j) costs[j] = j;

        for (size_t i = 0; i < m; ++i) {
            costs[0] = i + 1;
            size_t corner = i;
            for (size_t j = 0; j < n; ++j) {
                size_t upper = costs[j + 1];
                if (s1[i] == s2[j]) {
                    costs[j + 1] = corner;
                } else {
                    size_t t = corner < upper ? corner : upper;
                    costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
                }
                corner = upper;
            }
        }
        return costs[n];
    }
};

} // namespace veneer
