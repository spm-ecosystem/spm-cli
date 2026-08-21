# Veneer Spec — Extended Language Reference & Example Cookbook

> **Scope of this document**: This is an expanded, example-driven companion to the official
> [`veneer_spec.md`](https://github.com/spm-ecosystem/spm-cli/blob/main/docs/veneer_spec.md) and
> [`manifest_schema.md`](https://github.com/spm-ecosystem/spm-cli/blob/main/docs/manifest_schema.md)
> references shipped in `spm-cli`. It keeps every rule from the official docs intact and adds a
> much larger set of worked examples, edge cases, anti-patterns, and full real-world theme
> walkthroughs for the Veneer Spec (`.vnr`) DSL compiled by `spm-cli`.
>
> **Important note on scope**: This file documents the **Veneer Spec language** itself (lexing,
> parsing, class resolution, emission — everything `spm-cli` compiles) in depth, since that is
> what is described in the `spm-cli` repository docs. It does **not** invent prop schemas for
> individual React components (`UiGridPage`, `UiNavHeader`, `UiSearchBar`, etc.) beyond what is
> already demonstrated in `spm-cli`'s own docs — the authoritative prop lists for those live in
> `spm-components/docs`, which could not be crawled directly (GitHub blocks automated access to
> that repository's file-tree view, and the docs aren't otherwise indexed). Section 12
> ("Known Components — Observed Contract") lists everything that can be safely inferred from the
> `spm-cli` examples, marked accordingly. If you paste the contents of `spm-components/docs`
> (or raw file links) into the conversation, this document can be extended with the exact,
> per-component prop tables and hundreds of additional binding examples.

---

## Table of Contents

1. [Mental Model](#1-mental-model)
2. [Lexer & Token Reference](#2-lexer--token-reference)
3. [Extraction Syntax Deep Dive](#3-extraction-syntax-deep-dive)
4. [`theme` — Exhaustive Examples](#4-theme--exhaustive-examples)
5. [`class` / `extends` — Exhaustive Examples](#5-class--extends--exhaustive-examples)
6. [`selector` — Exhaustive Examples](#6-selector--exhaustive-examples)
7. [`reconstruct` — Exhaustive Examples](#7-reconstruct--exhaustive-examples)
8. [`child` — Exhaustive Examples](#8-child--exhaustive-examples)
9. [`bind` — Exhaustive Examples](#9-bind--exhaustive-examples)
10. [`preserve` — Exhaustive Examples](#10-preserve--exhaustive-examples)
11. [`scope` — Exhaustive Examples](#11-scope--exhaustive-examples)
12. [Known Components — Observed Contract](#12-known-components--observed-contract)
13. [Raw String Literals — Pattern Library](#13-raw-string-literals--pattern-library)
14. [Implicit JSON Type Deserialization — Exhaustive Cases](#14-implicit-json-type-deserialization--exhaustive-cases)
15. [Workspace / Multi-file Compilation Examples](#15-workspace--multi-file-compilation-examples)
16. [Full Worked Themes](#16-full-worked-themes)
17. [Common Errors, Anti-Patterns & Fixes](#17-common-errors-anti-patterns--fixes)
18. [CLI Recipes](#18-cli-recipes)
19. [Cheat Sheet](#19-cheat-sheet)

---

## 1. Mental Model

Veneer Spec never touches the DOM itself — it is a **compiler** that turns `.vnr` source into a
`manifest.json` file. That manifest is what the SPM browser-extension runtime reads to:

1. Find legacy elements (`selector`, `reconstruct`).
2. Decide what to do with them (`hide` vs `replace`).
3. Mount a React component in their place, inside an isolated Shadow DOM host.
4. Populate that component's props — statically (plain keys) or dynamically (`bind`, scraped at
   injection time from the *original* legacy DOM before it's hidden).

Everything else in the language (`class`/`extends`, `scope`, raw strings, implicit JSON typing)
exists purely to make steps 1–4 easier to author and validate.

```
 .vnr source files  ──lexer──▶ tokens ──parser──▶ AST ──resolver──▶ resolved AST ──emitter──▶ manifest.json
                                                (class inheritance,
                                                 circular-dep checks)
```

---

## 2. Lexer & Token Reference

| Token category   | Examples                                              | Notes |
|-------------------|-------------------------------------------------------|-------|
| Keywords          | `theme`, `class`, `extends`, `selector`, `reconstruct`, `child`, `bind`, `preserve`, `scope`, `variables`, `customStyles` | Case-sensitive, lowercase only. |
| Identifiers       | `PrimaryLink`, `UiGridPage`, `items`, `pageLinks`     | Used for class names, component names, child/prop names. |
| Arrow operator    | `->`                                                  | Links a `selector`/`reconstruct` target to a component name. |
| String literal    | `"Search…"`, `"#gallery"`                              | Standard double-quoted string; supports normal escaping (`\"`, `\\`). |
| Raw string literal| `R"(...)"`, `` R"delim(...)delim" ``                    | See [Section 13](#13-raw-string-literals--pattern-library). |
| Property line     | `key: value;`                                         | Every property assignment must terminate with `;`. |
| Block braces      | `{ }`                                                 | Delimit `theme`, `class`, `selector`, `reconstruct`, `child`, `preserve`, `variables` bodies. |
| Comments          | `// line comment`, `/* block comment */`               | Stripped during lexing; never appear in the emitted manifest. |

### 2.1 Comment examples

```vnr
// This whole file configures the primary navigation
selector "#navbar" -> UiNavHeader {
    action: replace; // swap the legacy header entirely
    /*
      className below must match a CSS class already
      shipped by the UiNavHeader stylesheet bundle
    */
    className: "site-navigation-header";
}
```

### 2.2 Whitespace & formatting

Veneer Spec is whitespace-insensitive between tokens. The following two snippets compile to an
identical AST:

```vnr
selector "#sidebar" { action: hide; }
```

```vnr
selector
    "#sidebar"
{
    action
        :
        hide
    ;
}
```

Idiomatic formatting (used throughout this document) is: one property per line, 4-space indent,
opening brace on the same line as the block keyword.

---

## 3. Extraction Syntax Deep Dive

The general shape of any dynamic extraction is:

```
"<selector-or-self> | <base-extractor> [ | <pipe> ]*"
```

### 3.1 Base extractor examples (one per extractor)

```vnr
bind title:        "h2.item-title | text";
bind descriptionHtml: ".item-body | html";
bind thumbnail:     "img.cover | attr:src";
bind altText:       "img.cover | attr:alt";
bind ctaUrl:        "a.buy-now | hrefOrOnclick";
bind priceLabel:    ".price | nextSiblingText";
bind formFields:    "form#checkout | hiddenInputs";
bind ownSelector:   "self | selector";
```

### 3.2 `self` vs explicit selector

`self` always refers to the element that was already matched by the enclosing `selector`,
`reconstruct`, or `child` block — it never re-queries the DOM.

```vnr
class ExternalLink {
    // "self" = the anchor tag matched by the child's own selector
    bind label: "self | text";
    bind url:   "self | attr:href";
}

class CardWithBadge {
    // relative selectors query *inside* the matched card element
    bind title: "h3.card-title | text";
    bind badge: "span.badge | text";
    // "self" still refers to the card element itself, e.g. for its own id
    bind cardId: "self | attr:data-id";
}
```

### 3.3 Chaining multiple pipes

```vnr
bind tagList:      "self | attr:data-tags | split";
bind tagListCsv:    "self | attr:data-tags | split:,";
bind price:         ".price-tag | text | cleanNumber";
bind stock:         ".stock-count | text | number";
bind categoryPath:  ".breadcrumb | text | split:›";
```

### 3.4 `split` examples

```vnr
// "featured bestseller limited" -> ["featured","bestseller","limited"]
bind badges: "self | attr:data-flags | split";

// "Electronics, Computers, Laptops" -> ["Electronics","Computers","Laptops"]
bind categories: ".breadcrumbs | text | split:,";

// "red|blue|green" -> ["red","blue","green"]
bind swatches: ".variant-colors | attr:data-colors | split:|";

// "2024-06-01/2024-06-15" -> ["2024-06-01","2024-06-15"]
bind dateRange: ".availability | attr:data-range | split:/";
```

### 3.5 `number` vs `cleanNumber`

```vnr
// "42" -> 42 (native JSON number)
bind reviewCount: ".review-count | text | number";

// "$ 1,200.50" -> 1200.5
bind price: ".price | text | cleanNumber";

// "R$ 89,90" -> 89.9  (Brazilian Real formatting also stripped)
bind priceBr: ".preco | text | cleanNumber";

// "€349" -> 349
bind priceEu: ".price-eur | text | cleanNumber";

// "1 234,56 kr" -> depends on compiler locale rules; prefer cleanNumber only
// for currency-adjacent values, use "number" for plain integers/decimals
bind quantity: ".qty-input | attr:value | number";
```

**Rule of thumb**: use `number` for already-clean numeric strings (quantities, IDs, ratings), and
`cleanNumber` whenever a currency symbol, thousands separator, or surrounding whitespace might be
present.

### 3.6 `hrefOrOnclick` examples

Legacy sites frequently wire navigation through `onclick="location.href='...'"` instead of a real
`href`. `hrefOrOnclick` normalizes both cases into a single destination string.

```vnr
// <a href="/item/42">View</a>
bind detailUrl: "a.view-link | hrefOrOnclick"; // -> "/item/42"

// <a onclick="window.location='/item/42'">View</a>
bind detailUrl: "a.view-link | hrefOrOnclick"; // -> "/item/42"

// <button onclick="document.location.href='/cart/add?id=7'">Add</button>
bind addToCartUrl: "button.add | hrefOrOnclick";
```

### 3.7 `hiddenInputs` examples

Useful for preserving CSRF tokens or hidden form state when a form is being replaced by a React
component but still needs to submit compatible payloads server-side.

```vnr
selector "#login-form" -> UiLoginForm {
    action: replace;
    bind csrfFields: "self | hiddenInputs";
    // -> '[{"name":"csrf_token","value":"9f2a..."},{"name":"redirect","value":"/home"}]'
}
```

### 3.8 `selector` (self-referential) examples

Occasionally a component needs to know the *unique selector string* of the element it was mounted
on, e.g. for analytics or for a "scroll back to here" feature.

```vnr
child items {
    selector: "#results .result-row";
    bind anchorSelector: "self | selector";
}
```

---

## 4. `theme` — Exhaustive Examples

### 4.1 Minimal theme

```vnr
theme "Minimal" {
    variables {
        --spm-accent: "#2563eb";
    }
}
```

### 4.2 Full design-token palette

```vnr
theme "Slate" {
    variables {
        --spm-bg-primary: "#0b0f14";
        --spm-bg-secondary: "#121820";
        --spm-bg-tertiary: "#1b232d";
        --spm-text-primary: "#f5f7fa";
        --spm-text-muted: "#8b98a5";
        --spm-accent: "#38bdf8";
        --spm-accent-fg: "#03131c";
        --spm-accent-hover: "#7dd3fc";
        --spm-border: "#233040";
        --spm-radius: "8px";
        --spm-radius-lg: "16px";
        --spm-shadow-sm: "0 1px 2px rgba(0,0,0,0.4)";
        --spm-font-sans: "'Inter', system-ui, sans-serif";
        --spm-font-mono: "'JetBrains Mono', monospace";
        --spm-spacing-1: "4px";
        --spm-spacing-2: "8px";
        --spm-spacing-3: "16px";
        --spm-spacing-4: "24px";
    }
    customStyles {
        "#legacy-ad-slot, #newsletter-modal, .cookie-banner { display: none !important; }"
    }
}
```

### 4.3 Theme with only global CSS overrides (no custom variables)

```vnr
theme "Bare" {
    variables {}
    customStyles {
        R"(
        body { overflow-x: hidden !important; }
        #legacy-footer-ads { display: none !important; }
        .popup-overlay { display: none !important; }
        )"
    }
}
```

### 4.4 Theme hiding multiple ad/tracking regions at once

```vnr
theme "AdFree" {
    variables {
        --spm-accent: "#16a34a";
    }
    customStyles {
        R"(
        #top-leaderboard-ad,
        #sidebar-ad-300x250,
        #interstitial-ad,
        .sponsored-listing,
        [id^="google_ads_"] { display: none !important; }
        )"
    }
}
```

### 4.5 Light/dark-ready token set (values chosen so the same class names work under either scheme)

```vnr
theme "AdaptiveContrast" {
    variables {
        --spm-bg-primary: "#ffffff";
        --spm-bg-secondary: "#f4f5f7";
        --spm-text-primary: "#111318";
        --spm-text-muted: "#5b6472";
        --spm-accent: "#7c3aed";
        --spm-accent-fg: "#ffffff";
        --spm-border: "#e2e4e9";
        --spm-radius: "12px";
    }
    customStyles {
        ""
    }
}
```

> **Note**: exactly one `theme` block is allowed per compiled project (per the base
> `spm-cli` docs). Declaring a second `theme` block anywhere in the workspace — even in a
> different `.vnr` file — is a compile-time error.

---

## 5. `class` / `extends` — Exhaustive Examples

### 5.1 Base link class + two specializations

```vnr
class BaseLink {
    bind label: "self | text";
    bind url: "self | attr:href";
}

class DocumentScopedLink extends BaseLink {
    scope: "document";
}

class ExternalLink extends BaseLink {
    bind isExternal: "self | attr:data-external | number";
}
```

### 5.2 Two-level inheritance chain

```vnr
class Interactive {
    bind isDisabled: "self | attr:disabled";
}

class Clickable extends Interactive {
    bind url: "self | attr:href";
}

class TrackedClickable extends Clickable {
    bind trackingId: "self | attr:data-track-id";
}
```

`TrackedClickable` resolves, at compile time, to the union of all three classes' bindings:
`isDisabled`, `url`, `trackingId`.

### 5.3 Overriding a parent binding

```vnr
class GenericCard {
    bind title: ".title | text";
    bind image: "img | attr:src";
}

class LazyLoadedCard extends GenericCard {
    // overrides GenericCard's "image" binding to read the lazy-load attribute instead
    bind image: "img | attr:data-src";
}
```

Compiled `LazyLoadedCard` usage keeps `title` from the parent and uses the child's `image` rule.

### 5.4 Class used purely for scope, no bindings of its own

```vnr
class GlobalPaginationLink {
    scope: "document";
    bind label: "self | text";
    bind url: "self | attr:href";
    bind isCurrent: "self | attr:aria-current";
}
```

### 5.5 Sibling classes referencing each other's shape (not inheritance, just consistent fields)

```vnr
class CommentAuthor {
    bind name: ".author-name | text";
    bind avatarUrl: ".author-avatar img | attr:src";
    bind profileUrl: ".author-name a | attr:href";
}

class ForumPostAuthor {
    bind name: ".poster-name | text";
    bind avatarUrl: ".poster-avatar img | attr:src";
    bind profileUrl: ".poster-name a | attr:href";
    bind postCount: ".poster-stats .posts | text | number";
    bind joinDate: ".poster-stats .joined | text";
}
```

### 5.6 Real usage — extending a class inside a `child` block

```vnr
reconstruct "#thread" -> UiThreadPage {
    child replies extends CommentAuthor {
        selector: ".reply";
        bind body: ".reply-body | html";
        bind postedAt: ".reply-meta time | attr:datetime";
    }
}
```

Note that `child` can both extend a class **and** declare its own additional `bind` lines — the
final `propsMap` is the merge of inherited + locally declared bindings, with local bindings
winning on conflicts (same override rule as class-to-class inheritance).

### 5.7 Circular dependency (compile error — shown for reference)

```vnr
// ❌ This will fail to compile: "circular inheritance detected: A -> B -> A"
class A extends B {
    bind x: "self | text";
}

class B extends A {
    bind y: "self | text";
}
```

### 5.8 Referencing an undeclared class in the same file (compile error)

```vnr
// ❌ Fails unless `MissingBase` is declared somewhere in this file or a sibling .vnr
// file in the same directory (see Sibling Class Autoloading, §15.3)
class Derived extends MissingBase {
    bind z: "self | text";
}
```

---

## 6. `selector` — Exhaustive Examples

### 6.1 Simple hide

```vnr
selector ".newsletter-signup-banner" {
    action: hide;
}
```

### 6.2 Hide with a comma-separated multi-target selector

```vnr
selector "#top-banner, .promo-strip, .site-notice" {
    action: hide;
}
```

### 6.3 Replace with only static props

```vnr
selector ".search-box" -> UiSearchBar {
    action: replace;
    placeholder: "Search the catalog…";
    submitUrl: "https://example.com/search";
    queryParamName: "q";
}
```

### 6.4 Replace with a mix of static props and `bind`

```vnr
selector "#account-widget" -> UiAccountMenu {
    action: replace;
    loginUrl: "https://example.com/login";
    logoutUrl: "https://example.com/logout";
    bind isLoggedIn: "self | attr:data-authenticated | number";
    bind username: ".account-name | text";
    bind avatarUrl: ".account-avatar img | attr:src";
}
```

### 6.5 Multiple independent `selector` blocks in one file

```vnr
selector "#top-nav" -> UiNavHeader {
    action: replace;
    logoHref: "https://example.com/";
}

selector "#footer-links" -> UiFooter {
    action: replace;
    bind columns: "self | attr:data-footer-json";
}

selector ".legacy-breadcrumbs" {
    action: hide;
}
```

### 6.6 Component with a large static JSON array prop, via raw strings

```vnr
selector "#quick-nav" -> UiQuickNav {
    action: replace;
    items: R"([
      { "icon": "home", "label": "Home", "url": "/" },
      { "icon": "search", "label": "Browse", "url": "/browse" },
      { "icon": "user", "label": "Account", "url": "/account" },
      { "icon": "cart", "label": "Cart", "url": "/cart" }
    ])";
}
```

### 6.7 Search bar with regex-validated submit target (illustrative)

```vnr
selector "#hero-search form" -> UiSearchBar {
    action: replace;
    placeholder: "Try 'wireless headphones'";
    submitUrl: "https://example.com/s";
    queryParamName: "query";
    bind defaultValue: "input[name='query'] | attr:value";
}
```

### 6.8 `selector` inside a nested subdirectory file (`layout/headers/top_nav.vnr`)

```vnr
// layout/headers/top_nav.vnr
selector "header.site-header" -> UiNavHeader {
    action: replace;
    className: "modernized-header";
    bind logoUrl: ".brand img | attr:src";

    child navLinks extends BaseLink {
        selector: "nav.primary a";
    }
}
```

---

## 7. `reconstruct` — Exhaustive Examples

### 7.1 Minimal reconstruct (no constraints)

```vnr
reconstruct "#app-root" -> UiHomePage {
    pageTitle: "Home";
}
```

### 7.2 Reconstruct constrained by `urlPattern` (plain substring)

```vnr
reconstruct "#listings" -> UiGridPage {
    urlPattern: "type=listing";
    pageTitle: "All Listings";
}
```

### 7.3 Reconstruct constrained by `urlPattern` (regex via raw string)

```vnr
reconstruct "#home" -> UiHeroLanding {
    urlPattern: R"(^https?:\/\/example\.com\/?(?:\?.*)?$)";
    tagline: "Find anything. Instantly.";
}
```

### 7.4 Reconstruct with `mediaQuery` gating (mobile-only mount)

```vnr
reconstruct "#mobile-nav-drawer" -> UiMobileDrawer {
    mediaQuery: "(max-width: 768px)";
    pageTitle: "Menu";
}
```

### 7.5 Reconstruct with static + dynamic props + one `child`

```vnr
reconstruct "#directory" -> UiDirectoryPage {
    urlPattern: "page=directory";
    pageTitle: "Business Directory";
    resultsPerPage: 24;
    showMap: true;

    bind searchDefaultValue: ".directory-search input[name='q'] | attr:value";

    child entries {
        selector: ".directory-entry";
        bind name: ".entry-name | text";
        bind category: ".entry-category | text";
        bind phone: ".entry-phone | text";
        bind url: ".entry-name a | attr:href";
    }
}
```

### 7.6 Reconstruct with multiple `child` blocks

```vnr
reconstruct "#forum-index" -> UiForumIndexPage {
    urlPattern: "board=index";
    pageTitle: "Forum";

    child categories {
        selector: ".forum-category";
        bind name: ".category-title | text";
        bind description: ".category-desc | text";
        bind topicCount: ".category-stats .topics | text | number";
        bind postCount: ".category-stats .posts | text | number";
    }

    child announcements {
        scope: "document";
        selector: "#sitewide-announcements li";
        bind title: "a | text";
        bind url: "a | attr:href";
    }
}
```

### 7.7 Reconstruct + `preserve` + `child` together (full combination)

```vnr
reconstruct "#item-detail" -> UiItemDetailsPage {
    urlPattern: R"(\/item\/\d+)";
    pageTitle: "Item Details";

    bind title: "h1.item-title | text";
    bind description: ".item-description | html";
    bind price: ".item-price | text | cleanNumber";
    bind imageUrl: ".item-gallery img.main | attr:src";

    preserve {
        commentsSlot: "#legacy-comments-widget";
        purchaseFormSlot: "#legacy-buy-box form";
    }

    child gallery {
        selector: ".item-gallery .thumb";
        bind imageUrl: "img | attr:src";
        bind altText: "img | attr:alt";
    }

    child specifications {
        selector: ".spec-table tr";
        bind label: "td:first-child | text";
        bind value: "td:last-child | text";
    }
}
```

### 7.8 Reconstruct with nested (recursive) children — comment threads

```vnr
reconstruct "#thread-view" -> UiThreadPage {
    urlPattern: "thread=";
    pageTitle: "Discussion Thread";

    child topLevelComments extends CommentAuthor {
        selector: ".comment.depth-0";
        bind body: ".comment-body | html";
        bind commentId: "self | attr:data-comment-id";

        child replies extends CommentAuthor {
            selector: ".comment.depth-1";
            bind body: ".comment-body | html";
            bind commentId: "self | attr:data-comment-id";
        }
    }
}
```

### 7.9 Reconstruct targeting several possible container selectors

```vnr
reconstruct "#results-grid, .search-results-container" -> UiGridPage {
    urlPattern: "q=";
    pageTitle: "Search Results";
}
```

### 7.10 Two reconstructs in the same file, gated by mutually-exclusive URL patterns

```vnr
reconstruct "#catalog" -> UiGridPage {
    urlPattern: R"(\/browse\/?$)";
    pageTitle: "Browse Catalog";
}

reconstruct "#catalog" -> UiListPage {
    urlPattern: R"(\/browse\?view=list)";
    pageTitle: "Browse Catalog (List View)";
}
```

---

## 8. `child` — Exhaustive Examples

### 8.1 Minimal child list

```vnr
child items {
    selector: ".item-card";
    bind title: ".title | text";
}
```

### 8.2 Child list with several bindings

```vnr
child products {
    selector: ".product-tile";
    bind id: "self | attr:data-product-id";
    bind title: ".product-name | text";
    bind price: ".product-price | text | cleanNumber";
    bind imageUrl: "img.product-image | attr:src";
    bind inStock: ".stock-badge | attr:data-in-stock | number";
    bind url: "a.product-link | attr:href";
}
```

### 8.3 Child extending a class, with local overrides

```vnr
child footerLinks extends BaseLink {
    selector: "#footer nav a";
    // adds a field not present on BaseLink
    bind section: "self | attr:data-section";
}
```

### 8.4 Child with `scope: "document"` for elements outside the reconstruct container

```vnr
child paginationLinks extends BaseLink {
    scope: "document";
    selector: "#pager .page-link";
}
```

### 8.5 Nested (recursive) child — category tree, 3 levels deep

```vnr
child rootCategories {
    selector: ".category-tree > .category-node";
    bind name: "> .category-label | text";
    bind slug: "self | attr:data-slug";

    child subCategories {
        selector: "> .category-children > .category-node";
        bind name: "> .category-label | text";
        bind slug: "self | attr:data-slug";

        child leafCategories {
            selector: "> .category-children > .category-node";
            bind name: "> .category-label | text";
            bind slug: "self | attr:data-slug";
        }
    }
}
```

### 8.6 Multiple sibling `child` blocks with different purposes inside one `reconstruct`

```vnr
reconstruct "#dashboard" -> UiDashboardPage {
    child metrics {
        selector: ".metric-tile";
        bind label: ".metric-label | text";
        bind value: ".metric-value | text | cleanNumber";
        bind trend: ".metric-trend | attr:data-trend";
    }

    child recentActivity {
        selector: ".activity-row";
        bind actor: ".activity-actor | text";
        bind action: ".activity-action | text";
        bind timestamp: ".activity-time | attr:datetime";
    }

    child quickLinks extends BaseLink {
        selector: ".quick-link";
    }
}
```

### 8.7 Child list scraping a data table (rows -> objects)

```vnr
child tableRows {
    selector: "table#report-table tbody tr";
    bind rank: "td:nth-child(1) | text | number";
    bind name: "td:nth-child(2) | text";
    bind score: "td:nth-child(3) | text | cleanNumber";
    bind change: "td:nth-child(4) | text";
}
```

### 8.8 Child list combining a `bind` and a nested `child` (item + its tags)

```vnr
child articles {
    selector: ".article-summary";
    bind headline: "h2 | text";
    bind excerpt: ".excerpt | text";
    bind publishedAt: "time | attr:datetime";
    bind url: "h2 a | attr:href";

    child tags {
        selector: ".article-tags a";
        bind label: "self | text";
        bind url: "self | attr:href";
    }
}
```

---

## 9. `bind` — Exhaustive Examples

### 9.1 Text content

```vnr
bind title: "h1 | text";
bind subtitle: "h2.subtitle | text";
bind footerNote: "#legal-disclaimer | text";
```

### 9.2 HTML content (rich text preserved)

```vnr
bind bodyHtml: ".post-content | html";
bind termsHtml: "#terms-block | html";
```

### 9.3 Attributes

```vnr
bind imageUrl: "img.hero | attr:src";
bind videoUrl: "video source | attr:src";
bind ariaLabel: "button.close | attr:aria-label";
bind dataId: "self | attr:data-id";
bind lang: "html | attr:lang";
```

### 9.4 Combined pipes for cleaned numeric data

```vnr
bind rating: ".star-rating | attr:data-rating | number";
bind reviewCount: ".review-count | text | cleanNumber";
bind discountPercent: ".discount-badge | text | cleanNumber";
```

### 9.5 Combined pipes for arrays

```vnr
bind sizes: ".size-options | attr:data-sizes | split:,";
bind colorSwatches: ".color-options | attr:data-colors | split:|";
bind keywords: "meta[name='keywords'] | attr:content | split:,";
```

### 9.6 Binding against `self` at every applicable level

```vnr
selector "#promo-banner" -> UiPromoBanner {
    action: replace;
    bind headline: "self | attr:data-headline";
    bind ctaUrl: "self | hrefOrOnclick";
}

reconstruct "#landing" -> UiHeroLanding {
    bind backgroundImage: "self | attr:data-bg";
}

child items {
    selector: ".item";
    bind id: "self | attr:id";
}
```

### 9.7 Multiple `bind` lines targeting the same descendant selector for different purposes

```vnr
bind thumbnailUrl: "img.cover | attr:src";
bind thumbnailAlt: "img.cover | attr:alt";
bind thumbnailWidth: "img.cover | attr:width | number";
```

### 9.8 `bind` used for form pre-fill values

```vnr
selector "#filter-form" -> UiFilterPanel {
    action: replace;
    bind selectedCategory: "select[name='category'] | attr:value";
    bind minPrice: "input[name='min_price'] | attr:value | number";
    bind maxPrice: "input[name='max_price'] | attr:value | number";
    bind inStockOnly: "input[name='in_stock'] | attr:checked";
}
```

---

## 10. `preserve` — Exhaustive Examples

### 10.1 Preserving a single legacy widget

```vnr
reconstruct "#item-detail" -> UiItemDetailsPage {
    preserve {
        reviewsSlot: "#legacy-reviews-widget";
    }
}
```

### 10.2 Preserving several widgets at once

```vnr
reconstruct "#checkout" -> UiCheckoutPage {
    preserve {
        paymentFormSlot: "#legacy-payment-iframe";
        couponWidgetSlot: ".coupon-code-box";
        shippingCalculatorSlot: "#shipping-estimator";
    }
}
```

### 10.3 Preserving a third-party embedded widget (chat, live support)

```vnr
reconstruct "#support-center" -> UiSupportPage {
    preserve {
        liveChatSlot: "#zendesk-chat-widget";
    }
}
```

### 10.4 Preserving a legacy comment form while reconstructing the whole thread

```vnr
reconstruct "#thread" -> UiThreadPage {
    preserve {
        newCommentFormSlot: "#legacy-comment-form";
    }

    child comments extends CommentAuthor {
        selector: ".comment";
        bind body: ".comment-body | html";
    }
}
```

> Per the manifest schema, the runtime looks for a host element with
> `id="{slotName}-container"` inside the React layout to reparent the preserved node into —
> layout components must declare matching containers for every slot name used in `preserve`.

---

## 11. `scope` — Exhaustive Examples

### 11.1 Default (implicit) container scope — no `scope` key needed

```vnr
reconstruct "#gallery" -> UiGridPage {
    child items {
        // implicitly scoped to descendants of "#gallery"
        selector: ".item-card";
    }
}
```

### 11.2 Explicit `document` scope for elements physically outside the container

```vnr
reconstruct "#gallery" -> UiGridPage {
    child pagination extends BaseLink {
        scope: "document";
        selector: "#global-pager a";
    }
}
```

### 11.3 Mixed scoping within the same `reconstruct`

```vnr
reconstruct "#results" -> UiGridPage {
    child items {
        // scoped to "#results" (default)
        selector: ".result-card";
    }

    child filters {
        // scoped to the whole document, since filters live in the sidebar,
        // outside of #results
        scope: "document";
        selector: "#sidebar-filters .filter-option";
        bind label: "self | text";
        bind value: "self | attr:data-value";
    }
}
```

### 11.4 `scope` on a class, inherited by every child that extends it

```vnr
class GlobalNavLink {
    scope: "document";
    bind label: "self | text";
    bind url: "self | attr:href";
}

reconstruct "#page" -> UiPage {
    child topNav extends GlobalNavLink {
        selector: "#site-header nav a"; // resolved from document root, not #page
    }
}
```

### 11.5 Why `scope: "container"` is never emitted

```vnr
child items {
    scope: "container"; // explicit, but redundant — this is already the default
    selector: ".item";
}
```

Compiled output omits the `scope` key entirely for this block (identical output to §11.1),
because `"container"` is the default and the compiler strips it to keep the manifest minimal.

---

## 12. Known Components — Observed Contract

The following components and prop names are used as illustrative examples across the official
`spm-cli` docs. They are documented here **only to the extent they appear in those examples** —
treat this as a partial, "as observed" reference, not an exhaustive spec. The authoritative,
complete prop tables for every component ship in `spm-components/docs`.

| Component | Observed static props | Observed dynamic (`bind`) props | Observed `child` usage |
|---|---|---|---|
| `UiNavHeader` | `className`, `logoHref`, `primaryLinks` (JSON array), `secondaryLinks` (JSON array) | `logoUrl`, `siteName` | link lists via extended classes |
| `UiSearchBar` | `placeholder`, `submitUrl`, `queryParamName` | `defaultValue` | — |
| `UiGridPage` | `pageTitle`, `className`, `height`, `sidebarWidth`, `showSearch`, `searchPlaceholder`, `searchSubmitUrl`, `searchParamName`, `mobileColumns`, `mobileGap`, `mobilePadding`, `mobileShowHeader`, `mobileHeaderSticky`, `mobileShowPagination`, `mobileCardAspectRatio`, `hideSidebarOnMobile`, `mobileBreakpoint`, `tagGroups` (JSON array) | `searchDefaultValue` | `items`, `tags` (extends a tag class), `pageLinks` (extends a link class) |
| `UiHeroLanding` | `tagline`, `subtext`, `ctaLabel`, `ctaUrl`, `searchPlaceholder`, `searchSubmitUrl`, `searchParamName` | `logoUrl`, `siteName` | `primaryLinks` (extends a link class) |
| `UiItemDetailsPage` | `pageTitle` (implied) | `title`, `description`, `price`, `imageUrl` (implied by `preserve` example) | `preserve.sidebarSlot`, gallery/spec children |

All numeric-looking static values (`mobileColumns: 2`, `mobileBreakpoint: 720`) are emitted as
native JSON numbers per the implicit type deserialization rules; all boolean-looking values
(`showSearch: true`) are emitted as native JSON booleans.

**To extend this table with the full, authoritative component catalog** (every prop each
component accepts, its type, whether it's required, and default values), share the contents of
`spm-ecosystem/spm-components/docs` — either pasted directly or as individual raw file URLs
(e.g. `https://raw.githubusercontent.com/spm-ecosystem/spm-components/main/docs/UiGridPage.md`).
Once available, this section can be rewritten into one full sub-section per component, each with
a complete prop table and dedicated `.vnr` examples exercising every prop.

---

## 13. Raw String Literals — Pattern Library

Raw strings (`R"(...)"`) exist so regexes and inline JSON never need backslash-escaping. The
delimiter defaults to nothing (`R"(...)"`) but can be customized (`` R"delim(...)delim" ``) if the
content itself contains the sequence `)"`.

### 13.1 URL pattern regexes

```vnr
// Root/home page only, with or without trailing slash or index.html
urlPattern: R"(example\.com\/?(?:index\.html)?$)";

// Any path under /blog/
urlPattern: R"(\/blog\/.+)";

// A specific numeric item route
urlPattern: R"(\/item\/\d+\/?$)";

// Query string contains page=gallery, in any position
urlPattern: R"([?&]page=gallery(&|$))";

// Exclude admin subpaths while matching everything else under /shop
urlPattern: R"(\/shop\/(?!admin).*)";

// Multiple TLDs for the same brand
urlPattern: R"(example\.(com|co\.uk|de)\/)";
```

### 13.2 Inline JSON arrays for static props

```vnr
primaryLinks: R"([
  { "label": "Home", "url": "https://example.com/" },
  { "label": "Shop", "url": "https://example.com/shop" },
  { "label": "About", "url": "https://example.com/about" }
])";
```

### 13.3 Inline JSON objects with nested structures

```vnr
tagGroups: R"([
  { "title": "Categories", "typeKey": "category" },
  { "title": "Brands", "typeKey": "brand", "collapsedByDefault": true },
  { "title": "Price Range", "typeKey": "price", "renderAs": "slider" }
])";
```

### 13.4 Table column definitions (common in dashboard/grid components)

```vnr
columns: R"([
  { "key": "id", "header": "ID", "width": "60px" },
  { "key": "name", "header": "Name", "type": "text" },
  { "key": "createdAt", "header": "Created", "type": "date" },
  { "key": "actions", "header": "", "type": "actions", "width": "80px" }
])";
```

### 13.5 Custom delimiter usage (content contains the default closing sequence)

```vnr
// The content includes literal `)"` inside a nested string, so a custom delimiter is required
customStyles {
    R"css(
    .quote::after { content: ")"; }
    )css"
}
```

### 13.6 Raw strings for multi-line CSS

```vnr
customStyles {
    R"(
    #legacy-hero-banner { display: none !important; }
    body.legacy-theme { background: var(--spm-bg-primary) !important; }
    .cta-button {
        border-radius: var(--spm-radius);
        background: var(--spm-accent);
        color: var(--spm-accent-fg);
    }
    )"
}
```

### 13.7 Regex with alternation for multi-selector matching contexts

```vnr
// Matches "/forum/thread/123" or "/forum/t/123"
urlPattern: R"(\/forum\/(?:thread|t)\/\d+)";
```

### 13.8 Escaping-avoidance comparison (why raw strings matter)

```vnr
// Without raw strings — every backslash and quote must be escaped:
urlPattern: "example\\.com\\/item\\/\\d+";

// With raw strings — written exactly as a regex engine would expect:
urlPattern: R"(example\.com\/item\/\d+)";
```

---

## 14. Implicit JSON Type Deserialization — Exhaustive Cases

The emitter attempts to parse every property value as JSON before falling back to a plain string.

| Written value | Emitted as | Type |
|---|---|---|
| `mobileColumns: 2;` | `2` | number |
| `mobileGap: "8px";` | `"8px"` | string (fails JSON number parse) |
| `showSearch: true;` | `true` | boolean |
| `hideSidebarOnMobile: false;` | `false` | boolean |
| `mobileBreakpoint: 720;` | `720` | number |
| `tagGroups: R"([{"title":"Tags"}])";` | `[{"title":"Tags"}]` | array |
| `metadata: R"({"source":"legacy"})";` | `{"source":"legacy"}` | object |
| `pageTitle: "Gallery";` | `"Gallery"` | string |
| `discountRate: "0.15";` | `0.15` | number (valid JSON number literal) |
| `phoneNumber: "5551234567";` | `5551234567` | **number** — caution, see 14.1 |
| `zipCode: "02139";` | `"02139"` | string — leading zero makes it invalid JSON number, stays a string |
| `isFeatured: "true";` | `true` | boolean (quoted `"true"` still parses as JSON boolean) |
| `emptyList: "[]";` | `[]` | array |
| `nullableField: "null";` | `null` | null |

### 14.1 Gotcha — numeric-looking IDs silently becoming numbers

```vnr
// DANGER: if this "phone" string is a pure digit sequence with no leading zero,
// it will be coerced to a JSON number, which can lose formatting significance
// (e.g. leading zeros, or values too large for a JS float to represent exactly).
bind accountNumber: "self | attr:data-account"; // e.g. "10023491"  -> emitted as 10023491
```

**Mitigation**: if a value must always remain a string (phone numbers, account numbers, postal
codes with leading zeros), keep at least one non-numeric character, prefix with a stable marker,
or rely on the fact that leading zeros already force string emission:

```vnr
// Leading zero forces string output naturally
bind zip: ".address .zip | text"; // "02139" stays a string

// For all-digit values with no leading zero, consider whether numeric emission
// is actually acceptable for the target prop's expected type before relying on it.
```

### 14.2 Booleans from `attr:checked` / `attr:disabled`

```vnr
// <input type="checkbox" checked> -> attr:checked returns "checked" (a string, not boolean!)
// This does NOT auto-coerce to `true` unless you validate with your own logic downstream.
bind subscribed: "input[name='newsletter'] | attr:checked";
```

### 14.3 Arrays and objects only coerce from **raw strings**, not from unescaped quoted strings

```vnr
// ❌ Will NOT parse as JSON — the escaped quotes make this a plain string containing braces
badExample: "{\"a\": 1}";

// ✅ Correct approach — use a raw string so the JSON body is unescaped
goodExample: R"({"a": 1})";
```

---

## 15. Workspace / Multi-file Compilation Examples

### 15.1 Recommended nested package layout

```
theme-project/
├── core/
│   └── classes.vnr          # shared class blueprints (BaseLink, CardBase, etc.)
├── layout/
│   ├── headers/
│   │   └── top_nav.vnr
│   └── footers/
│       └── site_footer.vnr
├── pages/
│   ├── home/
│   │   └── landing.vnr
│   ├── gallery/
│   │   └── grid_layout.vnr
│   └── item/
│       └── details.vnr
├── theme.vnr                 # single global theme block
└── manifest.json              # compiled output target (also merge source)
```

Compile the whole tree in one pass:

```bash
./spm compile theme-project/ -o theme-project/manifest.json
```

### 15.2 File naming is arbitrary — this is equally valid

```
theme-project/
├── a.vnr
├── b.vnr
├── zzz_misc_overrides.vnr
└── whatever_i_want.vnr
```

The compiler concatenates and globally resolves every `.vnr` file it finds recursively,
regardless of name.

### 15.3 Sibling Class Autoloading — linter mode example

```bash
# Compiling just pages/gallery/grid_layout.vnr in isolation (e.g. from an editor linter)
./spm compile theme-project/pages/gallery/grid_layout.vnr -o /tmp/grid_layout.json
```

If `grid_layout.vnr` references `class TagItem` (declared in a sibling file in the same
directory, e.g. `pages/gallery/classes.vnr`), the compiler transparently loads that sibling file
in the background purely to resolve the class — it does **not** emit anything from that sibling
file into `/tmp/grid_layout.json` beyond what `grid_layout.vnr` itself references.

### 15.4 Class declared in a completely different top-level directory (works, because resolution is global for full-workspace compiles)

```vnr
// core/classes.vnr
class BaseLink {
    bind label: "self | text";
    bind url: "self | attr:href";
}
```

```vnr
// pages/home/landing.vnr — different directory entirely
reconstruct "#hero" -> UiHeroLanding {
    child quickLinks extends BaseLink {
        selector: ".quick-link";
    }
}
```

This only resolves correctly when compiling the **directory** (`spm compile theme-project/ -o ...`),
since single-file linter mode only auto-loads *sibling* files, not the whole tree.

### 15.5 Deep merge with a pre-existing manifest (metadata preservation)

Given an existing `manifest.json`:

```json
{
  "targetUrl": "*://example.com/*",
  "version": "2.3.1",
  "minEngineVersion": "1.4.0",
  "theme": {
    "author": "acme-themes",
    "description": "Official Acme dark theme"
  }
}
```

Running `spm compile theme-project/ -o manifest.json` again — after editing only `.vnr` sources —
preserves `targetUrl`, `version`, `minEngineVersion`, `author`, and `description` in the freshly
emitted output, merging them with whatever the new compilation produces for `cssVariables`,
`customStyles`, `components`, and `reconstructs`.

---

## 16. Full Worked Themes

### 16.1 Marketplace theme

```vnr
// theme.vnr
theme "MarketplaceLight" {
    variables {
        --spm-bg-primary: "#ffffff";
        --spm-bg-secondary: "#f7f7f8";
        --spm-accent: "#ff5a1f";
        --spm-accent-fg: "#ffffff";
        --spm-border: "#e5e5e5";
        --spm-radius: "10px";
    }
    customStyles {
        "#legacy-promo-carousel, .sticky-ad-footer { display: none !important; }"
    }
}
```

```vnr
// core/classes.vnr
class ListingLink {
    bind label: "self | text";
    bind url: "self | attr:href";
}

class ListingCard {
    bind title: ".listing-title | text";
    bind price: ".listing-price | text | cleanNumber";
    bind imageUrl: "img.listing-photo | attr:src";
    bind location: ".listing-location | text";
    bind postedAt: ".listing-date | attr:datetime";
    bind url: "a.listing-link | attr:href";
}
```

```vnr
// layout/headers/top_nav.vnr
selector "#site-header" -> UiNavHeader {
    action: replace;
    className: "marketplace-header";
    logoHref: "https://example-market.com/";
    bind logoUrl: ".brand-logo img | attr:src";

    child categories extends ListingLink {
        selector: "#category-nav a";
    }
}

selector "#legacy-search-bar" -> UiSearchBar {
    action: replace;
    placeholder: "Search listings…";
    submitUrl: "https://example-market.com/search";
    queryParamName: "q";
    bind defaultValue: "input[name='q'] | attr:value";
}
```

```vnr
// pages/browse/grid.vnr
reconstruct "#listings-container" -> UiGridPage {
    urlPattern: R"(\/browse\/?(\?.*)?$)";
    pageTitle: "Browse Listings";
    mobileColumns: 2;
    mobileGap: "8px";
    showSearch: true;

    tagGroups: R"([
      { "title": "Category", "typeKey": "category" },
      { "title": "Condition", "typeKey": "condition" },
      { "title": "Price", "typeKey": "price" }
    ])";

    child items extends ListingCard {
        selector: ".listing-card";
    }

    child pageLinks extends ListingLink {
        scope: "document";
        selector: "#pagination a";
    }
}
```

```vnr
// pages/item/details.vnr
reconstruct "#listing-detail" -> UiItemDetailsPage {
    urlPattern: R"(\/listing\/\d+)";

    bind title: "h1.listing-title | text";
    bind description: ".listing-description | html";
    bind price: ".listing-price | text | cleanNumber";
    bind sellerName: ".seller-info .name | text";

    preserve {
        contactSellerSlot: "#legacy-contact-form";
    }

    child gallery {
        selector: ".listing-gallery .thumb";
        bind imageUrl: "img | attr:src";
    }
}
```

### 16.2 Forum theme

```vnr
theme "ForumDark" {
    variables {
        --spm-bg-primary: "#12141a";
        --spm-accent: "#5865f2";
        --spm-text-primary: "#e6e7ec";
        --spm-border: "#23252d";
    }
    customStyles {
        "#forum-sponsor-bar { display: none !important; }"
    }
}

class UserRef {
    scope: "document";
    bind username: "self | text";
    bind profileUrl: "self | attr:href";
}

reconstruct "#board-index" -> UiForumIndexPage {
    urlPattern: "board=index";
    pageTitle: "Community Forum";

    child categories {
        selector: ".forum-category-block";
        bind name: ".cat-title | text";
        bind topicCount: ".cat-stats .topics | text | number";
        bind postCount: ".cat-stats .posts | text | number";

        child lastPoster extends UserRef {
            selector: ".cat-last-post .username a";
        }
    }
}

reconstruct "#thread-view" -> UiThreadPage {
    urlPattern: R"(\/thread\/\d+)";
    pageTitle: "Thread";

    preserve {
        replyFormSlot: "#legacy-reply-box";
    }

    child posts {
        selector: ".forum-post";
        bind body: ".post-content | html";
        bind postedAt: ".post-meta time | attr:datetime";

        child author extends UserRef {
            selector: ".post-author a.username";
        }
    }
}
```

### 16.3 News / blog theme

```vnr
theme "EditorialClean" {
    variables {
        --spm-bg-primary: "#ffffff";
        --spm-text-primary: "#1a1a1a";
        --spm-accent: "#c0392b";
        --spm-font-sans: "'Source Sans Pro', sans-serif";
    }
    customStyles {
        "#autoplay-video-widget, .paywall-nag { display: none !important; }"
    }
}

class ArticleLink {
    bind headline: "h3 | text";
    bind url: "h3 a | attr:href";
    bind imageUrl: "img | attr:src";
    bind category: ".kicker | text";
    bind publishedAt: "time | attr:datetime";
}

reconstruct "#front-page" -> UiHeroLanding {
    urlPattern: R"(^https?:\/\/example-news\.com\/?$)";
    tagline: "Today's Top Stories";

    child topStories extends ArticleLink {
        selector: ".top-story";
    }
}

reconstruct "#category-feed" -> UiGridPage {
    urlPattern: R"(\/section\/[a-z-]+\/?$)";
    pageTitle: "Section";
    mobileColumns: 1;

    child items extends ArticleLink {
        selector: ".article-teaser";
    }
}

reconstruct "#article-body" -> UiItemDetailsPage {
    urlPattern: R"(\/article\/\d+)";

    bind title: "h1.headline | text";
    bind byline: ".byline | text";
    bind bodyHtml: ".article-content | html";
    bind publishedAt: "time.published | attr:datetime";

    preserve {
        commentsSlot: "#legacy-comments-plugin";
    }
}
```

### 16.4 Job board theme

```vnr
theme "CareerBoard" {
    variables {
        --spm-accent: "#0a66c2";
        --spm-radius: "6px";
    }
    customStyles {
        ""
    }
}

class JobCard {
    bind title: ".job-title | text";
    bind company: ".job-company | text";
    bind location: ".job-location | text";
    bind salary: ".job-salary | text | cleanNumber";
    bind postedAt: ".job-posted | attr:datetime";
    bind url: "a.job-link | attr:href";
    bind remote: "self | attr:data-remote | number";
}

reconstruct "#job-search-results" -> UiGridPage {
    urlPattern: "q=";
    pageTitle: "Job Search";
    mobileColumns: 1;
    showSearch: true;
    searchPlaceholder: "Job title, keyword, or company";
    searchSubmitUrl: "https://example-jobs.com/search";
    searchParamName: "q";

    bind searchDefaultValue: "#job-search-input | attr:value";

    child items extends JobCard {
        selector: ".job-result-row";
    }

    child pageLinks {
        scope: "document";
        selector: "#results-pager a";
        bind label: "self | text";
        bind url: "self | attr:href";
    }
}
```

### 16.5 Real estate theme

```vnr
theme "PropertyModern" {
    variables {
        --spm-accent: "#0f766e";
        --spm-bg-secondary: "#f0fdfa";
    }
    customStyles {
        ""
    }
}

class ListingSummary {
    bind price: ".price | text | cleanNumber";
    bind beds: ".beds | text | number";
    bind baths: ".baths | text | number";
    bind sqft: ".sqft | text | cleanNumber";
    bind address: ".address | text";
    bind imageUrl: "img.primary-photo | attr:src";
    bind url: "a.listing-link | attr:href";
}

reconstruct "#property-search" -> UiGridPage {
    urlPattern: "listings";
    pageTitle: "Property Listings";
    mobileColumns: 1;
    tagGroups: R"([
      { "title": "Beds", "typeKey": "beds" },
      { "title": "Price Range", "typeKey": "price" },
      { "title": "Property Type", "typeKey": "type" }
    ])";

    child items extends ListingSummary {
        selector: ".property-card";
    }
}

reconstruct "#property-detail" -> UiItemDetailsPage {
    urlPattern: R"(\/property\/\d+)";

    bind address: "h1.property-address | text";
    bind price: ".property-price | text | cleanNumber";
    bind description: ".property-description | html";

    preserve {
        contactAgentSlot: "#legacy-agent-contact-form";
        mortgageCalculatorSlot: "#legacy-mortgage-calc";
    }

    child photos {
        selector: ".property-gallery img";
        bind imageUrl: "self | attr:src";
    }

    child features {
        selector: ".feature-list li";
        bind label: "self | text";
    }
}
```

---

## 17. Common Errors, Anti-Patterns & Fixes

### 17.1 Missing semicolon

```vnr
// ❌ compile error: expected ';' after property value
selector "#nav" -> UiNavHeader {
    action: replace
    logoHref: "https://example.com/";
}
```

```vnr
// ✅
selector "#nav" -> UiNavHeader {
    action: replace;
    logoHref: "https://example.com/";
}
```

### 17.2 Two `theme` blocks in the workspace

```vnr
// file: a.vnr
theme "First" { variables {} }

// file: b.vnr — ❌ compile error: duplicate theme declaration
theme "Second" { variables {} }
```

### 17.3 Unescaped backslashes in a plain (non-raw) string regex

```vnr
// ❌ almost certainly not what was intended — "\d" is not a valid escape
// sequence in a plain string literal and will likely be mis-parsed or rejected
urlPattern: "\d+";

// ✅ use a raw string for anything regex-flavored
urlPattern: R"(\d+)";
```

### 17.4 Forgetting `action` on a `selector` block

```vnr
// ❌ compile error: selector block missing required "action" key
selector "#promo" -> UiPromoBanner {
    headline: "Sale!";
}
```

```vnr
// ✅
selector "#promo" -> UiPromoBanner {
    action: replace;
    headline: "Sale!";
}
```

### 17.5 Extending a class declared later in the same file, in a workspace/full-directory compile

This is actually **fine** — class resolution happens after full parsing, so declaration order
across the workspace does not matter for a directory compile:

```vnr
reconstruct "#page" -> UiPage {
    child links extends LinkClassDeclaredBelow {
        selector: ".link";
    }
}

class LinkClassDeclaredBelow {
    bind label: "self | text";
    bind url: "self | attr:href";
}
```

It only becomes a problem in **single-file linter mode** if the class lives in a file the
compiler's sibling autoloader can't find (e.g. it's in a different top-level directory) — see
§15.3–15.4.

### 17.6 Using `child` outside of a `selector`/`reconstruct` block

```vnr
// ❌ compile error: "child" is only valid nested inside a selector or reconstruct block
child orphanList {
    selector: ".x";
}
```

### 17.7 Typo'd base extractor name

```vnr
// ❌ compile error: unknown base extractor "txt" (did you mean "text"?)
bind title: "h2 | txt";
```

### 17.8 Reusing a `child` name twice at the same nesting level

```vnr
// ❌ likely a compile-time or resolver-level conflict: duplicate child name "items"
// within the same parent block
reconstruct "#page" -> UiGridPage {
    child items {
        selector: ".a";
    }
    child items {
        selector: ".b";
    }
}
```

```vnr
// ✅ give each list a distinct prop name
reconstruct "#page" -> UiGridPage {
    child primaryItems {
        selector: ".a";
    }
    child secondaryItems {
        selector: ".b";
    }
}
```

### 17.9 Raw string delimiter collision

```vnr
// ❌ the default raw-string terminator `)"` appears inside the content itself,
// closing the literal prematurely and leaving trailing garbage that fails to parse
badRegex: R"(\)")"; 
```

```vnr
// ✅ use a custom delimiter so the closing sequence becomes unambiguous
badRegex: R"tag(\)")tag";
```

### 17.10 Forgetting that `preserve` slot names must match layout-side container IDs

A `preserve` block referencing `mySlot` compiles successfully even if `UiSomePage` has no
`id="mySlot-container"` element in its rendered output — the *compiler* has no way to validate
against the React component internals. This fails silently at **runtime** (the node is removed
from the legacy DOM but never reparented anywhere visible), not at compile time. Always confirm
slot names against the target component's actual implementation/docs.

---

## 18. CLI Recipes

```bash
# Compile a full workspace to a manifest
./spm compile theme-project/ -o theme-project/manifest.json

# Lint a single file without writing a permanent manifest
./spm compile theme-project/pages/home/landing.vnr -o /tmp/landing.json

# Iterate on a single page's markup while live-reloading via the browser extension
./spm dev theme-project/

# Typical CI step: compile and fail the build on any compiler diagnostic
./spm compile theme-project/ -o theme-project/manifest.json || exit 1
```

---

## 19. Cheat Sheet

```
theme "<Label>" {
    variables { --token-name: "<value>"; }
    customStyles {
        "<raw-css>"
    }
}

class <Name> [extends <Parent>] {
    [scope: "document";]
    bind <prop>: "<selector-or-self> | <extractor> [| <pipe>]*";
}

selector "<css-selector>" [-> <Component>] {
    action: hide | replace;
    <staticProp>: <value>;
    bind <prop>: "<extraction>";
    [child <name> [extends <Class>] { ... }]
}

reconstruct "<css-selector>" -> <Component> {
    [urlPattern: "<substring-or-regex>";]
    [mediaQuery: "<css-media-query>";]
    <staticProp>: <value>;
    bind <prop>: "<extraction>";
    [preserve { <slotName>: "<selector>"; }]
    [child <name> [extends <Class>] {
        [scope: "document";]
        selector: "<selector>";
        bind <prop>: "<extraction>";
        [child <nestedName> { ... }]
    }]
}
```

**Base extractors**: `text` · `html` · `attr:<name>` · `hrefOrOnclick` · `nextSiblingText` ·
`hiddenInputs` · `selector`

**Pipes**: `split` · `split:<delim>` · `number` · `cleanNumber`

**Implicit JSON typing**: numbers, booleans, arrays (`R"([...])"`), objects (`R"({...})"`) all
auto-coerce from string literals when valid JSON; everything else stays a string.
