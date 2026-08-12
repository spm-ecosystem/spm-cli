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
              << "  dev -d <manifest.json>   Start local WebSocket dev server\n"
              << "  publish                  Publish theme to SPM registry\n"
              << "  help                     Show this help message\n";
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

int runDevServer(const std::string& manifestPath) {
    if (manifestPath.empty()) {
        std::cerr << "[Error] Missing manifest path. Use -d <path>\n";
        return 1;
    }

    int port = 8080;
    ix::WebSocketServer server(port, "0.0.0.0");

    std::cout << "===========================================\n";
    std::cout << "SPM Dev Server - ws://localhost:" << port << "\n";
    std::cout << "Monitoring: " << manifestPath << "\n";
    std::cout << "===========================================\n";

    server.setOnConnectionCallback([manifestPath](std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState) {
        auto ws = webSocket.lock();
        if (ws) {
            ws->setOnMessageCallback([webSocket, connectionState, manifestPath](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open) {
                    std::cout << "[WS] Connected: " << connectionState->getRemoteIp() << "\n";
                    auto wsActive = webSocket.lock();
                    if (wsActive) {
                        std::string currentManifest = readManifestFile(manifestPath);
                        if (!currentManifest.empty()) wsActive->send(currentManifest);
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

    std::string lastContent = "";
    while (true) {
        std::string currentContent = readManifestFile(manifestPath);
        if (!currentContent.empty() && currentContent != lastContent) {
            std::cout << "[Watcher] Syncing changes...\n";
            for (auto&& client : server.getClients()) {
                client->send(currentContent);
            }
            lastContent = currentContent;
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
        
        // 1. Extract and sanitize targetUrl (e.g., "*://safebooru.org/*" -> "safebooru.org")
        std::string targetUrl = manifest.value("targetUrl", "");
        targetUrl = std::regex_replace(targetUrl, std::regex(R"(^\*://|/\*$)"), "");
        
        // 2. Extract and sanitize theme name (e.g., "Obsidian Dark" -> "obsidian-dark")
        std::string themeLabel = manifest["theme"].value("label", "untitled");
        std::string themeName = std::regex_replace(themeLabel, std::regex(R"(\s+)"), "-");
        std::transform(themeName.begin(), themeName.end(), themeName.begin(), ::tolower);

        if (targetUrl.empty()) {
            std::cerr << "[Error] Invalid manifest: targetUrl is missing.\n";
            return 1;
        }

        std::string branchName = "theme/" + targetUrl + "/" + themeName;

        std::cout << "[Publish] Target Domain : " << targetUrl << "\n";
        std::cout << "[Publish] Theme Name    : " << themeName << "\n";
        std::cout << "[Publish] Executing Git pipeline...\n";

        // 3. Build Git Commands
        // Creates the branch (or switches if it exists)
        std::string cmdCheckout = "git checkout -b " + branchName + " 2> /dev/null || git checkout " + branchName;
        std::string cmdAdd = "git add .";
        std::string cmdCommit = "git commit -m \"feat(theme): publish " + themeName + " for " + targetUrl + "\"";
        std::string cmdPush = "git push -u origin " + branchName;

        // 4. Execute commands sequentially
        if (std::system(cmdCheckout.c_str()) != 0) {
            std::cerr << "[Error] Failed to switch/create git branch.\n"; 
            return 1;
        }
        
        if (std::system(cmdAdd.c_str()) != 0) {
            std::cerr << "[Error] Failed to add files to git.\n"; 
            return 1;
        }
        
        // We ignore the commit return value because it returns non-zero if there are no new changes to commit
        std::system(cmdCommit.c_str()); 
        
        if (std::system(cmdPush.c_str()) != 0) {
            std::cerr << "[Error] Failed to push to remote repository.\n"; 
            return 1;
        }

        std::cout << "===========================================\n";
        std::cout << "✅ Success! Theme pushed to GitHub.\n";
        std::cout << "Go to the spm-themes repository and open a Pull Request to trigger the Edge Deployment.\n";
        std::cout << "===========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "[Error] Manifest parse failed: " << e.what() << "\n";
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
    else if (command == "dev") {
        std::string manifestPath = "";
        for (int i = 2; i < argc; i++) {
            if (std::string(argv[i]) == "-d" && i + 1 < argc) {
                manifestPath = argv[i + 1];
                break;
            }
        }
        return runDevServer(manifestPath);
    } 
    else if (command == "publish") {
        return runPublish();
    } 
    else {
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}