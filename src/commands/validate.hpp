#pragma once
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "../utils/fs_utils.hpp"
#include "../veneer/html_validator.hpp"

namespace fs = std::filesystem;

namespace veneer {

inline int runValidate(int argc, char** argv) {
    std::string manifestPath = "";
    std::string snapshotPath = "";
    bool isJson = false;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--against" && i + 1 < argc) {
            snapshotPath = argv[++i];
        } else if (arg == "--json" || arg == "-j") {
            isJson = true;
        } else if (arg[0] != '-') {
            manifestPath = arg;
        }
    }

    if (manifestPath.empty() || snapshotPath.empty()) {
        std::cerr << "[Error] Usage: spm validate <manifest.json> --against <snapshot.html> [--json]\n";
        return 1;
    }

    if (!fs::exists(manifestPath) || !fs::exists(snapshotPath)) {
        std::cerr << "[Error] File not found: " << manifestPath << " or " << snapshotPath << "\n";
        return 1;
    }

    try {
        std::string manifestJson = readTextFile(manifestPath);
        std::string htmlContent = readTextFile(snapshotPath);

        nlohmann::json results = HtmlValidator::validate(manifestJson, htmlContent);

        if (results.contains("error")) {
            std::cerr << "[Error] " << results["error"].get<std::string>() << "\n";
            return 1;
        }

        int totalPass = 0;
        int totalFail = 0;

        if (results.contains("reconstructs") && results["reconstructs"].is_array()) {
            for (const auto& recon : results["reconstructs"]) {
                std::string status = recon.value("status", "FAIL");
                if (status == "PASS") totalPass++; else totalFail++;

                if (recon.contains("children") && recon["children"].is_array()) {
                    for (const auto& child : recon["children"]) {
                        std::string cStatus = child.value("status", "FAIL");
                        if (cStatus == "PASS") totalPass++; else totalFail++;
                    }
                }
            }
        }

        if (results.contains("components") && results["components"].is_array()) {
            for (const auto& comp : results["components"]) {
                std::string status = comp.value("status", "FAIL");
                if (status == "PASS") totalPass++; else totalFail++;
            }
        }

        if (isJson) {
            std::cout << results.dump(2) << "\n";
        } else {
            std::cout << "===========================================\n";
            std::cout << "SPM Validate Results\n";
            std::cout << "===========================================\n";

            if (results.contains("reconstructs") && results["reconstructs"].is_array()) {
                for (const auto& recon : results["reconstructs"]) {
                    std::string status = recon.value("status", "FAIL");
                    std::string icon = (status == "PASS") ? "✅" : "❌";
                    std::string containerSelector = recon.value("containerSelector", "");
                    int matched = recon.value("matched", 0);
                    std::cout << icon << " Reconstruct: " << containerSelector
                              << " -> " << status << " (" << matched << " match)\n";

                    if (recon.contains("binds") && recon["binds"].is_array()) {
                        for (const auto& bind : recon["binds"]) {
                            std::string bStatus = bind.value("status", "FAIL");
                            std::string bIcon = (bStatus == "PASS") ? "  ├─ ✅" : "  ├─ ❌";
                            std::string key = bind.value("key", "");
                            std::string rule = bind.value("rule", "");
                            std::string valStr = "null";
                            if (bind.contains("value") && !bind["value"].is_null()) {
                                if (bind["value"].is_string()) {
                                    valStr = bind["value"].get<std::string>();
                                } else {
                                    valStr = bind["value"].dump();
                                }
                            }
                            std::cout << bIcon << " Bind \"" << key << "\": \""
                                      << rule << "\" -> \"" << valStr << "\"\n";
                        }
                    }

                    if (recon.contains("children") && recon["children"].is_array()) {
                        for (const auto& child : recon["children"]) {
                            std::string cStatus = child.value("status", "FAIL");
                            std::string cIcon = (cStatus == "PASS") ? "  ├─ ✅" : "  ├─ ❌";
                            std::string name = child.value("name", "");
                            std::string selector = child.value("selector", "");
                            int cMatched = child.value("matched", 0);
                            std::cout << cIcon << " Child \"" << name << "\": \""
                                      << selector << "\" -> " << cStatus
                                      << " (" << cMatched << " matches)\n";
                        }
                    }
                }
            }

            if (results.contains("components") && results["components"].is_array()) {
                for (const auto& comp : results["components"]) {
                    std::string status = comp.value("status", "FAIL");
                    std::string icon = (status == "PASS") ? "✅" : "❌";
                    std::string selector = comp.value("selector", "");
                    std::string action = comp.value("action", "");
                    int cMatched = comp.value("matched", 0);
                    std::cout << icon << " Component Selector: \"" << selector
                              << "\" [" << action << "] -> " << status
                              << " (" << cMatched << " matches)\n";
                }
            }

            std::cout << "===========================================\n";
            std::cout << "Summary: " << totalPass << " Passed, " << totalFail << " Failed\n";
            std::cout << "===========================================\n";
        }

        if (totalFail > 0) return 1;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Error] Validation failed: " << e.what() << "\n";
        return 1;
    }
}

} // namespace veneer
