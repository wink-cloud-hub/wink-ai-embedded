#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
run_gates.py
============
Unified CI Gate Runner for WinkMicroOS ESP-IDF Classification Baseline.

Exit Codes:
  0: All executed rules passed (warnings and infos permitted)
  1: One or more findings with severity='error' (CI hard block)
  2: Executor internal error (zero rules executed, module load failure, bad config, etc.)
"""

import os
import sys
import io
import json
import argparse
import fnmatch
import importlib
from pathlib import Path
from datetime import datetime, timezone
import yaml


GATES_DIR = Path(__file__).resolve().parent
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))

try:
    from gate_context import build_context, normalize_posix_path
except ImportError as e:
    sys.stderr.write(f"FATAL [Executor Exit 2]: Failed to import gate_context: {e}\n")
    sys.exit(2)


def parse_args(args=None):
    parser = argparse.ArgumentParser(
        description="WinkMicroOS ESP-IDF Gate System Runner",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--mode",
        choices=["pr", "nightly"],
        default="pr",
        help="Trigger mode (pr: quick PR gate, nightly: full periodic regression)",
    )
    parser.add_argument(
        "--gate",
        action="append",
        type=int,
        choices=[1, 2, 3, 4, 5],
        help="Limit execution to specified Gate number(s) (e.g. --gate 1)",
    )
    parser.add_argument(
        "--rule",
        help="Limit execution to a single rule ID (e.g. g1.can_check_mark)",
    )
    parser.add_argument(
        "--changed-files",
        help="Path to text file containing git diff changed files (one per line)",
    )
    parser.add_argument(
        "--allow-empty-diff",
        action="store_true",
        help="Explicitly allow empty diff in PR mode (prevents error exit)",
    )
    parser.add_argument(
        "--config",
        default=str(GATES_DIR / "gates.yaml"),
        help="Path to gates.yaml registration file",
    )
    parser.add_argument(
        "--manifest",
        help="Override path to checklist.data.json",
    )
    parser.add_argument(
        "--catalog",
        help="Override path to capability-catalog.yaml",
    )
    parser.add_argument(
        "--quarantine",
        help="Override path to quarantine.yaml",
    )
    parser.add_argument(
        "--output-json",
        help="Path to write structured JSON report output",
    )
    parser.add_argument(
        "--require-executed",
        type=int,
        default=None,
        help=(
            "Minimum number of rules that must actually execute. Guards against an "
            "all-SKIP run (empty diff, unavailable trigger) being reported as a clean pass."
        ),
    )
    parser.add_argument(
        "--no-fail",
        action="store_true",
        help="Always exit with code 0 even if errors are found (for local debug only)",
    )
    return parser.parse_args(args)


def load_gates_config(config_path: str | Path) -> dict:
    c_path = Path(config_path)
    if not c_path.exists():
        sys.stderr.write(f"FATAL [Executor Exit 2]: gates.yaml not found at: {c_path}\n")
        sys.exit(2)
    try:
        with open(c_path, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)
        if not isinstance(cfg, dict) or "rules" not in cfg:
            raise ValueError("gates.yaml must be a dict containing a 'rules' list.")
        return cfg
    except Exception as e:
        sys.stderr.write(f"FATAL [Executor Exit 2]: Failed to parse gates.yaml: {e}\n")
        sys.exit(2)


def main(args=None):
    opts = parse_args(args)

    gates_cfg = load_gates_config(opts.config)
    all_rules = gates_cfg.get("rules", [])

    # Filter rules for current mode
    selected_rules = []
    for r in all_rules:
        if opts.mode not in r.get("modes", []):
            continue
        if opts.gate and r.get("gate") not in opts.gate:
            continue
        if opts.rule and r.get("id") != opts.rule:
            continue
        selected_rules.append(r)

    # DEFENSE: Zero active rules MUST fail with exit code 2
    if not selected_rules:
        sys.stderr.write(
            f"FATAL [Executor Exit 2]: Zero effective rules found for mode='{opts.mode}', "
            f"gate={opts.gate}, rule={opts.rule}. Refusing to pass silently!\n"
        )
        sys.exit(2)

    # Build Context
    try:
        context = build_context(
            manifest_path=opts.manifest,
            catalog_path=opts.catalog,
            quarantine_path=opts.quarantine,
            changed_files_path=opts.changed_files,
            mode=opts.mode,
            allow_empty_diff=opts.allow_empty_diff,
        )
    except Exception as e:
        sys.stderr.write(f"FATAL [Executor Exit 2]: Failed to build gate context: {e}\n")
        sys.exit(2)

    print("=" * 72)
    print(f"  WinkMicroOS ESP-IDF Gate System (Mode: {opts.mode.upper()})")
    print(f"  Run Time (UTC): {context['now_utc'].isoformat()}")
    print(f"  Active Rules Registered: {len(selected_rules)}")
    print(f"  Changed Files in Scope: {len(context['changed_files'])}")
    print("=" * 72)

    all_findings = []
    rule_results = {}
    skipped_count = 0
    executed_count = 0

    for rule in selected_rules:
        rule_id = rule.get("id", "unknown_rule")
        gate_num = rule.get("gate", 0)
        module_path = rule.get("module")

        # 1. Trigger paths check
        trigger = rule.get("trigger")
        trigger_paths = rule.get("trigger_paths", [])

        if trigger == "git_diff":
            if trigger_paths:
                matched = False
                for cf in context["changed_files"]:
                    for pat in trigger_paths:
                        norm_pat = normalize_posix_path(pat)
                        if fnmatch.fnmatch(cf, norm_pat):
                            matched = True
                            break
                    if matched:
                        break
                if not matched:
                    skipped_count += 1
                    rule_results[rule_id] = {
                        "gate": gate_num,
                        "status": "SKIP",
                        "reason": f"No changed files matched trigger_paths {trigger_paths}",
                    }
                    continue
            else:
                # Trigger git_diff with no trigger_paths: check if any changed_files exist
                if not context["changed_files"]:
                    skipped_count += 1
                    rule_results[rule_id] = {
                        "gate": gate_num,
                        "status": "SKIP",
                        "reason": "No changed files in git diff",
                    }
                    continue

        # 2. Dynamic module load
        if not module_path:
            sys.stderr.write(f"FATAL [Executor Exit 2]: Rule '{rule_id}' has no module specified.\n")
            sys.exit(2)

        try:
            mod = importlib.import_module(module_path)
        except Exception as e:
            sys.stderr.write(
                f"FATAL [Executor Exit 2]: Failed to load module '{module_path}' for rule '{rule_id}': {e}\n"
            )
            sys.exit(2)

        if not hasattr(mod, "run") or not callable(mod.run):
            sys.stderr.write(
                f"FATAL [Executor Exit 2]: Module '{module_path}' does not define callable 'run(context, config)'.\n"
            )
            sys.exit(2)

        # 3. Execute rule
        config = rule.get("config", {})
        default_sev = rule.get("severity", "error")
        override_sev = rule.get("mode_severity_override", {}).get(opts.mode)
        effective_rule_sev = override_sev if override_sev else default_sev

        executed_count += 1
        findings_before = len(all_findings)
        try:
            findings = mod.run(context, config) or []
        except Exception as e:
            # Rule internal crash converted to error Finding
            findings = [
                {
                    "rule_id": rule_id,
                    "gate": gate_num,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Unhandled exception inside rule execution: {e}",
                }
            ]

        # Normalize findings
        for f in findings:
            if "rule_id" not in f:
                f["rule_id"] = rule_id
            f["gate"] = gate_num

            # Apply severity override if the rule sets severity and finding did not specify
            if "severity" not in f:
                f["severity"] = effective_rule_sev
            elif effective_rule_sev == "warning" and f["severity"] == "error":
                f["severity"] = "warning"

            all_findings.append(f)

        new_findings = all_findings[findings_before:]
        errs = sum(1 for f in new_findings if f["severity"] == "error")
        warns = sum(1 for f in new_findings if f["severity"] == "warning")

        if errs > 0:
            status = "FAIL"
        elif warns > 0:
            status = "WARN"
        else:
            status = "PASS"

        rule_results[rule_id] = {
            "gate": gate_num,
            "status": status,
            "errors": errs,
            "warnings": warns,
        }

    # Sort all findings stably by (gate, rule_id, display_id or 0, message)
    all_findings.sort(
        key=lambda f: (
            f.get("gate", 0),
            f.get("rule_id", ""),
            f.get("display_id") if f.get("display_id") is not None else 0,
            f.get("message", ""),
        )
    )

    # Compute overall statistics
    total_errors = sum(1 for f in all_findings if f["severity"] == "error")
    total_warnings = sum(1 for f in all_findings if f["severity"] == "warning")
    total_infos = sum(1 for f in all_findings if f["severity"] == "info")
    passed_count = sum(1 for r, data in rule_results.items() if data["status"] == "PASS")

    # Gate-level summaries
    gate_summary = {}
    for g in [1, 2, 3, 4]:
        g_rules = {k: v for k, v in rule_results.items() if v["gate"] == g}
        if not g_rules:
            continue
        g_errors = sum(v.get("errors", 0) for v in g_rules.values())
        g_warnings = sum(v.get("warnings", 0) for v in g_rules.values())
        all_skipped = all(v["status"] == "SKIP" for v in g_rules.values())

        if g_errors > 0:
            g_status = "FAIL"
        elif all_skipped:
            g_status = "SKIP"
        elif g_warnings > 0:
            g_status = "WARN"
        else:
            g_status = "PASS"

        gate_summary[f"gate_{g}"] = {
            "status": g_status,
            "executed": sum(1 for v in g_rules.values() if v["status"] != "SKIP"),
            "skipped": sum(1 for v in g_rules.values() if v["status"] == "SKIP"),
            "errors": g_errors,
            "warnings": g_warnings,
        }

    # Print Report
    print("\n--- Gate Summary ---")
    for g_key, g_info in sorted(gate_summary.items()):
        status_str = f"[{g_info['status']}]"
        print(
            f"  {g_key.upper()}: {status_str:<6} "
            f"(Executed: {g_info['executed']}, Skipped: {g_info['skipped']}, "
            f"Errors: {g_info['errors']}, Warnings: {g_info['warnings']})"
        )

    print("\n--- Rule Execution Details ---")
    for r_id, r_info in rule_results.items():
        st = r_info["status"]
        if st == "SKIP":
            print(f"  [{st:<4}] {r_id} (Reason: {r_info.get('reason')})")
        else:
            print(f"  [{st:<4}] {r_id} (Errors: {r_info['errors']}, Warnings: {r_info['warnings']})")

    if all_findings:
        print("\n--- Findings ---")
        for f in all_findings:
            sev = f["severity"].upper()
            did = f"#{f['display_id']}" if f.get("display_id") is not None else ""
            eid = f"({f['entry_id']})" if f.get("entry_id") else ""
            cid = f"[{f['config_id']}]" if f.get("config_id") else ""
            loc = " ".join(filter(None, [did, eid, cid]))
            print(f"  [{sev}] [{f['rule_id']}] {loc}: {f['message']}")

    print("\n" + "=" * 72)
    print(
        f"  Total Rules: {len(selected_rules)} | Executed: {executed_count} | Skipped: {skipped_count}\n"
        f"  Errors: {total_errors} | Warnings: {total_warnings} | Infos: {total_infos}"
    )
    print("=" * 72)

    # Output JSON Report if requested
    report_dict = {
        "run_at": context["now_utc"].isoformat(),
        "mode": opts.mode,
        "spec_version": "2.0.0",
        "summary": {
            "total_rules": len(selected_rules),
            "executed": executed_count,
            "skipped": skipped_count,
            "passed": passed_count,
            "errors": total_errors,
            "warnings": total_warnings,
            "infos": total_infos,
        },
        "gate_summary": gate_summary,
        "findings": all_findings,
    }

    if opts.output_json:
        out_p = Path(opts.output_json)
        out_p.parent.mkdir(parents=True, exist_ok=True)
        with open(out_p, "w", encoding="utf-8") as f:
            json.dump(report_dict, f, indent=2, ensure_ascii=False)
        print(f"[*] Report successfully written to: {out_p.resolve()}")

    if total_errors > 0 and not opts.no_fail:
        print("\n[RESULT] FAILED: Gate checks identified blocking errors (exit code 1).")
        sys.exit(1)

    required = getattr(opts, "require_executed", None)
    if required and executed_count < required:
        print(
            f"\n[RESULT] FAILED: only {executed_count} of the required {required} rules actually "
            f"executed. An under-executed run is indistinguishable from a clean pass, "
            f"so it must not be reported as success (exit code 1)."
        )
        sys.exit(1)

    print("\n[RESULT] PASSED: Gate checks completed successfully (exit code 0).")
    sys.exit(0)


if __name__ == "__main__":
    if hasattr(sys.stdout, "buffer") and not isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "buffer") and not isinstance(sys.stderr, io.TextIOWrapper):
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
    main()
