#ifndef VENEER_HTML_VALIDATOR_HPP
#define VENEER_HTML_VALIDATOR_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <random>
#include <nlohmann/json.hpp>
#include "extractor_pipeline.hpp"

namespace veneer {

inline std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return lower;
}

inline std::string codePointToUtf8(uint32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

inline std::string decodeHtmlEntities(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    size_t i = 0;
    size_t len = input.size();

    while (i < len) {
        if (input[i] == '&') {
            size_t semi = input.find(';', i);
            if (semi != std::string::npos && semi - i <= 10) {
                std::string entity = input.substr(i + 1, semi - i - 1);
                if (entity == "amp") { result += '&'; i = semi + 1; continue; }
                else if (entity == "lt") { result += '<'; i = semi + 1; continue; }
                else if (entity == "gt") { result += '>'; i = semi + 1; continue; }
                else if (entity == "quot") { result += '"'; i = semi + 1; continue; }
                else if (entity == "apos" || entity == "#39") { result += '\''; i = semi + 1; continue; }
                else if (entity == "nbsp") { result += ' '; i = semi + 1; continue; }
                else if (!entity.empty() && entity[0] == '#') {
                    uint32_t cp = 0;
                    bool valid = false;
                    try {
                        if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(2), nullptr, 16));
                            valid = true;
                        } else if (entity.size() > 1) {
                            cp = static_cast<uint32_t>(std::stoul(entity.substr(1), nullptr, 10));
                            valid = true;
                        }
                    } catch (...) {
                        valid = false;
                    }
                    if (valid && cp <= 0x10FFFF) {
                        result += codePointToUtf8(cp);
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        result += input[i];
        i++;
    }
    return result;
}

// Forward declarations
struct HTMLElement;

enum class Combinator {
    Descendant,      // ' '
    Child,           // '>'
    AdjacentSibling, // '+'
    GeneralSibling   // '~'
};

enum class AttrMatch {
    Exists,     // [attr]
    Exact,      // [attr="val"]
    Contains,   // [attr*="val"]
    StartsWith, // [attr^="val"]
    EndsWith,   // [attr$="val"]
    Includes,   // [attr~="val"]
    DashMatch   // [attr|="val"]
};

struct AttrSelector {
    std::string name; // lowercase
    AttrMatch matchType = AttrMatch::Exists;
    std::string value;
};

struct SimpleSelector {
    std::string tagName; // lowercase or "*" or empty
    std::string id;
    std::vector<std::string> classes;
    std::vector<AttrSelector> attributes;
    std::vector<std::string> pseudoClasses;
    bool isSelf = false;
};

struct SelectorStep {
    Combinator combinator = Combinator::Descendant;
    SimpleSelector selector;
};

struct SelectorChain {
    std::vector<SelectorStep> steps;
};

struct HTMLNode {
    bool isElement = false;
    std::string text;
    std::shared_ptr<HTMLElement> element;
};

struct HTMLElement : public std::enable_shared_from_this<HTMLElement> {
    std::string tagName; // lowercase
    std::map<std::string, std::string> attributes; // lowercase keys
    std::weak_ptr<HTMLElement> parent;
    std::vector<std::shared_ptr<HTMLElement>> children;
    std::vector<HTMLNode> childNodes;
    std::string rawInnerHTML;

    std::optional<std::string> getAttribute(const std::string& name) const {
        std::string lower = toLower(name);
        auto it = attributes.find(lower);
        if (it != attributes.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void setAttribute(const std::string& name, const std::string& value) {
        attributes[toLower(name)] = value;
    }

    bool hasAttribute(const std::string& name) const {
        return attributes.find(toLower(name)) != attributes.end();
    }

    bool hasClass(const std::string& cls) const {
        auto valOpt = getAttribute("class");
        if (!valOpt.has_value()) return false;
        const std::string& classStr = valOpt.value();
        std::istringstream iss(classStr);
        std::string token;
        while (iss >> token) {
            if (token == cls) return true;
        }
        return false;
    }

    std::string textContent() const {
        std::string result;
        for (const auto& node : childNodes) {
            if (!node.isElement) {
                result += node.text;
            } else if (node.element) {
                result += node.element->textContent();
            }
        }
        return result;
    }

    std::string innerHTML() const {
        return rawInnerHTML;
    }

    std::shared_ptr<HTMLElement> previousElementSibling() const {
        auto p = parent.lock();
        if (!p) return nullptr;
        for (size_t i = 0; i < p->children.size(); ++i) {
            if (p->children[i].get() == this) {
                if (i > 0) return p->children[i - 1];
                return nullptr;
            }
        }
        return nullptr;
    }

    std::shared_ptr<HTMLElement> nextElementSibling() const {
        auto p = parent.lock();
        if (!p) return nullptr;
        for (size_t i = 0; i < p->children.size(); ++i) {
            if (p->children[i].get() == this) {
                if (i + 1 < p->children.size()) return p->children[i + 1];
                return nullptr;
            }
        }
        return nullptr;
    }

    bool isFirstElementChild() const {
        auto p = parent.lock();
        if (!p || p->children.empty()) return false;
        return p->children.front().get() == this;
    }

    bool isLastElementChild() const {
        auto p = parent.lock();
        if (!p || p->children.empty()) return false;
        return p->children.back().get() == this;
    }

    int elementChildIndex() const {
        auto p = parent.lock();
        if (!p) return -1;
        for (size_t i = 0; i < p->children.size(); ++i) {
            if (p->children[i].get() == this) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<std::shared_ptr<HTMLElement>> querySelectorAll(const std::string& selector);
    std::shared_ptr<HTMLElement> querySelector(const std::string& selector);
    bool matches(const std::string& selector);
};

class CssSelectorEngine {
public:
    static std::vector<SelectorChain> parseSelectorList(const std::string& selectorStr) {
        std::vector<SelectorChain> chains;
        std::vector<std::string> parts;

        // Split on comma or pipe (outside quotes and brackets)
        std::string current;
        bool inQuotes = false;
        char quoteChar = 0;
        int bracketDepth = 0;

        for (size_t i = 0; i < selectorStr.size(); ++i) {
            char c = selectorStr[i];
            if (inQuotes) {
                current += c;
                if (c == quoteChar) inQuotes = false;
            } else if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
                current += c;
            } else if (c == '[') {
                bracketDepth++;
                current += c;
            } else if (c == ']') {
                if (bracketDepth > 0) bracketDepth--;
                current += c;
            } else if ((c == ',' || c == '|') && bracketDepth == 0) {
                std::string trimmed = trim(current);
                if (!trimmed.empty()) parts.push_back(trimmed);
                current.clear();
            } else {
                current += c;
            }
        }
        std::string trimmed = trim(current);
        if (!trimmed.empty()) parts.push_back(trimmed);

        for (const auto& part : parts) {
            auto chain = parseSingleChain(part);
            if (!chain.steps.empty()) {
                chains.push_back(chain);
            }
        }
        return chains;
    }

    static SelectorChain parseSingleChain(const std::string& chainStr) {
        SelectorChain chain;
        size_t pos = 0;
        size_t len = chainStr.size();

        auto skipWhitespace = [&]() {
            while (pos < len && std::isspace(static_cast<unsigned char>(chainStr[pos]))) {
                pos++;
            }
        };

        Combinator nextComb = Combinator::Descendant;
        bool hasPendingComb = false;

        skipWhitespace();

        while (pos < len) {
            // Check combinators
            if (chainStr[pos] == '>') {
                nextComb = Combinator::Child;
                hasPendingComb = true;
                pos++;
                skipWhitespace();
                continue;
            } else if (chainStr[pos] == '+') {
                nextComb = Combinator::AdjacentSibling;
                hasPendingComb = true;
                pos++;
                skipWhitespace();
                continue;
            } else if (chainStr[pos] == '~') {
                nextComb = Combinator::GeneralSibling;
                hasPendingComb = true;
                pos++;
                skipWhitespace();
                continue;
            }

            SimpleSelector simple;
            // Check self
            if (chainStr.compare(pos, 4, "self") == 0 &&
                (pos + 4 == len || std::isspace(static_cast<unsigned char>(chainStr[pos + 4])) ||
                 chainStr[pos + 4] == '>' || chainStr[pos + 4] == '+' || chainStr[pos + 4] == '~' ||
                 chainStr[pos + 4] == '.' || chainStr[pos + 4] == '#' || chainStr[pos + 4] == '[')) {
                simple.isSelf = true;
                pos += 4;
            } else if (chainStr[pos] == '*') {
                simple.tagName = "*";
                pos++;
            } else if (std::isalpha(static_cast<unsigned char>(chainStr[pos]))) {
                size_t start = pos;
                while (pos < len && (std::isalnum(static_cast<unsigned char>(chainStr[pos])) || chainStr[pos] == '-' || chainStr[pos] == '_')) {
                    pos++;
                }
                simple.tagName = toLower(chainStr.substr(start, pos - start));
            }

            // Parse qualifiers: #id, .class, [attr], :pseudo
            while (pos < len) {
                if (chainStr[pos] == '#') {
                    pos++;
                    size_t start = pos;
                    while (pos < len && (std::isalnum(static_cast<unsigned char>(chainStr[pos])) || chainStr[pos] == '-' || chainStr[pos] == '_')) {
                        pos++;
                    }
                    simple.id = chainStr.substr(start, pos - start);
                } else if (chainStr[pos] == '.') {
                    pos++;
                    size_t start = pos;
                    while (pos < len && (std::isalnum(static_cast<unsigned char>(chainStr[pos])) || chainStr[pos] == '-' || chainStr[pos] == '_')) {
                        pos++;
                    }
                    simple.classes.push_back(chainStr.substr(start, pos - start));
                } else if (chainStr[pos] == '[') {
                    pos++;
                    skipWhitespace();
                    size_t start = pos;
                    while (pos < len && chainStr[pos] != '=' && chainStr[pos] != ']' &&
                           chainStr[pos] != '*' && chainStr[pos] != '^' && chainStr[pos] != '$' &&
                           chainStr[pos] != '~' && chainStr[pos] != '|' && !std::isspace(static_cast<unsigned char>(chainStr[pos]))) {
                        pos++;
                    }
                    std::string attrName = toLower(chainStr.substr(start, pos - start));
                    skipWhitespace();

                    AttrSelector attr;
                    attr.name = attrName;

                    if (pos < len && chainStr[pos] == ']') {
                        attr.matchType = AttrMatch::Exists;
                        pos++;
                    } else {
                        if (pos + 1 < len && chainStr[pos] == '*' && chainStr[pos + 1] == '=') {
                            attr.matchType = AttrMatch::Contains;
                            pos += 2;
                        } else if (pos + 1 < len && chainStr[pos] == '^' && chainStr[pos + 1] == '=') {
                            attr.matchType = AttrMatch::StartsWith;
                            pos += 2;
                        } else if (pos + 1 < len && chainStr[pos] == '$' && chainStr[pos + 1] == '=') {
                            attr.matchType = AttrMatch::EndsWith;
                            pos += 2;
                        } else if (pos + 1 < len && chainStr[pos] == '~' && chainStr[pos + 1] == '=') {
                            attr.matchType = AttrMatch::Includes;
                            pos += 2;
                        } else if (pos + 1 < len && chainStr[pos] == '|' && chainStr[pos + 1] == '=') {
                            attr.matchType = AttrMatch::DashMatch;
                            pos += 2;
                        } else if (pos < len && chainStr[pos] == '=') {
                            attr.matchType = AttrMatch::Exact;
                            pos++;
                        }
                        skipWhitespace();

                        std::string attrVal;
                        if (pos < len && (chainStr[pos] == '"' || chainStr[pos] == '\'')) {
                            char q = chainStr[pos++];
                            size_t vStart = pos;
                            while (pos < len && chainStr[pos] != q) {
                                pos++;
                            }
                            attrVal = chainStr.substr(vStart, pos - vStart);
                            if (pos < len && chainStr[pos] == q) pos++;
                        } else {
                            size_t vStart = pos;
                            while (pos < len && chainStr[pos] != ']' && !std::isspace(static_cast<unsigned char>(chainStr[pos]))) {
                                pos++;
                            }
                            attrVal = chainStr.substr(vStart, pos - vStart);
                        }
                        attr.value = attrVal;
                        skipWhitespace();
                        if (pos < len && chainStr[pos] == ']') pos++;
                    }
                    simple.attributes.push_back(attr);
                } else if (chainStr[pos] == ':') {
                    pos++;
                    size_t start = pos;
                    while (pos < len && (std::isalnum(static_cast<unsigned char>(chainStr[pos])) || chainStr[pos] == '-' || chainStr[pos] == '(' || chainStr[pos] == ')')) {
                        pos++;
                    }
                    simple.pseudoClasses.push_back(chainStr.substr(start, pos - start));
                } else {
                    break;
                }
            }

            SelectorStep step;
            step.combinator = hasPendingComb ? nextComb : (chain.steps.empty() ? Combinator::Descendant : Combinator::Descendant);
            step.selector = simple;
            chain.steps.push_back(step);
            hasPendingComb = false;

            // Check if there was whitespace after compound
            bool spaceAfter = false;
            while (pos < len && std::isspace(static_cast<unsigned char>(chainStr[pos]))) {
                spaceAfter = true;
                pos++;
            }

            if (spaceAfter && pos < len && chainStr[pos] != '>' && chainStr[pos] != '+' && chainStr[pos] != '~') {
                nextComb = Combinator::Descendant;
                hasPendingComb = true;
            }
        }

        return chain;
    }

    static bool matchSimple(const std::shared_ptr<HTMLElement>& elem, const SimpleSelector& simple, const std::shared_ptr<HTMLElement>& contextRoot) {
        if (!elem) return false;

        if (simple.isSelf) {
            if (contextRoot && elem == contextRoot) return true;
            if (!contextRoot) return true;
            return false;
        }

        if (!simple.tagName.empty() && simple.tagName != "*") {
            if (toLower(elem->tagName) != simple.tagName) return false;
        }

        if (!simple.id.empty()) {
            if (elem->getAttribute("id").value_or("") != simple.id) return false;
        }

        for (const auto& cls : simple.classes) {
            if (!elem->hasClass(cls)) return false;
        }

        for (const auto& attr : simple.attributes) {
            auto valOpt = elem->getAttribute(attr.name);
            if (!valOpt.has_value()) return false;
            const std::string& val = valOpt.value();
            switch (attr.matchType) {
                case AttrMatch::Exists:
                    break;
                case AttrMatch::Exact:
                    if (val != attr.value) return false;
                    break;
                case AttrMatch::Contains:
                    if (val.find(attr.value) == std::string::npos) return false;
                    break;
                case AttrMatch::StartsWith:
                    if (val.rfind(attr.value, 0) != 0) return false;
                    break;
                case AttrMatch::EndsWith:
                    if (val.size() < attr.value.size() ||
                        val.compare(val.size() - attr.value.size(), attr.value.size(), attr.value) != 0) return false;
                    break;
                case AttrMatch::Includes: {
                    std::istringstream iss(val);
                    std::string token;
                    bool found = false;
                    while (iss >> token) {
                        if (token == attr.value) { found = true; break; }
                    }
                    if (!found) return false;
                    break;
                }
                case AttrMatch::DashMatch:
                    if (val != attr.value && val.rfind(attr.value + "-", 0) != 0) return false;
                    break;
            }
        }

        for (const auto& pseudo : simple.pseudoClasses) {
            if (pseudo == "first-child") {
                if (!elem->isFirstElementChild()) return false;
            } else if (pseudo == "last-child") {
                if (!elem->isLastElementChild()) return false;
            } else if (pseudo.rfind("nth-child(", 0) == 0) {
                size_t closeP = pseudo.find(')');
                if (closeP != std::string::npos) {
                    std::string arg = pseudo.substr(10, closeP - 10);
                    int idx = elem->elementChildIndex() + 1;
                    if (arg == "odd") {
                        if (idx % 2 == 0) return false;
                    } else if (arg == "even") {
                        if (idx % 2 != 0) return false;
                    } else {
                        try {
                            int target = std::stoi(arg);
                            if (idx != target) return false;
                        } catch (...) {}
                    }
                }
            } else if (pseudo == "empty") {
                if (!elem->children.empty() || !trim(elem->textContent()).empty()) return false;
            }
        }

        return true;
    }

    static bool matchChain(const std::shared_ptr<HTMLElement>& elem,
                           const SelectorChain& chain,
                           size_t stepIndex,
                           const std::shared_ptr<HTMLElement>& contextRoot) {
        if (!elem) return false;
        if (!matchSimple(elem, chain.steps[stepIndex].selector, contextRoot)) return false;
        if (stepIndex == 0) return true;

        Combinator comb = chain.steps[stepIndex].combinator;
        if (comb == Combinator::Child) {
            auto parent = elem->parent.lock();
            if (!parent || (contextRoot && elem == contextRoot)) return false;
            return matchChain(parent, chain, stepIndex - 1, contextRoot);
        } else if (comb == Combinator::Descendant) {
            auto curr = elem->parent.lock();
            while (curr) {
                if (contextRoot && curr == contextRoot) {
                    if (matchChain(curr, chain, stepIndex - 1, contextRoot)) return true;
                    break;
                }
                if (matchChain(curr, chain, stepIndex - 1, contextRoot)) return true;
                curr = curr->parent.lock();
            }
            return false;
        } else if (comb == Combinator::AdjacentSibling) {
            auto prev = elem->previousElementSibling();
            if (!prev) return false;
            return matchChain(prev, chain, stepIndex - 1, contextRoot);
        } else if (comb == Combinator::GeneralSibling) {
            auto prev = elem->previousElementSibling();
            while (prev) {
                if (matchChain(prev, chain, stepIndex - 1, contextRoot)) return true;
                prev = prev->previousElementSibling();
            }
            return false;
        }

        return false;
    }

    static bool matchesSingle(const std::shared_ptr<HTMLElement>& elem,
                              const std::vector<SelectorChain>& chains,
                              const std::shared_ptr<HTMLElement>& contextRoot) {
        for (const auto& chain : chains) {
            if (chain.steps.empty()) continue;
            if (matchChain(elem, chain, chain.steps.size() - 1, contextRoot)) return true;
        }
        return false;
    }

    static void collect(const std::shared_ptr<HTMLElement>& node,
                        const std::vector<SelectorChain>& chains,
                        const std::shared_ptr<HTMLElement>& contextRoot,
                        std::vector<std::shared_ptr<HTMLElement>>& results,
                        bool includeCurrent = false) {
        if (includeCurrent) {
            if (matchesSingle(node, chains, contextRoot)) {
                results.push_back(node);
            }
        }
        for (const auto& child : node->children) {
            if (matchesSingle(child, chains, contextRoot)) {
                results.push_back(child);
            }
            collect(child, chains, contextRoot, results, false);
        }
    }
};

inline std::vector<std::shared_ptr<HTMLElement>> HTMLElement::querySelectorAll(const std::string& selector) {
    std::vector<std::shared_ptr<HTMLElement>> results;
    auto chains = CssSelectorEngine::parseSelectorList(selector);
    if (chains.empty()) return results;

    // If selector is "self"
    if (chains.size() == 1 && chains[0].steps.size() == 1 && chains[0].steps[0].selector.isSelf) {
        results.push_back(shared_from_this());
        return results;
    }

    CssSelectorEngine::collect(shared_from_this(), chains, shared_from_this(), results, false);
    return results;
}

inline std::shared_ptr<HTMLElement> HTMLElement::querySelector(const std::string& selector) {
    auto results = querySelectorAll(selector);
    return results.empty() ? nullptr : results.front();
}

inline bool HTMLElement::matches(const std::string& selector) {
    auto chains = CssSelectorEngine::parseSelectorList(selector);
    return CssSelectorEngine::matchesSingle(shared_from_this(), chains, nullptr);
}

class HtmlValidator {
public:
    static std::shared_ptr<HTMLElement> parseHtml(const std::string& html) {
        auto root = std::make_shared<HTMLElement>();
        root->tagName = "#document";

        std::vector<std::shared_ptr<HTMLElement>> stack;
        stack.push_back(root);

        struct StackInfo {
            std::shared_ptr<HTMLElement> elem;
            size_t openTagEndPos = 0;
        };
        std::vector<StackInfo> elementStack;
        elementStack.push_back({root, 0});

        static const std::vector<std::string> voidTags = {
            "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"
        };

        static const std::vector<std::string> rawTextTags = {
            "script", "style", "textarea", "title"
        };

        size_t pos = 0;
        size_t len = html.size();

        while (pos < len) {
            if (html[pos] == '<') {
                // Check comment
                if (pos + 3 < len && html.compare(pos, 4, "<!--") == 0) {
                    size_t endComment = html.find("-->", pos + 4);
                    if (endComment != std::string::npos) {
                        pos = endComment + 3;
                    } else {
                        pos = len;
                    }
                    continue;
                }

                // Check DOCTYPE
                if (pos + 8 < len && toLower(html.substr(pos, 9)) == "<!doctype") {
                    size_t endDoc = html.find('>', pos + 9);
                    if (endDoc != std::string::npos) {
                        pos = endDoc + 1;
                    } else {
                        pos = len;
                    }
                    continue;
                }

                // Check CDATA
                if (pos + 8 < len && html.compare(pos, 9, "<![CDATA[") == 0) {
                    size_t endCdata = html.find("]]>", pos + 9);
                    if (endCdata != std::string::npos) {
                        pos = endCdata + 3;
                    } else {
                        pos = len;
                    }
                    continue;
                }

                // Closing tag
                if (pos + 1 < len && html[pos + 1] == '/') {
                    size_t tagStart = pos;
                    pos += 2;
                    size_t nameStart = pos;
                    while (pos < len && (std::isalnum(static_cast<unsigned char>(html[pos])) || html[pos] == '-' || html[pos] == '_')) {
                        pos++;
                    }
                    std::string closeTagName = toLower(html.substr(nameStart, pos - nameStart));
                    while (pos < len && html[pos] != '>') pos++;
                    if (pos < len && html[pos] == '>') pos++;

                    // Find matching tag in elementStack
                    for (int s = static_cast<int>(elementStack.size()) - 1; s >= 1; --s) {
                        if (elementStack[s].elem->tagName == closeTagName) {
                            elementStack[s].elem->rawInnerHTML = html.substr(
                                elementStack[s].openTagEndPos,
                                tagStart - elementStack[s].openTagEndPos
                            );
                            elementStack.erase(elementStack.begin() + s, elementStack.end());
                            break;
                        }
                    }
                    continue;
                }

                // Opening tag
                size_t tagOpenStart = pos;
                pos++; // skip '<'
                size_t nameStart = pos;
                while (pos < len && (std::isalnum(static_cast<unsigned char>(html[pos])) || html[pos] == '-' || html[pos] == '_')) {
                    pos++;
                }
                std::string tagName = toLower(html.substr(nameStart, pos - nameStart));
                if (tagName.empty()) {
                    continue;
                }

                std::map<std::string, std::string> attrs;
                bool isSelfClosing = false;

                while (pos < len) {
                    while (pos < len && std::isspace(static_cast<unsigned char>(html[pos]))) pos++;
                    if (pos >= len) break;
                    if (html[pos] == '>') {
                        pos++;
                        break;
                    }
                    if (pos + 1 < len && html[pos] == '/' && html[pos + 1] == '>') {
                        isSelfClosing = true;
                        pos += 2;
                        break;
                    }

                    // Read attribute name
                    size_t aStart = pos;
                    while (pos < len && html[pos] != '=' && html[pos] != '>' && html[pos] != '/' &&
                           !std::isspace(static_cast<unsigned char>(html[pos]))) {
                        pos++;
                    }
                    std::string attrName = toLower(html.substr(aStart, pos - aStart));
                    while (pos < len && std::isspace(static_cast<unsigned char>(html[pos]))) pos++;

                    std::string attrVal = "";
                    if (pos < len && html[pos] == '=') {
                        pos++;
                        while (pos < len && std::isspace(static_cast<unsigned char>(html[pos]))) pos++;
                        if (pos < len && (html[pos] == '"' || html[pos] == '\'')) {
                            char q = html[pos++];
                            size_t vStart = pos;
                            while (pos < len && html[pos] != q) {
                                pos++;
                            }
                            attrVal = decodeHtmlEntities(html.substr(vStart, pos - vStart));
                            if (pos < len && html[pos] == q) pos++;
                        } else {
                            size_t vStart = pos;
                            while (pos < len && html[pos] != '>' && html[pos] != '/' &&
                                   !std::isspace(static_cast<unsigned char>(html[pos]))) {
                                pos++;
                            }
                            attrVal = decodeHtmlEntities(html.substr(vStart, pos - vStart));
                        }
                    }
                    if (!attrName.empty()) {
                        attrs[attrName] = attrVal;
                    }
                }

                auto newElem = std::make_shared<HTMLElement>();
                newElem->tagName = tagName;
                newElem->attributes = attrs;
                newElem->parent = elementStack.back().elem;

                elementStack.back().elem->children.push_back(newElem);
                elementStack.back().elem->childNodes.push_back(HTMLNode{true, "", newElem});

                bool isVoid = std::find(voidTags.begin(), voidTags.end(), tagName) != voidTags.end();
                if (isSelfClosing || isVoid) {
                    newElem->rawInnerHTML = "";
                    continue;
                }

                // Check raw text tags
                bool isRawText = std::find(rawTextTags.begin(), rawTextTags.end(), tagName) != rawTextTags.end();
                if (isRawText) {
                    size_t openEnd = pos;
                    std::string closePattern = "</" + tagName;
                    // Find closing tag case-insensitively
                    size_t closePos = std::string::npos;
                    for (size_t p = openEnd; p + closePattern.size() <= len; ++p) {
                        if (toLower(html.substr(p, closePattern.size())) == closePattern) {
                            closePos = p;
                            break;
                        }
                    }
                    if (closePos != std::string::npos) {
                        newElem->rawInnerHTML = html.substr(openEnd, closePos - openEnd);
                        newElem->childNodes.push_back(HTMLNode{false, newElem->rawInnerHTML, nullptr});
                        size_t endTag = html.find('>', closePos);
                        if (endTag != std::string::npos) {
                            pos = endTag + 1;
                        } else {
                            pos = len;
                        }
                    } else {
                        newElem->rawInnerHTML = html.substr(openEnd);
                        newElem->childNodes.push_back(HTMLNode{false, newElem->rawInnerHTML, nullptr});
                        pos = len;
                    }
                    continue;
                }

                elementStack.push_back({newElem, pos});
            } else {
                // Text node
                size_t textStart = pos;
                while (pos < len && html[pos] != '<') {
                    pos++;
                }
                std::string rawText = html.substr(textStart, pos - textStart);
                elementStack.back().elem->childNodes.push_back(HTMLNode{false, decodeHtmlEntities(rawText), nullptr});
            }
        }

        // Close any remaining unclosed elements
        while (elementStack.size() > 1) {
            auto top = elementStack.back();
            top.elem->rawInnerHTML = html.substr(top.openTagEndPos);
            elementStack.pop_back();
        }

        return root;
    }

    static std::optional<std::string> extractValue(const std::shared_ptr<HTMLElement>& element, const std::string& queryRule) {
        if (!element) return std::nullopt;

        // Split query rule by '|'
        std::vector<std::string> parts;
        std::istringstream iss(queryRule);
        std::string token;
        while (std::getline(iss, token, '|')) {
            parts.push_back(trim(token));
        }

        if (parts.size() < 2) return std::nullopt;

        // Find extractor position
        auto isExtractor = [](const std::string& str) {
            return str.rfind("attr:", 0) == 0 ||
                   str == "text" ||
                   str == "html" ||
                   str == "hrefOrOnclick" ||
                   str == "selector" ||
                   str == "nextSiblingText" ||
                   str == "hiddenInputs";
        };

        size_t extractorIdx = 1;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (isExtractor(parts[i])) {
                extractorIdx = i;
                break;
            }
        }

        std::string selector;
        for (size_t i = 0; i < extractorIdx; ++i) {
            if (i > 0) selector += " | ";
            selector += parts[i];
        }

        std::string extractor = parts[extractorIdx];
        std::vector<std::string> pipes;
        for (size_t i = extractorIdx + 1; i < parts.size(); ++i) {
            pipes.push_back(parts[i]);
        }

        if (selector.empty() || extractor.empty()) return std::nullopt;

        std::shared_ptr<HTMLElement> targetEl = (selector == "self") ? element : element->querySelector(selector);
        if (!targetEl) return std::nullopt;

        std::string val;
        bool hasVal = false;

        if (extractor.rfind("attr:", 0) == 0) {
            std::string attrName = extractor.substr(5);
            auto attrOpt = targetEl->getAttribute(attrName);
            if (attrOpt.has_value()) {
                val = attrOpt.value();
                hasVal = true;
            }
        } else if (extractor == "text") {
            val = targetEl->textContent();
            hasVal = true;
        } else if (extractor == "html") {
            val = targetEl->innerHTML();
            hasVal = true;
        } else if (extractor == "hrefOrOnclick") {
            auto hrefOpt = targetEl->getAttribute("href");
            std::string href = hrefOpt.value_or("");
            if (!href.empty() && href != "#" && href.rfind("javascript:", 0) != 0) {
                val = href;
                hasVal = true;
            } else {
                auto onclickOpt = targetEl->getAttribute("onclick");
                std::string onclick = onclickOpt.value_or("");
                std::smatch match;
                std::regex locRegex(R"((?:document|window)\.location(?:\.href)?\s*=\s*['"]([^'"]+)['"])", std::regex::icase);
                std::regex locRegex2(R"(document\.location\s*=\s*['"]([^'"]+)['"])", std::regex::icase);
                if (std::regex_search(onclick, match, locRegex) || std::regex_search(onclick, match, locRegex2)) {
                    val = match[1].str();
                    hasVal = true;
                } else if (!href.empty()) {
                    val = href;
                    hasVal = true;
                }
            }
        } else if (extractor == "selector") {
            auto spmIdOpt = targetEl->getAttribute("data-spm-id");
            std::string spmId;
            if (spmIdOpt.has_value() && !spmIdOpt.value().empty()) {
                spmId = spmIdOpt.value();
            } else {
                thread_local std::mt19937 gen(std::random_device{}());
                static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
                std::string randStr;
                for (int i = 0; i < 7; ++i) {
                    randStr += charset[gen() % (sizeof(charset) - 1)];
                }
                spmId = "spm-id-" + randStr;
                targetEl->setAttribute("data-spm-id", spmId);
            }
            val = "[data-spm-id=\"" + spmId + "\"]";
            hasVal = true;
        } else if (extractor == "nextSiblingText") {
            auto next = targetEl->nextElementSibling();
            if (next) {
                val = next->textContent();
                hasVal = true;
            }
        } else if (extractor == "hiddenInputs") {
            auto inputs = targetEl->querySelectorAll("input[type=\"hidden\"], input[type=\"HIDDEN\"]");
            nlohmann::json list = nlohmann::json::array();
            for (const auto& input : inputs) {
                auto nameOpt = input->getAttribute("name");
                if (nameOpt.has_value() && !nameOpt.value().empty()) {
                    std::string itemVal = input->getAttribute("value").value_or("");
                    nlohmann::json item = nlohmann::json::object();
                    item["name"] = nameOpt.value();
                    item["value"] = itemVal;
                    list.push_back(item);
                }
            }
            val = list.dump();
            hasVal = true;
        } else {
            return std::nullopt;
        }

        if (!hasVal) return std::nullopt;

        return applyPipes(val, pipes);
    }

    static nlohmann::json validate(const nlohmann::json& manifest, const std::shared_ptr<HTMLElement>& document) {
        nlohmann::json results = nlohmann::json::object();
        results["reconstructs"] = nlohmann::json::array();
        results["components"] = nlohmann::json::array();

        if (!document) return results;

        // Validate reconstructs
        if (manifest.contains("reconstructs") && manifest["reconstructs"].is_array()) {
            for (const auto& recon : manifest["reconstructs"]) {
                std::string containerSelector = recon.value("containerSelector", "");
                auto container = document->querySelector(containerSelector);

                nlohmann::json reconResult = nlohmann::json::object();
                reconResult["containerSelector"] = containerSelector;
                reconResult["status"] = container ? "PASS" : "FAIL";
                reconResult["matched"] = container ? 1 : 0;
                reconResult["children"] = nlohmann::json::array();
                reconResult["binds"] = nlohmann::json::array();

                if (container) {
                    // Validate binds
                    if (recon.contains("propsMap") && recon["propsMap"].is_object()) {
                        for (auto it = recon["propsMap"].begin(); it != recon["propsMap"].end(); ++it) {
                            std::string key = it.key();
                            std::string rule = it.value().is_string() ? it.value().get<std::string>() : "";
                            auto extracted = extractValue(container, rule);

                            nlohmann::json bindObj = nlohmann::json::object();
                            bindObj["key"] = key;
                            bindObj["rule"] = rule;
                            bindObj["status"] = extracted.has_value() ? "PASS" : "FAIL";
                            if (extracted.has_value()) {
                                bindObj["value"] = extracted.value();
                            } else {
                                bindObj["value"] = nullptr;
                            }
                            reconResult["binds"].push_back(bindObj);
                        }
                    }

                    // Validate children
                    if (recon.contains("children") && recon["children"].is_array()) {
                        for (const auto& child : recon["children"]) {
                            std::string childName = child.value("name", "");
                            std::string childSelector = child.value("selector", "");
                            std::string scope = child.value("scope", "container");

                            auto childItems = (scope == "document")
                                ? document->querySelectorAll(childSelector)
                                : container->querySelectorAll(childSelector);

                            nlohmann::json childResult = nlohmann::json::object();
                            childResult["name"] = childName;
                            childResult["selector"] = childSelector;
                            childResult["scope"] = scope;
                            childResult["matched"] = static_cast<int>(childItems.size());
                            childResult["status"] = childItems.empty() ? "FAIL" : "PASS";
                            childResult["itemsBinds"] = nlohmann::json::array();

                            bool allItemsPassed = !childItems.empty();

                            for (const auto& item : childItems) {
                                nlohmann::json itemBinds = nlohmann::json::array();
                                if (child.contains("propsMap") && child["propsMap"].is_object()) {
                                    for (auto it = child["propsMap"].begin(); it != child["propsMap"].end(); ++it) {
                                        std::string key = it.key();
                                        std::string rule = it.value().is_string() ? it.value().get<std::string>() : "";
                                        auto extracted = extractValue(item, rule);
                                        bool isBindPass = extracted.has_value();
                                        if (!isBindPass) allItemsPassed = false;

                                        nlohmann::json bindObj = nlohmann::json::object();
                                        bindObj["key"] = key;
                                        bindObj["rule"] = rule;
                                        bindObj["status"] = isBindPass ? "PASS" : "FAIL";
                                        if (extracted.has_value()) {
                                            bindObj["value"] = extracted.value();
                                        } else {
                                            bindObj["value"] = nullptr;
                                        }
                                        itemBinds.push_back(bindObj);
                                    }
                                }
                                childResult["itemsBinds"].push_back(itemBinds);
                            }

                            childResult["status"] = allItemsPassed ? "PASS" : "FAIL";
                            reconResult["children"].push_back(childResult);
                        }
                    }
                }

                results["reconstructs"].push_back(reconResult);
            }
        }

        // Validate components (selectors)
        if (manifest.contains("components") && manifest["components"].is_array()) {
            for (const auto& comp : manifest["components"]) {
                std::string selector = comp.value("selector", "");
                std::string action = comp.value("action", "replace");

                auto els = document->querySelectorAll(selector);

                nlohmann::json compResult = nlohmann::json::object();
                compResult["selector"] = selector;
                compResult["action"] = action;
                compResult["matched"] = static_cast<int>(els.size());
                compResult["status"] = (action == "hide" || !els.empty()) ? "PASS" : "FAIL";
                compResult["binds"] = nlohmann::json::array();

                if (!els.empty() && comp.contains("propsMap") && comp["propsMap"].is_object()) {
                    for (auto it = comp["propsMap"].begin(); it != comp["propsMap"].end(); ++it) {
                        std::string key = it.key();
                        std::string rule = it.value().is_string() ? it.value().get<std::string>() : "";
                        auto extracted = extractValue(els[0], rule);

                        nlohmann::json bindObj = nlohmann::json::object();
                        bindObj["key"] = key;
                        bindObj["rule"] = rule;
                        bindObj["status"] = extracted.has_value() ? "PASS" : "FAIL";
                        if (extracted.has_value()) {
                            bindObj["value"] = extracted.value();
                        } else {
                            bindObj["value"] = nullptr;
                        }
                        compResult["binds"].push_back(bindObj);
                    }
                }

                results["components"].push_back(compResult);
            }
        }

        return results;
    }

    static nlohmann::json validate(const std::string& manifestJsonStr, const std::string& htmlStr) {
        nlohmann::json manifest;
        try {
            manifest = nlohmann::json::parse(manifestJsonStr);
        } catch (...) {
            nlohmann::json err = nlohmann::json::object();
            err["error"] = "Invalid manifest JSON";
            return err;
        }

        auto document = parseHtml(htmlStr);
        return validate(manifest, document);
    }
};

} // namespace veneer

#endif // VENEER_HTML_VALIDATOR_HPP
