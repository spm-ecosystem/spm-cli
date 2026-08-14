#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace veneer {

inline std::string readCssFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

inline std::string bundleCssFiles(const fs::path& directory) {
    std::vector<fs::path> cssFiles;
    try {
        if (fs::exists(directory) && fs::is_directory(directory)) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".css") {
                    std::string pathStr = entry.path().string();
                    // Exclude common build/temp/system folders
                    if (pathStr.find("/.git/") != std::string::npos ||
                        pathStr.find("/.vscode/") != std::string::npos ||
                        pathStr.find("/node_modules/") != std::string::npos ||
                        pathStr.find("/tmp/") != std::string::npos ||
                        pathStr.find("spm_publish_") != std::string::npos) {
                        continue;
                    }
                    cssFiles.push_back(entry.path());
                }
            }
        }
    } catch (...) {}

    std::sort(cssFiles.begin(), cssFiles.end());

    std::string bundledContent = "";
    for (const auto& file : cssFiles) {
        bundledContent += "/* --- File: " + file.filename().string() + " --- */\n";
        bundledContent += readCssFile(file.string()) + "\n\n";
    }
    return bundledContent;
}

} // namespace veneer
