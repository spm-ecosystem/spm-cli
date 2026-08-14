#include "emitter.hpp"
#include "resolver.hpp"
#include "parser.hpp"
#include "lexer.hpp"
#include "../utils/css_bundler.hpp"
#include "../utils/file_watcher.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

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
    assert(j["components"][0]["selector"] == "#header-ad");
    assert(j["components"][0]["action"] == "hide");

    // Component 1: Component replacement with static & binding props
    assert(j["components"][1]["selector"] == ".legacy-sidebar");
    assert(j["components"][1]["name"] == "SidebarComponent");
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
    assert(recon["props"]["columns"] == 3);
    assert(recon["propsMap"]["activeTab"] == "app.activeTab");

    assert(recon["children"].is_array());
    assert(recon["children"].size() == 1);
    auto& child = recon["children"][0];
    assert(child["name"] == "sidebar");
    assert(child["selector"] == ".sidebar-slot");
    assert(!child.contains("scope"));
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
    assert(j["components"][0]["selector"] == ".old-btn");
    assert(j["components"][0]["action"] == "hide");

    std::cout << "testJsonMerging passed." << std::endl;
}
void testCssBundler() {
    fs::path tempDir = fs::current_path() / "spm_test_css";
    fs::create_directories(tempDir);

    std::ofstream f1(tempDir / "b.css");
    f1 << "body { background: blue; }";
    f1.close();

    std::ofstream f2(tempDir / "a.css");
    f2 << "a { color: red; }";
    f2.close();

    std::string result = veneer::bundleCssFiles(tempDir);

    assert(result.find("a { color: red; }") != std::string::npos);
    assert(result.find("body { background: blue; }") != std::string::npos);
    assert(result.find("a.css") < result.find("b.css")); // alphabet sorting check

    fs::remove_all(tempDir);
    std::cout << "testCssBundler passed.\n";
}
void testFileWatcher() {
    fs::path tempDir = fs::current_path() / "spm_test_watcher";
    fs::create_directories(tempDir);

    std::thread triggerThread([tempDir]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::ofstream triggerFile(tempDir / "change.css");
        triggerFile << "/* change */";
        triggerFile.close();
    });

    auto start = std::chrono::steady_clock::now();
    veneer::FileWatcher::waitChange(tempDir.string());
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    assert(elapsed >= 90);
    assert(elapsed < 2000);

    triggerThread.join();
    fs::remove_all(tempDir);
    std::cout << "testFileWatcher passed.\n";
}

int main() {
    testThemeEmitter();
    testSelectorEmitter();
    testReconstructEmitter();
    testJsonMerging();
    testCssBundler();
    testFileWatcher();
    std::cout << "All emitter tests passed successfully!" << std::endl;
    return 0;
}
