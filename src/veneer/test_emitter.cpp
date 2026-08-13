#include "emitter.hpp"
#include "resolver.hpp"
#include "parser.hpp"
#include "lexer.hpp"
#include <iostream>
#include <cassert>

using namespace veneer;

void testThemeEmitter() {
    std::string code = R"raw(
        theme "Dark" {
            variables {
                "--bg-color": "#121212";
                "--text-color": "#ffffff";
            }
            customStyles {
                ".dark-mode { background: #121212; }"
                "body { color: #ffffff; }"
            }
        }
    )raw";

    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    std::string jsonStr = Emitter::emit(ast);
    nlohmann::json j = nlohmann::json::parse(jsonStr);

    assert(j.contains("theme"));
    assert(j["theme"]["label"] == "Dark");
    assert(j["theme"]["cssVariables"]["--bg-color"] == "#121212");
    assert(j["theme"]["cssVariables"]["--text-color"] == "#ffffff");
    assert(j["theme"]["customStyles"] == ".dark-mode { background: #121212; }\nbody { color: #ffffff; }");

    std::cout << "testThemeEmitter passed." << std::endl;
}

void testSelectorEmitter() {
    std::string code = R"raw(
        selector "#header-ad" {
            action: "hide";
        }
        selector ".legacy-sidebar" -> SidebarComponent {
            title: "Navigation";
            bind items: "user.menuItems | uppercase";
        }
    )raw";

    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    std::string jsonStr = Emitter::emit(ast);
    nlohmann::json j = nlohmann::json::parse(jsonStr);

    assert(j.contains("components"));
    assert(j["components"].is_array());
    assert(j["components"].size() == 2);

    // Component 0: Hide action
    assert(j["components"][0]["containerSelector"] == "#header-ad");
    assert(j["components"][0]["action"] == "hide");

    // Component 1: Component replacement with static & binding props
    assert(j["components"][1]["containerSelector"] == ".legacy-sidebar");
    assert(j["components"][1]["layoutComponent"] == "SidebarComponent");
    assert(j["components"][1]["props"]["title"] == "Navigation");
    assert(j["components"][1]["propsMap"]["items"] == "user.menuItems | uppercase");

    std::cout << "testSelectorEmitter passed." << std::endl;
}

void testReconstructEmitter() {
    std::string code = R"raw(
        class BaseSidebar {
            scope: "container";
        }
        reconstruct ".main-layout" -> MainGrid {
            media: "(max-width: 1024px)";
            preserve {
                header: "#site-header";
                footer: "#site-footer";
            }
            columns: "3";
            bind activeTab: "app.activeTab";
            child sidebar extends BaseSidebar {
                selector: ".sidebar-slot";
                width: "250px";
                bind data: "app.sidebarData";
            }
        }
    )raw";

    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    std::string jsonStr = Emitter::emit(ast);
    nlohmann::json j = nlohmann::json::parse(jsonStr);

    assert(j.contains("reconstructs"));
    assert(j["reconstructs"].is_array());
    assert(j["reconstructs"].size() == 1);

    auto& recon = j["reconstructs"][0];
    assert(recon["containerSelector"] == ".main-layout");
    assert(recon["layoutComponent"] == "MainGrid");
    assert(recon["mediaQuery"] == "(max-width: 1024px)");
    assert(recon["preserve"]["header"] == "#site-header");
    assert(recon["preserve"]["footer"] == "#site-footer");
    assert(recon["props"]["columns"] == "3");
    assert(recon["propsMap"]["activeTab"] == "app.activeTab");

    assert(recon["children"].is_array());
    assert(recon["children"].size() == 1);
    auto& child = recon["children"][0];
    assert(child["name"] == "sidebar");
    assert(child["selector"] == ".sidebar-slot");
    assert(child["scope"] == "container");
    assert(child["props"]["width"] == "250px");
    assert(child["propsMap"]["data"] == "app.sidebarData");

    std::cout << "testReconstructEmitter passed." << std::endl;
}

void testJsonMerging() {
    std::string existingJson = R"({
        "targetUrl": "https://example.com/app",
        "version": "1.5.0",
        "customSettings": {
            "debug": true
        }
    })";

    std::string code = R"raw(
        theme "Light" {
            variables {
                "--bg-color": "#ffffff";
            }
        }
        selector ".old-btn" {
            action: "hide";
        }
    )raw";

    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    std::string jsonStr = Emitter::emit(ast, existingJson);
    nlohmann::json j = nlohmann::json::parse(jsonStr);

    // Retained fields
    assert(j["targetUrl"] == "https://example.com/app");
    assert(j["version"] == "1.5.0");
    assert(j["customSettings"]["debug"] == true);

    // Compiled fields
    assert(j["theme"]["label"] == "Light");
    assert(j["theme"]["cssVariables"]["--bg-color"] == "#ffffff");
    assert(j["components"].size() == 1);
    assert(j["components"][0]["containerSelector"] == ".old-btn");
    assert(j["components"][0]["action"] == "hide");

    std::cout << "testJsonMerging passed." << std::endl;
}

int main() {
    testThemeEmitter();
    testSelectorEmitter();
    testReconstructEmitter();
    testJsonMerging();
    std::cout << "All emitter tests passed successfully!" << std::endl;
    return 0;
}
