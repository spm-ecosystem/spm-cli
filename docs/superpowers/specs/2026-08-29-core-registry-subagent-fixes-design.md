# Core Registry & Subagent Pipeline Fixes Design

**Date:** 2026-08-29
**Author:** Antigravity AI Assistant & Engineering Team
**Target Repositories:** `spm-cli`, `spm-veneer-coder`, `spm-components` (`extension`)

---

## 1. Executive Summary

This design addresses four critical ecosystem gaps identified during recent technical auditing:
1. **Gap 0.1 / Item 1 (`spm-veneer-coder`)**: `scripts/subagent_cli.py` ignores semantic validation (`validate_vnr_semantics`) and component schema RAG grounding (`get_grounding_prompt`).
2. **Gap 0.2 / Item 2 (`spm-cli`)**: `ComponentSchemaRegistry` (`src/veneer/component_registry.hpp`) has manually written, inaccurate prop schemas for 10 out of 17 components (e.g. `UiImageCard` missing `linkUrl`, `UiSearchBar` missing `queryParamName`, `UiModernGridPage` missing mobile responsive props).
3. **Gap 9 (`spm-veneer-coder`)**: Empty `css_code` block in subagent outputs is never validated or retried.
4. **Gap 10 (`spm-components`)**: `scripts/build-registry-safe.js` points to legacy folder path `vscode-theme-manifest-intellisense` instead of `spm-vscode`.

---

## 2. Component Schema Registry Generator & Audit (`spm-cli`)

### 2.1 Python Generator Script (`scripts/generate_component_registry.py`)
A standalone Python script will parse the TypeScript prop interface definitions (`export interface UiXxxProps { ... }`) from `/home/watashi/Projects/extension/src/components/` (or relative path to `spm-components`) and output a C++ header file `src/veneer/component_registry.hpp`.

**Prop extraction rules:**
- Parse interface definitions matching `export interface Ui[A-Za-z0-9]+Props` and `export interface LayoutPrimitiveProps` / `UiBoxProps` etc.
- Extract field names (ignoring trailing `?`, `: type`, comments, and function props like `onClick`).
- Always include common React / SPM universal props: `className`, `style`, `id`, `children`, `key`.
- Output `src/veneer/component_registry.hpp` with exact `std::unordered_map<std::string, std::unordered_set<std::string>>` mappings.

### 2.2 Component Schema Matrix (Target State)
The generated schema will cover all 17 dedicated components and 8 primitives:
- **`UiImageCard`**: `title`, `imageUrl`, `linkUrl`, `aspectRatio`, `showTitle`, `loading`, `imageFit`, `id`, `width`, `className`, `style`
- **`UiSearchBar`**: `placeholder`, `submitUrl`, `queryParamName`, `defaultValue`, `method`, `hiddenFields`, `className`, `style`
- **`UiPostDetails`**: `tagGroups`, `statisticsHtml`, `buttons`, `showSearch`, `searchPlaceholder`, `searchSubmitUrl`, `searchParamName`, `className`, `style`
- **`UiDashboardPage`**: `pageTitle`, `subTitle`, `cards`, `height`, `className`, `style`
- **`UiStatsDashboard`**: `pageTitle`, `dateRangeText`, `navLinks`, `sections`, `height`, `className`, `style`
- **`UiImageViewer`**: `src`, `alt`, `title`, `fit`, `background`, `className`, `style`
- **`UiModernGridPage`**: `pageTitle`, `items`, `tagGroups`, `pageLinks`, `showSearch`, `searchPlaceholder`, `searchSubmitUrl`, `sidebarHtml`, `mobileColumns`, `mobileBreakpoint`, `hideSidebarOnMobile`, `height`, `className`, `style`
- **`UiCommentListPage`**: `pageTitle`, `threads`, `pageLinks`, `height`, `className`, `style`
- **`UiSplitLayout`**: `imageSlot`, `tags`, `buttons`, `statisticsHtml`, `sidebarWidth`, `sidebarSide`, `imageFit`, `height`, `splitButtons`, `showSearch`, `searchPlaceholder`, `searchSubmitUrl`, `searchParamName`, `mainHtml`, `className`, `style`
- **`UiToast`**: `message`, `type`, `onClose`, `className`, `style`
- *(Plus all remaining components: `UiNavHeader`, `UiHeroLanding`, `UiTableListPage`, `UiPaginationBar`, `UiScrollPanel`, `UiTable`, `UiTagBadge`, and Primitives)*

### 2.3 Unit Testing (`test_registry.cpp`)
Update `src/veneer/test_registry.cpp` to test real prop validations:
- Verify `isValidProp("UiImageCard", "linkUrl")` returns `true`.
- Verify `isValidProp("UiImageCard", "url")` returns `false` and `getDidYouMean("UiImageCard", "url")` returns `"linkUrl"`.
- Verify `isValidProp("UiSearchBar", "queryParamName")` returns `true`.
- Verify `isValidProp("UiSearchBar", "paramName")` returns `false` and `getDidYouMean` returns `"queryParamName"`.

---

## 3. Subagent CLI Pipeline Enhancements (`spm-veneer-coder`)

### 3.1 `subagent_cli.py` Workflow Updates
Modify `run_subagent_flow()` in `scripts/subagent_cli.py`:
1. **Schema Grounding**: Call `get_grounding_prompt(task_content)` from `veneer_coder.schema` and prepend/append component reference schemas to `base_prompt`.
2. **Semantic Validation**: In the retry loop, call `validate_vnr_semantics(vnr_code, sample_html=html_content)` instead of basic `compile_vnr(vnr_code)`.
3. **CSS Block Validation**: If `vnr_code` is valid but `css_code` is empty or missing, treat as a retryable error and append a warning to `current_prompt`:
   ```python
   if not css_code and "customStyles" in vnr_code:
       # Trigger retry asking for content.css block
   ```

---

## 4. Path Fix in `spm-components` (`extension`)

### 4.1 `scripts/build-registry-safe.js` Path Alignment
Update line 7 of `scripts/build-registry-safe.js` in `spm-components`:
- Change `../../vscode-theme-manifest-intellisense/scripts/build-registry.js` to `../../spm-vscode/scripts/build-registry.js`.
- Add secondary fallback check for sibling repo directory structure.

---

## 5. Verification Plan

1. **`spm-cli`**:
   - Run `python3 scripts/generate_component_registry.py`.
   - Build via CMake: `cmake -B build && cmake --build build`.
   - Run test binary: `./build/test_registry`. All tests must pass.
2. **`spm-veneer-coder`**:
   - Run unit tests: `pytest tests/`.
   - Test `subagent_cli.py` with mock payload; confirm `get_grounding_prompt` and `validate_vnr_semantics` are invoked.
3. **`spm-components`**:
   - Verify `scripts/build-registry-safe.js` resolves path cleanly when `spm-vscode` is present.
