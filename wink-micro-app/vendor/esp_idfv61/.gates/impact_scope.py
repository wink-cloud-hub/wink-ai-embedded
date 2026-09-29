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


def compute_impact_closure(
    changed_files: list[str],
    catalog: dict,
    manifest: dict,
    max_inline_entries: int = 30,
) -> dict:
    """
    Computes the reverse transitive dependency closure:
    1. Matches changed files against owned_paths of capabilities
    2. Diffuses non-code changes (scenario files, catalog changes)
    3. Traverses reverse dependency graph (mandatory and conditional depends_on)
    4. Gathers all checklist entries referencing the impacted capability closure
    5. Returns impact scope summary and pr_inline decision
    """
    capabilities = catalog.get("capabilities", {})
    norm_changed = [normalize_posix_path(f) for f in changed_files]

    direct_caps = set()

    # 1. Match directly affected capabilities by owned_paths
    for cap_id, cap_def in capabilities.items():
        if not isinstance(cap_def, dict):
            continue
        for owned in cap_def.get("owned_paths", []):
            norm_owned = normalize_posix_path(owned)
            for f in norm_changed:
                if fnmatch.fnmatch(f, norm_owned) or fnmatch.fnmatch(f, f"*/{norm_owned}") or f == norm_owned:
                    direct_caps.add(cap_id)
                    break

    # 2. Non-code changes diffusion
    # If capability-catalog.yaml itself changed: mark all capabilities declared/modified
    catalog_changed = any("capability-catalog.yaml" in f for f in norm_changed)
    if catalog_changed:
        for cap_id in capabilities:
            direct_caps.add(cap_id)

    # 3. Scenario scripts changed directly
    direct_impact_entries = set()
    for entry in manifest.get("entries", []):
        display_id = entry.get("display_id")
        for ex in entry.get("executions", []):
            sc_path = ex.get("acceptance", {}).get("scenario_path")
            if sc_path:
                norm_sc = normalize_posix_path(sc_path)
                if any(f.endswith(norm_sc) or norm_sc.endswith(f) for f in norm_changed):
                    direct_impact_entries.add(display_id)

    # 4. Construct reverse dependency graph and traverse transitive closure
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

    # 5. Collect impacted entries from manifest
    impact_entries = set(direct_impact_entries)
    for entry in manifest.get("entries", []):
        display_id = entry.get("display_id")
        reqs = set(entry.get("required_capabilities", []))
        if reqs & closure_caps:
            impact_entries.add(display_id)

    sorted_impact_entries = sorted(list(impact_entries))
    pr_inline = len(sorted_impact_entries) <= max_inline_entries

    return {
        "direct_capabilities": sorted(list(direct_caps)),
        "affected_capabilities": sorted(list(closure_caps)),
        "impact_entries": sorted_impact_entries,
        "impact_count": len(sorted_impact_entries),
        "pr_inline": pr_inline,
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
