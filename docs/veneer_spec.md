# Veneer Spec Language Reference Manual

The Veneer Spec (`.vnr`) configuration language is a custom declarative Domain Specific Language (DSL) built for the Site Package Manager (SPM) compiler. 

---

## 1. Introduction & Rationale

Traditionally, layout overrides and DOM modernization mappings in SPM were written directly as raw JSON files (`manifest.json`). However, as configuration complexity scales (with hundreds of properties, nested loops, data scraping selectors, and layout variables), writing raw JSON becomes extremely verbose, difficult to validate at write-time, and error-prone due to character escaping.

Veneer Spec solves these issues by providing:
1.  **Strong Typing and Semantic Syntax**: Structural keywords instead of generic JSON key-value pairs.
2.  **Object-Oriented blue-printing (`class` / `extends`)**: Allows creating base layouts and inheriting selectors/bindings, removing duplication (DRY configuration).
3.  **Raw String Literal Blocks**: Bypasses backslash escaping for regexes and pure JSON lists/tables.
4.  **Static Validation and Compiler Diagnostics**: Resolves inheritance paths, detects circular dependencies, checks type compatibility, and reports exact syntax error lines before compiling.

---

## 2. Core Concepts & Mental Model

### The Layout Override Paradigm
SPM works by intercepting the legacy site's HTML, hiding targeted sections, and injecting modern React views. The Veneer DSL defines **what** parts of the page to target, **which** React component to mount, and **how** to extract unstructured data from the legacy DOM to populate the React component's props.

### Scraping Mappings & Bindings
A key concept of the Veneer Spec is the declarative binding of HTML nodes to component props using the extraction syntax:

$$\text{Selector} \quad | \quad \text{BaseExtractor} \quad [ \ | \ \text{Pipe} \ ]^*$$

The Veneer engine evaluates this query at runtime against the page structure, scraping text content, attributes, or raw HTML, and then running any subsequent pipeline operations sequentially.

#### 1. Base Extractors
*   `text` - Extracts the `textContent` of the matched element.
*   `html` - Extracts the `innerHTML` of the matched element.
*   `attr:<name>` - Extracts the specified attribute value (e.g. `attr:src`, `attr:href`).
*   `hrefOrOnclick` - Automatically resolves link destination from `href` or fallback inline `onclick` assignment.
*   `nextSiblingText` - Extracts the text content of the immediate next sibling element.
*   `hiddenInputs` - Collects all `<input type="hidden">` tags within the element as a JSON array string.
*   `selector` - Generates a unique selector string for the element.

#### 2. Pipe Operations (Sequential Processing)
Pipes can be chained together sequentially using the `|` character.
*   `split` - Splits a space-separated text string into a JSON stringified array of tokens (e.g. `"tag1 tag2"` -> `["tag1", "tag2"]`).
*   `split:<delimiter>` - Splits a string by a custom delimiter and trims each token (e.g. `split:,` turns `"tag1, tag2"` -> `["tag1", "tag2"]`).
*   `number` - Converts a valid number string directly into a JSON numeric value.
*   `cleanNumber` - Strips currency symbols (`$`, `R$`, `€`), commas, and spacing, then parses the value into a JSON float number (e.g. `"$ 1,200.50"` -> `1200.5`).

---

## 3. Keyword-by-Keyword Reference

### `theme`
The `theme` block defines the metadata, visual design tokens (CSS custom properties), and raw CSS stylesheet modifications injected into the global document scope.

*   **Role**: Groups styling tokens and global page overrides under a single visual label.
*   **Rules**:
    *   Only one `theme` block is allowed per compiled project.
    *   Variables are defined inside the `variables` sub-block and compile to the manifest's `"cssVariables"`.
    *   Custom stylesheet rules are defined under `customStyles: "<raw-css>";`.
*   **Syntax**:
    ```scss
    theme "ModernDark" {
        variables {
            --spm-accent: "#7c6af5";
            --spm-bg-primary: "#000000";
        }
        customStyles: "#advertisement-banner { display: none !important; }";
    }
    ```
*   **Compiled Output**:
    ```json
    "theme": {
      "label": "ModernDark",
      "cssVariables": {
        "--spm-accent": "#7c6af5",
        "--spm-bg-primary": "#000000"
      },
      "customStyles": "#advertisement-banner { display: none !important; }"
    }
    ```

---

### `class` & `extends`
Classes act as blueprints defining reusable data extraction fields and scopes.

*   **Role**: Simplifies the declaration of repetitive structures (like list item cards, buttons, or navigation links) by letting child nodes inherit and override properties.
*   **Rules**:
    *   Classes are resolved at compile-time and are completely omitted from the final JSON output (zero runtime cost).
    *   A class can inherit properties from a parent class using the `extends` keyword.
    *   If a property or binding is declared in both the child and parent, the child's value overrides the parent's.
*   **Syntax**:
    ```scss
    class LinkBase {
        bind label: "self | text";
        bind url: "self | attr:href";
    }

    class DocumentLink extends LinkBase {
        scope: "document";
    }
    ```

---

### `selector`
A `selector` block targets an individual legacy element in the DOM to replace it with an isolated React component or hide it entirely.

*   **Role**: Alters targeted, individual legacy elements (like headers, sidebars, or search bars) without replacing the whole page grid.
*   **Rules**:
    *   Must specify a target string representing the CSS selector of the legacy node.
    *   Can map directly to a component using the arrow syntax: `selector "#element" -> UiComponent`.
    *   Must contain an `action` key (either `hide` or `replace`).
*   **Syntax**:
    ```scss
    selector "#sub-navbar" {
        action: hide;
    }

    selector "#search-input" -> UiSearchBar {
        action: replace;
        placeholder: "Search…";
        bind defaultValue: "input[name='q'] | attr:value";
    }
    ```
*   **Compiled Output**:
    ```json
    "components": [
      {
        "selector": "#sub-navbar",
        "action": "hide"
      },
      {
        "selector": "#search-input",
        "name": "UiSearchBar",
        "action": "replace",
        "props": {
          "placeholder": "Search…"
        },
        "propsMap": {
          "defaultValue": "input[name='q'] | attr:value"
        }
      }
    ]
    ```

---

### `reconstruct`
Transforms a large page container (like a full catalog feed, comment board, or landing page) into a modern React view mounted inside an isolated Shadow DOM host.

*   **Role**: Performs full-viewport page overrides.
*   **Rules**:
    *   Targets a container using a CSS selector (which gets its legacy children hidden at injection).
    *   Maps to a React layout component using the arrow syntax: `reconstruct "#container" -> LayoutComponent`.
    *   Can configure constraints like `urlPattern` or `mediaQuery` so the layout only mounts on specific pages or device breakpoints.
*   **Syntax**:
    ```scss
    reconstruct "#gallery" -> UiGridPage {
        urlPattern: "page=gallery";
        pageTitle: "Catalog Gallery";
    }
    ```
*   **Compiled Output**:
    ```json
    "reconstructs": [
      {
        "containerSelector": "#gallery",
        "layoutComponent": "UiGridPage",
        "urlPattern": "page=gallery",
        "props": {
          "pageTitle": "Catalog Gallery"
        }
      }
    ]
    ```

---

### `child`
Defines a nested data array scraped from matching legacy elements inside the page.

*   **Role**: Creates list arrays (like item grids, comments, tags, or nav lists) and sends them to the parent React layout as a prop array.
*   **Rules**:
    *   Declares a name that maps to the prop array key on the layout component (e.g. `child items` defines the `items` prop array).
    *   Can extend a class to inherit pre-configured bindings.
    *   Must specify a `selector` indicating which elements inside the container represent the list items.
*   **Syntax**:
    ```scss
    reconstruct "#gallery" -> UiGridPage {
        child items {
            selector: "#gallery .item-card";
            bind id: "self | attr:id";
            bind imageUrl: "img | attr:src";
        }
    }
    ```
*   **Compiled Output**:
    ```json
    "reconstructs": [
      {
        "containerSelector": "#gallery",
        "layoutComponent": "UiGridPage",
        "children": [
          {
            "name": "items",
            "selector": "#gallery .item-card",
            "propsMap": {
              "id": "self | attr:id",
              "imageUrl": "img | attr:src"
            }
          }
        ]
      }
    ]
    ```

---

### `bind`
Sets up dynamic scraping instructions. 

*   **Role**: Tells the engine how to extract a property value from the matched element's DOM at runtime.
*   **Rules**:
    *   Follows the pattern `bind <prop-name>: "<selector> | <operation>";`.
    *   Compiles into the `"propsMap"` object of the target manifest item.
*   **Syntax**:
    ```scss
    bind title: "h2 | text";
    bind logoUrl: "img.brand | attr:src";
    ```
*   **Compiled Output**:
    ```json
    "propsMap": {
      "title": "h2 | text",
      "logoUrl": "img.brand | attr:src"
    }
    ```

---

### `preserve`
Prevents specific interactive elements (like a legacy comment form or complex sidebar) from being hidden, reparenting them inside dedicated slot templates in the modern React Shadow DOM.

*   **Role**: Links legacy interactive DOM structures into the new modern React layout without breaking their event handlers, cookies, or states.
*   **Rules**:
    *   Maps a React layout slot name (e.g. `sidebarSlot`) to the legacy element CSS selector (e.g. `.sidebar`).
    *   Compiles into the `"preserve"` block of the target manifest reconstruct.
*   **Syntax**:
    ```scss
    reconstruct "#item-view" -> UiItemDetailsPage {
        preserve {
            sidebarSlot: ".sidebar";
        }
    }
    ```
*   **Compiled Output**:
    ```json
    "reconstructs": [
      {
        "containerSelector": "#item-view",
        "layoutComponent": "UiItemDetailsPage",
        "preserve": {
          "sidebarSlot": ".sidebar"
        }
      }
    ]
    ```

---

### `scope`
Configures the boundary limits of the CSS selector query.

*   **Role**: Tells the runtime engine whether it should search for elements only within the container element's boundary (`scope: "container"`) or search the entire page (`scope: "document"`).
*   **Rules**:
    *   The default scope is `"container"` (meaning selectors inside child nodes only query descendants of the parent reconstruct container).
    *   Setting `scope: "document";` is useful for items like global pagination elements or secondary search bars located outside the main layout container.
    *   If `"container"` is configured, the compiler omits the key in the compiled output to keep the JSON clean.
*   **Syntax**:
    ```scss
    child pageLinks {
        scope: "document";
        selector: "#paginator .pagination a";
    }
    ```
*   **Compiled Output**:
    ```json
    "children": [
      {
        "name": "pageLinks",
        "selector": "#paginator .pagination a",
        "scope": "document"
      }
    ]
    ```

---

## 4. Delimiters & Type Rules

### Raw String Literals: `R"delim(content)delim"`
In standard string literal syntax, special characters like backslashes (`\`) or double quotes (`"`) must be escaped (e.g. `"\\w+"` or `"\"value\""`). This makes regex rules and complex JSON configurations difficult to read.

Veneer Spec supports C++ style **Raw String Literals**, which treat everything inside `R"delim(...)delim"` as a raw, unescaped string.
*   **Usage**: Recommended for compiling regular expressions (like `urlPattern`) and inline JSON blocks (like table `columns` or array data sets).
*   **Syntax**:
    ```scss
    urlPattern: R"(example\.com\/?(?:index\.html)?$)";
    
    columns: R"([
      { "key": "id", "header": "ID", "width": "50px" },
      { "key": "name", "header": "Item Title", "type": "text" }
    ])";
    ```

### Implicit JSON Type Deserialization
When emitting properties to the manifest JSON, the compiler automatically runs a deserialization check on all values:
- If a value represents a valid JSON type (a number `3`, a boolean `true`, an array `[...]`, or an object `{...}`), it parses and emits it as a **native JSON type** instead of a string.
- If it fails parsing (like `"280px"`), it is emitted as a standard string.

---

## 5. Advanced Workspace Features

### Workspace directory compilation & Package structures
When executing `spm compile <directory_path> -o manifest.json`, the compiler recursively searches all `.vnr` files under the target path, concatenates their source contents, and resolves class blueprints globally. 

*   **Arbitrary File Naming**: File names are completely arbitrary; they do not need to follow standard names like `navigation.vnr` or `classes.vnr`. The compiler will read and parse any file ending with the `.vnr` extension.
*   **Nested Package Structures (Java-style)**: You are not restricted to keeping files in a single flat directory. You can organize your theme configuration into deeply nested folders and subpackages (e.g. `core/models/blueprints.vnr`, `layout/headers/top_nav.vnr`, `pages/gallery/grid_layout.vnr`). The compiler traverses all subfolders recursively, merging all declarations into a single global compilation context. This allows developers to build large-scale, modular theme packages.


### Sibling Class Autoloading
When compiling a single file (like `pages.vnr`), the compiler automatically inspects its directory context. If it detects that a class is referenced as a base but not declared in the current file, it loads and parses sibling `.vnr` files in the background to import their class blueprints. This resolves the classes automatically, allowing isolated background validation for syntax linters.

---

## 6. Complete Theme: Agnostic Example

The following is a modular layout configuration for a generic catalog page.

### `classes.vnr`
```scss
class StandardLink {
    bind label: "self | text";
    bind url: "self | attr:href";
}

class DocumentLink extends StandardLink {
    scope: "document";
}

class TagItem {
    scope: "document";
    bind name: "a:last-of-type | text";
    bind count: "span.count-badge | text";
    bind type: "self | attr:class";
    bind url: "a:last-of-type | attr:href";
}
```

### `theme.vnr`
```scss
theme "ModernDark" {
    variables {
        --spm-accent: "#7c6af5";
        --spm-accent-fg: "#ffffff";
        --spm-accent-hover: "#9d8fff";
        --spm-bg-primary: "#000000";
        --spm-bg-secondary: "#111111";
        --spm-bg-tertiary: "#222222";
        --spm-border: "#333333";
        --spm-radius: "10px";
        --spm-text-muted: "#a1a1aa";
        --spm-text-primary: "#ffffff";
    }
    customStyles: "#system-banner, #cookie-consent-bar { display: none !important; }";
}
```

### `navigation.vnr`
```scss
selector "#header-container, #navbar, header" -> UiNavHeader {
    action: replace;
    className: "site-navigation-header";
    logoHref: "https://example.com/";
    
    primaryLinks: R"([
      { "label": "My Account", "url": "https://example.com/account" },
      { "label": "Items", "url": "https://example.com/items?action=list" },
      { "label": "Comments", "url": "https://example.com/comments" },
      { "label": "Wiki Pages", "url": "https://example.com/wiki" },
      { "label": "Statistics", "url": "https://example.com/stats" },
      { "label": "Help Desk", "url": "https://example.com/help" }
    ])";

    secondaryLinks: R"([
      { "label": "Upload Item", "url": "https://example.com/items/upload" },
      { "label": "My Favorites", "url": "https://example.com/favorites" },
      { "label": "Random Item", "url": "https://example.com/items/random" },
      { "label": "Contact Us", "url": "https://example.com/contact" },
      { "label": "Terms of Service", "url": "https://example.com/tos" }
    ])";

    bind logoUrl: "#site-logo img | attr:src";
    bind siteName: "#site-logo a | text";
}

selector "#sub-navbar" {
    action: hide;
}

selector "#sidebar-search form, .search-container form" -> UiSearchBar {
    action: replace;
    placeholder: "Search items…";
    submitUrl: "https://example.com/items";
    queryParamName: "q";
    bind defaultValue: "input[name='q'] | attr:value";
}
```

### `pages.vnr`
```scss
reconstruct "#home-landing" -> UiHeroLanding {
    urlPattern: R"(example\.com\/?(?:index\.html)?$)";
    tagline: "The Modern Search Engine";
    subtext: "Browse millions of cataloged resources, updated in real time.";
    ctaLabel: "Browse Catalog";
    ctaUrl: "https://example.com/items?action=list";
    searchPlaceholder: "Search catalog... (e.g. category:news keyword)";
    searchSubmitUrl: "https://example.com/items";
    searchParamName: "q";

    bind logoUrl: "img[alt='Company Logo'] | attr:src";
    bind siteName: "img[alt='Company Logo'] | attr:alt";

    child primaryLinks extends StandardLink {
        selector: "#quick-links a";
    }
}

reconstruct "#gallery-view" -> UiGridPage {
    urlPattern: "page=gallery";
    pageTitle: "Catalog Gallery";
    className: "modern-grid-gallery";
    height: "calc(100vh - 80px)";
    sidebarWidth: "260px";
    showSearch: true;
    searchPlaceholder: "Search items…";
    searchSubmitUrl: "https://example.com/items";
    searchParamName: "q";
    mobileColumns: 2;
    mobileGap: "8px";
    mobilePadding: "8px";
    mobileShowHeader: true;
    mobileHeaderSticky: true;
    mobileShowPagination: true;
    mobileCardAspectRatio: "1 / 1.28";
    hideSidebarOnMobile: true;
    mobileBreakpoint: 720;
    
    tagGroups: R"([
      { "title": "Categories", "typeKey": "category" },
      { "title": "Tags", "typeKey": "tag" },
      { "title": "Creators", "typeKey": "creator" },
      { "title": "System Data", "typeKey": "metadata" }
    ])";

    bind searchDefaultValue: ".sidebar-filter form input[name='q'] | attr:value";

    child items {
        selector: "#gallery-view .item-card";
        bind id: "self | attr:id";
        bind imageUrl: "img | attr:src";
        bind linkUrl: "a | attr:href";
        bind title: "img | attr:title";
    }

    child tags extends TagItem {
        selector: "#sidebar-tags li";
    }

    child pageLinks extends StandardLink {
        selector: "#paginator .pagination a";
    }
}
```
