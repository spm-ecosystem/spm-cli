#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <optional>
#include "extractor_pipeline.hpp"
#include "html_validator.hpp"

void testParseCleanNumber() {
    using namespace veneer;

    // Metric suffix conversions
    assert(parseCleanNumber("1.5k").value_or("") == "1500");
    assert(parseCleanNumber("2.5M").value_or("") == "2500000");
    assert(parseCleanNumber("10B").value_or("") == "10000000000");
    assert(parseCleanNumber("0.5k").value_or("") == "500");
    assert(parseCleanNumber("100k").value_or("") == "100000");

    // Currency and thousands / decimal separators
    assert(parseCleanNumber("  $ 2,500.50 ").value_or("") == "2500.5");
    assert(parseCleanNumber("€ 1.234,56").value_or("") == "1234.56");
    assert(parseCleanNumber("R$ 99,90").value_or("") == "99.9");
    assert(parseCleanNumber("£ 1,000,000").value_or("") == "1000000");
    assert(parseCleanNumber("$1,200").value_or("") == "1200");
    assert(parseCleanNumber("1,200.50").value_or("") == "1200.5");
    assert(parseCleanNumber("1.200,50").value_or("") == "1200.5");
    assert(parseCleanNumber("1200,50").value_or("") == "1200.5");
    assert(parseCleanNumber("1,500").value_or("") == "1500");

    // Negative numbers
    assert(parseCleanNumber(" - R$ 100 ").value_or("") == "-100");
    assert(parseCleanNumber("-$50.25").value_or("") == "-50.25");
    assert(parseCleanNumber("$-10").value_or("") == "-10");
    assert(parseCleanNumber("- 50").value_or("") == "-50");

    // Invalid / empty values
    assert(!parseCleanNumber("").has_value());
    assert(!parseCleanNumber("   ").has_value());
    assert(!parseCleanNumber("abc").has_value());
    assert(!parseCleanNumber("$ -- ").has_value());

    std::cout << "[PASS] testParseCleanNumber passed!\n";
}

void testApplyPipes() {
    using namespace veneer;

    // No pipes -> returns initial value
    assert(applyPipes("hello", {}).value_or("") == "hello");

    // cleanNumber pipe
    std::vector<std::string> pipesClean = {"cleanNumber"};
    assert(applyPipes(" 5.2k ", pipesClean).value_or("") == "5200");
    assert(applyPipes("  $ 2,500.50 ", pipesClean).value_or("") == "2500.5");
    assert(!applyPipes("invalid_num", pipesClean).has_value());

    // number pipe
    std::vector<std::string> pipesNumber = {"number"};
    assert(applyPipes("42", pipesNumber).value_or("") == "42");
    assert(applyPipes(" 3.14 ", pipesNumber).value_or("") == "3.14");
    assert(applyPipes("-100", pipesNumber).value_or("") == "-100");
    assert(!applyPipes("abc", pipesNumber).has_value());
    assert(!applyPipes("", pipesNumber).has_value());

    // split pipe (whitespace delimited)
    std::vector<std::string> pipesSplit = {"split"};
    assert(applyPipes("foo  bar\tbaz", pipesSplit).value_or("") == "[\"foo\",\"bar\",\"baz\"]");
    assert(applyPipes("", pipesSplit).value_or("") == "[]");

    // split:delim pipe
    std::vector<std::string> pipesSplitComma = {"split:,"};
    assert(applyPipes("apple, banana , orange", pipesSplitComma).value_or("") == "[\"apple\",\"banana\",\"orange\"]");

    std::vector<std::string> pipesSplitColon = {"split::"};
    assert(applyPipes("a:b:c", pipesSplitColon).value_or("") == "[\"a\",\"b\",\"c\"]");

    // Unknown pipe -> fails
    std::vector<std::string> pipesUnknown = {"invalidPipe"};
    assert(!applyPipes("hello", pipesUnknown).has_value());

    // Chained pipes
    std::vector<std::string> chained = {"cleanNumber", "number"};
    assert(applyPipes(" $ 1.5k ", chained).value_or("") == "1500");

    std::cout << "[PASS] testApplyPipes passed!\n";
}

void testParseShadowSelectorInfo() {
    using namespace veneer;

    // Arrow syntax: shadow: host->inner
    auto info1 = parseShadowSelector("shadow:custom-element->.btn-inner");
    assert(info1.isShadow == true);
    assert(info1.shadowHost == "custom-element");
    assert(info1.innerSelector == ".btn-inner");

    // Space syntax: shadow: host inner
    auto info2 = parseShadowSelector("shadow: custom-host .sub-item");
    assert(info2.isShadow == true);
    assert(info2.shadowHost == "custom-host");
    assert(info2.innerSelector == ".sub-item");

    // Standalone host: shadow: host
    auto info3 = parseShadowSelector("shadow: standalone-host");
    assert(info3.isShadow == true);
    assert(info3.shadowHost == "standalone-host");
    assert(info3.innerSelector == "");

    // Non-shadow selector
    auto info4 = parseShadowSelector(".regular #selector");
    assert(info4.isShadow == false);
    assert(info4.shadowHost == "");
    assert(info4.innerSelector == "");

    std::cout << "[PASS] testParseShadowSelectorInfo passed!\n";
}

void testShadowExtractionPipeline() {
    using namespace veneer;

    std::string html = R"raw(
        <div id="app">
            <div id="host-element" class="custom-card">
                <h2 class="card-title">Shadow Card Title</h2>
                <button class="action-btn">Shadow Action</button>
                <div class="item-list">
                    <span class="item">Item A</span>
                    <span class="item">Item B</span>
                </div>
            </div>
            <div id="normal-element">
                <p>Standard DOM content</p>
            </div>
        </div>
    )raw";

    auto doc = HtmlValidator::parseHtml(html);
    assert(doc != nullptr);

    // 1. queryShadowSelector single element with arrow syntax
    auto actionBtn = queryShadowSelector(doc, "shadow: #host-element->.action-btn");
    assert(actionBtn != nullptr);
    assert(actionBtn->tagName == "button");
    assert(actionBtn->textContent() == "Shadow Action");

    // 2. queryShadowSelector with explicit isShadow, shadowHost, innerSelector
    auto cardTitle = queryShadowSelector(doc, "shadow: #host-element .card-title", true, "#host-element", ".card-title");
    assert(cardTitle != nullptr);
    assert(cardTitle->textContent() == "Shadow Card Title");

    // 3. queryShadowSelector standalone host
    auto hostElem = queryShadowSelector(doc, "shadow: #host-element");
    assert(hostElem != nullptr);
    assert(hostElem->getAttribute("id").value_or("") == "host-element");

    // 4. queryShadowSelector non-matching host or inner
    auto nonExistentHost = queryShadowSelector(doc, "shadow: #missing-host->.action-btn");
    assert(nonExistentHost == nullptr);
    auto nonExistentInner = queryShadowSelector(doc, "shadow: #host-element->.missing-inner");
    assert(nonExistentInner == nullptr);

    // 5. queryShadowSelectorAll multiple elements
    auto items = queryShadowSelectorAll(doc, "shadow: #host-element->.item-list .item");
    assert(items.size() == 2);
    assert(items[0]->textContent() == "Item A");
    assert(items[1]->textContent() == "Item B");

    // 6. extractReconstructData PASS case
    nlohmann::json reconPass = {
        {"containerSelector", "shadow: #host-element->.item-list"},
        {"isShadow", true},
        {"shadowHost", "#host-element"},
        {"innerSelector", ".item-list"}
    };
    auto passRes = extractReconstructData(reconPass, doc);
    assert(passRes["status"] == "PASS");
    assert(passRes["matched"] == 1);
    assert(passRes["isShadow"] == true);
    assert(passRes["shadowHost"] == "#host-element");
    assert(passRes["innerSelector"] == ".item-list");

    // 7. extractReconstructData FAIL case (missing host)
    nlohmann::json reconFail = {
        {"containerSelector", "shadow: #missing-host->.item-list"},
        {"isShadow", true},
        {"shadowHost", "#missing-host"},
        {"innerSelector", ".item-list"}
    };
    auto failRes = extractReconstructData(reconFail, doc);
    assert(failRes["status"] == "FAIL");
    assert(failRes["matched"] == 0);

    // 8. HtmlValidator::validate with shadow DOM reconstructs and components
    nlohmann::json manifest = {
        {"reconstructs", nlohmann::json::array({
            {
                {"containerSelector", "shadow: #host-element->.item-list"},
                {"isShadow", true},
                {"shadowHost", "#host-element"},
                {"innerSelector", ".item-list"},
                {"children", nlohmann::json::array({
                    {
                        {"name", "ShadowItem"},
                        {"selector", ".item"},
                        {"scope", "container"},
                        {"propsMap", {
                            {"text", "self | text"}
                        }}
                    }
                })}
            }
        })},
        {"components", nlohmann::json::array({
            {
                {"selector", "shadow: #host-element->.action-btn"},
                {"isShadow", true},
                {"shadowHost", "#host-element"},
                {"innerSelector", ".action-btn"},
                {"action", "replace"}
            }
        })}
    };

    auto valResult = HtmlValidator::validate(manifest.dump(), html);
    assert(valResult.contains("reconstructs"));
    assert(valResult["reconstructs"].size() == 1);
    assert(valResult["reconstructs"][0]["status"] == "PASS");
    assert(valResult["reconstructs"][0]["isShadow"] == true);
    assert(valResult["reconstructs"][0]["shadowHost"] == "#host-element");
    assert(valResult["reconstructs"][0]["innerSelector"] == ".item-list");
    assert(valResult["reconstructs"][0]["children"].size() == 1);
    assert(valResult["reconstructs"][0]["children"][0]["matched"] == 2);
    assert(valResult["reconstructs"][0]["children"][0]["status"] == "PASS");

    assert(valResult.contains("components"));
    assert(valResult["components"].size() == 1);
    assert(valResult["components"][0]["status"] == "PASS");
    assert(valResult["components"][0]["isShadow"] == true);
    assert(valResult["components"][0]["shadowHost"] == "#host-element");
    assert(valResult["components"][0]["innerSelector"] == ".action-btn");

    std::cout << "[PASS] testShadowExtractionPipeline passed!\n";
}

int main() {
    std::cout << "=== Extractor Pipeline Test ===\n";
    testParseCleanNumber();
    testApplyPipes();
    testParseShadowSelectorInfo();
    testShadowExtractionPipeline();
    std::cout << "ALL EXTRACTOR TESTS PASSED!\n";
    return 0;
}
