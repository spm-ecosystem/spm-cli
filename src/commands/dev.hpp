#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXConnectionState.h>
#include "../utils/fs_utils.hpp"
#include "../utils/css_bundler.hpp"
#include "../veneer/lexer.hpp"
#include "../veneer/parser.hpp"
#include "../veneer/resolver.hpp"
#include "../veneer/emitter.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace veneer {

inline std::string g_manifestPath = "";
inline std::mutex g_manifestMutex;

inline std::string buildDevPayload(const std::string& manifestPath) {
    fs::path p(manifestPath);
    std::string manifestContent;
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
    } else {
        manifestContent = readTextFile(manifestPath);
        if (manifestContent.empty()) return "";
    }

    fs::path themeDir = fs::is_directory(p) ? p : p.parent_path();
    std::string cssContent = veneer::bundleCssFiles(themeDir);

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

inline int runDevServer(const std::string& initialManifestPath) {
    {
        std::lock_guard<std::mutex> lock(g_manifestMutex);
        g_manifestPath = initialManifestPath;
    }

    int port = 8080;
    ix::WebSocketServer server(port, "0.0.0.0");

    std::cout << "===========================================\n"
              << "SPM Dev Server - ws://localhost:" << port << "\n";
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
                      } catch (const std::exception& e) {}
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

  } // namespace veneer
