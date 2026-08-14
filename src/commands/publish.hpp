#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <algorithm>
#include "../utils/fs_utils.hpp"
#include "../utils/css_bundler.hpp"

namespace fs = std::filesystem;

namespace veneer {

inline int runPublish(const std::string& manifestPath = "manifest.json") {
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

        std::string cmdClone = "git clone --depth 1 --filter=blob:none --sparse " + targetRepoUrl + " " + tempWorkspace.string() + " -q";
        if (std::system(cmdClone.c_str()) != 0) {
            std::cerr << "[Error] Failed to connect to repository. Check your git permissions.\n";
            return 1;
        }

        std::string cmdCheckout = "git -C " + tempWorkspace.string() + " checkout -b " + branchName + " -q";
        std::system(cmdCheckout.c_str());

        fs::path targetFolder = tempWorkspace / targetUrl / themeName;
        fs::create_directories(targetFolder);

        std::cout << "[Publish] Packaging theme files...\n";
        for (const auto& entry : fs::directory_iterator(currentDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                fs::copy(entry.path(), targetFolder / entry.path().filename(), fs::copy_options::overwrite_existing);
            }
        }

        std::string bundledCss = veneer::bundleCssFiles(currentDir);
        if (!bundledCss.empty()) {
            std::ofstream cssOut(targetFolder / "content.css");
            if (cssOut.is_open()) {
                cssOut << bundledCss;
                cssOut.close();
            } else {
                std::cerr << "[Warning] Failed to write bundled content.css\n";
            }
        }

        for (const auto& entry : fs::recursive_directory_iterator(currentDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".vnr") {
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
        std::cout << "===========================================\n"
                  << "✅ Success! Theme pushed to GitHub securely.\n"
                  << "Go to https://github.com/spm-ecosystem/spm-websites/pulls\n"
                  << "Open a Pull Request to trigger the Edge Deployment.\n"
                  << "===========================================\n";
    } catch (const std::exception& e) {
        std::cerr << "[Error] Process failed: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

} // namespace veneer
