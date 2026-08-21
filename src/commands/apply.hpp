#pragma once
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include "execute.hpp"

namespace fs = std::filesystem;

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

    // Resolve the apply.js script path relative to the running binary
    fs::path exePath = fs::absolute(argv[0]).parent_path();
    fs::path scriptPath = exePath / "scripts/apply.js";
    if (!fs::exists(scriptPath)) {
        scriptPath = exePath / "src/scripts/apply.js";
    }
    if (!fs::exists(scriptPath)) {
        scriptPath = exePath / "../src/scripts/apply.js";
    }

    if (!fs::exists(scriptPath)) {
        std::cerr << "[Error] Apply helper script not found at: " << scriptPath << "\n";
        return 1;
    }

    std::vector<std::string> args = { scriptPath.string(), manifestPath, "--input", inputPath, "-o", outputPath };

    return safeExecute("node", args);
}

} // namespace veneer
