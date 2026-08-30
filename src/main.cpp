#include <iostream>
#include <string>
#include <filesystem>
#include "utils/fs_utils.hpp"
#include "commands/compile.hpp"
#include "commands/publish.hpp"
#include "commands/dev.hpp"
#include "commands/validate.hpp"
#include "commands/apply.hpp"

namespace fs = std::filesystem;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <command> [options]\n\n"
              << "Commands:\n"
              << "  install                  Install SPM CLI to system PATH\n"
              << "  compile <source.vnr> -o <output.json>   Compile Veneer Spec file to manifest.json\n"
              << "  dev -d <source.vnr|manifest.json>       Start local WebSocket dev server\n"
              << "  publish                  Publish theme to SPM registry\n"
              << "  validate <manifest.json> --against <snapshot.html>\n"
              << "  apply <manifest.json> --input <input.html> -o <output.html>\n"
              << "  help                     Show this help message\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "install") {
        return veneer::installToPath(argv[0]);
    } 
    else if (command == "compile") {
        return veneer::runCompile(argc, argv);
    }
    else if (command == "dev") {
        std::string manifestPath = "";
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if ((arg == "-d" || arg == "--dir") && i + 1 < argc) {
                manifestPath = argv[i + 1];
                break;
            } else if (!arg.empty() && arg[0] != '-' && manifestPath.empty()) {
                manifestPath = arg;
            }
        }
        if (manifestPath.empty()) {
            manifestPath = fs::current_path().string();
        }
        return veneer::runDevServer(manifestPath);
    } 
    else if (command == "publish") {
        return veneer::runPublish();
    } 
    else if (command == "validate") {
        return veneer::runValidate(argc, argv);
    }
    else if (command == "apply") {
        return veneer::runApply(argc, argv);
    }
    else if (command == "help") {
        printUsage(argv[0]);
        return 0;
    }
    else {
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}