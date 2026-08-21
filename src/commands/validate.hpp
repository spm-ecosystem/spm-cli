#pragma once
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include "execute.hpp"

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

    // Resolve the validate.js script path relative to the running binary
    fs::path exePath = fs::absolute(argv[0]).parent_path();
    fs::path scriptPath = exePath / "scripts/validate.js";
    if (!fs::exists(scriptPath)) {
        scriptPath = exePath / "src/scripts/validate.js";
    }
    if (!fs::exists(scriptPath)) {
        scriptPath = exePath / "../src/scripts/validate.js";
    }

    if (!fs::exists(scriptPath)) {
        std::cerr << "[Error] Validation helper script not found at: " << scriptPath << "\n";
        return 1;
    }

    std::vector<std::string> args = { scriptPath.string(), manifestPath, "--against", snapshotPath };
    if (isJson) {
        args.push_back("--json");
    }

    return safeExecute("node", args);
}

} // namespace veneer
