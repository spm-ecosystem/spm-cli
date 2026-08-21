# Site Package Manager (SPM) - CLI Tooling & Pipeline Manual (`cli-tooling.md`)

The `spm-cli` C++17 binary acts as the official compiler and runtime development tools orchestrator for Site Package Manager (`spm`).

---

## 1. CLI Commands & Subcommands

### A. Development Mode (`spm dev`)
Starts an in-memory compiler and file watcher server with live hot-reloading for extension themes.
```bash
spm dev
```
- Monitors `.vnr` source files inside `vnr_project/` subdirectories.
- Automatically compiles `.vnr` changes into dynamic payloads and notifies the running browser extension to hot-reload Shadow DOM styles and component mounts.

### B. Project Compilation (`spm compile`)
Compiles a Veneer Spec (`.vnr`) project folder into a standalone, production-ready `manifest.json`.
```bash
spm compile [path/to/vnr_project]
```
- Lexes, parses, resolves class inheritance, and emits formatted JSON.
- Deep-merges preexisting metadata (`author`, `description`, `targetUrl`, `version`) into the output `manifest.json`.

### C. Selector Validation (`spm validate`)
Validates compiled manifest selectors and data extraction pipes against local HTML page snapshots offline.
```bash
spm validate <manifest.json> --against <snapshot.html> [--json]
```
- Parses HTML snapshots using a JSDOM-equivalent layout engine.
- Matches CSS selectors for all components and reconstructs.
- Runs data extraction pipes (`text`, `attr`, `split`, `number`, `cleanNumber`) and validates that expected properties extract successfully.
- Outputs detailed pass/fail status reports, or clean JSON metadata if `--json` is supplied.

### D. Transformation Application (`spm apply`)
Applies compiled manifest transformations to a local HTML snapshot and saves the modernized layout result.
```bash
spm apply <manifest.json> --input <input.html> -o <output.html>
```
- Implements layout replacements, components reconstruction, and element hiding.
- Injects theme CSS variables and custom styles directly in the `<head>` style block.
- Outputs the finalized modern HTML file for visual verification.

---

## 2. Recommended Directory Layout for Themes

```text
site-domain/
└── theme-name/
    ├── manifest.json              # Auto-compiled production output
    ├── content.css                # Global CSS overrides
    └── vnr_project/               # Veneer Spec source folder
        ├── theme.vnr              # Theme tokens & custom styles
        ├── classes.vnr            # Scraping blueprints
        ├── navigation.vnr         # Header reconstructs
        └── pages.vnr              # Feed and page reconstructs
```
