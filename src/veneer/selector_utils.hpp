#ifndef VENEER_SELECTOR_UTILS_HPP
#define VENEER_SELECTOR_UTILS_HPP

#include <string>
#include <cctype>

namespace veneer {

struct ShadowSelectorInfo {
    bool isShadow = false;
    std::string shadowHost;
    std::string innerSelector;
};

inline ShadowSelectorInfo parseShadowSelector(const std::string& rawSel) {
    ShadowSelectorInfo info;
    if (rawSel.find("shadow:") == 0) {
        info.isShadow = true;
        std::string rest = rawSel.substr(7);
        
        size_t start = rest.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) rest = rest.substr(start);
        else rest.clear();

        size_t arrowPos = rest.find("->");
        if (arrowPos != std::string::npos) {
            info.shadowHost = rest.substr(0, arrowPos);
            info.innerSelector = rest.substr(arrowPos + 2);
        } else {
            size_t spacePos = rest.find(' ');
            if (spacePos != std::string::npos) {
                info.shadowHost = rest.substr(0, spacePos);
                info.innerSelector = rest.substr(spacePos + 1);
            } else {
                info.shadowHost = rest;
                info.innerSelector = "";
            }
        }
        
        size_t endHost = info.shadowHost.find_last_not_of(" \t\n\r");
        if (endHost != std::string::npos) info.shadowHost.erase(endHost + 1);
        
        size_t startInner = info.innerSelector.find_first_not_of(" \t\n\r");
        if (startInner != std::string::npos) info.innerSelector = info.innerSelector.substr(startInner);
        else info.innerSelector.clear();
    }
    return info;
}

} // namespace veneer

#endif // VENEER_SELECTOR_UTILS_HPP
