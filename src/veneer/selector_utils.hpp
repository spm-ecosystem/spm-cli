#ifndef VENEER_SELECTOR_UTILS_HPP
#define VENEER_SELECTOR_UTILS_HPP

#include <string>
#include <string_view>

namespace veneer {

struct ShadowSelectorInfo {
    bool isShadow = false;
    std::string shadowHost;
    std::string innerSelector;
};

inline ShadowSelectorInfo parseShadowSelector(std::string_view rawSel) {
    ShadowSelectorInfo info;
    if (rawSel.rfind("shadow:", 0) == 0) {
        info.isShadow = true;
        std::string_view rest = rawSel.substr(7);
        
        size_t start = rest.find_first_not_of(" \t\n\r");
        if (start != std::string_view::npos) {
            rest = rest.substr(start);
        } else {
            rest = "";
        }

        std::string_view hostView;
        std::string_view innerView;

        size_t arrowPos = rest.find("->");
        if (arrowPos != std::string_view::npos) {
            hostView = rest.substr(0, arrowPos);
            innerView = rest.substr(arrowPos + 2);
        } else {
            size_t spacePos = rest.find(' ');
            if (spacePos != std::string_view::npos) {
                hostView = rest.substr(0, spacePos);
                innerView = rest.substr(spacePos + 1);
            } else {
                hostView = rest;
                innerView = "";
            }
        }

        // Trim hostView
        size_t endHost = hostView.find_last_not_of(" \t\n\r");
        if (endHost != std::string_view::npos) {
            hostView = hostView.substr(0, endHost + 1);
        } else {
            hostView = "";
        }

        // Trim innerView
        size_t startInner = innerView.find_first_not_of(" \t\n\r");
        if (startInner != std::string_view::npos) {
            innerView = innerView.substr(startInner);
        } else {
            innerView = "";
        }

        info.shadowHost = std::string(hostView);
        info.innerSelector = std::string(innerView);
    }
    return info;
}

} // namespace veneer

#endif // VENEER_SELECTOR_UTILS_HPP
