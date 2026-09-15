#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""License map gate for the Wink-AI embedded repository (ADR-0083 / ADR-0084).

Reads .github/license-map.json and verifies every tracked file against the
layered license map:

  - enforce      : if a file carries SPDX-License-Identifier it must match the
                   rule license; with must_have the identifier is required for
                   the configured extensions.
  - text         : the file must contain the configured license text markers
                   (used for vendored third-party trees without SPDX tags).
  - dir-license  : the matched files inherit a directory-level LICENSE file
                   (used for comment-less JSON data directories).
  - skip         : no checks (binary assets, generated docs, vendor fixtures).

A global forbidden-prefix scan rejects license identifiers that the project
must never ship (for example GPL-2.0-only).

Usage: python .github/scripts/check_license_map.py [--root PATH]
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

SPDX_RE = re.compile(r"SPDX-License-Identifier:\s*([A-Za-z0-9.\-+]+)")
DEFAULT_MUST_HAVE_EXT = {".c", ".h", ".cpp", ".hpp", ".cc", ".py"}


def load_config(root: Path) -> dict:
    cfg_path = root / ".github" / "license-map.json"
    return json.loads(cfg_path.read_text(encoding="utf-8"))


def tracked_files(root: Path) -> list[str]:
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        capture_output=True,
        check=True,
    )
    return [f for f in out.stdout.decode("utf-8", "surrogateescape").split("\0") if f]


def match_rule(rel: str, rules: list[dict]) -> dict | None:
    for rule in rules:
        if fnmatch.fnmatchcase(rel, rule["path"]):
            return rule
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[2]))
    args = parser.parse_args()
    root = Path(args.root).resolve()

    cfg = load_config(root)
    rules = cfg["rules"]
    forbidden = tuple(cfg.get("forbidden_license_prefixes", ()))
    default_must_have_ext = {e.lower() for e in cfg.get("must_have_ext", DEFAULT_MUST_HAVE_EXT)}
    skip_ext = {e.lower() for e in cfg.get("skip_ext", [])}

    failures: list[str] = []
    counts: Counter[str] = Counter()

    for rel in tracked_files(root):
        ext = Path(rel).suffix.lower()
        rule = match_rule(rel, rules)
        if ext in skip_ext:
            continue
        path = root / rel
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        match = SPDX_RE.search(text)
        if match:
            counts[match.group(1)] += 1
            if forbidden and match.group(1).startswith(forbidden):
                failures.append(f"{rel}: forbidden license identifier '{match.group(1)}'")

        if rule is None:
            continue

        mode = rule.get("mode", "enforce")
        expected = rule.get("license")

        if mode == "skip":
            continue
        if rel in rule.get("except", []):
            continue
        if mode == "text":
            for needle in rule.get("contains", []):
                if needle not in text:
                    failures.append(f"{rel}: missing license text marker {needle!r} (rule {rule['path']})")
            continue

        if mode == "enforce":
            if match and expected and match.group(1) != expected:
                failures.append(
                    f"{rel}: SPDX '{match.group(1)}' does not match rule license '{expected}' ({rule['path']})"
                )
            if rule.get("must_have"):
                must_ext = {e.lower() for e in rule.get("must_have_ext", default_must_have_ext)}
                if ext in must_ext and not match:
                    failures.append(f"{rel}: missing SPDX-License-Identifier (expected '{expected}')")

    for rule in rules:
        for lic_file in rule.get("require_files", []):
            if not (root / lic_file).is_file():
                failures.append(f"required license file missing: {lic_file} (rule {rule['path']})")

    print("license census (files carrying SPDX identifiers):")
    for license_id, count in counts.most_common():
        print(f"  {count:5d}  {license_id}")

    if failures:
        print(f"\nFAIL: {len(failures)} finding(s):")
        for failure in failures[:100]:
            print("  -", failure)
        if len(failures) > 100:
            print(f"  ... and {len(failures) - 100} more")
        return 1

    print("\nOK: license map satisfied")
    return 0


if __name__ == "__main__":
    sys.exit(main())
