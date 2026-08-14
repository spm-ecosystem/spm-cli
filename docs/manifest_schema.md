# Site Package Manager (SPM) - Theme Manifest Schema

The `manifest.json` file is the compiled output configuration generated from Veneer Spec (`.vnr`) sources. It is evaluated at runtime by the SPM content script to reconstruct legacy HTML elements or layout sections with React components.

---

## Root Fields

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `targetUrl` | `string` | Yes | Matching URL glob pattern indicating which domains this theme should activate on (e.g. `*://example.com/*`). |
| `version` | `string` | No | Version of the theme package (e.g. `"1.0.0"`). |
| `minEngineVersion` | `string` | No | Minimum compatible version of the SPM extension engine (e.g. `"1.0.0"`). |
| `theme` | `object` | Yes | Defines global styling variables, custom styles overrides, and descriptive metadata. |
| `components` | `array` | No | List of selector overrides that target, hide, or replace individual legacy elements. |
| `reconstructs` | `array` | No | List of page-level section replacements that mount React layout components inside isolated Shadow DOM hosts. |

---

## 1. Theme Configuration

Defines styling variables (custom property mappings) and custom CSS rules injected globally.

```json
"theme": {
  "label": "Obsidian",
  "author": "spm-ecosystem",
  "description": "Auto-versioning Obsidian dark theme on pipeline",
  "cssVariables": {
    "--spm-bg-primary": "#000000",
    "--spm-bg-secondary": "#111111",
    "--spm-bg-tertiary": "#222222",
    "--spm-text-primary": "#ffffff",
    "--spm-text-muted": "#a1a1aa",
    "--spm-accent": "#7c6af5",
    "--spm-accent-fg": "#ffffff",
    "--spm-accent-hover": "#9d8fff",
    "--spm-border": "#333333",
    "--spm-radius": "10px"
  },
  "customStyles": "#notice, #long-notice { display: none !important; }",
  "noticeSelector": "#has-mail-notice"
}
```

### Properties
*   `label` (string): Human-readable name of the theme shown in the extension popup.
*   `author` (string): Developer signature.
*   `description` (string): Short explanation of theme styling goals.
*   `cssVariables` (object): Map of custom CSS property names to valid CSS color, spacing, or sizing values. Components should exclusively rely on these variables.
*   `customStyles` (string): Raw CSS stylesheet injected globally into the main document to hide ads, banners, or apply global page layout overrides.
*   `noticeSelector` (string): Optional selector targeting notices that should trigger extension banner overlays.

---

## 2. Components Configuration

Replaces or hides targeted elements individually.

```json
"components": [
  {
    "name": "UiSearchBar",
    "selector": ".sidebar form",
    "action": "replace",
    "props": {
      "placeholder": "Search…",
      "submitUrl": "https://example.com/search",
      "queryParamName": "q"
    },
    "propsMap": {
      "defaultValue": "input[name='q'] | attr:value"
    }
  },
  {
    "name": "UiNavHeader",
    "selector": "#subnavbar",
    "action": "hide"
  }
]
```

### Properties
*   `name` (string, required): React component name matching an entry in the React registry.
*   `selector` (string, required): Standard CSS selector targeting the legacy DOM element.
*   `action` (string, required): Action to perform. Supported:
    *   `"replace"`: Hides the element and mounts the React component in its place.
    *   `"hide"`: Sets `display: none !important` on the selector to remove it visually.
*   `props` (object): Static prop values passed directly to the React component.
*   `propsMap` (object): Dynamic prop mappings extracted live from the DOM at injection time (see *Prop Mapping Rules* below).

---

## 3. Reconstructs Configuration

Reconstructs entire sections or full page contents with isolated React structures in the Shadow DOM.

```json
"reconstructs": [
  {
    "containerSelector": "#gallery",
    "layoutComponent": "UiGridPage",
    "urlPattern": "page=gallery",
    "props": {
      "pageTitle": "Gallery",
      "mobileColumns": 2,
      "mobileGap": "8px"
    },
    "propsMap": {
      "searchDefaultValue": ".sidebar form input[name='q'] | attr:value"
    },
    "preserve": {
      "sidebarSlot": ".sidebar"
    },
    "children": [
      {
        "name": "items",
        "selector": "#gallery .item",
        "propsMap": {
          "id": "self | attr:id",
          "imageUrl": "img | attr:src",
          "linkUrl": "a | attr:href",
          "title": "img | attr:title"
        }
      }
    ]
  }
]
```

### Properties
*   `containerSelector` (string, required): CSS selector of the container element that will be replaced. The element will have its children hidden, and a Shadow DOM root host appended.
*   `layoutComponent` (string, required): Name of the React component that acts as the container layout.
*   `urlPattern` (string): JavaScript regular expression matched against the current page URL path/query (e.g. `page=gallery`). The reconstruct will only load if the pattern matches.
*   `props` (object): Static prop values passed directly to the layout component.
*   `propsMap` (object): Dynamic properties extracted from the container element's DOM before it is hidden.
*   `preserve` (object): Map of slot names to selectors. Targeted nodes are removed from the legacy page before hiding the container and reparented inside the Shadow DOM (into elements having `id="{slotName}-container"`).
*   `children` (array): Array of nested data extractions. Each child queries matching elements and generates an array of objects passed as a prop array to the layout component.
    *   `name`: Prop array name on the React component.
    *   `selector`: CSS selector targeting matching children.
    *   `scope`: (Optional) `"document"` to run queries from the root document instead of relative to the container element.
    *   `propsMap`: Data extraction rules for child item fields.
    *   `children`: Recursive nested child array definitions for complex layouts (e.g. threads containing comments).

---

## 4. Prop Mapping Rules

Dynamic extractions extract content from matching DOM nodes and bind it to props. Extractions follow the format:

$$\text{Selector} \quad | \quad \text{Operation}$$

| Rule Syntax | Targeted Element | Result |
| :--- | :--- | :--- |
| `<css-selector> \| text` | First matching descendant | Text content (`textContent`) of the node. |
| `<css-selector> \| html` | First matching descendant | Inner HTML (`innerHTML`) of the node. |
| `<css-selector> \| attr:<name>` | First matching descendant | Value of the specified attribute (e.g. `attr:src` for images). |
| `self \| text` | Targeted element itself | Text content of the targeted element itself. |
| `self \| html` | Targeted element itself | Inner HTML of the targeted element itself. |
| `self \| attr:<name>` | Targeted element itself | Value of the specified attribute of the targeted element itself. |
| `self \| hrefOrOnclick` | Targeted element itself | Resolves `href` value, or extracts navigation destination from an inline `onclick` handler if `href` is empty. |
| `<css-selector> \| nextSiblingText` | Matching descendant | Extracts text content from the sibling node immediately following the matched element. |

---

## 5. Metadata Merging

During compilation via `spm compile`, the CLI automatically parses any preexisting target `manifest.json` file. It performs a **deep merge** on the `"theme"` block:
- Global properties like `author`, `description`, `targetUrl`, and `minEngineVersion` present in the destination JSON file are **preserved** and merged back into the compilation output.
- This ensures GitOps metadata is maintained across compilations without manual restoration.
