#include <iostream>
#include <cassert>
#include "component_registry.hpp"

using namespace veneer;

void test_component_registry() {
    // 1. Check known component recognition (17 dedicated + 8 primitives)
    assert(ComponentSchemaRegistry::isKnownComponent("UiNavHeader") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiHeroLanding") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiTableListPage") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiModernGridPage") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiCommentListPage") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiDashboardPage") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiImageCard") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiImageViewer") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiPaginationBar") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiPostDetails") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiScrollPanel") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiSearchBar") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiSplitLayout") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiStatsDashboard") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiTable") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiTagBadge") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiToast") == true);
    // Primitives
    assert(ComponentSchemaRegistry::isKnownComponent("UiBox") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiFlexRow") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiFlexColumn") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiGrid") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiText") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiImage") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiLink") == true);
    assert(ComponentSchemaRegistry::isKnownComponent("UiScrollBox") == true);
    // Unknown components
    assert(ComponentSchemaRegistry::isKnownComponent("UnknownComp") == false);
    assert(ComponentSchemaRegistry::isKnownComponent("CustomDiv") == false);

    // 2. Check real-world prop validation
    // Known valid props
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "siteName") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "primaryLinks") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "sticky") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiHeroLanding", "tagline") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiToast", "message") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiImageCard", "linkUrl") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiSearchBar", "queryParamName") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiDashboardPage", "cards") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiModernGridPage", "mobileColumns") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiPostDetails", "tagGroups") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiText", "content") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiImage", "src") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiLink", "href") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiScrollBox", "maxHeight") == true);

    // Invalid / deprecated / non-existent props
    assert(ComponentSchemaRegistry::isValidProp("UiImageCard", "url") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiSearchBar", "paramName") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiDashboardPage", "widgets") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "invalidFakeProp") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiHeroLanding", "nonExistent") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiToast", "nonExistentProp") == false);

    // 3. Check Levenshtein suggestions ("did you mean")
    // Close typo suggestions
    std::string suggestion1 = ComponentSchemaRegistry::getDidYouMean("UiImageCard", "linkUr");
    assert(suggestion1 == "linkUrl");

    std::string suggestion2 = ComponentSchemaRegistry::getDidYouMean("UiSearchBar", "queryParamNam");
    assert(suggestion2 == "queryParamName");

    std::string suggestion3 = ComponentSchemaRegistry::getDidYouMean("UiDashboardPage", "card");
    assert(suggestion3 == "cards");

    std::string suggestion4 = ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "siteNam");
    assert(suggestion4 == "siteName");

    std::string suggestion5 = ComponentSchemaRegistry::getDidYouMean("UiHeroLanding", "taglin");
    assert(suggestion5 == "tagline");

    std::string suggestion6 = ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "primarLnk");
    assert(suggestion6 == "primaryLinks");

    std::string suggestion7 = ComponentSchemaRegistry::getDidYouMean("UiModernGridPage", "mobileColumn");
    assert(suggestion7 == "mobileColumns");

    std::string suggestion8 = ComponentSchemaRegistry::getDidYouMean("UiPostDetails", "tagGroup");
    assert(suggestion8 == "tagGroups");

    // Distant / invalid props return empty string
    assert(ComponentSchemaRegistry::getDidYouMean("UiImageCard", "url").empty());
    assert(ComponentSchemaRegistry::getDidYouMean("UiSearchBar", "paramName").empty());
    assert(ComponentSchemaRegistry::getDidYouMean("UiDashboardPage", "widgets").empty());
    assert(ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "completelyUnrelatedWordXYZ").empty());

    // 4. Unknown component passes gracefully
    assert(ComponentSchemaRegistry::isValidProp("CustomUnknownComp", "anyProp") == true);
    assert(ComponentSchemaRegistry::isValidProp("CustomDiv", "anything") == true);
    assert(ComponentSchemaRegistry::isValidProp("MySpecialComponent", "fooBar") == true);
    assert(ComponentSchemaRegistry::getDidYouMean("CustomUnknownComp", "foo").empty());

    // 5. Schema map integrity
    const auto& schemas = ComponentSchemaRegistry::getSchemaMap();
    assert(schemas.size() == 25);
    assert(schemas.find("UiNavHeader") != schemas.end());
    assert(schemas.find("UiToast") != schemas.end());
    assert(schemas.find("UiBox") != schemas.end());
    assert(schemas.find("UiScrollBox") != schemas.end());

    std::cout << "[PASS] ComponentSchemaRegistry unit tests passed successfully!\n";
}

int main() {
    test_component_registry();
    return 0;
}
