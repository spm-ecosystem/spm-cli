#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "../utils/fs_utils.hpp"
#include "../utils/css_bundler.hpp"
#include "../veneer/lexer.hpp"
#include "../veneer/parser.hpp"
#include "../veneer/resolver.hpp"
#include "../veneer/emitter.hpp"

namespace fs = std::filesystem;

namespace veneer {

inline int runCompile(int argc, char** argv) {
    std::string sourcePath = "";
    std::string outputPath = "";
    bool strict = false;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--strict") {
            strict = true;
        } else if (arg[0] != '-') {
            sourcePath = arg;
        }
    }

    if (sourcePath.empty() || outputPath.empty()) {
        std::cerr << "[Error] Usage: spm compile <source.vnr|directory> -o <output.json> [--strict]\n";
        return 1;
    }

    if (!fs::exists(sourcePath)) {
        std::cerr << "[Error] Source path does not exist: " << sourcePath << "\n";
        return 1;
    }

    std::string combinedVnr = "";
    if (fs::is_directory(sourcePath)) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(sourcePath, fs::directory_options::skip_permission_denied)) {
                try {
                    if (entry.is_regular_file() && entry.path().extension() == ".vnr") {
                        combinedVnr += readTextFile(entry.path().string()) + "\n\n";
                    }
                } catch (...) {}
            }
        } catch (const std::exception& e) {
            std::cerr << "[Warning] Directory iteration error: " << e.what() << "\n";
        }
        if (combinedVnr.empty()) {
            std::cerr << "[Error] No .vnr files found in directory: " << sourcePath << "\n";
            return 1;
        }
    } else {
        combinedVnr = readTextFile(sourcePath);
        if (combinedVnr.empty()) {
            std::cerr << "[Error] Could not read source file or file is empty: " << sourcePath << "\n";
            return 1;
        }
    }

    try {
        veneer::Lexer lexer(combinedVnr);
        veneer::Parser parser(lexer.tokenize());
        veneer::ASTNode ast = parser.parse();

        if (!fs::is_directory(sourcePath)) {
            fs::path parentDir = fs::path(sourcePath).parent_path();
            if (parentDir.empty()) parentDir = ".";
            try {
                for (const auto& entry : fs::directory_iterator(parentDir, fs::directory_options::skip_permission_denied)) {
                    try {
                        if (entry.is_regular_file() && entry.path().extension() == ".vnr" && entry.path() != fs::path(sourcePath)) {
                            try {
                                std::string siblingContent = readTextFile(entry.path().string());
                                veneer::Lexer siblingLexer(siblingContent);
                                veneer::Parser siblingParser(siblingLexer.tokenize());
                                veneer::ASTNode siblingAst = siblingParser.parse();
                                if (ast.targetUrl.empty() && !siblingAst.targetUrl.empty()) {
                                    ast.targetUrl = siblingAst.targetUrl;
                                }
                                for (const auto& cls : siblingAst.classes) {
                                    bool exists = false;
                                    for (const auto& c : ast.classes) {
                                        if (c.name == cls.name) {
                                            exists = true;
                                            break;
                                        }
                                    }
                                    if (!exists) {
                                        ast.classes.push_back(cls);
                                    }
                                }
                            } catch (...) {}
                        }
                    } catch (...) {}
                }
            } catch (...) {}
        }

        std::string existingJson = "";
        if (fs::exists(outputPath)) {
            existingJson = readTextFile(outputPath);
            try {
                nlohmann::json existingObj = nlohmann::json::parse(existingJson);
                if (ast.targetUrl.empty() && existingObj.contains("targetUrl") && existingObj["targetUrl"].is_string()) {
                    std::string tUrl = existingObj["targetUrl"].get<std::string>();
                    if (!tUrl.empty()) {
                        ast.targetUrl = tUrl;
                    }
                }
            } catch (...) {}
        }

        veneer::Resolver resolver(ast);
        resolver.resolve();

        for (const auto& warn : resolver.getWarnings()) {
            std::cerr << warn << "\n";
        }

        if (strict && ast.targetUrl.empty()) {
            std::cerr << "[Error] Veneer compilation failed in strict mode: Manifest lacks required root 'targetUrl' property.\n";
            return 1;
        }

        fs::path themeDir = fs::is_directory(sourcePath) ? fs::path(sourcePath) : fs::path(sourcePath).parent_path();
        std::string bundledCss = veneer::bundleCssFiles(themeDir);
        if (!bundledCss.empty() && !ast.themes.empty()) {
            ast.themes[0].customStyles.push_back(bundledCss);
        }

        std::string compiledJson = veneer::Emitter::emit(ast, existingJson);

        std::ofstream outFile(outputPath);
        if (!outFile.is_open()) {
            std::cerr << "[Error] Failed to open output file for writing: " << outputPath << "\n";
            return 1;
        }
        outFile << compiledJson << "\n";
        outFile.close();

        std::cout << "[SPM] Successfully compiled " << sourcePath << " -> " << outputPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Error] Veneer compilation failed: " << e.what() << "\n";
        return 1;
    }
}

} // namespace veneer
