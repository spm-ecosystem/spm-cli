#ifndef VENEER_PARSER_HPP
#define VENEER_PARSER_HPP

#include "lexer.hpp"
#include "selector_utils.hpp"
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>

namespace veneer {

struct ASTProperty {
    std::string key;
    std::string value;
    bool isBinding = false;
    std::string bindingTarget;
    std::string bindingOperation;
};

struct ASTChild {
    std::string name;
    std::string selector;
    std::string extendsClass;
    std::string scope;
    bool isShadow = false;
    std::string shadowHost;
    std::string innerSelector;
    std::vector<ASTProperty> properties;
    std::vector<ASTChild> children;
};

struct ThemeNode {
    std::string label;
    std::string targetUrl;
    std::map<std::string, std::string> variables;
    std::vector<std::string> customStyles;
};

struct ClassNode {
    std::string name;
    std::string extendsClass;
    std::string scope = "container";
    std::vector<ASTProperty> properties;
};

struct SelectorNode {
    std::string selector;
    std::string component;
    std::string action;
    bool isShadow = false;
    std::string shadowHost;
    std::string innerSelector;
    std::vector<ASTProperty> properties;
};

struct ReconstructNode {
    std::string selector;
    std::string component;
    bool isShadow = false;
    std::string shadowHost;
    std::string innerSelector;
    std::vector<ASTProperty> properties;
    std::map<std::string, std::string> preservationSlots;
    std::string mediaQuery;
    std::vector<ASTChild> children;
};

struct ASTNode {
    std::string targetUrl;
    std::vector<ThemeNode> themes;
    std::vector<ClassNode> classes;
    std::vector<SelectorNode> selectors;
    std::vector<ReconstructNode> reconstructs;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens), pos_(0) {}

    static void parseShadowSelectorInfo(const std::string& rawSel, bool& outIsShadow, std::string& outHost, std::string& outInner) {
        auto info = parseShadowSelector(rawSel);
        outIsShadow = info.isShadow;
        outHost = info.shadowHost;
        outInner = info.innerSelector;
    }

    ASTNode parse() {
        ASTNode root;
        while (!isAtEnd()) {
            if (match(TokenType::KeywordTheme)) {
                root.themes.push_back(parseTheme());
            } else if (match(TokenType::KeywordClass)) {
                root.classes.push_back(parseClass());
            } else if (match(TokenType::KeywordSelector)) {
                root.selectors.push_back(parseSelector());
            } else if (match(TokenType::KeywordReconstruct)) {
                root.reconstructs.push_back(parseReconstruct());
            } else if (matchKeywordOrIdentifier(TokenType::KeywordTargetUrl, "targetUrl")) {
                match(TokenType::Colon);
                root.targetUrl = parseStringOrIdentifier();
                match(TokenType::Semicolon);
            } else {
                throw error(peek(), "Unexpected token in global scope");
            }
        }
        if (root.targetUrl.empty()) {
            for (const auto& theme : root.themes) {
                if (!theme.targetUrl.empty()) {
                    root.targetUrl = theme.targetUrl;
                    break;
                }
            }
        }
        return root;
    }

private:
    std::vector<Token> tokens_;
    size_t pos_;

    bool isAtEnd() const {
        return peek().type == TokenType::EOFToken;
    }

    const Token& peek() const {
        return tokens_[pos_];
    }

    const Token& previous() const {
        return tokens_[pos_ - 1];
    }

    const Token& advance() {
        if (!isAtEnd()) pos_++;
        return previous();
    }

    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    const Token& consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        throw error(peek(), message);
    }

    bool matchKeywordOrIdentifier(TokenType keywordType, std::string_view identifierValue) {
        if (match(keywordType)) {
            return true;
        }
        if (peek().type == TokenType::Identifier && peek().value == identifierValue) {
            advance();
            return true;
        }
        return false;
    }

    std::runtime_error error(const Token& token, const std::string& message) {
        return std::runtime_error("[Parser Error] Line " + std::to_string(token.line) + ": " + message);
    }

    std::string parseStringOrIdentifier() {
        if (match(TokenType::StringLiteral) || match(TokenType::Identifier) ||
            match(TokenType::KeywordTheme) || match(TokenType::KeywordSelector) ||
            match(TokenType::KeywordReconstruct) || match(TokenType::KeywordClass) ||
            match(TokenType::KeywordExtends) || match(TokenType::KeywordBind) ||
            match(TokenType::KeywordPreserve) || match(TokenType::KeywordChild) ||
            match(TokenType::KeywordVariables) || match(TokenType::KeywordStyles) ||
            match(TokenType::KeywordScope) || match(TokenType::KeywordShadow) ||
            match(TokenType::KeywordTargetUrl)) {
            std::string val(previous().value);
            if (previous().type == TokenType::StringLiteral) {
                if ((val.rfind("R\"", 0) == 0 || val.rfind("r\"", 0) == 0) && val.back() == '"') {
                    size_t parenOpen = val.find('(');
                    size_t parenClose = val.rfind(')');
                    if (parenOpen != std::string::npos && parenClose != std::string::npos && parenClose > parenOpen) {
                        val = val.substr(parenOpen + 1, parenClose - parenOpen - 1);
                    }
                } else if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
            }
            return val;
        }
        throw error(peek(), "Expected string literal or identifier");
    }

    ThemeNode parseTheme() {
        ThemeNode theme;
        theme.label = parseStringOrIdentifier();
        consume(TokenType::BraceOpen, "Expected '{' after theme name");

        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (match(TokenType::KeywordVariables)) {
                consume(TokenType::BraceOpen, "Expected '{' after variables");
                while (!check(TokenType::BraceClose) && !isAtEnd()) {
                    std::string key = parseStringOrIdentifier();
                    consume(TokenType::Colon, "Expected ':' after variable name");
                    std::string val = parseStringOrIdentifier();
                    consume(TokenType::Semicolon, "Expected ';' after variable value");
                    theme.variables[key] = val;
                }
                consume(TokenType::BraceClose, "Expected '}' after variables block");
            } else if (matchKeywordOrIdentifier(TokenType::KeywordStyles, "customStyles")) {
                consume(TokenType::BraceOpen, "Expected '{' after customStyles");
                while (!check(TokenType::BraceClose) && !isAtEnd()) {
                    theme.customStyles.push_back(parseStringOrIdentifier());
                }
                consume(TokenType::BraceClose, "Expected '}' after customStyles block");
            } else {
                std::string key = parseStringOrIdentifier();
                consume(TokenType::Colon, "Expected ':' after variable name");
                std::string val = parseStringOrIdentifier();
                consume(TokenType::Semicolon, "Expected ';' after variable value");
                if (key == "targetUrl") {
                    theme.targetUrl = val;
                }
                theme.variables[key] = val;
            }
        }
        consume(TokenType::BraceClose, "Expected '}' after theme block");
        return theme;
    }

    ClassNode parseClass() {
        ClassNode cls;
        cls.name = parseStringOrIdentifier();
        if (matchKeywordOrIdentifier(TokenType::KeywordExtends, "extends")) {
            cls.extendsClass = parseStringOrIdentifier();
        }
        consume(TokenType::BraceOpen, "Expected '{' after class name");

        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (matchKeywordOrIdentifier(TokenType::KeywordScope, "scope")) {
                consume(TokenType::Colon, "Expected ':' after scope");
                cls.scope = parseStringOrIdentifier();
                consume(TokenType::Semicolon, "Expected ';' after scope value");
            } else {
                cls.properties.push_back(parseProperty());
            }
        }
        consume(TokenType::BraceClose, "Expected '}' after class block");
        return cls;
    }

    SelectorNode parseSelector() {
        SelectorNode sel;
        sel.selector = parseStringOrIdentifier();
        parseShadowSelectorInfo(sel.selector, sel.isShadow, sel.shadowHost, sel.innerSelector);
        if (match(TokenType::Arrow)) {
            sel.component = parseStringOrIdentifier();
        }
        consume(TokenType::BraceOpen, "Expected '{' after selector");

        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (peek().type == TokenType::Identifier && peek().value == "action") {
                advance();
                consume(TokenType::Colon, "Expected ':' after action");
                sel.action = parseStringOrIdentifier();
                consume(TokenType::Semicolon, "Expected ';' after action value");
            } else {
                sel.properties.push_back(parseProperty());
            }
        }
        consume(TokenType::BraceClose, "Expected '}' after selector block");
        return sel;
    }

    ASTChild parseChild() {
        ASTChild child;
        child.name = parseStringOrIdentifier();
        if (matchKeywordOrIdentifier(TokenType::KeywordExtends, "extends")) {
            child.extendsClass = parseStringOrIdentifier();
        }
        consume(TokenType::BraceOpen, "Expected '{' after child definition");
        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (matchKeywordOrIdentifier(TokenType::KeywordSelector, "selector")) {
                consume(TokenType::Colon, "Expected ':' after selector");
                child.selector = parseStringOrIdentifier();
                parseShadowSelectorInfo(child.selector, child.isShadow, child.shadowHost, child.innerSelector);
                consume(TokenType::Semicolon, "Expected ';' after selector value");
            } else if (matchKeywordOrIdentifier(TokenType::KeywordScope, "scope")) {
                consume(TokenType::Colon, "Expected ':' after scope");
                child.scope = parseStringOrIdentifier();
                consume(TokenType::Semicolon, "Expected ';' after scope value");
            } else if (matchKeywordOrIdentifier(TokenType::KeywordChild, "child")) {
                child.children.push_back(parseChild());
            } else {
                child.properties.push_back(parseProperty());
            }
        }
        consume(TokenType::BraceClose, "Expected '}' after child block");
        return child;
    }

    ReconstructNode parseReconstruct() {
        ReconstructNode recon;
        recon.selector = parseStringOrIdentifier();
        parseShadowSelectorInfo(recon.selector, recon.isShadow, recon.shadowHost, recon.innerSelector);
        consume(TokenType::Arrow, "Expected '->' after selector in reconstruct");
        recon.component = parseStringOrIdentifier();
        consume(TokenType::BraceOpen, "Expected '{' after reconstruct definition");

        while (!check(TokenType::BraceClose) && !isAtEnd()) {
            if (matchKeywordOrIdentifier(TokenType::KeywordPreserve, "preserve")) {
                consume(TokenType::BraceOpen, "Expected '{' after preserve");
                while (!check(TokenType::BraceClose) && !isAtEnd()) {
                    std::string slot = parseStringOrIdentifier();
                    consume(TokenType::Colon, "Expected ':' after preserve slot");
                    std::string sel = parseStringOrIdentifier();
                    consume(TokenType::Semicolon, "Expected ';' after preserve value");
                    recon.preservationSlots[slot] = sel;
                }
                consume(TokenType::BraceClose, "Expected '}' after preserve block");
            } else if (peek().type == TokenType::Identifier && (peek().value == "media" || peek().value == "mediaQuery")) {
                advance();
                consume(TokenType::Colon, "Expected ':' after media");
                recon.mediaQuery = parseStringOrIdentifier();
                consume(TokenType::Semicolon, "Expected ';' after media query");
            } else if (matchKeywordOrIdentifier(TokenType::KeywordChild, "child")) {
                recon.children.push_back(parseChild());
            } else {
                recon.properties.push_back(parseProperty());
            }
        }
        consume(TokenType::BraceClose, "Expected '}' after reconstruct block");
        return recon;
    }

    std::string parseJsonValue() {
        std::string jsonStr;
        int braceDepth = 0;
        int bracketDepth = 0;
        
        while (!isAtEnd()) {
            const Token& tok = peek();
            if (tok.value == "[") {
                bracketDepth++;
            } else if (tok.value == "]") {
                bracketDepth--;
            } else if (tok.type == TokenType::BraceOpen) {
                braceDepth++;
            } else if (tok.type == TokenType::BraceClose) {
                braceDepth--;
            }
            
            jsonStr += tok.value;
            advance();
            
            if (braceDepth == 0 && bracketDepth == 0) {
                break;
            }
        }
        return jsonStr;
    }

    ASTProperty parseProperty() {
        ASTProperty prop;
        if (matchKeywordOrIdentifier(TokenType::KeywordBind, "bind")) {
            prop.isBinding = true;
            prop.key = parseStringOrIdentifier();
            consume(TokenType::Colon, "Expected ':' after bind key");
            
            std::string bindExpr = parseStringOrIdentifier();
            // Parse bindExpr "target | op:param" or "target"
            size_t pipePos = bindExpr.find('|');
            if (pipePos != std::string::npos) {
                prop.bindingTarget = bindExpr.substr(0, pipePos);
                // trim spaces
                while(!prop.bindingTarget.empty() && std::isspace(prop.bindingTarget.back())) prop.bindingTarget.pop_back();
                size_t opStart = pipePos + 1;
                while(opStart < bindExpr.size() && std::isspace(bindExpr[opStart])) opStart++;
                prop.bindingOperation = bindExpr.substr(opStart);
            } else {
                prop.bindingTarget = bindExpr;
            }
            consume(TokenType::Semicolon, "Expected ';' after bind expression");
        } else {
            prop.key = parseStringOrIdentifier();
            consume(TokenType::Colon, "Expected ':' after property key");
            if (peek().value == "[" || peek().type == TokenType::BraceOpen) {
                prop.value = parseJsonValue();
            } else {
                prop.value = parseStringOrIdentifier();
            }
            consume(TokenType::Semicolon, "Expected ';' after property value");
        }
        return prop;
    }
};

} // namespace veneer

#endif // VENEER_PARSER_HPP
