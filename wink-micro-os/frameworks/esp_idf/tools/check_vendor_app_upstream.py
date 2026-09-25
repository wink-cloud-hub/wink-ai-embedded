#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ESP-IDF vendor app upstream traceability checker (master plan §7.1.1).

Validates every `wink-micro-app/vendor/esp_idfv61/<app>/wink-app.json`:
  * schema: app_name = esp_idfv61_<dir>, upstream {vendor, version, source_dir, files}
  * each pinned source exists and its normalized SHA-256 matches (LF, no trailing
    whitespace, single trailing newline -> stable across git checkouts/OS)
  * optional `--idf-tree <path>`: diff pinned sources against the pinned IDF tree
    (`<tree>/<source_dir>/<file>`); `--allow-hash-drift` downgrades mismatches to
    warnings (used for the non-blocking v5.1 probe, where only presence is enforced).

Usage:
  check_vendor_app_upstream.py [--root DIR] [--idf-tree DIR] [--allow-hash-drift]
Exit code: 0 = pass, 1 = violations.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

DEFAULT_ROOT = "wink-micro-app/vendor/esp_idfv61"


def normalize(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = "\n".join(line.rstrip() for line in text.split("\n")).rstrip("\n") + "\n"
    return text


def digest(path: Path) -> str:
    return hashlib.sha256(normalize(path.read_text(encoding="utf-8-sig")).encode("utf-8")).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=DEFAULT_ROOT, help="vendor suite root (default: repo-relative)")
    parser.add_argument("--idf-tree", default=None, help="pinned ESP-IDF tree for source diff")
    parser.add_argument("--allow-hash-drift", action="store_true",
                        help="report IDF-tree hash mismatches as warnings (presence still enforced)")
    parser.add_argument("--allow-missing", action="store_true",
                        help="report IDF-tree missing files as warnings (probe mode for older IDF trees)")
    parser.add_argument("--print-hashes", action="store_true",
                        help="print normalized SHA-256 for every source (maintainer pin refresh) and exit")
    args = parser.parse_args(argv)

    root = Path(args.root)
    if not root.is_dir():
        print(f"[vendor-upstream] root not found: {root}", file=sys.stderr)
        return 1

    if args.print_hashes:
        for app_dir in sorted(p for p in root.iterdir() if p.is_dir()):
            for src in sorted(app_dir.glob("*.c")):
                print(f"{app_dir.name}/{src.name} {digest(src)}")
        return 0

    errors: list[str] = []
    warnings: list[str] = []
    apps = sorted(p for p in root.iterdir() if p.is_dir() and (p / "wink-app.json").is_file())
    if not apps:
        errors.append(f"no vendor apps found under {root}")

    for app_dir in apps:
        manifest = json.loads((app_dir / "wink-app.json").read_text(encoding="utf-8"))
        expected_name = f"esp_idfv61_{app_dir.name}"
        if manifest.get("app_name") != expected_name:
            errors.append(f"{app_dir.name}: app_name must be '{expected_name}'")
        upstream = manifest.get("upstream") or {}
        for key in ("vendor", "version", "source_dir", "files"):
            if not upstream.get(key):
                errors.append(f"{app_dir.name}: upstream.{key} missing")
        files = upstream.get("files") or {}
        for name, pinned in sorted(files.items()):
            local = app_dir / name
            if not local.is_file():
                errors.append(f"{app_dir.name}: pinned source missing on disk: {name}")
                continue
            actual = digest(local)
            if actual != pinned:
                errors.append(f"{app_dir.name}: {name} hash mismatch (disk={actual[:12]} pin={str(pinned)[:12]})")
            if args.idf_tree:
                upstream_file = Path(args.idf_tree) / upstream["source_dir"] / name
                if not upstream_file.is_file():
                    message = f"{app_dir.name}: IDF tree source missing: {upstream_file}"
                    if args.allow_missing:
                        warnings.append(message)
                    else:
                        errors.append(message)
                elif digest(upstream_file) != pinned:
                    message = (f"{app_dir.name}: {name} differs from IDF tree "
                               f"({upstream['source_dir']})")
                    if args.allow_hash_drift:
                        warnings.append(message)
                    else:
                        errors.append(message)
        for src in sorted(app_dir.glob("*.c")):
            if src.name not in files:
                warnings.append(f"{app_dir.name}: source not pinned in upstream.files: {src.name}")

    print(f"[vendor-upstream] root={root} apps={len(apps)} idf_tree={args.idf_tree or '-'} "
          f"errors={len(errors)} warnings={len(warnings)}")
    for w in warnings:
        print(f"[vendor-upstream] WARN {w}")
    for e in errors[:50]:
        print(f"[vendor-upstream] FAIL {e}")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
