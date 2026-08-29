#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <cassert>

using namespace veneer;

void test_parser() {
    std::string source = R"raw(
        theme "Obsidian" {
            variables {
                "--bg-color": "black";
                "--text-color": "white";
            }
            customStyles {
                ".button { color: red; }"
            }
        }

        class TagBadge {
            scope: "global";
            color: "blue";
            bind title: "label | capitalize";
        }

        selector "#nav" {
            action: "hide";
            display: "none";
        }

        reconstruct "#view" -> UiSplitLayout {
            preserve {
                slot1: ".header";
                slot2: ".footer";
            }
            media: "(max-width: 600px)";
            child mainPanel extends BasePanel {
                selector: ".main-content";
                scope: "panel";
                padding: "10px";
            }
        }
    )raw";

    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    Parser parser(tokens);
    ASTNode ast = parser.parse();

    // Verify Theme
    assert(ast.themes.size() == 1);
    assert(ast.themes[0].label == "Obsidian");
    assert(ast.themes[0].variables["--bg-color"] == "black");
    assert(ast.themes[0].variables["--text-color"] == "white");
    assert(ast.themes[0].customStyles.size() == 1);
    assert(ast.themes[0].customStyles[0] == ".button { color: red; }");

    // Verify Class
    assert(ast.classes.size() == 1);
    assert(ast.classes[0].name == "TagBadge");
    assert(ast.classes[0].scope == "global");
    assert(ast.classes[0].properties.size() == 2);
    assert(ast.classes[0].properties[0].key == "color");
    assert(ast.classes[0].properties[0].value == "blue");
    assert(ast.classes[0].properties[1].isBinding == true);
    assert(ast.classes[0].properties[1].key == "title");
    assert(ast.classes[0].properties[1].bindingTarget == "label");
    assert(ast.classes[0].properties[1].bindingOperation == "capitalize");

    // Verify Selector
    assert(ast.selectors.size() == 1);
    assert(ast.selectors[0].selector == "#nav");
    assert(ast.selectors[0].action == "hide");
    assert(ast.selectors[0].properties.size() == 1);
    assert(ast.selectors[0].properties[0].key == "display");
    assert(ast.selectors[0].properties[0].value == "none");

    // Verify Reconstruct
    assert(ast.reconstructs.size() == 1);
    assert(ast.reconstructs[0].selector == "#view");
    assert(ast.reconstructs[0].component == "UiSplitLayout");
    assert(ast.reconstructs[0].mediaQuery == "(max-width: 600px)");
    assert(ast.reconstructs[0].preservationSlots["slot1"] == ".header");
    assert(ast.reconstructs[0].preservationSlots["slot2"] == ".footer");
    assert(ast.reconstructs[0].children.size() == 1);
    assert(ast.reconstructs[0].children[0].name == "mainPanel");
    assert(ast.reconstructs[0].children[0].extendsClass == "BasePanel");
    assert(ast.reconstructs[0].children[0].selector == ".main-content");
    assert(ast.reconstructs[0].children[0].scope == "panel");
    assert(ast.reconstructs[0].children[0].properties.size() == 1);
    assert(ast.reconstructs[0].children[0].properties[0].key == "padding");
    assert(ast.reconstructs[0].children[0].properties[0].value == "10px");

    std::cout << "All parser tests passed!" << std::endl;
}

void test_parser_error() {
    std::string source = R"(
        theme "Obsidian" {
            variables {
                "--bg-color" "black"; // Missing colon
            }
        }
    )";
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    try {
        parser.parse();
        assert(false && "Expected parser error");
    } catch (const std::runtime_error& e) {
        std::cout << "Caught expected error: " << e.what() << std::endl;
    }
}

void test_parser_shadow_dom() {
    // Helper unit tests
    bool isShadow = false;
    std::string host, inner;

    Parser::parseShadowSelectorInfo("shadow:custom-element->.btn-inner", isShadow, host, inner);
    assert(isShadow == true);
    assert(host == "custom-element");
    assert(inner == ".btn-inner");

    Parser::parseShadowSelectorInfo("shadow: custom-host .sub-item", isShadow, host, inner);
    assert(isShadow == true);
    assert(host == "custom-host");
    assert(inner == ".sub-item");

    Parser::parseShadowSelectorInfo("shadow: standalone-host", isShadow, host, inner);
    assert(isShadow == true);
    assert(host == "standalone-host");
    assert(inner == "");

    Parser::parseShadowSelectorInfo(".regular #selector", isShadow, host, inner);
    assert(isShadow == false);
    assert(host == "");
    assert(inner == "");

    // AST integration test
    std::string source = R"raw(
        selector "shadow: custom-card->.header" {
            action: "highlight";
            color: "gold";
        }

        reconstruct "shadow: my-host->#root-slot" -> MyCustomView {
            child shadowChild extends BaseChild {
                selector: "shadow: nested-host .content";
                scope: "shadow-scope";
            }
        }
    )raw";

    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();

    assert(ast.selectors.size() == 1);
    assert(ast.selectors[0].selector == "shadow: custom-card->.header");
    assert(ast.selectors[0].isShadow == true);
    assert(ast.selectors[0].shadowHost == "custom-card");
    assert(ast.selectors[0].innerSelector == ".header");

    assert(ast.reconstructs.size() == 1);
    assert(ast.reconstructs[0].selector == "shadow: my-host->#root-slot");
    assert(ast.reconstructs[0].isShadow == true);
    assert(ast.reconstructs[0].shadowHost == "my-host");
    assert(ast.reconstructs[0].innerSelector == "#root-slot");
    assert(ast.reconstructs[0].children.size() == 1);
    assert(ast.reconstructs[0].children[0].name == "shadowChild");
    assert(ast.reconstructs[0].children[0].selector == "shadow: nested-host .content");
    assert(ast.reconstructs[0].children[0].isShadow == true);
    assert(ast.reconstructs[0].children[0].shadowHost == "nested-host");
    assert(ast.reconstructs[0].children[0].innerSelector == ".content");

    std::cout << "All shadow DOM parser tests passed!" << std::endl;
}

int main() {
    test_parser();
    test_parser_error();
    test_parser_shadow_dom();
    return 0;
}
