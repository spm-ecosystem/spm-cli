#ifndef VENEER_LEXER_HPP
#define VENEER_LEXER_HPP

#include <string_view>
#include <vector>
#include <string>
#include <optional>
#include <cctype>

namespace veneer {

enum class TokenType {
    KeywordTheme,
    KeywordSelector,
    KeywordReconstruct,
    KeywordClass,
    KeywordExtends,
    KeywordBind,
    KeywordPreserve,
    KeywordChild,
    KeywordVariables,
    KeywordStyles,
    KeywordScope,
    KeywordShadow,
    Arrow,
    Identifier,
    StringLiteral,
    Pipe,
    BraceOpen,
    BraceClose,
    Colon,
    Semicolon,
    EOFToken,
    Unknown
};

inline const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::KeywordTheme:       return "KeywordTheme";
        case TokenType::KeywordSelector:    return "KeywordSelector";
        case TokenType::KeywordReconstruct: return "KeywordReconstruct";
        case TokenType::KeywordClass:       return "KeywordClass";
        case TokenType::KeywordExtends:     return "KeywordExtends";
        case TokenType::KeywordBind:        return "KeywordBind";
        case TokenType::KeywordPreserve:    return "KeywordPreserve";
        case TokenType::KeywordChild:       return "KeywordChild";
        case TokenType::KeywordVariables:   return "KeywordVariables";
        case TokenType::KeywordStyles:      return "KeywordStyles";
        case TokenType::KeywordScope:       return "KeywordScope";
        case TokenType::KeywordShadow:      return "KeywordShadow";
        case TokenType::Arrow:              return "Arrow";
        case TokenType::Identifier:         return "Identifier";
        case TokenType::StringLiteral:      return "StringLiteral";
        case TokenType::Pipe:               return "Pipe";
        case TokenType::BraceOpen:          return "BraceOpen";
        case TokenType::BraceClose:         return "BraceClose";
        case TokenType::Colon:              return "Colon";
        case TokenType::Semicolon:          return "Semicolon";
        case TokenType::EOFToken:           return "EOFToken";
        case TokenType::Unknown:            return "Unknown";
    }
    return "Unknown";
}

struct Token {
    TokenType type;
    std::string_view value;
    size_t line{1};
};

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source), pos_(0), line_(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        const size_t len = source_.length();

        while (pos_ < len) {
            char c = source_[pos_];

            // 1. Skip Whitespace
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (c == '\n') {
                    line_++;
                }
                pos_++;
                continue;
            }

            // 2. Skip Comments (// ... or /* ... */)
            if (c == '/' && pos_ + 1 < len) {
                if (source_[pos_ + 1] == '/') {
                    pos_ += 2;
                    while (pos_ < len && source_[pos_] != '\n') {
                        pos_++;
                    }
                    continue;
                } else if (source_[pos_ + 1] == '*') {
                    pos_ += 2;
                    while (pos_ < len) {
                        if (source_[pos_] == '\n') {
                            line_++;
                        } else if (source_[pos_] == '*' && pos_ + 1 < len && source_[pos_ + 1] == '/') {
                            pos_ += 2;
                            break;
                        }
                        pos_++;
                    }
                    continue;
                }
            }

            // 3. Arrow ("->")
            if (c == '-' && pos_ + 1 < len && source_[pos_ + 1] == '>') {
                tokens.push_back({TokenType::Arrow, source_.substr(pos_, 2), line_});
                pos_ += 2;
                continue;
            }

            // 4. Single-character delimiters
            if (c == '{') {
                tokens.push_back({TokenType::BraceOpen, source_.substr(pos_, 1), line_});
                pos_++;
                continue;
            }
            if (c == '}') {
                tokens.push_back({TokenType::BraceClose, source_.substr(pos_, 1), line_});
                pos_++;
                continue;
            }
            if (c == ':') {
                tokens.push_back({TokenType::Colon, source_.substr(pos_, 1), line_});
                pos_++;
                continue;
            }
            if (c == ';') {
                tokens.push_back({TokenType::Semicolon, source_.substr(pos_, 1), line_});
                pos_++;
                continue;
            }
            if (c == '|') {
                tokens.push_back({TokenType::Pipe, source_.substr(pos_, 1), line_});
                pos_++;
                continue;
            }

            // 5. Raw String Literals (R"delim(content)delim")
            if ((c == 'R' || c == 'r') && pos_ + 1 < len && source_[pos_ + 1] == '"') {
                size_t start = pos_;
                size_t tokenLine = line_;
                pos_ += 2; // skip R"
                
                size_t delimStart = pos_;
                while (pos_ < len && source_[pos_] != '(') {
                    pos_++;
                }
                std::string_view delimiter = source_.substr(delimStart, pos_ - delimStart);
                pos_++; // skip '('
                
                std::string targetEnd = ")";
                targetEnd += delimiter;
                targetEnd += '"';
                
                while (pos_ < len) {
                    if (source_[pos_] == '\n') {
                        line_++;
                    }
                    if (source_.substr(pos_).rfind(targetEnd, 0) == 0) {
                        pos_ += targetEnd.size();
                        break;
                    }
                    pos_++;
                }
                tokens.push_back({TokenType::StringLiteral, source_.substr(start, pos_ - start), tokenLine});
                continue;
            }

            // 6. Standard String Literals ("...")
            if (c == '"') {
                size_t start = pos_;
                size_t tokenLine = line_;
                pos_++; // skip opening quote
                while (pos_ < len) {
                    if (source_[pos_] == '\\' && pos_ + 1 < len) {
                        pos_ += 2;
                    } else if (source_[pos_] == '"') {
                        pos_++; // skip closing quote
                        break;
                    } else {
                        if (source_[pos_] == '\n') {
                            line_++;
                        }
                        pos_++;
                    }
                }
                tokens.push_back({TokenType::StringLiteral, source_.substr(start, pos_ - start), tokenLine});
                continue;
            }

            // 6. Identifiers and Keywords
            if (isIdentifierStart(c)) {
                size_t start = pos_;
                size_t tokenLine = line_;
                while (pos_ < len && isIdentifierChar(source_[pos_])) {
                    pos_++;
                }
                std::string_view word = source_.substr(start, pos_ - start);
                auto kw = getKeywordTokenType(word);
                if (kw.has_value()) {
                    tokens.push_back({*kw, word, tokenLine});
                } else {
                    tokens.push_back({TokenType::Identifier, word, tokenLine});
                }
                continue;
            }

            // 7. Unknown token fallback
            tokens.push_back({TokenType::Unknown, source_.substr(pos_, 1), line_});
            pos_++;
        }

        tokens.push_back({TokenType::EOFToken, "", line_});
        return tokens;
    }

private:
    std::string_view source_;
    size_t pos_;
    size_t line_;

    static bool isIdentifierStart(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) ||
               c == '_' || c == '-' || c == '.' || c == '#' ||
               c == '@' || c == '*' || c == '[' || c == ']' ||
               c == '\'' || c == '=' || c == '(';
    }

    static bool isIdentifierChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) ||
               c == '_' || c == '-' || c == '.' || c == '#' ||
               c == '@' || c == '*' || c == '[' || c == ']' ||
               c == '\'' || c == '=' || c == '%' || c == '(' || c == ')';
    }

    static std::optional<TokenType> getKeywordTokenType(std::string_view text) {
        if (text == "theme")       return TokenType::KeywordTheme;
        if (text == "selector")    return TokenType::KeywordSelector;
        if (text == "reconstruct") return TokenType::KeywordReconstruct;
        if (text == "class")       return TokenType::KeywordClass;
        if (text == "extends")     return TokenType::KeywordExtends;
        if (text == "bind")        return TokenType::KeywordBind;
        if (text == "preserve")    return TokenType::KeywordPreserve;
        if (text == "child")       return TokenType::KeywordChild;
        if (text == "variables")   return TokenType::KeywordVariables;
        if (text == "styles")      return TokenType::KeywordStyles;
        if (text == "scope")       return TokenType::KeywordScope;
        if (text == "shadow")      return TokenType::KeywordShadow;
        return std::nullopt;
    }
};

} // namespace veneer

#endif // VENEER_LEXER_HPP
