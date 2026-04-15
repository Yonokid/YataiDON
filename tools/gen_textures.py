#!/usr/bin/env python3
"""
Generate texture_ids_generated.h from the texture.json files in a skin.

Usage: gen_textures.py <graphics_dir> <output_header>

Walks <graphics_dir>/{screen}/{subset}/texture.json (depth-2 relative to
graphics_dir). Produces:
  - enum TexID : uint32_t  — one value per (subset, texture_name) pair
  - per-subset namespaces  — YELLOW_BOX::CROWN_FC etc. for ergonomic access
  - tex_id_map             — "subset/name" -> TexID, used once at load time

Subsets shared across screens are merged (union of texture names).
Names beginning with a digit are prefixed with '_'.
"""
import json
import os
import sys
from pathlib import Path


def to_identifier(name: str) -> str:
    upper = name.upper()
    if upper and upper[0].isdigit():
        upper = "_" + upper
    return upper


def generate(graphics_dir: str, output_path: str) -> None:
    root = Path(graphics_dir)

    # Collect ordered (subset, texture_name) pairs, union across all screens.
    # texture.json files may live at any depth under the graphics root; the subset
    # name is always the leaf folder (tex_json.parent.name).
    subset_textures: dict[str, set[str]] = {}
    for tex_json in sorted(root.rglob("texture.json")):
        subset_dir = tex_json.parent
        subset_name = subset_dir.name
        with open(tex_json, encoding="utf-8") as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError as e:
                print(f"  WARNING: skipping {tex_json}: {e}", file=sys.stderr)
                continue
        subset_textures.setdefault(subset_name, set()).update(data.keys())
        # Also include frame sub-directories (e.g. indicator/text/) not listed in texture.json
        for entry in subset_dir.iterdir():
            if entry.is_dir() and entry.name not in data:
                subset_textures[subset_name].add(entry.name)

    # Assign globally unique sequential IDs
    entries: list[tuple[str, str, int]] = []   # (subset, tex_name, id)
    idx = 0
    for subset in sorted(subset_textures):
        for tex_name in sorted(subset_textures[subset]):
            entries.append((subset, tex_name, idx))
            idx += 1

    lines = [
        "// AUTO-GENERATED — do not edit by hand.",
        "// Source: skin Graphics directory (texture.json files)",
        "// Re-run CMake (or: python3 tools/gen_textures.py <graphics_dir> <output.h>)",
        "#pragma once",
        "#include <string>",
        "#include <unordered_map>",
        "",
        "enum TexID : uint32_t {",
    ]
    for subset, tex_name, id_ in entries:
        enum_name = to_identifier(subset) + "__" + to_identifier(tex_name)
        lines.append(f"    {enum_name} = {id_},")
    lines += [
        "};",
        "",
    ]

    # Per-subset namespaces
    current_subset = None
    for subset, tex_name, id_ in entries:
        if subset != current_subset:
            if current_subset is not None:
                lines.append("}")
                lines.append("")
            lines.append(f"namespace {to_identifier(subset)} {{")
            current_subset = subset
        enum_name = to_identifier(subset) + "__" + to_identifier(tex_name)
        lines.append(f"    inline constexpr TexID {to_identifier(tex_name)} = TexID::{enum_name};")
    if current_subset is not None:
        lines.append("}")
        lines.append("")

    # Load-time string -> TexID map
    lines += [
        "inline const std::unordered_map<std::string, TexID> tex_id_map {",
    ]
    for subset, tex_name, id_ in entries:
        enum_name = to_identifier(subset) + "__" + to_identifier(tex_name)
        lines.append(f'    {{"{subset}/{tex_name}", TexID::{enum_name}}},')
    lines += [
        "};",
        "",
    ]

    content = "\n".join(lines)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)

    total_subsets = len(subset_textures)
    print(f"Generated {output_path} ({total_subsets} subsets, {len(entries)} textures)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <graphics_dir> <output_header>", file=sys.stderr)
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])
