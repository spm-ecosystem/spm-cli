#include "resolver.hpp"
#include <iostream>
#include <cassert>

using namespace veneer;

void testSingleInheritance() {
    std::string code = R"(
        class Base {
            color: "red";
            size: "large";
        }
        class Child extends Base {
            color: "blue";
            margin: "10px";
        }
    )";
    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    assert(ast.classes.size() == 2);
    auto& child = ast.classes[1];
    assert(child.name == "Child");
    
    bool hasColor = false;
    bool hasSize = false;
    bool hasMargin = false;
    for (const auto& p : child.properties) {
        if (p.key == "color") {
            assert(p.value == "blue");
            hasColor = true;
        } else if (p.key == "size") {
            assert(p.value == "large");
            hasSize = true;
        } else if (p.key == "margin") {
            assert(p.value == "10px");
            hasMargin = true;
        }
    }
    assert(hasColor && hasSize && hasMargin);
    std::cout << "testSingleInheritance passed." << std::endl;
}

void testMultiLevelInheritance() {
    std::string code = R"(
        class A {
            propA: "A";
            shared: "A";
        }
        class B extends A {
            propB: "B";
            shared: "B";
        }
        class C extends B {
            propC: "C";
        }
    )";
    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    auto& clsC = ast.classes[2];
    bool hasShared = false;
    bool hasPropA = false;
    bool hasPropB = false;
    for (const auto& p : clsC.properties) {
        if (p.key == "shared") {
            assert(p.value == "B");
            hasShared = true;
        }
        if (p.key == "propA") hasPropA = true;
        if (p.key == "propB") hasPropB = true;
    }
    assert(hasShared && hasPropA && hasPropB);
    std::cout << "testMultiLevelInheritance passed." << std::endl;
}

void testChildBlockInheritance() {
    std::string code = R"(
        class BasePanel {
            bg: "white";
        }
        reconstruct panel -> div {
            child mainPanel extends BasePanel {
                bg: "black";
                fg: "white";
            }
        }
    )";
    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    auto& child = ast.reconstructs[0].children[0];
    bool hasBg = false;
    bool hasFg = false;
    for (const auto& p : child.properties) {
        if (p.key == "bg") {
            assert(p.value == "black");
            hasBg = true;
        }
        if (p.key == "fg") {
            assert(p.value == "white");
            hasFg = true;
        }
    }
    assert(hasBg && hasFg);
    std::cout << "testChildBlockInheritance passed." << std::endl;
}

void testCircularDependency() {
    std::string code = R"(
        class A extends B {
            propA: "A";
        }
        class B extends A {
            propB: "B";
        }
    )";
    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    
    bool caught = false;
    try {
        resolver.resolve();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        assert(msg.find("[Resolver Error] Circular inheritance detected") != std::string::npos);
        assert(msg.find("A -> B -> A") != std::string::npos || msg.find("B -> A -> B") != std::string::npos);
    }
    assert(caught);
    std::cout << "testCircularDependency passed." << std::endl;
}

void testUnknownPropWarning() {
    std::string code = R"(
        reconstruct "#header" -> UiNavHeader {
            siteNam: "My Brand";
            sticky: "true";
        }
        selector ".card" -> UiImageCard {
            titl: "Card Title";
            completelyUnknownProperty: "123";
        }
        selector ".custom" -> CustomComponent {
            customField: "hello";
        }
    )";
    Lexer lexer(code);
    Parser parser(lexer.tokenize());
    ASTNode ast = parser.parse();
    Resolver resolver(ast);
    resolver.resolve();

    const auto& warnings = resolver.getWarnings();
    assert(warnings.size() == 3);

    // Warning 1: UiNavHeader siteNam -> Did you mean 'siteName'?
    assert(warnings[0] == "[Compiler Warning] Property 'siteNam' is not recognized on component 'UiNavHeader'. Did you mean 'siteName'?");

    // Warning 2: UiImageCard titl -> Did you mean 'title'?
    assert(warnings[1] == "[Compiler Warning] Property 'titl' is not recognized on component 'UiImageCard'. Did you mean 'title'?");

    // Warning 3: UiImageCard completelyUnknownProperty -> No did-you-mean hint
    assert(warnings[2] == "[Compiler Warning] Property 'completelyUnknownProperty' is not recognized on component 'UiImageCard'.");

    std::cout << "testUnknownPropWarning passed." << std::endl;
}

int main() {
    testSingleInheritance();
    testMultiLevelInheritance();
    testChildBlockInheritance();
    testCircularDependency();
    testUnknownPropWarning();
    std::cout << "All resolver tests passed." << std::endl;
    return 0;
}

