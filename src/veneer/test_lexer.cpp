#include "lexer.hpp"
#include <iostream>
#include <iomanip>
#include <string_view>
#include <cassert>

int main() {
    constexpr std::string_view sampleVnr = R"(// Veneer Spec (.vnr) Test Case
theme "Modern Dark" {
    scope "global";

    variables {
        --bg-color: "#1e1e2e";
        --text-color: "#cdd6f4";
        --accent: "#cba6f7";
    }

    class "button-primary" extends "base-button" {
        selector ".btn-primary | #submit-btn";
        styles {
            background-color: var(--bg-color);
            color: var(--text-color);
        }
    }

    reconstruct "header-nav" -> "navbar" {
        bind "logo" -> ".site-logo";
        preserve "custom-nav-items";
        child "search-bar";
    }

    /* Multi-line comment test
       Multi-line string test follows */
    description "Line 1
Line 2";
}
)";

    std::cout << "=== Veneer Spec Lexer Test ===" << std::endl;
    veneer::Lexer lexer(sampleVnr);
    auto tokens = lexer.tokenize();

    std::cout << std::left 
              << std::setw(6)  << "Line" 
              << std::setw(20) << "Token Type" 
              << "Value" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    size_t keywordCount = 0;
    bool foundArrow = false;
    bool foundPipe = false;
    bool foundMultiLineString = false;

    for (const auto& token : tokens) {
        std::cout << std::left 
                  << std::setw(6)  << token.line 
                  << std::setw(20) << veneer::tokenTypeToString(token.type) 
                  << token.value << std::endl;

        if (token.type == veneer::TokenType::KeywordTheme ||
            token.type == veneer::TokenType::KeywordScope ||
            token.type == veneer::TokenType::KeywordVariables ||
            token.type == veneer::TokenType::KeywordClass ||
            token.type == veneer::TokenType::KeywordExtends ||
            token.type == veneer::TokenType::KeywordSelector ||
            token.type == veneer::TokenType::KeywordStyles ||
            token.type == veneer::TokenType::KeywordReconstruct ||
            token.type == veneer::TokenType::KeywordBind ||
            token.type == veneer::TokenType::KeywordPreserve ||
            token.type == veneer::TokenType::KeywordChild) {
            keywordCount++;
        }

        if (token.type == veneer::TokenType::Arrow) {
            foundArrow = true;
        }

        if (token.type == veneer::TokenType::Pipe) {
            foundPipe = true;
        }

        if (token.type == veneer::TokenType::StringLiteral && token.value.find('\n') != std::string_view::npos) {
            foundMultiLineString = true;
        }
    }

    std::cout << std::string(50, '-') << std::endl;
    std::cout << "Total tokens scanned: " << tokens.size() << std::endl;
    std::cout << "Keywords recognized: "  << keywordCount << std::endl;

    assert(!tokens.empty());
    assert(tokens.back().type == veneer::TokenType::EOFToken);
    assert(keywordCount >= 11);
    assert(foundArrow);
    assert(foundPipe);
    assert(foundMultiLineString);

    std::cout << "ALL LEXER TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
