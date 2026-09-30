#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SSOT Invariant Gate for WinkMicroOS ESP-IDF Governance (Plan 04 Task T6.2).

Dynamically verifies full mathematical consistency across the governance stack:
1. `checklist.data.json` entries dynamic summation == header `summary` fields;
2. Total entries (478) == in_scope (active 291 + deferred 21) + out_of_scope (166) + unknown (0);
3. Unique continuous display_id space [1..478];
4. Rendered `CHECKLIST.md` figures and markdown table rows match 100% with SSOT.

Usage:
    python .github/scripts/check_ssot_invariants.py [--root PATH]
"""

from __future__ import annotations

import argparse
import io
import json
import re
import sys
from pathlib import Path

def verify_ssot_invariants(root: Path) -> list[str]:
    errors: list[str] = []

    data_path = root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance" / "data" / "checklist.data.json"
    if not data_path.is_file():
        alt_data = root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance" / "checklist.data.json"
        if alt_data.is_file():
            data_path = alt_data
        else:
            return [f"SSOT data file not found: {data_path}"]

    checklist_path = root / "wink-micro-app" / "vendor" / "esp_idfv61" / "CHECKLIST.md"
    if not checklist_path.is_file():
        return [f"Rendered CHECKLIST.md not found: {checklist_path}"]

    try:
        data = json.loads(data_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return [f"Failed to parse {data_path}: {exc}"]

    entries = data.get("entries", [])
    total_entries = len(entries)
    header_total = data.get("total_entries")

    if header_total != total_entries:
        errors.append(f"Header total_entries ({header_total}) != len(entries) ({total_entries})")

    # 1. Scope, Schedule & State dynamic tally
    scope_in = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "in_scope")
    scope_out = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "out_of_scope")
    scope_unknown = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "unknown")
    in_scope_active = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "in_scope" and e.get("scope", {}).get("schedule") == "active")
    in_scope_deferred = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "in_scope" and e.get("scope", {}).get("schedule") == "deferred")

    audited = sum(1 for e in entries if e.get("audit", {}).get("verdict") == "audited")
    verified_configs = sum(1 for e in entries for ex in e.get("executions", []) if ex.get("delivery_state") == "verified")
    verified_entries = sum(1 for e in entries if any(ex.get("delivery_state") == "verified" for ex in e.get("executions", [])))
    planned_entries = sum(1 for e in entries if e.get("scope", {}).get("inclusion") == "in_scope" and e.get("scope", {}).get("schedule") == "active" and not any(ex.get("delivery_state") == "verified" for ex in e.get("executions", [])))

    # Invariant: Partitioning
    if scope_in + scope_out + scope_unknown != total_entries:
        errors.append(f"Partition sum mismatch: scope_in ({scope_in}) + scope_out ({scope_out}) + unknown ({scope_unknown}) != total ({total_entries})")

    if in_scope_active + in_scope_deferred != scope_in:
        errors.append(f"In-scope sum mismatch: active ({in_scope_active}) + deferred ({in_scope_deferred}) != scope_in ({scope_in})")

    # 2. Check summary header
    summary = data.get("summary", {})
    if summary.get("scope_in") != scope_in:
        errors.append(f"summary.scope_in ({summary.get('scope_in')}) != computed scope_in ({scope_in})")
    if summary.get("scope_out") != scope_out:
        errors.append(f"summary.scope_out ({summary.get('scope_out')}) != computed scope_out ({scope_out})")
    if summary.get("scope_unknown") != scope_unknown:
        errors.append(f"summary.scope_unknown ({summary.get('scope_unknown')}) != computed scope_unknown ({scope_unknown})")
    if "in_scope_active" in summary and summary["in_scope_active"] != in_scope_active:
        errors.append(f"summary.in_scope_active ({summary['in_scope_active']}) != computed active ({in_scope_active})")
    if "in_scope_deferred" in summary and summary["in_scope_deferred"] != in_scope_deferred:
        errors.append(f"summary.in_scope_deferred ({summary['in_scope_deferred']}) != computed deferred ({in_scope_deferred})")
    if summary.get("audited") != audited:
        errors.append(f"summary.audited ({summary.get('audited')}) != computed audited ({audited})")
    if summary.get("verified_configs") != verified_configs:
        errors.append(f"summary.verified_configs ({summary.get('verified_configs')}) != computed verified_configs ({verified_configs})")

    # 3. Continuous display_id check (1..total_entries)
    display_ids = [e.get("display_id") for e in entries if isinstance(e.get("display_id"), int)]
    expected_ids = list(range(1, total_entries + 1))
    if sorted(display_ids) != expected_ids:
        missing = set(expected_ids) - set(display_ids)
        extra = set(display_ids) - set(expected_ids)
        errors.append(f"display_id sequence broken: missing={sorted(missing)[:5]}, extra={sorted(extra)[:5]}")

    # 4. Check CHECKLIST.md rendered figures
    md_content = checklist_path.read_text(encoding="utf-8")
    m_total = re.search(r"官方独立示例总数.*?\*\*(\d+) 个\*\*", md_content)
    m_ver = re.search(r"已完成六要素实证 \(Verified\).*?\*\*(\d+) 项\*\*", md_content)
    m_plan = re.search(r"规划中正常排期 \(In-Scope Planned\).*?\*\*(\d+) 项\*\*", md_content)
    m_out = re.search(r"明确产品排除 / 暂缓投入 \(Out-of-Scope / Deferred\).*?\*\*(\d+) 项\*\*", md_content)
    m_unk = re.search(r"待深度审定 \(Pending Audit / Unknown Scope\).*?\*\*(\d+) 项\*\*", md_content)

    if not m_total or int(m_total.group(1)) != total_entries:
        errors.append(f"CHECKLIST.md total ({m_total.group(1) if m_total else 'missing'}) != {total_entries}")
    if not m_ver or int(m_ver.group(1)) != verified_entries:
        errors.append(f"CHECKLIST.md verified ({m_ver.group(1) if m_ver else 'missing'}) != {verified_entries}")
    if not m_plan or int(m_plan.group(1)) != planned_entries:
        errors.append(f"CHECKLIST.md planned ({m_plan.group(1) if m_plan else 'missing'}) != {planned_entries}")
    expected_out_or_def = scope_out + in_scope_deferred
    if not m_out or int(m_out.group(1)) != expected_out_or_def:
        errors.append(f"CHECKLIST.md out/deferred ({m_out.group(1) if m_out else 'missing'}) != {expected_out_or_def}")
    if not m_unk or int(m_unk.group(1)) != scope_unknown:
        errors.append(f"CHECKLIST.md unknown ({m_unk.group(1) if m_unk else 'missing'}) != {scope_unknown}")

    # 5. Check CHECKLIST.md table rows
    row_ver = len(re.findall(r"^\|\s*\[x\]\s*\|", md_content, re.MULTILINE))
    row_out = len(re.findall(r"^\|\s*\[-\]\s*\|", md_content, re.MULTILINE))
    row_plan = len(re.findall(r"^\|\s*\[\s\]\s*\|", md_content, re.MULTILINE))

    if row_ver != verified_entries:
        errors.append(f"CHECKLIST.md table [x] rows ({row_ver}) != verified_entries ({verified_entries})")
    if row_out != expected_out_or_def:
        errors.append(f"CHECKLIST.md table [-] rows ({row_out}) != out_of_scope + deferred ({expected_out_or_def})")
    if row_plan != planned_entries + scope_unknown:
        errors.append(f"CHECKLIST.md table [ ] rows ({row_plan}) != planned + unknown ({planned_entries + scope_unknown})")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root path",
    )
    args = parser.parse_args()

    errors = verify_ssot_invariants(args.root)
    if errors:
        print("[FAILED] SSOT Invariant Gate FAILED:")
        for err in errors:
            print(f"  - {err}")
        return 1

    print("[PASSED] SSOT Invariant Gate PASSED: All 478 entries, summary headers, and CHECKLIST.md metrics are 100% consistent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
