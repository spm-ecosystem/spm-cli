#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "html_validator.hpp"

void testHtmlParser() {
    using namespace veneer;
    std::cout << "--- Testing HTML Parsing ---\n";

    std::string html = R"raw(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Test Page</title>
            <style>body { color: red; }</style>
        </head>
        <body>
            <div id="main" class="container active" data-test="true">
                <h1>Hello &amp; Welcome &lt;World&gt;</h1>
                <p class="desc">A simple &quot;paragraph&#39; with non-breaking&nbsp;space.</p>
                <img src="/img/logo.png" alt="Logo" />
                <input type="hidden" name="csrf" value="tok123" />
                <ul class="items">
                    <li class="item" data-id="1">First</li>
                    <li class="item" data-id="2">Second</li>
                </ul>
            </div>
        </body>
        </html>
    )raw";

    auto doc = HtmlValidator::parseHtml(html);
    assert(doc != nullptr);

    // Document contains html root
    auto mainDiv = doc->querySelector("#main");
    assert(mainDiv != nullptr);
    assert(mainDiv->tagName == "div");
    assert(mainDiv->getAttribute("id").value_or("") == "main");
    assert(mainDiv->getAttribute("data-test").value_or("") == "true");
    assert(mainDiv->hasClass("container"));
    assert(mainDiv->hasClass("active"));
    assert(!mainDiv->hasClass("inactive"));

    // Text content decoding
    auto h1 = doc->querySelector("h1");
    assert(h1 != nullptr);
    assert(h1->textContent() == "Hello & Welcome <World>");

    auto p = doc->querySelector("p.desc");
    assert(p != nullptr);
    assert(p->textContent() == "A simple \"paragraph' with non-breaking space.");

    // Void elements
    auto img = doc->querySelector("img");
    assert(img != nullptr);
    assert(img->getAttribute("src").value_or("") == "/img/logo.png");
    assert(img->children.empty());

    auto input = doc->querySelector("input[name='csrf']");
    assert(input != nullptr);
    assert(input->getAttribute("value").value_or("") == "tok123");

    std::cout << "[PASS] HTML Parsing tests passed!\n";
}

void testCssSelectors() {
    using namespace veneer;
    std::cout << "--- Testing CSS Selectors ---\n";

    std::string html = R"raw(
        <div class="wrapper">
            <header id="site-header" class="header main-header">
                <nav class="nav">
                    <a href="/home" class="nav-link active">Home</a>
                    <a href="/about" class="nav-link">About</a>
                </nav>
            </header>
            <main id="content">
                <section class="section hero">
                    <h2 class="title">Hero Title</h2>
                    <p class="subtitle">Hero Subtitle</p>
                </section>
                <section class="section cards">
                    <div class="card" data-category="tech" data-spm-id="spm-c1">
                        <span class="card-title">Card 1</span>
                    </div>
                    <div class="card featured" data-category="design" data-spm-id="spm-c2">
                        <span class="card-title">Card 2</span>
                    </div>
                </section>
            </main>
        </div>
    )raw";

    auto doc = HtmlValidator::parseHtml(html);
    assert(doc != nullptr);

    // ID selector
    assert(doc->querySelector("#site-header") != nullptr);
    assert(doc->querySelector("#non-existent") == nullptr);

    // Class selector & multiple classes
    assert(doc->querySelector(".main-header") != nullptr);
    assert(doc->querySelector(".header.main-header") != nullptr);
    assert(doc->querySelector(".header.invalid") == nullptr);

    // Descendant combinator
    auto cardTitles = doc->querySelectorAll(".section.cards .card-title");
    assert(cardTitles.size() == 2);
    assert(cardTitles[0]->textContent() == "Card 1");
    assert(cardTitles[1]->textContent() == "Card 2");

    // Child combinator (>)
    auto navLinks = doc->querySelectorAll("nav.nav > a.nav-link");
    assert(navLinks.size() == 2);

    auto invalidDirectChild = doc->querySelectorAll(".wrapper > a.nav-link");
    assert(invalidDirectChild.empty());

    // Attribute selectors
    auto techCard = doc->querySelector("[data-category='tech']");
    assert(techCard != nullptr);
    assert(techCard->getAttribute("data-spm-id").value_or("") == "spm-c1");

    auto spmCard = doc->querySelector("[data-spm-id=\"spm-c2\"]");
    assert(spmCard != nullptr);
    assert(spmCard->hasClass("featured"));

    // Fallback selectors (comma and pipe)
    auto fallbackComma = doc->querySelectorAll(".card-title, .subtitle");
    assert(fallbackComma.size() == 3);

    auto fallbackPipe = doc->querySelectorAll(".card-title | .subtitle");
    assert(fallbackPipe.size() == 3);

    // self selector
    auto heroSection = doc->querySelector("section.hero");
    assert(heroSection != nullptr);
    auto selfMatch = heroSection->querySelector("self");
    assert(selfMatch == heroSection);

    std::cout << "[PASS] CSS Selector tests passed!\n";
}

void testExtractors() {
    using namespace veneer;
    std::cout << "--- Testing SPM Extractors ---\n";

    std::string html = R"raw(
        <div class="product-item">
            <h2 class="title">Wireless Noise-Canceling Headphones</h2>
            <span class="price-old">$ 199.99</span>
            <span class="price"> $ 149.50 </span>
            <div class="meta">
                <span class="label">SKU:</span>
                <span class="value">PROD-9988</span>
            </div>
            <a id="btn-buy" href="/checkout?item=1" class="btn">Buy Now</a>
            <a id="btn-details" href="#" onclick="window.location.href='/details/headphones-pro'">Details</a>
            <a id="btn-js" href="javascript:void(0)" onclick="document.location = '/info/headphones'">Info</a>
            <div class="raw-html"><b>Bold text</b> and <i>italic</i></div>
            <div class="tags">audio wireless bluetooth noise-canceling</div>
            <div class="csv-tags">red, green , blue</div>
            <form id="prod-form">
                <input type="hidden" name="prod_id" value="p9988" />
                <input type="hidden" name="token" value="sec_tok_xyz" />
            </form>
        </div>
    )raw";

    auto doc = HtmlValidator::parseHtml(html);
    auto prod = doc->querySelector(".product-item");
    assert(prod != nullptr);

    // 1. text extractor
    auto title = HtmlValidator::extractValue(prod, ".title | text");
    assert(title.has_value() && title.value() == "Wireless Noise-Canceling Headphones");

    // 2. text + cleanNumber pipe
    auto price = HtmlValidator::extractValue(prod, ".price | text | cleanNumber");
    assert(price.has_value() && price.value() == "149.5");

    // 3. attr extractor
    auto buyHref = HtmlValidator::extractValue(prod, "#btn-buy | attr:href");
    assert(buyHref.has_value() && buyHref.value() == "/checkout?item=1");

    // 4. hrefOrOnclick extractor
    auto directLink = HtmlValidator::extractValue(prod, "#btn-buy | hrefOrOnclick");
    assert(directLink.has_value() && directLink.value() == "/checkout?item=1");

    auto onclickLink = HtmlValidator::extractValue(prod, "#btn-details | hrefOrOnclick");
    assert(onclickLink.has_value() && onclickLink.value() == "/details/headphones-pro");

    auto jsOnclickLink = HtmlValidator::extractValue(prod, "#btn-js | hrefOrOnclick");
    assert(jsOnclickLink.has_value() && jsOnclickLink.value() == "/info/headphones");

    // 5. html extractor
    auto rawHtml = HtmlValidator::extractValue(prod, ".raw-html | html");
    assert(rawHtml.has_value() && rawHtml.value() == "<b>Bold text</b> and <i>italic</i>");

    // 6. nextSiblingText extractor
    auto nextText = HtmlValidator::extractValue(prod, ".meta .label | nextSiblingText");
    assert(nextText.has_value() && nextText.value() == "PROD-9988");

    // 7. selector extractor (generates data-spm-id)
    auto btn = prod->querySelector("#btn-buy");
    assert(!btn->getAttribute("data-spm-id").has_value());
    auto selectorVal = HtmlValidator::extractValue(prod, "#btn-buy | selector");
    assert(selectorVal.has_value());
    assert(selectorVal.value().rfind("[data-spm-id=\"spm-id-", 0) == 0);
    assert(btn->getAttribute("data-spm-id").has_value());

    // 8. hiddenInputs extractor
    auto hiddenInputsJson = HtmlValidator::extractValue(prod, "#prod-form | hiddenInputs");
    assert(hiddenInputsJson.has_value());
    nlohmann::json parsedHidden = nlohmann::json::parse(hiddenInputsJson.value());
    assert(parsedHidden.is_array());
    assert(parsedHidden.size() == 2);
    assert(parsedHidden[0]["name"] == "prod_id" && parsedHidden[0]["value"] == "p9988");
    assert(parsedHidden[1]["name"] == "token" && parsedHidden[1]["value"] == "sec_tok_xyz");

    // 9. split & split:delim pipes
    auto splitTags = HtmlValidator::extractValue(prod, ".tags | text | split");
    assert(splitTags.has_value());
    nlohmann::json tagsJson = nlohmann::json::parse(splitTags.value());
    assert(tagsJson.size() == 4);
    assert(tagsJson[0] == "audio" && tagsJson[3] == "noise-canceling");

    auto splitCsv = HtmlValidator::extractValue(prod, ".csv-tags | text | split:,");
    assert(splitCsv.has_value());
    nlohmann::json csvJson = nlohmann::json::parse(splitCsv.value());
    assert(csvJson.size() == 3);
    assert(csvJson[0] == "red" && csvJson[1] == "green" && csvJson[2] == "blue");

    std::cout << "[PASS] SPM Extractor tests passed!\n";
}

void testHtmlValidatorValidate() {
    using namespace veneer;
    std::cout << "--- Testing HtmlValidator::validate ---\n";

    std::string html = R"raw(
        <div id="app">
            <div id="promo-banner" class="banner">50% off today!</div>
            <div id="catalog-container" class="product-grid">
                <div class="count-badge">3 items</div>
                <div class="product-card" data-id="101">
                    <h3 class="prod-title">Keyboard RGB</h3>
                    <span class="prod-price">$ 79.99</span>
                    <a class="prod-link" href="/prod/101">View</a>
                </div>
                <div class="product-card" data-id="102">
                    <h3 class="prod-title">Mouse Gaming</h3>
                    <span class="prod-price">$ 49.50</span>
                    <a class="prod-link" href="/prod/102">View</a>
                </div>
                <div class="product-card" data-id="103">
                    <h3 class="prod-title">Desk Mat</h3>
                    <span class="prod-price">$ 19.00</span>
                    <a class="prod-link" href="/prod/103">View</a>
                </div>
            </div>
            <footer id="footer">
                <a class="footer-logo" href="/home">SPM Shop</a>
            </footer>
        </div>
    )raw";

    nlohmann::json manifest = {
        {"reconstructs", nlohmann::json::array({
            {
                {"containerSelector", "#catalog-container"},
                {"propsMap", {
                    {"totalCount", ".count-badge | text | cleanNumber | number"}
                }},
                {"children", nlohmann::json::array({
                    {
                        {"name", "ProductItem"},
                        {"selector", ".product-card"},
                        {"scope", "container"},
                        {"propsMap", {
                            {"title", ".prod-title | text"},
                            {"price", ".prod-price | text | cleanNumber"},
                            {"url", ".prod-link | hrefOrOnclick"}
                        }}
                    }
                })}
            }
        })},
        {"components", nlohmann::json::array({
            {
                {"selector", "#promo-banner"},
                {"action", "hide"}
            },
            {
                {"selector", "#non-existent-ad"},
                {"action", "hide"}
            },
            {
                {"selector", "#footer"},
                {"action", "replace"},
                {"propsMap", {
                    {"link", ".footer-logo | attr:href"}
                }}
            }
        })}
    };

    auto result = HtmlValidator::validate(manifest.dump(), html);

    // Verify reconstructs
    assert(result.contains("reconstructs"));
    assert(result["reconstructs"].size() == 1);
    const auto& reconRes = result["reconstructs"][0];
    assert(reconRes["containerSelector"] == "#catalog-container");
    assert(reconRes["status"] == "PASS");
    assert(reconRes["matched"] == 1);
    assert(reconRes["binds"].size() == 1);
    assert(reconRes["binds"][0]["key"] == "totalCount");
    assert(reconRes["binds"][0]["status"] == "PASS");
    assert(reconRes["binds"][0]["value"] == "3");

    assert(reconRes["children"].size() == 1);
    const auto& childRes = reconRes["children"][0];
    assert(childRes["name"] == "ProductItem");
    assert(childRes["selector"] == ".product-card");
    assert(childRes["matched"] == 3);
    assert(childRes["status"] == "PASS");
    assert(childRes["itemsBinds"].size() == 3);

    // Verify first product bindings
    assert(childRes["itemsBinds"][0].size() == 3);
    // Find price in first item binds
    for (const auto& bind : childRes["itemsBinds"][0]) {
        if (bind["key"] == "title") {
            assert(bind["status"] == "PASS" && bind["value"] == "Keyboard RGB");
        } else if (bind["key"] == "price") {
            assert(bind["status"] == "PASS" && bind["value"] == "79.99");
        } else if (bind["key"] == "url") {
            assert(bind["status"] == "PASS" && bind["value"] == "/prod/101");
        }
    }

    // Verify components
    assert(result.contains("components"));
    assert(result["components"].size() == 3);
    assert(result["components"][0]["selector"] == "#promo-banner");
    assert(result["components"][0]["status"] == "PASS");
    assert(result["components"][1]["selector"] == "#non-existent-ad");
    assert(result["components"][1]["status"] == "PASS"); // action hide passes even if 0 match
    assert(result["components"][2]["selector"] == "#footer");
    assert(result["components"][2]["status"] == "PASS");
    assert(result["components"][2]["binds"].size() == 1);
    assert(result["components"][2]["binds"][0]["value"] == "/home");

    std::cout << "[PASS] HtmlValidator::validate tests passed!\n";
}

void testEdgeCasesAndFailures() {
    using namespace veneer;
    std::cout << "--- Testing Edge Cases & Failures ---\n";

    std::string html = R"raw(
        <div class="test-root" lang="en-US">
            <h2>Header</h2>
            <p class="first-para" data-tags="news sale featured">Para 1</p>
            <p class="second-para">Para 2</p>
            <div class="empty-div"></div>
            <ul class="list">
                <li class="item" data-code="ABC-01">Item 1</li>
                <li class="item" data-code="ABC-02">Item 2</li>
                <li class="item" data-code="XYZ-03">Item 3</li>
            </ul>
        </div>
    )raw";

    auto doc = HtmlValidator::parseHtml(html);

    // 1. Sibling combinators: '+' (adjacent) and '~' (general)
    auto adjPara = doc->querySelector("h2 + p");
    assert(adjPara != nullptr && adjPara->hasClass("first-para"));

    auto genParas = doc->querySelectorAll("h2 ~ p");
    assert(genParas.size() == 2);

    // 2. Advanced attribute matching
    auto langElem = doc->querySelector("[lang|='en']");
    assert(langElem != nullptr && langElem->hasClass("test-root"));

    auto startsWithCode = doc->querySelectorAll("[data-code^='ABC']");
    assert(startsWithCode.size() == 2);

    auto endsWithCode = doc->querySelector("[data-code$='03']");
    assert(endsWithCode != nullptr && endsWithCode->textContent() == "Item 3");

    auto containsCode = doc->querySelectorAll("[data-code*='-']");
    assert(containsCode.size() == 3);

    auto includesTag = doc->querySelector("[data-tags~='sale']");
    assert(includesTag != nullptr && includesTag->hasClass("first-para"));

    // 3. Pseudo-classes
    auto firstLi = doc->querySelector("ul.list > li:first-child");
    assert(firstLi != nullptr && firstLi->textContent() == "Item 1");

    auto lastLi = doc->querySelector("ul.list > li:last-child");
    assert(lastLi != nullptr && lastLi->textContent() == "Item 3");

    auto secondLi = doc->querySelector("ul.list > li:nth-child(2)");
    assert(secondLi != nullptr && secondLi->textContent() == "Item 2");

    auto oddLis = doc->querySelectorAll("ul.list > li:nth-child(odd)");
    assert(oddLis.size() == 2);

    auto emptyBox = doc->querySelector(".empty-div:empty");
    assert(emptyBox != nullptr);

    // 4. Validation failure cases
    nlohmann::json failingManifest = {
        {"reconstructs", nlohmann::json::array({
            {
                {"containerSelector", "#missing-container"},
                {"propsMap", {{"title", ".missing | text"}}}
            },
            {
                {"containerSelector", ".test-root"},
                {"children", nlohmann::json::array({
                    {
                        {"name", "MissingChild"},
                        {"selector", ".does-not-exist"}
                    },
                    {
                        {"name", "FailingBindingChild"},
                        {"selector", "ul.list > li"},
                        {"propsMap", {
                            {"nonExistentProp", ".non-existent-span | text"}
                        }}
                    }
                })}
            }
        })},
        {"components", nlohmann::json::array({
            {
                {"selector", ".non-existent-comp"},
                {"action", "replace"}
            }
        })}
    };

    auto res = HtmlValidator::validate(failingManifest.dump(), html);

    // Missing container fails
    assert(res["reconstructs"][0]["status"] == "FAIL");
    assert(res["reconstructs"][0]["matched"] == 0);

    // Missing child fails
    assert(res["reconstructs"][1]["children"][0]["status"] == "FAIL");
    assert(res["reconstructs"][1]["children"][0]["matched"] == 0);

    // Child with broken bindings fails
    assert(res["reconstructs"][1]["children"][1]["matched"] == 3);
    assert(res["reconstructs"][1]["children"][1]["status"] == "FAIL");

    // Component with replace action and 0 matches fails
    assert(res["components"][0]["status"] == "FAIL");
    assert(res["components"][0]["matched"] == 0);

    std::cout << "[PASS] Edge Cases & Failures tests passed!\n";
}

int main() {
    std::cout << "=== Native C++ HTML Validator Tests ===\n";
    testHtmlParser();
    testCssSelectors();
    testExtractors();
    testHtmlValidatorValidate();
    testEdgeCasesAndFailures();
    std::cout << "ALL HTML VALIDATOR TESTS PASSED!\n";
    return 0;
}
