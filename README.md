# spm-cli

[![Documentation Portal](https://img.shields.io/badge/docs-spm--portal-blue?style=for-the-badge)](https://spm-ecosystem.github.io/spm-portal/)

A high-performance C++ compiler, file watcher, WebSocket dev server, and utility engine for Site Package Manager (SPM) theme layout configurations.

---

## What is spm-cli?
`spm-cli` is the core compilation engine of the Site Package Manager (SPM) ecosystem. It takes modular Veneer Spec configuration scripts and transforms them into standardized layout manifests. During development, it serves as a background linter and live hot-reloader, syncing design revisions directly to the browser extension.

---

## Deep Dive: The Veneer Spec Compiler

The primary purpose of `spm-cli` is to act as the compiler for **Veneer Spec**, a declarative layout definition DSL built specifically to modernize legacy website architectures.

### Why Veneer Spec?
Writing raw JSON manifests for complex page overrides is error-prone, verbose, and lacks key developer ergonomics (like inheritance, clean regex definitions, and syntax validation). Veneer Spec solves these issues by providing a structured, readable syntax:

```scss
class PrimaryLink {
    bind label: "self | text";
    bind url: "self | attr:href";
}

reconstruct "#gallery" -> UiGridPage {
    urlPattern: R"(example\.com\/?(?:index\.html)?$)";
    child items extends PrimaryLink {
        selector: "#gallery-view .item";
    }
}
```

### The Compilation Pipeline
`spm-cli` processes Veneer source code through four primary compilation phases:

1.  **Lexing & Tokenization (`lexer.hpp`)**:
    Scans Veneer source characters, strips comments, and produces tokens. It supports **Raw String Literals** (`R"delim(content)delim"`), allowing developers to write regular expressions or inline JSON structures without escaping backslashes.
2.  **AST Generation (`parser.hpp`)**:
    A recursive-descent parser that consumes tokens and builds a strongly typed Abstract Syntax Tree (AST) representing classes, theme variables, selectors, and reconstruct blocks.
3.  **Blueprinting & Class Resolution (`resolver.hpp`)**:
    Processes class declarations (`class` and `extends`). It builds an inheritance graph, propagates properties and scraping bindings from base classes down to subclasses, and dynamically checks for inheritance anomalies (such as circular dependencies).
4.  **Serialization & Emission (`emitter.hpp`)**:
    Translates the resolved AST into the target layout schema manifest format. It executes dynamic JSON parsing checks, ensuring valid JSON strings (numbers, booleans, arrays, or objects) are written as native types. It also performs a **deep merge** with preexisting target manifest metadata to preserve author, version, and matching url values across compilation runs.

---

## Workspace Features

### Recursive Directory Compilation
`spm-cli` compiles modular workspaces by recursively scanning directories for `.vnr` files. Developers can organize layouts into Java-style nested packages (e.g. `core/models/`, `layout/headers/`, `pages/gallery/`). The compiler traverses all subfolders recursively and unifies the definitions in a single compilation pass.

### WebSocket Dev Watcher
When run in dev mode (`spm dev`), `spm-cli` spawns an asynchronous file watcher and a WebSocket server on port `8080`. It monitors both layout files and style override files (`content.css`). When a modification is detected, it broadcasts a unified sync payload over WebSockets to the SPM browser extension, allowing instant style hot-reloading in real-time.

---

## Build & Installation

### Requirements
- C++17 compatible compiler (e.g. GCC 8+ or Clang 7+)
- CMake 3.12+
- Make

### Compilation
```bash
# Generate build configuration
cmake .

# Compile target binary
make
```

The compiled binary will be generated as `spm` in the root of the workspace.

---

## Usage Guide

### 1. Compile Theme Directory
```bash
./spm compile vnr_project/ -o vnr_project/manifest.json
```

### 2. Compile Single Spec File (Linter Mode)
```bash
./spm compile vnr_project/pages.vnr -o /tmp/pages.json
```

### 3. Launch WebSocket Dev Server
```bash
./spm dev /path/to/theme/project/
```

---

## Documentation

- [🌐 Interactive Documentation Portal](https://spm-ecosystem.github.io/spm-portal/)
- [Veneer Spec Syntax Guide](https://github.com/spm-ecosystem/spm-components/blob/main/docs/veneer-reference.md)
- [Theme Manifest Schema Reference](https://github.com/spm-ecosystem/spm-components/blob/main/docs/manifest-schema.md)

---

## License

This project is licensed under the MIT License - see the [LICENSE](./LICENSE) file for details.
