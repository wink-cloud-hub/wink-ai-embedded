#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Vendored harvested-header gate for wink-ai-embedded (ADR-0086 / P0-B).

Checks a vendored `wink-micro-os/frameworks/esp_idf/include` tree produced by the
closed-source SDK harvester (PLAN-20260925-SDK-HARVESTER-ENGINE, Task 7):

  1. manifest.json self-anchor: recompute `hash` over stable fields (excluding
     `hash` / `build_host` / `file_hashes`) and compare; verify every entry in
     `file_hashes` (per-file sha256) and that no file is left uncovered;
  2. generated headers (banner `AUTOMATICALLY GENERATED FILE`) must carry the same
     `Manifest: <hash>`; headers without the banner are hand-written (stubs/exempt
     files) and are not covered by the artifact hash — with `--rules` the gate
     enforces that `exempt_files` were NOT overwritten by the artifact;
  3. docs fragments: `api-coverage-matrix.inc.md` / `include-closure-inventory.inc.md`
     machine-readable summaries must agree with the manifest, the generated-header
     set, the API declarations, and the include rows;
  4. optional `--rules <esp_idf.yaml>`: every emitted `#include` must be declared in
     allowlist/rewrite/stub tables; `verify.abi_headers`/`macro_headers` must exist.

Usage:
  python .github/scripts/check_harvested_headers.py [--include-dir DIR] [--rules YAML]

Exit code: 0 = pass, 1 = violations.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from fnmatch import fnmatch
from pathlib import Path

BANNER_RE = re.compile(r"Manifest:\s*([0-9a-f]{16})")
API_SUMMARY_RE = re.compile(r"<!-- harvest-summary: (\{.*\}) -->")
INV_SUMMARY_RE = re.compile(r"<!-- harvest-inventory-summary: (\{.*\}) -->")
API_ROW_RE = re.compile(r"^\| `([^`]+)` \| `([^`]+)` \| (✅ Supported|🚫 Out-of-scope) \|")
FILE_ROW_RE = re.compile(r"^\| `([^`]+)` \| (\d+) \| (\d+) \| (\d+) \|$")
INV_ROW_RE = re.compile(r"^\| `([^`]+)` \| ([a-z]+)(?: → `([^`]+)`)? \|")
INCLUDE_RE = re.compile(r'#\s*include\s*"([^"]+)"')
GEN_MARK = "AUTOMATICALLY GENERATED FILE"
KNOWN_INV_STATUS = {"passthrough", "rewrite", "stub", "denied", "unlisted"}


def recompute_manifest_hash(manifest: dict) -> str:
    data = dict(manifest)
    for key in ("hash", "build_host", "file_hashes"):
        data.pop(key, None)
    return hashlib.sha256(json.dumps(data, sort_keys=True, ensure_ascii=False).encode()).hexdigest()[:16]


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rules(path: Path, sdk_tag: str) -> dict:
    try:
        import yaml  # type: ignore
    except ModuleNotFoundError as exc:  # pragma: no cover
        raise SystemExit("--rules requires PyYAML: python -m pip install pyyaml") from exc
    rule = yaml.safe_load(path.read_text(encoding="utf-8"))
    overlay = ((rule.get("version_overlays") or {}).get(sdk_tag)) or {}
    for key in ("include_allowlist", "include_denylist", "stub_headers", "exempt_files"):
        if key in overlay:
            rule[key] = list(overlay[key])
            continue
        base = list(rule.get(key) or [])
        removed = set(overlay.get(f"{key}_remove") or [])
        added = [x for x in (overlay.get(f"{key}_add") or []) if x not in base and x not in removed]
        rule[key] = [x for x in base if x not in removed] + added
    rule["include_rewrite"] = {**(rule.get("include_rewrite") or {}), **(overlay.get("include_rewrite") or {})}
    return rule


def main(argv: list[str] | None = None) -> int:
    here = Path(__file__).resolve()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--include-dir",
        default=str(here.parents[2] / "wink-micro-os" / "frameworks" / "esp_idf" / "include"),
    )
    parser.add_argument("--rules", default=None, help="closed-source rules/esp_idf.yaml（可选）")
    args = parser.parse_args(argv)
    root = Path(args.include_dir).resolve()
    errors: list[str] = []
    warnings: list[str] = []

    if not root.is_dir():
        print(f"[harvest-gate] include dir not found: {root}", file=sys.stderr)
        return 1
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        print(f"[harvest-gate] manifest.json missing under {root}", file=sys.stderr)
        return 1
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    mhash = str(manifest.get("hash") or "")
    if not mhash:
        errors.append("manifest.hash missing")
    elif recompute_manifest_hash(manifest) != mhash:
        errors.append("manifest.hash mismatch (self-anchor broken)")

    recorded = manifest.get("file_hashes") or {}
    actual = {
        p.relative_to(root).as_posix(): p
        for p in sorted(root.rglob("*"))
        if p.is_file() and p.name != "manifest.json"
    }
    for rel, digest in sorted(recorded.items()):
        p = actual.get(rel)
        if p is None:
            errors.append(f"file_hashes entry missing on disk: {rel}")
        elif sha256_file(p) != digest:
            errors.append(f"file tampered (sha256 mismatch): {rel}")
    for rel in sorted(set(actual) - set(recorded)):
        p = actual[rel]
        if p.suffix == ".h" and GEN_MARK in p.read_text(encoding="utf-8", errors="replace"):
            errors.append(f"generated header not covered by manifest.file_hashes: {rel}")
        elif rel in ("wink_sla.h", "NOTICE.inc", "api-coverage-matrix.inc.md", "include-closure-inventory.inc.md"):
            errors.append(f"generated file not covered by manifest.file_hashes: {rel}")

    headers = sorted(root.rglob("*.h"))
    generated = [p for p in headers if GEN_MARK in p.read_text(encoding="utf-8", errors="replace")]
    generated_rels = {p.relative_to(root).as_posix() for p in generated}
    frag_rels = generated_rels - {"wink_sla.h"}  # SLA 桩头不属于 API 覆盖片段行集合
    banner_mismatch = 0
    for p in generated:
        m = BANNER_RE.search(p.read_text(encoding="utf-8", errors="replace"))
        rel = p.relative_to(root).as_posix()
        if not m:
            errors.append(f"{rel}: generated header missing Manifest banner")
        elif m.group(1) != mhash:
            banner_mismatch += 1
    if banner_mismatch:
        errors.append(f"{banner_mismatch} generated headers banner hash != manifest {mhash}")
    unmarked = [p.relative_to(root).as_posix() for p in headers if p not in generated]

    rules = None
    if args.rules:
        rules = load_rules(Path(args.rules), str(manifest.get("sdk_tag") or ""))
        exempt = list(rules.get("exempt_files") or [])
        for p in generated:
            rel = p.relative_to(root).as_posix()
            if any(fnmatch(rel, pat) for pat in exempt):
                errors.append(f"exempt file was overwritten by generated artifact: {rel}")
    else:
        warnings.append(f"{len(unmarked)} hand-written headers skipped (pass --rules to enforce exempt protection)")

    api_path = root / "api-coverage-matrix.inc.md"
    inv_path = root / "include-closure-inventory.inc.md"
    if not api_path.is_file() or not inv_path.is_file():
        errors.append("docs fragments missing (api-coverage-matrix / include-closure-inventory)")
    else:
        api_text = api_path.read_text(encoding="utf-8")
        m = API_SUMMARY_RE.search(api_text)
        if not m:
            errors.append("api fragment harvest-summary missing")
        else:
            summary = json.loads(m.group(1))
            if summary.get("manifest") != mhash:
                errors.append("api fragment manifest hash mismatch")
            for key in ("sdk_tag", "sdk_sha", "soc", "idf"):
                if key in manifest and summary.get(key) != manifest.get(key):
                    errors.append(f"api fragment {key}={summary.get(key)!r} != manifest {manifest.get(key)!r}")
            sla = manifest.get("sla_stats") or {}
            for key in ("files", "supported", "out_of_scope"):
                if summary.get(key) != sla.get(key):
                    errors.append(f"api fragment {key}={summary.get(key)} != manifest.sla_stats.{key}={sla.get(key)}")
            file_rows = {mm.group(1) for mm in (FILE_ROW_RE.match(ln) for ln in api_text.splitlines()) if mm}
            if file_rows != frag_rels:
                errors.append(
                    "api fragment file set mismatch "
                    f"(fragment-only={sorted(file_rows - frag_rels)[:5]}, tree-only={sorted(frag_rels - file_rows)[:5]})"
                )
            rows = [mm for mm in (API_ROW_RE.match(ln) for ln in api_text.splitlines()) if mm]
            n_supported = sum(1 for mm in rows if mm.group(3) == "✅ Supported")
            n_oos = len(rows) - n_supported
            if n_supported != sla.get("supported") or n_oos != sla.get("out_of_scope"):
                errors.append(
                    f"api detail rows (supported={n_supported}, oos={n_oos}) != manifest "
                    f"({sla.get('supported')}, {sla.get('out_of_scope')})"
                )
            for mm in rows:
                name, rel = mm.group(1), mm.group(2)
                hp = root / rel
                if not hp.is_file():
                    errors.append(f"api row header missing: {rel}")
                elif not re.search(rf"\b{re.escape(name)}\s*\(", hp.read_text(encoding="utf-8", errors="replace")):
                    errors.append(f"api row '{name}' not declared in {rel}")

        inv_text = inv_path.read_text(encoding="utf-8")
        mi = INV_SUMMARY_RE.search(inv_text)
        if not mi:
            errors.append("inventory fragment harvest-inventory-summary missing")
        else:
            isum = json.loads(mi.group(1))
            if isum.get("manifest") != mhash:
                errors.append("inventory fragment manifest hash mismatch")
            inv_rows = [mm for mm in (INV_ROW_RE.match(ln) for ln in inv_text.splitlines()) if mm]
            if isum.get("unique_includes") != len(inv_rows):
                errors.append(f"inventory summary unique_includes={isum.get('unique_includes')} != rows={len(inv_rows)}")
            by_status: dict[str, int] = {}
            for mm in inv_rows:
                status = mm.group(2)
                by_status[status] = by_status.get(status, 0) + 1
                if status not in KNOWN_INV_STATUS:
                    errors.append(f"inventory row unknown status: {status}")
            if isum.get("by_status") != by_status:
                errors.append(f"inventory by_status {isum.get('by_status')} != rows {by_status}")

    if rules is not None:
        allow = set(rules.get("include_allowlist") or [])
        rewrite = set((rules.get("include_rewrite") or {}).keys())
        stub = set(rules.get("stub_headers") or [])
        for p in generated:
            rel = p.relative_to(root).as_posix()
            for inc in INCLUDE_RE.findall(p.read_text(encoding="utf-8", errors="replace")):
                if inc == "wink_sla.h":
                    continue  # Harvester 随产物下发的 SLA 桩头，不属于厂商 include 三表
                if inc not in allow and inc not in rewrite and inc not in stub:
                    errors.append(f"{rel}: emitted include not declared in rules tables: {inc}")
        for key in ("abi_headers", "macro_headers"):
            for rel in (rules.get("verify") or {}).get(key, []):
                if not (root / rel).is_file():
                    errors.append(f"rule.verify.{key} header missing: {rel}")
        rule_cfgs = {(c.get("soc"), c.get("idf")) for c in rules.get("configs") or []}
        for c in manifest.get("configs") or []:
            if (c.get("soc"), c.get("idf")) not in rule_cfgs:
                errors.append(f"manifest config {c.get('soc')}/{c.get('idf')} not declared in rules.configs")

    print(
        f"[harvest-gate] root={root} manifest={mhash or '-'} generated_headers={len(generated)} "
        f"unmarked={len(unmarked)} errors={len(errors)}"
    )
    for w in warnings:
        print(f"[harvest-gate] WARN {w}")
    for e in errors[:50]:
        print(f"[harvest-gate] FAIL {e}")
    if len(errors) > 50:
        print(f"[harvest-gate] ... and {len(errors) - 50} more")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
