#ifndef VENEER_EMITTER_HPP
#define VENEER_EMITTER_HPP

#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace veneer {

class Emitter {
public:
    static nlohmann::json emitChild(const ASTChild& child, const std::function<nlohmann::json(const std::string&)>& parsePropValue) {
        nlohmann::json childObj = nlohmann::json::object();
        childObj["name"] = child.name;
        childObj["selector"] = child.selector;
        if (child.isShadow) {
            childObj["isShadow"] = true;
            if (!child.shadowHost.empty()) childObj["shadowHost"] = child.shadowHost;
            if (!child.innerSelector.empty()) childObj["innerSelector"] = child.innerSelector;
        }
        if (!child.scope.empty() && child.scope != "container") {
            childObj["scope"] = child.scope;
        }

        nlohmann::json childProps = nlohmann::json::object();
        nlohmann::json childPropsMap = nlohmann::json::object();

        for (const auto& prop : child.properties) {
            if (prop.isBinding) {
                std::string val = prop.bindingTarget;
                if (!prop.bindingOperation.empty()) {
                    val += " | " + prop.bindingOperation;
                }
                childPropsMap[prop.key] = val;
            } else {
                childProps[prop.key] = parsePropValue(prop.value);
            }
        }

        if (!childProps.empty()) childObj["props"] = childProps;
        if (!childPropsMap.empty()) childObj["propsMap"] = childPropsMap;

        if (!child.children.empty()) {
            nlohmann::json childChildrenArr = nlohmann::json::array();
            for (const auto& c : child.children) {
                childChildrenArr.push_back(emitChild(c, parsePropValue));
            }
            childObj["children"] = childChildrenArr;
        }

        return childObj;
    }

    static nlohmann::json toJSON(const ASTNode& ast) {
        nlohmann::json root = nlohmann::json::object();

        auto parsePropValue = [](const std::string& valueStr) -> nlohmann::json {
            std::string val = valueStr;
            while(!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
            while(!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
            try {
                return nlohmann::json::parse(val);
            } catch (...) {
                return valueStr;
            }
        };

        // 1. Theme
        if (!ast.themes.empty()) {
            const auto& themeNode = ast.themes[0];
            nlohmann::json themeObj = nlohmann::json::object();
            themeObj["label"] = themeNode.label;
            
            nlohmann::json varsObj = nlohmann::json::object();
            for (const auto& [k, v] : themeNode.variables) {
                varsObj[k] = v;
            }
            themeObj["cssVariables"] = varsObj;

            std::string customStylesStr;
            for (size_t i = 0; i < themeNode.customStyles.size(); ++i) {
                if (i > 0) customStylesStr += "\n";
                customStylesStr += themeNode.customStyles[i];
            }
            themeObj["customStyles"] = customStylesStr;

            root["theme"] = themeObj;
        }

        // 2. Selectors -> components
        nlohmann::json componentsArr = nlohmann::json::array();
        for (const auto& sel : ast.selectors) {
            nlohmann::json selObj = nlohmann::json::object();
            selObj["selector"] = sel.selector;
            if (sel.isShadow) {
                selObj["isShadow"] = true;
                if (!sel.shadowHost.empty()) selObj["shadowHost"] = sel.shadowHost;
                if (!sel.innerSelector.empty()) selObj["innerSelector"] = sel.innerSelector;
            }
            if (!sel.component.empty()) {
                selObj["name"] = sel.component;
            }

            if (sel.action == "hide") {
                selObj["action"] = "hide";
            } else {
                selObj["action"] = sel.action.empty() ? "replace" : sel.action;

                nlohmann::json propsObj = nlohmann::json::object();
                nlohmann::json propsMapObj = nlohmann::json::object();

                for (const auto& prop : sel.properties) {
                    if (prop.isBinding) {
                        std::string val = prop.bindingTarget;
                        if (!prop.bindingOperation.empty()) {
                            val += " | " + prop.bindingOperation;
                        }
                        propsMapObj[prop.key] = val;
                    } else {
                        propsObj[prop.key] = parsePropValue(prop.value);
                    }
                }

                if (!propsObj.empty()) selObj["props"] = propsObj;
                if (!propsMapObj.empty()) selObj["propsMap"] = propsMapObj;
            }

            componentsArr.push_back(selObj);
        }
        root["components"] = componentsArr;

        // 3. Reconstructs -> reconstructs
        nlohmann::json reconstructsArr = nlohmann::json::array();
        for (const auto& recon : ast.reconstructs) {
            nlohmann::json reconObj = nlohmann::json::object();
            reconObj["containerSelector"] = recon.selector;
            reconObj["layoutComponent"] = recon.component;
            if (recon.isShadow) {
                reconObj["isShadow"] = true;
                if (!recon.shadowHost.empty()) reconObj["shadowHost"] = recon.shadowHost;
                if (!recon.innerSelector.empty()) reconObj["innerSelector"] = recon.innerSelector;
            }

            if (!recon.mediaQuery.empty()) {
                reconObj["mediaQuery"] = recon.mediaQuery;
            }

            if (!recon.preservationSlots.empty()) {
                nlohmann::json preserveObj = nlohmann::json::object();
                for (const auto& [slot, selector] : recon.preservationSlots) {
                    preserveObj[slot] = selector;
                }
                reconObj["preserve"] = preserveObj;
            }

            nlohmann::json propsObj = nlohmann::json::object();
            nlohmann::json propsMapObj = nlohmann::json::object();

            for (const auto& prop : recon.properties) {
                if (prop.key == "urlPattern" || prop.key == "infiniteScroll") {
                    reconObj[prop.key] = parsePropValue(prop.value);
                } else if (prop.isBinding) {
                    std::string val = prop.bindingTarget;
                    if (!prop.bindingOperation.empty()) {
                        val += " | " + prop.bindingOperation;
                    }
                    propsMapObj[prop.key] = val;
                } else {
                    propsObj[prop.key] = parsePropValue(prop.value);
                }
            }

            if (!propsObj.empty()) reconObj["props"] = propsObj;
            if (!propsMapObj.empty()) reconObj["propsMap"] = propsMapObj;

            nlohmann::json childrenArr = nlohmann::json::array();
            for (const auto& child : recon.children) {
                childrenArr.push_back(emitChild(child, parsePropValue));
            }

            reconObj["children"] = childrenArr;

            reconstructsArr.push_back(reconObj);
        }
        root["reconstructs"] = reconstructsArr;

        return root;
    }

    static std::string emit(const ASTNode& ast, const std::string& existingJsonStr = "") {
        nlohmann::json root = nlohmann::json::object();

        if (!existingJsonStr.empty()) {
            try {
                root = nlohmann::json::parse(existingJsonStr);
                if (!root.is_object()) {
                    root = nlohmann::json::object();
                }
            } catch (...) {
                root = nlohmann::json::object();
            }
        }

        nlohmann::json compiled = toJSON(ast);

        for (auto it = compiled.begin(); it != compiled.end(); ++it) {
            if (it.key() == "theme" && root.contains("theme") && root["theme"].is_object() && it.value().is_object()) {
                nlohmann::json mergedTheme = it.value();
                for (auto themeIt = root["theme"].begin(); themeIt != root["theme"].end(); ++themeIt) {
                    if (!mergedTheme.contains(themeIt.key())) {
                        mergedTheme[themeIt.key()] = themeIt.value();
                    }
                }
                root["theme"] = mergedTheme;
            } else {
                root[it.key()] = it.value();
            }
        }

        return root.dump(4);
    }
};

inline std::string emit(const ASTNode& ast, const std::string& existingJsonStr = "") {
    return Emitter::emit(ast, existingJsonStr);
}

} // namespace veneer

#endif // VENEER_EMITTER_HPP
