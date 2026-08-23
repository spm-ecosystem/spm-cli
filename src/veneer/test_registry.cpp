#include <iostream>
#include <cassert>
#include "component_registry.hpp"

using namespace veneer;

void testKnownComponentsAndProps() {
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
    assert(ComponentSchemaRegistry::isKnownComponent("CustomDiv") == false);

    // Known props on known components
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "siteName") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "primaryLinks") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "sticky") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiHeroLanding", "tagline") == true);
    assert(ComponentSchemaRegistry::isValidProp("UiToast", "message") == true);

    // Unknown props on known components
    assert(ComponentSchemaRegistry::isValidProp("UiNavHeader", "invalidFakeProp") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiHeroLanding", "nonExistent") == false);
    assert(ComponentSchemaRegistry::isValidProp("UiToast", "nonExistentProp") == false);

    // Unknown components allow any prop gracefully
    assert(ComponentSchemaRegistry::isValidProp("CustomDiv", "anything") == true);
    assert(ComponentSchemaRegistry::isValidProp("MySpecialComponent", "fooBar") == true);

    std::cout << "testKnownComponentsAndProps passed.\n";
}

void testDidYouMeanSuggestions() {
    // 1 typo distance
    std::string hint1 = ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "siteNam");
    assert(hint1 == "siteName");

    // 2 typo distance
    std::string hint2 = ComponentSchemaRegistry::getDidYouMean("UiHeroLanding", "taglin");
    assert(hint2 == "tagline");

    // 3 typo distance
    std::string hint3 = ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "primarLnk"); // primaryLinks (distance 3)
    assert(hint3 == "primaryLinks");

    // Too far (> 3 distance) -> returns empty string
    std::string hintFar = ComponentSchemaRegistry::getDidYouMean("UiNavHeader", "completelyUnrelatedWordXYZ");
    assert(hintFar.empty());

    // Unknown component -> returns empty string
    std::string hintUnknownComp = ComponentSchemaRegistry::getDidYouMean("UnknownComponent", "foo");
    assert(hintUnknownComp.empty());

    std::cout << "testDidYouMeanSuggestions passed.\n";
}

void testSchemaMapIntegrity() {
    const auto& schemas = ComponentSchemaRegistry::getSchemaMap();
    assert(schemas.size() == 17);
    assert(schemas.find("UiNavHeader") != schemas.end());
    assert(schemas.find("UiToast") != schemas.end());
    std::cout << "testSchemaMapIntegrity passed.\n";
}

int main() {
    std::cout << "=== Component Registry Test ===\n";
    testKnownComponentsAndProps();
    testDidYouMeanSuggestions();
    testSchemaMapIntegrity();
    std::cout << "ALL REGISTRY TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
