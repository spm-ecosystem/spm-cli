#!/usr/bin/env python3
"""
Generates C++ header src/veneer/component_registry.hpp from TypeScript component prop interfaces in spm-components.
"""
import re
import sys
from pathlib import Path

COMPONENTS_DIR = Path(__file__).resolve().parent.parent.parent / "extension" / "src" / "components"
HEADER_PATH = Path(__file__).resolve().parent.parent / "src" / "veneer" / "component_registry.hpp"

# Fallback path if running inside spm-cli root
if not COMPONENTS_DIR.exists():
    COMPONENTS_DIR = Path("/home/watashi/Projects/extension/src/components")

def parse_ts_interfaces(components_dir: Path) -> dict[str, set[str]]:
    schemas = {}
    ts_files = list(components_dir.glob("**/*.tsx")) + list(components_dir.glob("**/*.ts"))
    
    interface_pat = re.compile(r"export\s+interface\s+([A-Za-z0-9]+Props)(?:<[^>]+>)?\s*(?:extends\s+[^\{]+)?\s*\{([^\}]+)\}", re.DOTALL)
    prop_pat = re.compile(r"^\s*([a-zA-Z0-9_]+)\s*\??\s*:", re.MULTILINE)

    universal_props = {"className", "style", "id", "children", "key"}

    for ts_file in ts_files:
        path_str = str(ts_file)
        if "node_modules" in path_str or "dist" in path_str:
            continue
        content = ts_file.read_text(encoding="utf-8")
        for match in interface_pat.finditer(content):
            if_name = match.group(1)
            if_body = match.group(2)

            comp_name = if_name[:-5] if if_name.endswith("Props") else if_name
            if comp_name in ["Primitive", "Text", "Image", "Link", "ScrollBox", "UiCommentCard", "UiCommentReply", "UiConfirmDialog"]:
                continue
            if comp_name.startswith("LayoutPrimitive"):
                continue

            props = set(prop_pat.findall(if_body))
            props.update(universal_props)
            
            if comp_name not in schemas:
                schemas[comp_name] = set()
            schemas[comp_name].update(props)

    # Primitives mapping (from LayoutPrimitives.tsx)
    primitive_mapping = {
        "UiBox": universal_props | {"children"},
        "UiFlexRow": universal_props | {"children"},
        "UiFlexColumn": universal_props | {"children"},
        "UiGrid": universal_props | {"children"},
        "UiText": universal_props | {"content"},
        "UiImage": universal_props | {"src", "alt"},
        "UiLink": universal_props | {"href", "children"},
        "UiScrollBox": universal_props | {"children", "height", "maxHeight", "overflow", "overflowX", "overflowY"},
    }
    for prim, pset in primitive_mapping.items():
        schemas[prim] = pset

    return schemas

def generate_header(schemas: dict[str, set[str]]) -> str:
    sorted_comps = sorted(schemas.keys())
    
    entries = []
    for comp in sorted_comps:
        sorted_props = sorted(schemas[comp])
        props_str = ", ".join(f'"{p}"' for p in sorted_props)
        entries.append(f'            {{"{comp}", {{{props_str}}}}}')
    
    entries_code = ",\n".join(entries)

    return f"""#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>

namespace veneer {{

class ComponentSchemaRegistry {{
public:
    static const std::unordered_map<std::string, std::unordered_set<std::string>>& getSchemaMap() {{
        static const std::unordered_map<std::string, std::unordered_set<std::string>> schemas = {{
{entries_code}
        }};
        return schemas;
    }}

    static bool isKnownComponent(const std::string& compName) {{
        const auto& schemas = getSchemaMap();
        return schemas.find(compName) != schemas.end();
    }}

    static bool isValidProp(const std::string& compName, const std::string& propKey) {{
        const auto& schemas = getSchemaMap();
        auto it = schemas.find(compName);
        if (it == schemas.end()) return true; // Unrecognized components pass gracefully
        return it->second.find(propKey) != it->second.end();
    }}

    static std::string getDidYouMean(const std::string& compName, const std::string& invalidKey) {{
        const auto& schemas = getSchemaMap();
        auto it = schemas.find(compName);
        if (it == schemas.end()) return "";

        std::string bestMatch = "";
        size_t minDistance = 999;
        size_t maxAllowedDist = std::max<size_t>(2, invalidKey.length() / 3);

        for (const auto& validProp : it->second) {{
            size_t dist = levenshteinDistance(invalidKey, validProp);
            if (dist <= maxAllowedDist) {{
                if (dist < minDistance || (dist == minDistance && (bestMatch.empty() || validProp < bestMatch))) {{
                    minDistance = dist;
                    bestMatch = validProp;
                }}
            }}
        }}
        return bestMatch;
    }}

private:
    static size_t levenshteinDistance(const std::string& s1, const std::string& s2) {{
        const size_t m = s1.length();
        const size_t n = s2.length();
        if (m == 0) return n;
        if (n == 0) return m;

        std::vector<size_t> costs(n + 1);
        for (size_t j = 0; j <= n; ++j) costs[j] = j;

        for (size_t i = 0; i < m; ++i) {{
            costs[0] = i + 1;
            size_t corner = i;
            for (size_t j = 0; j < n; ++j) {{
                size_t upper = costs[j + 1];
                if (s1[i] == s2[j]) {{
                    costs[j + 1] = corner;
                }} else {{
                    size_t t = corner < upper ? corner : upper;
                    costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
                }}
                corner = upper;
            }}
        }}
        return costs[n];
    }}
}};

}} // namespace veneer
"""

if __name__ == "__main__":
    if not COMPONENTS_DIR.exists():
        print(f"Error: Components directory not found at {COMPONENTS_DIR}", file=sys.stderr)
        sys.exit(1)
    
    schemas = parse_ts_interfaces(COMPONENTS_DIR)
    code = generate_header(schemas)
    HEADER_PATH.parent.mkdir(parents=True, exist_ok=True)
    HEADER_PATH.write_text(code, encoding="utf-8")
    print(f"Successfully generated {HEADER_PATH} with {len(schemas)} component schemas.")
