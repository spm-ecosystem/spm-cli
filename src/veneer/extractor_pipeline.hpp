#pragma once
#include <string>
#include <vector>
#include <optional>
#include <regex>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "selector_utils.hpp"

namespace veneer {

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

inline std::optional<std::string> parseCleanNumber(const std::string& raw) {
    std::string trimmed = trim(raw);
    if (trimmed.empty()) return std::nullopt;

    double multiplier = 1.0;
    std::string workStr = trimmed;

    std::smatch suffixMatch;
    std::regex suffixRegex(R"(([0-9.,]+)\s*([kKmMbB])\b)");
    if (std::regex_search(trimmed, suffixMatch, suffixRegex)) {
        char unit = std::tolower(static_cast<unsigned char>(suffixMatch[2].str()[0]));
        if (unit == 'k') multiplier = 1000.0;
        else if (unit == 'm') multiplier = 1000000.0;
        else if (unit == 'b') multiplier = 1000000000.0;
        workStr = suffixMatch[1].str();
    }

    bool isNegative = std::regex_search(trimmed, std::regex(R"(^\s*-\s*[\$€R£\d])")) ||
                      std::regex_search(trimmed, std::regex(R"([\$€R£\s]-\s*[\d.])"));

    std::string numStr = "";
    for (char c : workStr) {
        if ((c >= '0' && c <= '9') || c == '.' || c == ',') {
            numStr += c;
        }
    }
    if (numStr.empty()) return std::nullopt;

    if (numStr.find(',') != std::string::npos && numStr.find('.') != std::string::npos) {
        if (numStr.rfind(',') > numStr.rfind('.')) {
            // European format: 1.234,56 -> 1234.56
            numStr.erase(std::remove(numStr.begin(), numStr.end(), '.'), numStr.end());
            std::replace(numStr.begin(), numStr.end(), ',', '.');
        } else {
            // US format: 1,234.56 -> 1234.56
            numStr.erase(std::remove(numStr.begin(), numStr.end(), ','), numStr.end());
        }
    } else if (numStr.find(',') != std::string::npos) {
        size_t commaPos = numStr.find(',');
        // If comma has at most 2 decimal digits after it and is the only comma
        if (numStr.rfind(',') == commaPos && (numStr.length() - commaPos - 1) <= 2) {
            numStr[commaPos] = '.';
        } else {
            numStr.erase(std::remove(numStr.begin(), numStr.end(), ','), numStr.end());
        }
    }

    try {
        size_t parsedLen = 0;
        double numVal = std::stod(numStr, &parsedLen);
        if (parsedLen == 0) return std::nullopt;

        double result = (isNegative ? -1.0 : 1.0) * numVal * multiplier;
        
        std::ostringstream ss;
        if (result == std::floor(result) && !std::isinf(result)) {
            ss << static_cast<long long>(result);
        } else {
            ss << result;
        }
        return ss.str();
    } catch (...) {
        return std::nullopt;
    }
}

inline std::optional<std::string> applyPipes(const std::string& initialVal, const std::vector<std::string>& pipes) {
    std::string currentVal = initialVal;
    for (const auto& pipe : pipes) {
        if (pipe == "number") {
            std::string trimmed = trim(currentVal);
            if (trimmed.empty()) return std::nullopt;
            try {
                size_t parsedLen = 0;
                double d = std::stod(trimmed, &parsedLen);
                if (parsedLen != trimmed.size()) return std::nullopt;
                std::ostringstream ss;
                if (d == std::floor(d) && !std::isinf(d)) {
                    ss << static_cast<long long>(d);
                } else {
                    ss << d;
                }
                currentVal = ss.str();
            } catch (...) {
                return std::nullopt;
            }
        } else if (pipe == "cleanNumber") {
            auto cleaned = parseCleanNumber(currentVal);
            if (!cleaned.has_value()) return std::nullopt;
            currentVal = cleaned.value();
        } else if (pipe == "split") {
            std::stringstream ss(currentVal);
            std::string token;
            nlohmann::json jsonArr = nlohmann::json::array();
            while (ss >> token) {
                jsonArr.push_back(token);
            }
            currentVal = jsonArr.dump();
        } else if (pipe.rfind("split:", 0) == 0) {
            std::string delim = pipe.substr(6);
            nlohmann::json jsonArr = nlohmann::json::array();
            if (delim.empty()) {
                for (char c : currentVal) {
                    jsonArr.push_back(std::string(1, c));
                }
            } else {
                size_t start = 0, end = 0;
                while ((end = currentVal.find(delim, start)) != std::string::npos) {
                    std::string token = currentVal.substr(start, end - start);
                    jsonArr.push_back(trim(token));
                    start = end + delim.length();
                }
                std::string token = currentVal.substr(start);
                jsonArr.push_back(trim(token));
            }
            currentVal = jsonArr.dump();
        } else {
            return std::nullopt;
        }
    }
    return currentVal;
}

template <typename ElementPtr>
inline ElementPtr queryShadowSelector(const ElementPtr& root, const std::string& selector, bool isShadow = false, const std::string& shadowHost = "", const std::string& innerSelector = "") {
    if (!root) return nullptr;
    if (isShadow || selector.rfind("shadow:", 0) == 0) {
        std::string host = shadowHost;
        std::string inner = innerSelector;
        if (host.empty() && inner.empty()) {
            auto parsed = parseShadowSelector(selector);
            host = parsed.shadowHost;
            inner = parsed.innerSelector;
        }
        auto hostEl = root->querySelector(host);
        if (!hostEl) return nullptr;
        if (inner.empty()) return hostEl;
        return hostEl->querySelector(inner);
    }
    return root->querySelector(selector);
}

template <typename ElementPtr>
inline std::vector<ElementPtr> queryShadowSelectorAll(const ElementPtr& root, const std::string& selector, bool isShadow = false, const std::string& shadowHost = "", const std::string& innerSelector = "") {
    std::vector<ElementPtr> results;
    if (!root) return results;
    if (isShadow || selector.rfind("shadow:", 0) == 0) {
        std::string host = shadowHost;
        std::string inner = innerSelector;
        if (host.empty() && inner.empty()) {
            auto parsed = parseShadowSelector(selector);
            host = parsed.shadowHost;
            inner = parsed.innerSelector;
        }
        auto hostEl = root->querySelector(host);
        if (!hostEl) return results;
        if (inner.empty()) {
            results.push_back(hostEl);
            return results;
        }
        return hostEl->querySelectorAll(inner);
    }
    return root->querySelectorAll(selector);
}

template <typename ElementPtr>
inline nlohmann::json extractReconstructData(const nlohmann::json& recon, const ElementPtr& document) {
    nlohmann::json result = nlohmann::json::object();
    if (!document) {
        result["matched"] = 0;
        result["status"] = "FAIL";
        return result;
    }

    std::string containerSelector = recon.value("containerSelector", "");
    bool isShadow = recon.value("isShadow", false);
    std::string shadowHost = recon.value("shadowHost", "");
    std::string innerSelector = recon.value("innerSelector", "");

    auto container = queryShadowSelector(document, containerSelector, isShadow, shadowHost, innerSelector);
    if (!container) {
        result["containerSelector"] = containerSelector;
        result["matched"] = 0;
        result["status"] = "FAIL";
        return result;
    }

    result["containerSelector"] = containerSelector;
    result["matched"] = 1;
    result["status"] = "PASS";
    if (isShadow) {
        result["isShadow"] = true;
        if (!shadowHost.empty()) result["shadowHost"] = shadowHost;
        if (!innerSelector.empty()) result["innerSelector"] = innerSelector;
    }

    return result;
}

} // namespace veneer
