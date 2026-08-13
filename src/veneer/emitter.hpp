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
    static nlohmann::json toJSON(const ASTNode& ast) {
        nlohmann::json root = nlohmann::json::object();

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
            selObj["containerSelector"] = sel.selector;

            if (sel.action == "hide") {
                selObj["action"] = "hide";
            } else {
                if (!sel.component.empty()) {
                    selObj["layoutComponent"] = sel.component;
                }
                if (!sel.action.empty() && sel.action != "replace") {
                    selObj["action"] = sel.action;
                }

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
                        propsObj[prop.key] = prop.value;
                    }
                }

                selObj["props"] = propsObj;
                selObj["propsMap"] = propsMapObj;
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
                if (prop.isBinding) {
                    std::string val = prop.bindingTarget;
                    if (!prop.bindingOperation.empty()) {
                        val += " | " + prop.bindingOperation;
                    }
                    propsMapObj[prop.key] = val;
                } else {
                    propsObj[prop.key] = prop.value;
                }
            }

            reconObj["props"] = propsObj;
            reconObj["propsMap"] = propsMapObj;

            nlohmann::json childrenArr = nlohmann::json::array();
            for (const auto& child : recon.children) {
                nlohmann::json childObj = nlohmann::json::object();
                childObj["name"] = child.name;
                childObj["selector"] = child.selector;
                childObj["scope"] = child.scope.empty() ? "container" : child.scope;

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
                        childProps[prop.key] = prop.value;
                    }
                }

                childObj["props"] = childProps;
                childObj["propsMap"] = childPropsMap;

                childrenArr.push_back(childObj);
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
            root[it.key()] = it.value();
        }

        return root.dump(4);
    }
};

inline std::string emit(const ASTNode& ast, const std::string& existingJsonStr = "") {
    return Emitter::emit(ast, existingJsonStr);
}

} // namespace veneer

#endif // VENEER_EMITTER_HPP
