#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
impact_scope.py
===============
Gate 4 Tool: Computes reverse transitive dependency closure from changed files
to affected capabilities and checklist entries.
"""

import os
import sys
import json
import argparse
import fnmatch
from pathlib import Path
from collections import defaultdict
import yaml

GATES_DIR = Path(__file__).resolve().parent
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))

from gate_context import normalize_posix_path, find_workspace_root


GLOBAL_IMPACT_PATTERNS = [
    "*/esp_idf_bridge.c",
    "esp_idf_bridge.c",
    "*/esp_sim_handle.c",
    "esp_sim_handle.c",
    "*CMakeLists.txt",
    "*/cmake/**",
    "cmake/**",
    "*/pal/**",
    "pal/**",
    "*/targets/**",
    "targets/**",
    "*/osal/**",
    "osal/**",
    "wink-micro-os/CMakeLists.txt",
]

IGNORED_NON_CODE_PATTERNS = [
    "*.md", "*.txt", "*.rst", "*.json", "*.yaml", "*.yml", "*.png", "*.jpg", "*.svg", "*.lock",
    "*/docs/**", "docs/**",
    "*/.governance/**", ".governance/**",
    "*/tools/**", "tools/**",
    "*/test/**", "test/**", "tests/**", "*/tests/**",
    "*.ps1", "*.sh", "*.py",
]


def compute_impact_closure(
    changed_files: list[str],
    catalog: dict,
    manifest: dict,
    max_inline_entries: int = 30,
) -> dict:
    """
    Computes the reverse transitive dependency closure:
    1. Matches changed files against owned_paths of capabilities
    2. Checks GLOBAL_IMPACT_PATHS (esp_idf_bridge.c, esp_sim_handle.c, cmake, etc.)
    3. Detects unmapped code files under frameworks/esp_idf or vendor/esp_idfv61 (Fail-Closed)
    4. Traverses reverse dependency graph (mandatory and conditional depends_on)
    5. Gathers all checklist entries referencing impacted capabilities or target_app_dir
    6. Returns impact scope summary and pr_inline decision
    """
    capabilities = catalog.get("capabilities", {})
    norm_changed = [normalize_posix_path(f) for f in changed_files]

    direct_caps = set()
    mapped_files = set()

    # 1. Match directly affected capabilities by owned_paths
    for cap_id, cap_def in capabilities.items():
        if not isinstance(cap_def, dict):
            continue
        for owned in cap_def.get("owned_paths", []):
            norm_owned = normalize_posix_path(owned)
            for f in norm_changed:
                if fnmatch.fnmatch(f, norm_owned) or fnmatch.fnmatch(f, f"*/{norm_owned}") or f == norm_owned:
                    direct_caps.add(cap_id)
                    mapped_files.add(f)

    # 2. Match GLOBAL_IMPACT_PATTERNS
    hit_global = False
    for f in norm_changed:
        for g_pat in GLOBAL_IMPACT_PATTERNS:
            if fnmatch.fnmatch(f, g_pat) or fnmatch.fnmatch(f, f"*/{g_pat}") or f == g_pat:
                hit_global = True
                mapped_files.add(f)
                break

    # 3. Non-code changes diffusion (catalog changes)
    catalog_changed = any("capability-catalog.yaml" in f for f in norm_changed)
    if catalog_changed:
        for cap_id in capabilities:
            direct_caps.add(cap_id)

    # 4. App directory changes direct mapping
    direct_impact_entries = set()
    app_target_dirs = {}
    for entry in manifest.get("entries", []):
        t_dir = entry.get("target_app_dir")
        if t_dir:
            app_target_dirs[normalize_posix_path(t_dir)] = entry.get("display_id")

    for f in norm_changed:
        for t_dir, did in app_target_dirs.items():
            if f"/{t_dir}/" in f"/{f}/" or f.startswith(t_dir) or f.endswith(t_dir):
                mapped_files.add(f)
                direct_impact_entries.add(did)
                break

    # 5. Scenario scripts changed directly
    for entry in manifest.get("entries", []):
        display_id = entry.get("display_id")
        for ex in entry.get("executions", []):
            sc_path = ex.get("acceptance", {}).get("scenario_path")
            if sc_path:
                norm_sc = normalize_posix_path(sc_path)
                for f in norm_changed:
                    if f.endswith(norm_sc) or norm_sc.endswith(f):
                        direct_impact_entries.add(display_id)
                        mapped_files.add(f)

    # 6. Ignored non-code files
    for f in norm_changed:
        for ign in IGNORED_NON_CODE_PATTERNS:
            if fnmatch.fnmatch(f, ign) or fnmatch.fnmatch(f, f"*/{ign}"):
                mapped_files.add(f)
                break

    # 7. Check for unmapped unknown code files (Fail-Closed)
    unknown_paths = []
    for f in norm_changed:
        if (
            "frameworks/esp_idf" in f or "vendor/esp_idfv61" in f or
            f.startswith("wink-micro-os/") or f.startswith("wink-micro-app/")
        ):
            if f not in mapped_files:
                if f.endswith((".c", ".h", ".cpp", ".hpp", ".S")):
                    unknown_paths.append(f)

    # 8. Construct reverse dependency graph and traverse transitive closure
    reverse_graph = defaultdict(set)
    for parent_id, cap_def in capabilities.items():
        if not isinstance(cap_def, dict):
            continue
        depends_on = cap_def.get("depends_on", {})
        for req in depends_on.get("mandatory", []):
            reverse_graph[req].add(parent_id)

        for cond in depends_on.get("conditional", []):
            for req in cond.get("requires", []):
                reverse_graph[req].add(parent_id)

    closure_caps = set(direct_caps)
    worklist = list(direct_caps)
    while worklist:
        curr = worklist.pop(0)
        for parent in reverse_graph.get(curr, []):
            if parent not in closure_caps:
                closure_caps.add(parent)
                worklist.append(parent)

    # 9. Collect impacted entries from manifest
    impact_entries = set(direct_impact_entries)
    for entry in manifest.get("entries", []):
        display_id = entry.get("display_id")
        reqs = set(entry.get("required_capabilities", []))
        if reqs & closure_caps:
            impact_entries.add(display_id)

    # If global core was hit or checklist.data.json changed, add all verified entries
    if hit_global or any("checklist.data.json" in f for f in norm_changed):
        for entry in manifest.get("entries", []):
            display_id = entry.get("display_id")
            for ex in entry.get("executions", []):
                if ex.get("delivery_state") == "verified":
                    impact_entries.add(display_id)
                    break

    sorted_impact_entries = sorted(list(impact_entries))
    pr_inline = len(sorted_impact_entries) <= max_inline_entries

    return {
        "direct_capabilities": sorted(list(direct_caps)),
        "affected_capabilities": sorted(list(closure_caps)),
        "impact_entries": sorted_impact_entries,
        "impact_count": len(sorted_impact_entries),
        "pr_inline": pr_inline,
        "hit_global": hit_global,
        "unknown_paths": sorted(list(set(unknown_paths))),
    }


def parse_args():
    parser = argparse.ArgumentParser(
        description="ESP-IDF Reverse Dependency Impact Scope Analyzer",
    )
    parser.add_argument(
        "--changed-files",
        help="Path to file with changed file paths (one per line)",
    )
    parser.add_argument(
        "--manifest",
        help="Path to checklist.data.json",
    )
    parser.add_argument(
        "--catalog",
        help="Path to capability-catalog.yaml",
    )
    parser.add_argument(
        "--output-json",
        help="Path to write impact analysis JSON output",
    )
    parser.add_argument(
        "--max-inline",
        type=int,
        default=30,
        help="Maximum entries to regress inline in PR mode",
    )
    return parser.parse_args()


def main():
    opts = parse_args()
    ws_root = find_workspace_root()

    default_esp_dir = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
    m_path = Path(opts.manifest) if opts.manifest else default_esp_dir / "checklist.data.json"
    c_path = Path(opts.catalog) if opts.catalog else default_esp_dir / "capability-catalog.yaml"

    with open(m_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    with open(c_path, "r", encoding="utf-8") as f:
        catalog = yaml.safe_load(f) or {}

    changed_files = []
    if opts.changed_files:
        with open(opts.changed_files, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    changed_files.append(line)

    result = compute_impact_closure(
        changed_files=changed_files,
        catalog=catalog,
        manifest=manifest,
        max_inline_entries=opts.max_inline,
    )

    out_str = json.dumps(result, indent=2, ensure_ascii=False)
    print(out_str)

    if opts.output_json:
        out_p = Path(opts.output_json)
        out_p.parent.mkdir(parents=True, exist_ok=True)
        with open(out_p, "w", encoding="utf-8") as f:
            f.write(out_str)


if __name__ == "__main__":
    main()
