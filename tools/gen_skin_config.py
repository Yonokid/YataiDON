#!/usr/bin/env python3
"""
Generate src/libs/skin_config_generated.h from a skin_config.json file.

Usage: gen_skin_config.py <skin_config.json> <output_header>

The JSON top-level keys (e.g. "free_play") become enum members (e.g. FREE_PLAY)
and entries in skin_config_map. Add a new key to skin_config.json and re-build;
the enum and map update automatically.
"""
import json
import os
import sys


def key_to_enum(key: str) -> str:
    return key.upper()


def generate(json_path: str, output_path: str) -> None:
    with open(json_path, encoding="utf-8") as f:
        data = json.load(f)

    keys = list(data.keys())

    lines = [
        "// AUTO-GENERATED — do not edit by hand.",
        f"// Source: {os.path.basename(json_path)}",
        "// Re-run CMake (or: python3 tools/gen_skin_config.py <skin_config.json> <output.h>)",
        "#pragma once",
        "#include <string>",
        "#include <unordered_map>",
        "",
        "enum class SC : unsigned int {",
    ]

    for key in keys:
        lines.append(f"    {key_to_enum(key)},")

    lines += [
        "};",
        "",
        "namespace std {",
        "    template<> struct hash<SC> {",
        "        size_t operator()(SC v) const noexcept {",
        "            return hash<unsigned int>{}(static_cast<unsigned int>(v));",
        "        }",
        "    };",
        "}",
        "",
        "const std::unordered_map<std::string, SC> skin_config_map {",
    ]

    for key in keys:
        lines.append(f'    {{"{key}", SC::{key_to_enum(key)}}},')

    lines += [
        "};",
        "",
    ]

    content = "\n".join(lines)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"Generated {output_path} ({len(keys)} entries)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <skin_config.json> <output_header>", file=sys.stderr)
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])
