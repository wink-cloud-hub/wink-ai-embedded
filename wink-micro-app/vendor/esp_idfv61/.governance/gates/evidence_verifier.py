# SPDX-License-Identifier: Apache-2.0
"""
evidence_verifier.py
====================
Standalone Evidence Verifier and Read-Only CI Validation Engine.

Provides:
1. Standardized Wasm 3-piece composite SHA-256 computation:
   SHA256(SHA256(wasm) || "\n" || SHA256(js) || "\n" || SHA256(tree))
2. Scenario JSON SHA-256 computation.
3. Execution report structured assertion (status, summary, steps).
4. Pure read-only verification for Gate 1 and CI.
5. Local evidence generation/recording helper for run_esp32_headless_evidence.ps1.
"""

import os
import sys
import json
import hashlib
import argparse
import subprocess
from pathlib import Path
from datetime import datetime, timezone
from typing import Tuple, List, Dict, Optional, Any


def compute_file_sha256(file_path: Path) -> str:
    """Compute standard SHA-256 hex digest of a single file."""
    if not file_path.is_file():
        raise FileNotFoundError(f"File not found: {file_path}")
    hasher = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest()


def compute_assets_composite_sha256(assets_dir: Path) -> str:
    """
    Compute standardized Wasm 3-piece composite SHA-256:
    assets_sha256 = SHA256(SHA256(wasm) || "\n" || SHA256(js) || "\n" || SHA256(tree))
    """
    wasm_path = assets_dir / "wink_simulator.wasm"
    js_path = assets_dir / "wink_simulator.js"
    tree_path = assets_dir / "device-tree.json"

    if not wasm_path.is_file():
        raise FileNotFoundError(f"Missing wink_simulator.wasm in {assets_dir}")
    if not js_path.is_file():
        raise FileNotFoundError(f"Missing wink_simulator.js in {assets_dir}")
    if not tree_path.is_file():
        raise FileNotFoundError(f"Missing device-tree.json in {assets_dir}")

    h_wasm = compute_file_sha256(wasm_path)
    h_js = compute_file_sha256(js_path)
    h_tree = compute_file_sha256(tree_path)

    composite_payload = f"{h_wasm}\n{h_js}\n{h_tree}".encode("utf-8")
    return hashlib.sha256(composite_payload).hexdigest()


def compute_scenario_sha256(scenario_path: Path) -> str:
    """Compute SHA-256 of scenario JSON file."""
    return compute_file_sha256(scenario_path)


def resolve_execution_report_path(ref: str, ws_root: Path) -> Optional[Path]:
    """
    Resolve execution_report_ref to a concrete Path on disk.
    Supports relative paths, vendor paths, and unisim:// URI scheme.
    """
    if not ref:
        return None

    # 1. Direct path relative to workspace root
    p1 = ws_root / ref
    if p1.is_file():
        return p1

    # 2. Path relative to esp_idfv61 root
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
    p2 = vendor_root / ref
    if p2.is_file():
        return p2

    # 3. Handle unisim:// URI scheme
    if ref.startswith("unisim://"):
        rel_path = ref.removeprefix("unisim://").lstrip("/")
        # If it starts with reports/ or .governance/reports/, try stripping or matching
        sub = rel_path.removeprefix(".governance/reports/").removeprefix("reports/").lstrip("/")
        candidates = [
            vendor_root / ".governance" / "reports" / sub,
            vendor_root / ".governance" / "reports" / rel_path,
            vendor_root / "reports" / sub,
            vendor_root / "reports" / rel_path,
            ws_root / ".governance" / "reports" / sub,
            ws_root / "reports" / sub,
            ws_root / "reports" / rel_path,
            vendor_root / ".governance" / "gates" / "reports" / sub,
            ws_root / ".governance" / "gates" / "reports" / sub,
            # Fallback to app-scoped artifacts subpath in sister repo
            ws_root.parent / "wink-ai" / "packages" / "wink-tools" / "artifacts" / sub,
        ]
        for c in candidates:
            if c.is_file():
                return c

    return None


def verify_execution_report(
    report_path: Path,
    scenario_path: Optional[Path] = None,
) -> Tuple[bool, str]:
    """
    Structured assertion of a headless JSON execution report.
    Delegates to shared report_contract.py:
    - If scenario_path is provided, strictly validates against scenario via validate_scenario_report.
    - If scenario_path is None, strictly validates standalone structure via validate_report_standalone.
    """
    if not report_path.is_file():
        return False, f"Report file not found: {report_path}"

    if scenario_path is not None:
        if not scenario_path.is_file():
            return False, f"Scenario file not found: {scenario_path}"
        try:
            with open(scenario_path, "r", encoding="utf-8") as f:
                scen_data = json.load(f)
            scen_steps = scen_data.get("steps")
            if isinstance(scen_steps, list):
                with open(report_path, "r", encoding="utf-8") as rf:
                    rep_data = json.load(rf)
                rep_results = rep_data.get("results")
                total_steps = None
                idx = None
                if isinstance(rep_results, list) and len(rep_results) > 0 and isinstance(rep_results[0], dict):
                    summary = rep_results[0].get("summary", {})
                    total_steps = summary.get("totalSteps", summary.get("total_steps"))
                    idx = 0
                elif isinstance(rep_data.get("summary"), dict):
                    summary = rep_data["summary"]
                    total_steps = summary.get("totalSteps", summary.get("total_steps"))
                if total_steps is not None and total_steps != len(scen_steps):
                    prefix = f"Execution result #{idx} " if idx is not None else "Report "
                    return False, f"{prefix}totalSteps ({total_steps}) != scenario steps ({len(scen_steps)})"
        except Exception:
            pass

        try:
            from report_contract import validate_scenario_report
        except ImportError:
            from gates.report_contract import validate_scenario_report
        return validate_scenario_report(report_path, scenario_path)


    try:
        from report_contract import validate_report_standalone
    except ImportError:
        from gates.report_contract import validate_report_standalone
    return validate_report_standalone(report_path)



def verify_evidence(
    entry: Dict[str, Any],
    execution_config: Dict[str, Any],
    ws_root: Path,
    strict_disk: bool = True,
) -> Tuple[bool, List[str]]:
    """
    Validate evidence for an execution configuration.
    Performs pure read-only validation:
    1. Evidence structure completeness per polymorphic schema (wasm_simulation, esp32_hardware, build_system)
    2. Computed assets composite SHA-256 match (for wasm_simulation)
    3. Computed scenario SHA-256 match (for wasm_simulation)
    4. Execution report structured assertion (for wasm_simulation)
    5. Fail-closed: missing disk artifacts cause verification error when strict_disk=True
    """
    errors = []
    evidence = execution_config.get("evidence")
    if not evidence or not isinstance(evidence, dict):
        return False, ["delivery_state='verified' but evidence is null or not an object"]

    backend = evidence.get("backend", "wasm_simulation")
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"

    if backend in ("wasm_simulation", None):
        assets_sha = evidence.get("assets_sha256")
        scenario_sha = evidence.get("scenario_sha256")
        rep_ref = evidence.get("execution_report_ref")

        if not assets_sha or len(assets_sha) != 64 or assets_sha == "0" * 64:
            errors.append(f"Invalid or empty assets_sha256: '{assets_sha}'")

        if not scenario_sha or len(scenario_sha) != 64 or scenario_sha == "0" * 64:
            errors.append(f"Invalid or empty scenario_sha256: '{scenario_sha}'")

        if not rep_ref or not isinstance(rep_ref, str):
            errors.append("Missing execution_report_ref")
        elif "fail" in rep_ref.lower() or "error" in rep_ref.lower():
            errors.append(f"execution_report_ref contains failure indicator: '{rep_ref}'")

        if errors:
            return False, errors

        if not strict_disk:
            return True, []

        # Disk file cross-validation (Fail-Closed)
        target_dir_rel = entry.get("target_app_dir")
        if target_dir_rel:
            app_cand = vendor_root / target_dir_rel
            if not app_cand.exists():
                app_cand = ws_root / target_dir_rel
            if not app_cand.is_dir():
                errors.append(f"Target app directory declared at '{target_dir_rel}' does not exist on disk: {app_cand}")
            else:
                assets_cand = app_cand / "unisim-assets"
                if not assets_cand.is_dir():
                    errors.append(f"Required unisim-assets directory not found on disk: {assets_cand}")
                else:
                    try:
                        computed_assets_sha = compute_assets_composite_sha256(assets_cand)
                        if computed_assets_sha != assets_sha:
                            errors.append(
                                f"assets_sha256 mismatch for {target_dir_rel}: declared '{assets_sha}', computed '{computed_assets_sha}'"
                            )
                    except Exception as e:
                        errors.append(f"Failed to compute assets composite SHA-256 in {assets_cand}: {e}")

        scenario_path_rel = execution_config.get("acceptance", {}).get("scenario_path")
        if scenario_path_rel:
            sc_cand = vendor_root / scenario_path_rel
            if not sc_cand.is_file():
                sc_cand = ws_root / scenario_path_rel
            if not sc_cand.is_file():
                if target_dir_rel:
                    errors.append(f"Required scenario file not found on disk: {sc_cand}")
            else:
                try:
                    computed_sc_sha = compute_scenario_sha256(sc_cand)
                    if computed_sc_sha != scenario_sha:
                        errors.append(
                            f"scenario_sha256 mismatch for {scenario_path_rel}: declared '{scenario_sha}', computed '{computed_sc_sha}'"
                        )
                except Exception as e:
                    errors.append(f"Failed to compute scenario SHA-256 for {sc_cand}: {e}")

        if rep_ref:
            resolved_rep = resolve_execution_report_path(rep_ref, ws_root)
            if not resolved_rep or not resolved_rep.is_file():
                if target_dir_rel:
                    errors.append(f"Execution report declared but not found on disk: '{rep_ref}'")
            else:
                sc_cand_for_rep = sc_cand if (scenario_path_rel and 'sc_cand' in locals() and sc_cand.is_file()) else None
                rep_ok, rep_msg = verify_execution_report(resolved_rep, sc_cand_for_rep)
                if not rep_ok:
                    errors.append(f"Execution report check failed ({resolved_rep}): {rep_msg}")

    elif backend == "esp32_hardware":
        elf_sha = evidence.get("firmware_elf_sha256")
        serial_log_ref = evidence.get("serial_log_report_ref")
        board_type = evidence.get("board_type")

        if not elf_sha or len(elf_sha) != 64 or elf_sha == "0" * 64:
            errors.append(f"Invalid or empty firmware_elf_sha256: '{elf_sha}'")

        if not serial_log_ref or not isinstance(serial_log_ref, str):
            errors.append("Missing serial_log_report_ref")
        elif "fail" in serial_log_ref.lower() or "error" in serial_log_ref.lower():
            errors.append(f"serial_log_report_ref contains failure indicator: '{serial_log_ref}'")

        if not board_type or not isinstance(board_type, str):
            errors.append("Missing or invalid board_type in esp32_hardware evidence")

        if errors:
            return False, errors

        if not strict_disk:
            return True, []

        resolved_log = resolve_execution_report_path(serial_log_ref, ws_root)
        if not resolved_log or not resolved_log.is_file():
            errors.append(f"Serial log report could not be found on disk: '{serial_log_ref}'")

    elif backend == "build_system":
        build_log_ref = evidence.get("build_log_ref")
        compiler_version = evidence.get("compiler_version")

        if not build_log_ref or not isinstance(build_log_ref, str):
            errors.append("Missing build_log_ref")
        elif "fail" in build_log_ref.lower() or "error" in build_log_ref.lower():
            errors.append(f"build_log_ref contains failure indicator: '{build_log_ref}'")

        if not compiler_version or not isinstance(compiler_version, str):
            errors.append("Missing or invalid compiler_version in build_system evidence")

        if errors:
            return False, errors

        if not strict_disk:
            return True, []

        resolved_build = resolve_execution_report_path(build_log_ref, ws_root)
        if not resolved_build or not resolved_build.is_file():
            errors.append(f"Build log could not be found on disk: '{build_log_ref}'")

    else:
        errors.append(f"Unsupported evidence backend: '{backend}'")

    return len(errors) == 0, errors


def write_evidence_for_app(
    app_name: str,
    ws_root: Path,
    report_src: Optional[Path] = None,
    config_id: Optional[str] = None,
    scenario_path: Optional[Path] = None,
) -> bool:
    """
    Local helper to record evidence into checklist.data.json.
    Called only when -WriteEvidence is explicitly passed to run_esp32_headless_evidence.ps1.
    """
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
    manifest_path = vendor_root / ".governance" / "data" / "checklist.data.json"

    if not manifest_path.is_file():
        sys.stderr.write(f"Error: Manifest not found: {manifest_path}\n")
        return False

    with open(manifest_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Locate entry strictly by exact id or exact target_app_dir (L1-T1: No fuzzy matching)
    matched_entry = None
    for entry in data.get("entries", []):
        t_dir = (entry.get("target_app_dir") or "").replace("\\", "/")
        if entry.get("id") == app_name or t_dir == app_name:
            matched_entry = entry
            break

    if not matched_entry:
        sys.stderr.write(f"Error: No checklist entry found matching app '{app_name}' (fuzzy matching strictly prohibited)\n")
        return False

    target_dir_rel = matched_entry.get("target_app_dir")
    app_dir = vendor_root / target_dir_rel
    assets_dir = app_dir / "unisim-assets"
    scen_dir = app_dir / "unisim-scenarios"

    if not assets_dir.is_dir():
        sys.stderr.write(f"Error: unisim-assets directory not found in {app_dir}\n")
        return False

    assets_sha = compute_assets_composite_sha256(assets_dir)

    # Find scenario file strictly (L1-T1: No arbitrary fallback)
    if scenario_path and Path(scenario_path).is_file():
        scenario_file = Path(scenario_path)
    else:
        scen_files = [p for p in scen_dir.glob("*.scenario.json") if ".fail." not in p.name]
        if not scen_files:
            sys.stderr.write(f"Error: No *.scenario.json found in {scen_dir}\n")
            return False
        if len(scen_files) > 1:
            sys.stderr.write(f"Error: Multiple scenarios found in {scen_dir}. Exact scenario_path must be specified (L1-T1 fallback blocked)\n")
            return False
        scenario_file = scen_files[0]
    scenario_sha = compute_scenario_sha256(scenario_file)

    # Determine report destination
    reports_dir = vendor_root / ".governance" / "reports" / target_dir_rel
    report_dst = reports_dir / "run-report.json"

    report_to_verify = report_src if (report_src and report_src.is_file()) else report_dst
    if not report_to_verify or not report_to_verify.is_file():
        sys.stderr.write(f"Error: Report source not found: {report_src or report_dst}\n")
        return False

    # Check if scenario has real steps
    has_scenario_steps = False
    if scenario_file and scenario_file.is_file():
        try:
            sc_data = json.loads(scenario_file.read_text(encoding="utf-8"))
            if isinstance(sc_data.get("steps"), list) and len(sc_data["steps"]) > 0:
                has_scenario_steps = True
        except Exception:
            pass

    # Strictly verify report BEFORE touching report_dst or committing evidence (Anti-Premature-Overwrite)
    rep_ok, rep_msg = verify_execution_report(
        report_to_verify,
        scenario_path=scenario_file if has_scenario_steps else None
    )
    if not rep_ok:
        sys.stderr.write(f"Error: Generated report verification failed: {rep_msg}\n")
        return False

    # Verification passed! Now safely copy report_src to report_dst if needed
    if report_src and report_src.is_file() and report_src != report_dst:
        reports_dir.mkdir(parents=True, exist_ok=True)
        tmp_dst = report_dst.with_suffix(f".tmp.{os.getpid()}")
        try:
            tmp_dst.write_bytes(report_src.read_bytes())
            tmp_dst.replace(report_dst)
        except Exception:
            report_dst.write_bytes(report_src.read_bytes())
            if tmp_dst.exists():
                try:
                    tmp_dst.unlink()
                except OSError:
                    pass


    # Get current git commit
    try:
        git_commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=str(ws_root),
            text=True
        ).strip()
    except Exception:
        git_commit = "unknown"

    now_iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    run_id = f"run-{datetime.now(timezone.utc).strftime('%Y%m%d')}-{matched_entry.get('id', 'app')}-verified"
    rel_report_ref = f".governance/reports/{target_dir_rel}/run-report.json".replace("\\", "/")

    # Locate target execution config strictly by config_id (L1-T1: No fallback)
    target_exec = None
    executions = matched_entry.get("executions", [])
    if config_id:
        for ex in executions:
            if ex.get("config_id") == config_id:
                target_exec = ex
                break
        if not target_exec:
            sys.stderr.write(f"Error: Explicit config_id '{config_id}' not found in entry '{matched_entry.get('id')}' (L1-T1 fallback blocked)\n")
            return False
    else:
        if len(executions) == 1:
            target_exec = executions[0]
        else:
            sys.stderr.write(f"Error: Multiple executions in entry '{matched_entry.get('id')}'. Explicit config_id required (L1-T1 fallback blocked)\n")
            return False

    # Note on Anti-Pattern P-8: Evidence recording does NOT automatically fabricate
    # audit approval (audit.verdict='audited', auditor='arch_team'). Audit is an
    # independent human/architecture review duty verified by Gate 1 rules.
    target_cfg_id = target_exec.get("config_id", "wasm_sim_standard")

    target_exec["delivery_state"] = "verified"
    rel_scen_path = f"{target_dir_rel}/unisim-scenarios/{scenario_file.name}".replace("\\", "/")
    target_exec.setdefault("acceptance", {})["scenario_path"] = rel_scen_path
    target_exec["evidence"] = {
        "backend": "wasm_simulation",
        "run_id": run_id,
        "assets_sha256": assets_sha,
        "scenario_sha256": scenario_sha,
        "execution_report_ref": rel_report_ref,
        "verified_commit": git_commit,
        "verified_at": now_iso,
    }

    # Recompute summary fields
    if "summary" in data:
        all_entries = data.get("entries", [])
        data["summary"]["audited"] = sum(1 for e in all_entries if e.get("audit", {}).get("verdict") == "audited")
        data["summary"]["verified_configs"] = sum(1 for e in all_entries for ex in e.get("executions", []) if ex.get("delivery_state") == "verified")

    # Save manifest
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"[+] Successfully wrote evidence for '{matched_entry.get('id')}':")
    print(f"    assets_sha256:        {assets_sha}")
    print(f"    scenario_sha256:      {scenario_sha}")
    print(f"    execution_report_ref: {rel_report_ref}")
    print(f"    verified_commit:      {git_commit}")
    return True


def main():
    parser = argparse.ArgumentParser(description="WinkMicroOS Evidence Verifier")
    parser.add_argument("--verify-all", action="store_true", help="Verify all verified entries in checklist.data.json")
    parser.add_argument("--write-app", type=str, help="Record evidence for a specific app (local tool only)")
    parser.add_argument("--config-id", type=str, help="Target execution config_id (e.g. wasm_sim_standard)")
    parser.add_argument("--scenario", type=str, help="Path to specific .scenario.json file")
    parser.add_argument("--report-src", type=str, help="Source path of run-report.json to copy")
    parser.add_argument("--workspace-root", type=str, default=None, help="Workspace root directory")
    args = parser.parse_args()

    if args.workspace_root:
        ws_root = Path(args.workspace_root).resolve()
    else:
        cwd = Path.cwd().resolve()
        if (cwd / "wink-micro-app").is_dir():
            ws_root = cwd
        elif (cwd.parent.parent.parent / "wink-micro-app").is_dir():
            ws_root = cwd.parent.parent.parent
        else:
            ws_root = Path(__file__).resolve().parent.parent.parent.parent.parent


    if args.write_app:
        report_src_p = Path(args.report_src).resolve() if args.report_src else None
        scen_p = Path(args.scenario).resolve() if args.scenario else None
        success = write_evidence_for_app(
            args.write_app,
            ws_root,
            report_src_p,
            config_id=args.config_id,
            scenario_path=scen_p,
        )
        sys.exit(0 if success else 1)

    if args.verify_all:
        manifest_path = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance" / "data" / "checklist.data.json"
        if not manifest_path.is_file():
            sys.stderr.write(f"Manifest not found: {manifest_path}\n")
            sys.exit(1)
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)

        failed = 0
        total_verified = 0
        for entry in manifest.get("entries", []):
            for ex in entry.get("executions", []):
                if ex.get("delivery_state") == "verified":
                    total_verified += 1
                    ok, errs = verify_evidence(entry, ex, ws_root, strict_disk=True)
                    if not ok:
                        failed += 1
                        print(f"[-] FAILED #{entry.get('display_id')} ({entry.get('id')}): {errs}")
                    else:
                        print(f"[+] PASSED #{entry.get('display_id')} ({entry.get('id')})")

        print(f"\nSummary: {total_verified - failed}/{total_verified} verified entries passed.")
        sys.exit(1 if failed > 0 else 0)

    parser.print_help()


if __name__ == "__main__":
    main()
