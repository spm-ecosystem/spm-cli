# spm-cli

A high-performance C++ CLI compiler, watcher, WebSocket hot-reloader, and developer utility for Site Package Manager (SPM) theme layouts.

---

## Short Description
`spm-cli` is the core layout compilation engine and development server for the Site Package Manager. It compiles modular Veneer Spec (`.vnr`) files into unified layout `manifest.json` structures, provides contextual class resolution, and broadcasts style updates over WebSockets for instant visual hot-reloading in the browser.

---

## Key Features

1.  **Veneer Spec Compiler**: Parses custom declarative DSL `.vnr` files, resolves strict class inheritance graphs, parses raw string blocks, and emits target schema-compliant `manifest.json` layout profiles.
2.  **WebSocket Dev Server (`spm dev`)**: Monitors directory files, detects modifications, and broadcasts style/manifest payloads to connected browser extension instances for real-time styling updates without reloading.
3.  **Sibling Class Auto-Loader**: Resolves dependencies dynamically during single-file compile passes (e.g. background VS Code validation) by pulling in sibling class declarations automatically.
4.  **Deep Layout Merging**: Merges compiled theme blocks with preexisting target metadata (like author, version, and url pattern rules) to safeguard GitOps values across compile runs.

---

## Build & Installation

### Requirements
- C++17 compatible compiler (e.g. GCC 8+ or Clang 7+)
- CMake 3.12+
- Make

### Compiling from Source
Navigate to the repository root directory and build:
```bash
# Generate build configuration
cmake .

# Compile target binary
make
```

The compiled binary will be generated as `spm` in the root of the workspace directory.

---

## Usage Guide

### 1. Compile Theme Directory
Combines all `.vnr` files in a workspace folder recursively and outputs the target manifest JSON:
```bash
./spm compile vnr_project/ -o vnr_project/manifest.json
```

### 2. Compile Single Spec File
Compiles an isolated spec file, autoloading class blueprints from sibling files to check inheritance:
```bash
./spm compile vnr_project/pages.vnr -o /tmp/pages.json
```

### 3. Launch Dev Watcher & WebSocket Server
Starts a local server on `ws://localhost:8080` to watch a target theme directory and broadcast updates:
```bash
./spm dev /path/to/theme/project/
```

---

## Suggested GitHub Topic Tags
`cplusplus` | `compiler-dsl` | `websocket-server` | `chrome-extension-helper` | `theme-modernization` | `development-tool`

---

## License

This project is licensed under the MIT License - see the [LICENSE](file:///home/watashi/Projects/spm-cli/LICENSE) file for details.
