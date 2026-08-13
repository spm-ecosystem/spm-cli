#ifndef VENEER_RESOLVER_HPP
#define VENEER_RESOLVER_HPP

#include "parser.hpp"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

namespace veneer {

class Resolver {
public:
    explicit Resolver(ASTNode& ast) : ast_(ast) {
        for (auto& cls : ast_.classes) {
            classMap_[cls.name] = &cls;
        }
    }

    void resolve() {
        for (auto& cls : ast_.classes) {
            resolveClass(cls);
        }
        for (auto& recon : ast_.reconstructs) {
            for (auto& child : recon.children) {
                resolveChild(child);
            }
        }
    }

private:
    ASTNode& ast_;
    std::map<std::string, ClassNode*> classMap_;
    std::unordered_set<std::string> resolvedClasses_;
    std::unordered_set<std::string> resolvingClasses_;
    std::vector<std::string> resolveStack_;

    void resolveClass(ClassNode& cls) {
        if (resolvedClasses_.count(cls.name)) {
            return;
        }
        if (resolvingClasses_.count(cls.name)) {
            std::string cycle = "";
            bool inCycle = false;
            for (const auto& name : resolveStack_) {
                if (name == cls.name) inCycle = true;
                if (inCycle) cycle += name + " -> ";
            }
            cycle += cls.name;
            throw std::runtime_error("[Resolver Error] Circular inheritance detected: " + cycle);
        }

        resolvingClasses_.insert(cls.name);
        resolveStack_.push_back(cls.name);

        if (!cls.extendsClass.empty()) {
            auto it = classMap_.find(cls.extendsClass);
            if (it != classMap_.end()) {
                resolveClass(*(it->second));
                mergeProperties(cls.properties, it->second->properties);
            } else {
                throw std::runtime_error("[Resolver Error] Unknown base class: " + cls.extendsClass);
            }
        }

        resolveStack_.pop_back();
        resolvingClasses_.erase(cls.name);
        resolvedClasses_.insert(cls.name);
    }

    void resolveChild(ASTChild& child) {
        if (!child.extendsClass.empty()) {
            auto it = classMap_.find(child.extendsClass);
            if (it != classMap_.end()) {
                resolveClass(*(it->second));
                mergeProperties(child.properties, it->second->properties);
                if (child.scope.empty()) {
                    child.scope = it->second->scope;
                }
            } else {
                throw std::runtime_error("[Resolver Error] Unknown base class for child: " + child.extendsClass);
            }
        }
        for (auto& c : child.children) {
            resolveChild(c);
        }
    }

    void mergeProperties(std::vector<ASTProperty>& target, const std::vector<ASTProperty>& base) {
        std::vector<ASTProperty> merged;
        std::map<std::string, size_t> targetKeys;
        
        for (size_t i = 0; i < target.size(); ++i) {
            targetKeys[target[i].key] = i;
        }

        for (const auto& baseProp : base) {
            if (targetKeys.find(baseProp.key) == targetKeys.end()) {
                merged.push_back(baseProp);
            }
        }

        for (const auto& targetProp : target) {
            merged.push_back(targetProp);
        }

        target = std::move(merged);
    }
};

} // namespace veneer

#endif // VENEER_RESOLVER_HPP
