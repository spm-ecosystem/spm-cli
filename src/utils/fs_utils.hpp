#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace veneer {

inline std::string readTextFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

inline std::string readManifestFile(const std::string& filepath) {
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

inline int installToPath(const std::string& currentExePath) {
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

} // namespace veneer
