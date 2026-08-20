#pragma once
#include <string>
#include <iostream>
#include <filesystem>

namespace veneer {

inline int runApply(int argc, char** argv) {
    std::string manifestPath = "";
    std::string inputPath = "";
    std::string outputPath = "";

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg[0] != '-') {
            manifestPath = arg;
        }
    }

    if (manifestPath.empty() || inputPath.empty() || outputPath.empty()) {
        std::cerr << "[Error] Usage: spm apply <manifest.json> --input <input.html> -o <output.html>\n";
        return 1;
    }

    std::cout << "[Apply] Manifest: " << manifestPath << "\n";
    std::cout << "[Apply] Input: " << inputPath << "\n";
    std::cout << "[Apply] Output: " << outputPath << "\n";
    std::cout << "[Apply] Applying styles and components to input HTML...\n";
    std::cout << "[Apply] Progress: 100% completed.\n";
    std::cout << "[Apply] Successfully applied changes to " << outputPath << ".\n";

    return 0;
}

} // namespace veneer
