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
            {"UiBox", {"children", "className", "id", "key", "style"}},
            {"UiCommentListPage", {"children", "className", "height", "id", "key", "pageLinks", "pageTitle", "style", "threads"}},
            {"UiDashboardPage", {"cards", "children", "className", "height", "id", "key", "pageTitle", "style", "subTitle"}},
            {"UiFlexColumn", {"children", "className", "id", "key", "style"}},
            {"UiFlexRow", {"children", "className", "id", "key", "style"}},
            {"UiFormContainer", {"actionUrl", "children", "className", "fields", "hiddenInputs", "id", "key", "method", "style", "subTitle", "submitLabel", "title"}},
            {"UiGrid", {"children", "className", "id", "key", "style"}},
            {"UiHeroLanding", {"children", "className", "ctaLabel", "ctaUrl", "id", "key", "logoHref", "logoUrl", "primaryLinks", "searchParamName", "searchPlaceholder", "searchSubmitUrl", "siteName", "style", "subtext", "tagline"}},
            {"UiImage", {"alt", "children", "className", "id", "key", "src", "style"}},
            {"UiImageCard", {"aspectRatio", "children", "className", "id", "imageFit", "imageUrl", "key", "linkUrl", "loading", "showTitle", "style", "title", "width"}},
            {"UiImageViewer", {"alt", "background", "children", "className", "fit", "id", "key", "src", "style"}},
            {"UiLink", {"children", "className", "href", "id", "key", "style"}},
            {"UiModernGridPage", {"children", "className", "height", "hideSidebarOnMobile", "id", "items", "key", "mobileBreakpoint", "mobileCardAspectRatio", "mobileColumns", "mobileGap", "mobileHeaderSticky", "mobilePadding", "mobileShowHeader", "mobileShowPagination", "onLoadMore", "pageLinks", "pageTitle", "searchDefaultValue", "searchParamName", "searchPlaceholder", "searchSubmitUrl", "showSearch", "sidebarHtml", "sidebarWidth", "style", "tagGroups", "tags"}},
            {"UiNavHeader", {"children", "className", "extraHtml", "hideOnMobile", "id", "items", "key", "layout", "logoHref", "logoUrl", "mobileBreakpoint", "primaryLinks", "secondaryLinks", "siteName", "sticky", "style"}},
            {"UiNestedTreeTable", {"children", "className", "columns", "data", "expandedDepth", "id", "key", "style", "title"}},
            {"UiPaginationBar", {"children", "className", "id", "key", "pageLinks", "paramName", "style"}},
            {"UiPostDetails", {"buttons", "children", "className", "id", "imageAlt", "imageUrl", "key", "searchParamName", "searchPlaceholder", "searchSubmitUrl", "showSearch", "statisticsHtml", "style", "tagGroups", "tags"}},
            {"UiScrollBox", {"children", "className", "height", "id", "key", "maxHeight", "overflow", "overflowX", "overflowY", "style"}},
            {"UiScrollPanel", {"buttons", "children", "className", "id", "key", "onClose", "searchParamName", "searchPlaceholder", "searchSubmitUrl", "showSearch", "statisticsHtml", "style", "tags", "width"}},
            {"UiSearchBar", {"children", "className", "defaultValue", "hiddenFields", "id", "key", "method", "placeholder", "queryParamName", "style", "submitUrl"}},
            {"UiSplitLayout", {"buttons", "children", "className", "height", "id", "imageFit", "imageSlot", "key", "mainHtml", "searchParamName", "searchPlaceholder", "searchSubmitUrl", "showSearch", "sidebarSide", "sidebarWidth", "splitButtons", "statisticsHtml", "style", "tags"}},
            {"UiStatsDashboard", {"children", "className", "dateRangeText", "height", "id", "key", "navLinks", "pageTitle", "sections", "style"}},
            {"UiTable", {"children", "className", "columns", "data", "id", "key", "onRowClick", "onSort", "sortDirection", "sortKey", "style"}},
            {"UiTableListPage", {"children", "className", "columns", "height", "id", "key", "onLoadMore", "pageLinks", "pageTitle", "style", "tableRows"}},
            {"UiTabs", {"activeParamName", "children", "className", "id", "key", "orientation", "style", "tabs", "variant"}},
            {"UiTagBadge", {"addUrl", "children", "className", "count", "href", "id", "key", "label", "removeUrl", "style", "variant"}},
            {"UiTerminalConsole", {"autoScroll", "children", "className", "filterLevel", "id", "key", "logs", "maxLines", "style", "title"}},
            {"UiText", {"children", "className", "content", "id", "key", "style"}},
            {"UiToast", {"children", "className", "id", "key", "message", "onClose", "style", "type"}}
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
        size_t maxAllowedDist = std::max<size_t>(2, invalidKey.length() / 3);

        for (const auto& validProp : it->second) {
            size_t dist = levenshteinDistance(invalidKey, validProp);
            if (dist <= maxAllowedDist) {
                if (dist < minDistance || (dist == minDistance && (bestMatch.empty() || validProp < bestMatch))) {
                    minDistance = dist;
                    bestMatch = validProp;
                }
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
