#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXConnectionState.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <algorithm>
#include <mutex>

#include "veneer/lexer.hpp"
#include "veneer/parser.hpp"
#include "veneer/resolver.hpp"
#include "veneer/emitter.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <command> [options]\n\n"
              << "Commands:\n"
              << "  install                  Install SPM CLI to system PATH\n"
              << "  compile <source.vnr> -o <output.json>   Compile Veneer Spec file to manifest.json\n"
              << "  dev -d <source.vnr|manifest.json>       Start local WebSocket dev server\n"
              << "  publish                  Publish theme to SPM registry\n"
              << "  help                     Show this help message\n";
}

std::string readTextFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

std::string buildDevPayload(const std::string& manifestPath) {
    fs::path p(manifestPath);
    std::string manifestContent;
    fs::path cssPath;
    fs::path siblingManifest;

    if (fs::is_directory(p)) {
        std::string combinedVnr = "";
        for (const auto& entry : fs::recursive_directory_iterator(p)) {
            if (entry.is_regular_file() && entry.path().extension() == ".vnr") {
                combinedVnr += readTextFile(entry.path().string()) + "\n\n";
            }
        }
        if (combinedVnr.empty()) {
            std::cerr << "[Watcher] No .vnr files found in directory: " << manifestPath << "\n";
            return "";
        }
        try {
            veneer::Lexer lexer(combinedVnr);
            veneer::Parser parser(lexer.tokenize());
            veneer::ASTNode ast = parser.parse();
            veneer::Resolver resolver(ast);
            resolver.resolve();

            std::string existingJson = "";
            siblingManifest = p / "manifest.json";
            if (fs::exists(siblingManifest)) {
                existingJson = readTextFile(siblingManifest.string());
            }

            manifestContent = veneer::Emitter::emit(ast, existingJson);
        } catch (const std::exception& e) {
            std::cerr << "[Watcher] Error compiling Veneer directory: " << e.what() << "\n";
            return "";
        }
        cssPath = p / "content.css";
    } else if (p.extension() == ".vnr") {
        std::string vnrContent = readTextFile(manifestPath);
        if (vnrContent.empty()) return "";
        try {
            veneer::Lexer lexer(vnrContent);
            veneer::Parser parser(lexer.tokenize());
            veneer::ASTNode ast = parser.parse();
            veneer::Resolver resolver(ast);
            resolver.resolve();

            std::string existingJson = "";
            siblingManifest = p.parent_path() / "manifest.json";
            if (fs::exists(siblingManifest)) {
                existingJson = readTextFile(siblingManifest.string());
            }

            manifestContent = veneer::Emitter::emit(ast, existingJson);
        } catch (const std::exception& e) {
            std::cerr << "[Watcher] Error compiling Veneer spec: " << e.what() << "\n";
            return "";
        }
        cssPath = p.parent_path() / "content.css";
    } else {
        manifestContent = readTextFile(manifestPath);
        if (manifestContent.empty()) return "";
        cssPath = p.parent_path() / "content.css";
    }

    std::string cssContent = "";
    if (fs::exists(cssPath)) {
        cssContent = readTextFile(cssPath.string());
    }

    try {
        json jManifest = json::parse(manifestContent);
        json payload;
        payload["manifest"] = jManifest;
        payload["css"] = cssContent;
        return payload.dump();
    } catch (const std::exception& e) {
        std::cerr << "[Watcher] Error parsing manifest JSON: " << e.what() << "\n";
        return "";
    }
}

std::string readManifestFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";

    try {
        json j = json::parse(file);
        return j.dump();
    } catch (const std::exception& e) {
        std::cerr << "[Error] Manifest parse failed: " << e.what() << "\n";
        return "";
    }
}

int installToPath(const std::string& currentExePath) {
    fs::path exeDir = fs::absolute(currentExePath).parent_path();

#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        char buffer[8192];
        DWORD bufferSize = sizeof(buffer);
        
        if (RegQueryValueExA(hKey, "Path", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            std::string currentPath(buffer);
            if (currentPath.find(exeDir.string()) == std::string::npos) {
                std::string newPath = currentPath + ";" + exeDir.string();
                RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (const BYTE*)newPath.c_str(), newPath.length() + 1);
                std::cout << "[SPM] Successfully added to PATH.\n";
                SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
            } else {
                std::cout << "[SPM] Directory already in PATH.\n";
            }
        }
        RegCloseKey(hKey);
    }
#else
    const char* home = getenv("HOME");
    if (!home) return 1;

    std::string homeDir(home);
    std::string exportCmd = "\n# SPM CLI\nexport PATH=\"$PATH:" + exeDir.string() + "\"\n";

    const char* configs[] = {".bashrc", ".zshrc"};
    for (const char* conf : configs) {
        std::string rcPath = homeDir + "/" + conf;
        std::ofstream out(rcPath, std::ios_base::app);
        if (out.is_open()) {
            out << exportCmd;
            std::cout << "[SPM] Appended to ~/" << conf << "\n";
        }
    }
    std::cout << "[SPM] Restart your terminal or run 'source ~/.bashrc' to apply.\n";
#endif
    return 0;
}

int runCompile(int argc, char** argv) {
    std::string sourcePath = "";
    std::string outputPath = "";

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg[0] != '-') {
            sourcePath = arg;
        }
    }

    if (sourcePath.empty() || outputPath.empty()) {
        std::cerr << "[Error] Usage: spm compile <source.vnr|directory> -o <output.json>\n";
        return 1;
    }

    if (!fs::exists(sourcePath)) {
        std::cerr << "[Error] Source path does not exist: " << sourcePath << "\n";
        return 1;
    }

    std::string combinedVnr = "";
    if (fs::is_directory(sourcePath)) {
        for (const auto& entry : fs::recursive_directory_iterator(sourcePath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".vnr") {
                combinedVnr += readTextFile(entry.path().string()) + "\n\n";
            }
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

        // If sourcePath is a file, load sibling classes to resolve dependencies
        if (!fs::is_directory(sourcePath)) {
            fs::path parentDir = fs::path(sourcePath).parent_path();
            if (parentDir.empty()) parentDir = ".";
            for (const auto& entry : fs::directory_iterator(parentDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".vnr" && entry.path() != fs::path(sourcePath)) {
                    try {
                        std::string siblingContent = readTextFile(entry.path().string());
                        veneer::Lexer siblingLexer(siblingContent);
                        veneer::Parser siblingParser(siblingLexer.tokenize());
                        veneer::ASTNode siblingAst = siblingParser.parse();
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
                    } catch (...) {
                        // Sibling files might be syntactically incomplete, ignore errors
                    }
                }
            }
        }

        veneer::Resolver resolver(ast);
        resolver.resolve();

        std::string existingJson = "";
        if (fs::exists(outputPath)) {
            existingJson = readTextFile(outputPath);
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

std::string g_manifestPath = "";
std::mutex g_manifestMutex;

int runDevServer(const std::string& initialManifestPath) {
    {
        std::lock_guard<std::mutex> lock(g_manifestMutex);
        g_manifestPath = initialManifestPath;
    }

    int port = 8080;
    ix::WebSocketServer server(port, "0.0.0.0");

    std::cout << "===========================================\n";
    std::cout << "SPM Dev Server - ws://localhost:" << port << "\n";
    if (!initialManifestPath.empty()) {
        std::cout << "Monitoring: " << initialManifestPath << "\n";
    } else {
        std::cout << "Monitoring: (Waiting for extension client to specify theme path...)\n";
    }
    std::cout << "===========================================\n";

    server.setOnConnectionCallback([&server](std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState) {
        auto ws = webSocket.lock();
        if (ws) {
            ws->setOnMessageCallback([webSocket, connectionState, &server](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open) {
                    std::cout << "[WS] Connected: " << connectionState->getRemoteIp() << "\n";
                    std::string currentPath;
                    {
                        std::lock_guard<std::mutex> lock(g_manifestMutex);
                        currentPath = g_manifestPath;
                    }
                    if (!currentPath.empty()) {
                        auto wsActive = webSocket.lock();
                        if (wsActive) {
                            std::string currentPayload = buildDevPayload(currentPath);
                            if (!currentPayload.empty()) wsActive->send(currentPayload);
                        }
                    }
                } else if (msg->type == ix::WebSocketMessageType::Message) {
                    try {
                        auto j = json::parse(msg->str);
                        if (j.contains("action") && j["action"] == "watch") {
                            std::string path = j.value("path", "");
                            auto wsActive = webSocket.lock();
                            if (path.empty()) {
                                if (wsActive) wsActive->send(json{{"status", "error"}, {"message", "Path is empty"}}.dump());
                                return;
                            }
                            if (!fs::exists(path)) {
                                std::cerr << "[WS] Client requested invalid path: " << path << "\n";
                                if (wsActive) wsActive->send(json{{"status", "error"}, {"message", "File does not exist: " + path}}.dump());
                                return;
                            }
                            
                            std::cout << "[WS] Monitoring target set to: " << path << "\n";
                            {
                                std::lock_guard<std::mutex> lock(g_manifestMutex);
                                g_manifestPath = path;
                            }
                            
                            if (wsActive) {
                                wsActive->send(json{{"status", "success"}, {"watching", path}}.dump());
                                std::string currentPayload = buildDevPayload(path);
                                if (!currentPayload.empty()) wsActive->send(currentPayload);
                            }
                        }
                    } catch (const std::exception& e) {
                        // ignore malformed ws messages
                    }
                } else if (msg->type == ix::WebSocketMessageType::Close) {
                    std::cout << "[WS] Disconnected: " << connectionState->getRemoteIp() << "\n";
                }
            });
        }
    });

    auto res = server.listen();
    if (!res.first) {
        std::cerr << "[Error] Port " << port << " failed: " << res.second << "\n";
        return 1;
    }
    server.start();

    std::string lastPath = "";
    std::string lastPayload = "";
    while (true) {
        std::string currentPath;
        {
            std::lock_guard<std::mutex> lock(g_manifestMutex);
            currentPath = g_manifestPath;
        }
        if (!currentPath.empty()) {
            std::string currentPayload = buildDevPayload(currentPath);
            if (!currentPayload.empty() && (currentPath != lastPath || currentPayload != lastPayload)) {
                std::cout << "[Watcher] Syncing changes...\n";
                for (auto&& client : server.getClients()) {
                    client->send(currentPayload);
                }
                lastPath = currentPath;
                lastPayload = currentPayload;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    server.stop();
    return 0;
}

int runPublish(const std::string& manifestPath = "manifest.json") {
    std::cout << "[Publish] Reading manifest metadata...\n";
    std::string rawJson = readManifestFile(manifestPath);
    
    if (rawJson.empty()) {
        std::cerr << "[Error] Could not read " << manifestPath << ". Are you in the correct directory?\n";
        return 1;
    }

    try {
        json manifest = json::parse(rawJson);
        
        std::string targetUrl = manifest.value("targetUrl", "");
        targetUrl = std::regex_replace(targetUrl, std::regex(R"(^\*://|/\*$)"), "");
        
        std::string themeLabel = manifest["theme"].value("label", "untitled");
        std::string themeName = std::regex_replace(themeLabel, std::regex(R"(\s+)"), "-");
        std::transform(themeName.begin(), themeName.end(), themeName.begin(), ::tolower);

        if (targetUrl.empty()) {
            std::cerr << "[Error] Invalid manifest: targetUrl is missing.\n";
            return 1;
        }

        std::string branchName = "theme/" + targetUrl + "/" + themeName;
        std::string targetRepoUrl = "https://github.com/spm-ecosystem/spm-websites.git";

        fs::path currentDir = fs::current_path();
        fs::path tempWorkspace = fs::temp_directory_path() / ("spm_publish_" + themeName);

        std::cout << "[Publish] Creating isolated workspace...\n";
        
        if (fs::exists(tempWorkspace)) {
            fs::remove_all(tempWorkspace);
        }

        // Shallow clone with sparse checkout (Zero file downloads, instant fetch)
        std::string cmdClone = "git clone --depth 1 --filter=blob:none --sparse " + targetRepoUrl + " " + tempWorkspace.string() + " -q";
        if (std::system(cmdClone.c_str()) != 0) {
            std::cerr << "[Error] Failed to connect to repository. Check your git permissions.\n";
            return 1;
        }

        // Create and checkout the new branch
        std::string cmdCheckout = "git -C " + tempWorkspace.string() + " checkout -b " + branchName + " -q";
        std::system(cmdCheckout.c_str());

        // Create the deterministic directory structure: targetUrl/theme-name/
        fs::path targetFolder = tempWorkspace / targetUrl / themeName;
        fs::create_directories(targetFolder);

        std::cout << "[Publish] Packaging theme files...\n";
        for (const auto& entry : fs::directory_iterator(currentDir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension();
                if (ext == ".json" || ext == ".css") {
                    fs::copy(entry.path(), targetFolder / entry.path().filename(), fs::copy_options::overwrite_existing);
                }
            }
        }

        // Find and copy all .vnr files recursively from the current directory, preserving their relative paths
        for (const auto& entry : fs::recursive_directory_iterator(currentDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".vnr") {
                // If spm_publish_ temp workspace is inside currentDir or matches, ignore it
                if (entry.path().string().find(tempWorkspace.string()) != std::string::npos) {
                    continue;
                }
                fs::path relPath = fs::relative(entry.path(), currentDir);
                fs::path destFile = targetFolder / relPath;
                fs::create_directories(destFile.parent_path());
                fs::copy(entry.path(), destFile, fs::copy_options::overwrite_existing);
            }
        }

        std::cout << "[Publish] Pushing to remote registry...\n";

        // ADD --sparse bypasses the sparse-checkout limits for these specific new files
        std::string cmdAdd = "git -C " + tempWorkspace.string() + " add --sparse .";
        std::string cmdCommit = "git -C " + tempWorkspace.string() + " commit -m \"feat(theme): publish " + themeName + " for " + targetUrl + "\" -q";
        std::string cmdPush = "git -C " + tempWorkspace.string() + " push -u origin " + branchName + " -q";

        std::system(cmdAdd.c_str());
        std::system(cmdCommit.c_str()); 
        
        if (std::system(cmdPush.c_str()) != 0) {
            std::cerr << "[Error] Failed to push to remote repository.\n";
            fs::remove_all(tempWorkspace); 
            return 1;
        }

        fs::remove_all(tempWorkspace);

        std::cout << "===========================================\n";
        std::cout << "✅ Success! Theme pushed to GitHub securely.\n";
        std::cout << "Go to https://github.com/spm-ecosystem/spm-websites/pulls\n";
        std::cout << "Open a Pull Request to trigger the Edge Deployment.\n";
        std::cout << "===========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "[Error] Process failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "install") {
        return installToPath(argv[0]);
    } 
    else if (command == "compile") {
        return runCompile(argc, argv);
    }
    else if (command == "dev") {
        std::string manifestPath = "";
        for (int i = 2; i < argc; i++) {
            if (std::string(argv[i]) == "-d" && i + 1 < argc) {
                manifestPath = argv[i + 1];
                break;
            }
        }
        if (manifestPath.empty()) {
            manifestPath = fs::current_path().string();
        }
        return runDevServer(manifestPath);
    } 
    else if (command == "publish") {
        return runPublish();
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