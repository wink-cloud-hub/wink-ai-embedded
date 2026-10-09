#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""CMS8S78xx trustworthy build-baseline batch driver
(PLAN-20261009-CMS8S78XX-BUILD-BASELINE, S3/S4).

The driver only resolves public entry points and schedules the existing gates; it
never reimplements a builder or a simulation runner. Two things it must prove with
bytes rather than prose:

  * **independent compile trees** — the public ``build sim`` command keeps its CMake
    tree at ``<embedded_root>/build/wasm/<app_id>`` and ``--out`` only relocates the
    emitted assets, so A/B rounds run against a staged copy of the SDK/App tree bound
    through the documented ``WINK_AI_EMBEDDED_DIR`` anchor. Those copies live under a
    short, gitignored root because MinGW make reports a breached ``CMAKE_OBJECT_PATH_MAX``
    as a missing rule, and each tree must be freshly staged or the round is BLOCKED;
  * **bound execution assets** — ``sim run`` always auto-builds (the public CLI exposes
    ``--no-build`` only on ``consistency``), so a build-phase hash can never be claimed as
    the executed artifact. The scenario phase stages the selected A/B triple into its own
    asset directory and re-hashes those bytes after the engine returns; only an unchanged
    triple counts as bound.

Statuses are ``PASS``/``FAIL``/``ERROR``/``TIMEOUT``/``BLOCKED``/``NOT_RUN``; nothing
that did not execute becomes ``PASS``.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
EMBEDDED_ROOT = HERE.parents[3]
VENDOR_APPS_REL = Path("wink-micro-app/vendor/cms8s78xx")
ASSET_NAMES = ("device-tree.json", "wink_simulator.js", "wink_simulator.wasm")
TOOL_ID = "run_cms8s78xx_baseline/v1"

EXIT_PASS = 0
EXIT_FAIL = 1


def now_iso() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def asset_identity(directory: Path) -> dict:
    """Raw-byte identity of one emitted asset triple; missing files are recorded."""
    out = {}
    for name in ASSET_NAMES:
        handle = directory / name
        if handle.is_file():
            out[name] = {"sha256": sha256_file(handle), "size_bytes": handle.stat().st_size,
                         "path": str(handle).replace("\\", "/")}
        else:
            out[name] = {"sha256": None, "size_bytes": None,
                         "path": str(handle).replace("\\", "/"), "missing": True}
    out["complete"] = all(v.get("sha256") for k, v in out.items() if k in ASSET_NAMES)
    return out


def run_proc(argv, *, cwd=None, env=None, timeout=None, log_path=None, expect_stdout=True):
    """Run one public entry point, recording the real argv/cwd/exit code."""
    record = {"argv": [str(x) for x in argv], "cwd": str(cwd or Path.cwd()).replace("\\", "/"),
              "started_at": now_iso()}
    proc_env = dict(os.environ)
    if env:
        proc_env.update({k: str(v) for k, v in env.items()})
    stdout = stderr = ""
    try:
        proc = subprocess.run([str(x) for x in argv], cwd=str(cwd) if cwd else None,
                              env=proc_env, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=timeout)
        record["exit_code"] = proc.returncode
        stdout, stderr = proc.stdout or "", proc.stderr or ""
    except subprocess.TimeoutExpired as exc:
        record["exit_code"] = None
        record["status"] = "TIMEOUT"
        stdout, stderr = (exc.stdout or ""), (exc.stderr or "")
    record["finished_at"] = now_iso()
    if log_path:
        Path(log_path).parent.mkdir(parents=True, exist_ok=True)
        with open(log_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("$ " + " ".join(record["argv"]) + "\n")
            handle.write(f"# cwd={record['cwd']}\n# exit={record['exit_code']}\n")
            handle.write("--- stdout ---\n" + stdout + "\n--- stderr ---\n" + stderr + "\n")
    record["stdout_tail"] = stdout[-400:] if expect_stdout else ""
    record["stderr_tail"] = stderr[-400:]
    if record.get("status") != "TIMEOUT":
        record["status"] = "PASS" if record["exit_code"] == 0 else "FAIL"
    return record, stdout, stderr


def resolve_entry(wink_ai_root: str | None = None) -> dict:
    """Pick the CLI entry: explicit ``WINK_AI_ROOT`` > sibling repo > installed ``winkcli``.

    An explicit override that lacks the launcher is a configuration error; it never
    silently falls back to another version.
    """
    explicit = wink_ai_root if wink_ai_root is not None else os.environ.get("WINK_AI_ROOT", "").strip()
    if explicit:
        launcher = Path(explicit) / "packages" / "wink-tools" / "wink.py"
        if not launcher.is_file():
            raise SystemExit(f"entry_error: explicit WINK_AI_ROOT has no packages/wink-tools/wink.py: {launcher}")
        return {"mode": "sibling_source", "launcher": str(launcher).replace("\\", "/"),
                "command_prefix": [sys.executable, str(launcher)],
                "env": {"WINK_DEV": "1"}, "wink_ai_root": str(Path(explicit).resolve()).replace("\\", "/"),
                "reason": "explicit WINK_AI_ROOT override"}
    sibling = EMBEDDED_ROOT.parent / "wink-ai" / "packages" / "wink-tools" / "wink.py"
    if sibling.is_file():
        return {"mode": "sibling_source", "launcher": str(sibling).replace("\\", "/"),
                "command_prefix": [sys.executable, str(sibling)],
                "env": {"WINK_DEV": "1"},
                "wink_ai_root": str(sibling.parents[2]).replace("\\", "/"),
                "reason": "sibling wink-ai repository of the embedded root"}
    installed = shutil.which("winkcli")
    if installed:
        return {"mode": "installed_cli", "launcher": installed, "command_prefix": [installed],
                "env": {}, "wink_ai_root": None,
                "reason": "no sibling launcher; using installed winkcli from PATH (WINK_DEV not inherited)"}
    raise SystemExit(f"entry_error: no launcher found (tried {sibling} and winkcli on PATH)")


def entry_fingerprint(entry: dict) -> dict:
    """Version/fingerprint of the chosen entry; unexercised modes stay NOT_RUN."""
    fingerprint = {"mode": entry["mode"], "launcher": entry["launcher"],
                   "launcher_sha256": sha256_file(Path(entry["launcher"]))
                   if entry["mode"] == "sibling_source" else None,
                   "version": None, "version_probe": "NOT_RUN"}
    if entry["mode"] == "installed_cli":
        record, stdout, _ = run_proc([entry["launcher"], "--version"], timeout=120)
        fingerprint["version_probe"] = record["status"]
        fingerprint["version"] = (stdout or "").strip().splitlines()[:1]
    else:
        fingerprint["version"] = "source tree (no packaged version)"
    fingerprint["alternate_modes"] = {"installed_cli": "NOT_RUN"
                                      if entry["mode"] != "installed_cli" else "SELECTED"}
    return fingerprint


def enumerate_apps(vendor_root: Path) -> list[Path]:
    """Explicit enumeration of vendor apps (never a workspace-wide ``--all`` scan)."""
    apps = []
    for child in sorted(p for p in vendor_root.iterdir() if p.is_dir()):
        manifest = child / "wink-app.json"
        if not manifest.is_file():
            continue
        data = json.loads(manifest.read_text(encoding="utf-8"))
        if not (data.get("upstream") or {}).get("source_dir"):
            raise SystemExit(f"manifest_error: {child.name} declares no upstream.source_dir")
        apps.append(child)
    if not apps:
        raise SystemExit(f"manifest_error: no vendor apps under {vendor_root}")
    return apps


STAGE_IGNORE = ("build", "artifacts", "__pycache__", ".git", "*.obj")
# CMake resolves tooling and shared app sources as siblings of the SDK root
# (``../wink-tools/tools/codegen/boards/...``, ``../wink-micro-app/common``), so an
# isolated embedded root must stage them too or the board-config step silently degrades to
# a fallback path and the build breaks. Kept as real copies (never junctions) so nothing a
# build writes can reach the shared tree.
STAGE_SIBLING_ROOTS = ("wink-micro-os", "wink-tools")
STAGE_SIBLING_DIRS = ("wink-micro-app/common",)


def stage_isolated_copy(embedded_root: Path, app_dir: Path, destination: Path,
                        include_scenarios: bool = False) -> dict:
    """Create an isolated build input so the CLI gets its own compile root.

    ``WINK_AI_EMBEDDED_DIR`` is the documented public anchor for the embedded root, so
    the staged copy binds ``wink-micro-os`` and emits ``build/wasm/<app_id>`` inside
    itself; the shared tree is never rewritten to obtain isolation.
    """
    app_rel = app_dir.relative_to(embedded_root)
    destination.mkdir(parents=True, exist_ok=True)
    ignore = shutil.ignore_patterns(*STAGE_IGNORE)
    staged = []
    for rel in list(STAGE_SIBLING_ROOTS) + list(STAGE_SIBLING_DIRS):
        source = embedded_root / rel
        if not source.is_dir():
            continue
        target = destination / rel
        if target.exists():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, target, ignore=ignore, symlinks=False)
        staged.append(rel)
    app_dst = destination / app_rel
    app_excludes = ["unisim-assets", "__pycache__"] + ([] if include_scenarios else ["unisim-scenarios"])
    if not app_dst.exists():
        app_dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(app_dir, app_dst, ignore=shutil.ignore_patterns(*app_excludes),
                        symlinks=False)
    return {"isolated_embedded_root": str(destination).replace("\\", "/"),
            "isolated_sdk_root": str(destination / "wink-micro-os").replace("\\", "/"),
            "isolated_app_dir": str(app_dst).replace("\\", "/"),
            "staged_roots": staged,
            "staged_app": str(app_rel).replace("\\", "/"),
            "copy_ignore": list(STAGE_IGNORE),
            "app_copy_excludes": app_excludes,
            "sdk_copy_sources": {"from": str(embedded_root / "wink-micro-os").replace("\\", "/"),
                                 "ignore": list(STAGE_IGNORE)}}


def compile_root_for(isolated_root: Path, app_rel: Path) -> Path:
    """Predict the CLI compile root: ``<embedded_root>/build/wasm/<vendor/.../app>``."""
    app_id = str(app_rel.relative_to(VENDOR_APPS_REL.parent.parent)).replace("\\", "/")
    return isolated_root / "build" / "wasm" / app_id


# MinGW make reports a breached CMAKE_OBJECT_PATH_MAX as
# "No rule to make target '<...>.cms8s_register.cpp.o'" instead of a length error, so the
# pilot's short app name hid the fact that long app names cannot compile inside the deep
# evidence tree at all. The transient isolated trees therefore live under a short,
# gitignored root keyed by the frozen app index, and the budget is checked before any build
# is attempted so an over-long tree is BLOCKED rather than a bogus per-app compile failure.
OBJECT_PATH_LIMIT = 250
DEEPEST_OBJECT_SUFFIX = ("frameworks/mcs51/CMakeFiles/wink_mcs51_cms8s78xx_register.dir/"
                         "chips/cms8s78xx/src/cms8s_register.cpp.o")


def iso_base_of(run_root: Path) -> Path:
    """Short root for the transient isolated build trees, unique per batch.

    The token keeps the run id's date and time so two batches never share a tree, while the
    path stays short enough for CMake's object-path budget even for the longest app name.
    """
    token = "-".join(run_root.name.split("-")[-3:])[:19] or run_root.name[:19]
    return EMBEDDED_ROOT / "artifacts" / ".iso" / token


def iso_tree(iso_base: Path, apps: list[Path], name: str) -> Path:
    """Per-app isolated tree directory, keyed by the frozen enumeration order."""
    return iso_base / f"{[app.name for app in apps].index(name):02d}"


def iso_path_budget(compile_root: Path) -> dict:
    """Project the deepest object path CMake will emit under this compile root."""
    projected = f"{compile_root}/{DEEPEST_OBJECT_SUFFIX}"
    return {"rule": "cmake-object-path-max", "limit": OBJECT_PATH_LIMIT,
            "deepest_suffix": DEEPEST_OBJECT_SUFFIX,
            "projected_path": str(projected).replace("\\", "/"),
            "projected_length": len(projected),
            "status": "PASS" if len(projected) <= OBJECT_PATH_LIMIT else "BLOCKED"}


def iso_tree_fresh(iso_root: Path) -> bool:
    """A reused isolated tree could carry the previous round's objects, so it is refused."""
    return not iso_root.exists()


def path_leakage(assets_dir: Path, needle: str) -> dict:
    """Scan emitted bytes for the build location; a hit means the artifact is not relocatable.

    ``assert()`` text embeds ``__FILE__``, so an absolute staged root inside a shipped
    asset makes two clean builds of identical inputs differ. The CMake layer maps the
    workspace prefix; this gate verifies the *emitted bytes* rather than the flag.
    """
    hits = {}
    for name in ASSET_NAMES:
        handle = assets_dir / name
        if not handle.is_file():
            continue
        data = handle.read_bytes()
        variants = sorted({needle.replace("\\", "/"), needle.replace("/", "\\")})
        found = [v for v in variants if v.encode() in data]
        if found:
            hits[name] = found
    return hits


def assert_clean_target(leaf: Path, expected_parent: Path) -> None:
    """Refuse to clean anything outside the expected isolated build tree."""
    resolved = leaf.resolve()
    parent = expected_parent.resolve()
    if parent not in resolved.parents and resolved != parent:
        raise SystemExit(f"clean_target_error: refusing to clean {resolved} (outside {parent})")


def blocked_round(round_label: str, failure: str, iso_root: Path, leaf: Path,
                  assets_dir: Path, budget: dict) -> dict:
    """A round that never started: recorded as BLOCKED, never as a compile failure."""
    return {"round": round_label, "status": "BLOCKED", "failure": failure,
            "path_budget": budget, "iso_root": str(iso_root).replace("\\", "/"),
            "compile_root": str(leaf).replace("\\", "/"), "compile_root_exists": False,
            "assets_dir": str(assets_dir).replace("\\", "/"),
            "assets": asset_identity(assets_dir)}


def build_round(entry: dict, app_dir: Path, embedded_root: Path, run_root: Path,
                app_name: str, app_tree: Path, round_label: str, timeout: int) -> dict:
    """One clean ``build sim`` round with a private compile root and captured identity."""
    iso_root = app_tree / round_label
    assets_dir = run_root / "apps" / app_name / "builds" / round_label / "assets"
    app_rel = app_dir.relative_to(embedded_root)
    leaf = compile_root_for(iso_root, app_rel)
    budget = iso_path_budget(leaf)
    if not iso_tree_fresh(iso_root):
        return blocked_round(round_label, "iso_tree_not_fresh", iso_root, leaf, assets_dir, budget)
    if budget["status"] != "PASS":
        return blocked_round(round_label, "iso_path_budget", iso_root, leaf, assets_dir, budget)
    stage = stage_isolated_copy(embedded_root, app_dir, iso_root)
    assets_dir.mkdir(parents=True, exist_ok=True)
    assert_clean_target(leaf, iso_root / "build")
    command = entry["command_prefix"] + ["build", "sim", "--app", stage["isolated_app_dir"],
                                         "--clean", "--sdk-mode", "source", "--out", str(assets_dir)]
    env = dict(entry["env"])
    env["WINK_AI_EMBEDDED_DIR"] = stage["isolated_embedded_root"]
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONUTF8"] = "1"
    shared_leaf = compile_root_for(EMBEDDED_ROOT, app_rel)
    record, _, _ = run_proc(command, cwd=embedded_root, env=env, timeout=timeout,
                            log_path=run_root / "apps" / app_name / "builds" / round_label / "build.log")
    result = {"round": round_label, "status": record["status"], "command": record,
              "isolation": stage, "entry_env": env, "path_budget": budget,
              "clean_scope": {"flag": "--clean", "cleaned_leaf": str(leaf).replace("\\", "/"),
                              "expected_build_root": str(iso_root / "build").replace("\\", "/"),
                              "accepted_by_guard": True},
              "compile_root": str(leaf).replace("\\", "/"),
              "compile_root_exists": leaf.is_dir(),
              "shared_tree_compile_root": str(shared_leaf).replace("\\", "/"),
              "shared_tree_compile_root_exists": shared_leaf.is_dir(),
              "assets_dir": str(assets_dir).replace("\\", "/"),
              "assets": asset_identity(assets_dir)}
    result["path_leakage"] = path_leakage(assets_dir, stage["isolated_embedded_root"])
    if record["status"] != "PASS":
        result["status"] = record["status"]
    elif not result["assets"]["complete"]:
        result["status"] = "FAIL"
        result["failure"] = "missing_asset"
    elif not result["compile_root_exists"]:
        result["status"] = "FAIL"
        result["failure"] = "compile_root_not_isolated"
    return result


def compare_rounds(a: dict, b: dict) -> dict:
    """Raw-byte A/B reproducibility; normalized comparison is not a substitute.

    A round that did not build successfully blocks the verdict before its bytes are ever
    compared: identical leftover outputs from two failed rounds are not reproducibility.
    """
    per_asset = {}
    reproducible = True
    for name in ASSET_NAMES:
        ha, hb = a["assets"].get(name, {}).get("sha256"), b["assets"].get(name, {}).get("sha256")
        same = bool(ha) and ha == hb
        per_asset[name] = {"round_A": ha, "round_B": hb, "identical": same}
        reproducible = reproducible and same
    if a["status"] != "PASS" or b["status"] != "PASS":
        status = "BLOCKED"
    else:
        status = "PASS" if reproducible else "FAIL"
    return {"comparison_basis": "raw sha256 of emitted bytes", "status": status,
            "round_A_status": a["status"], "round_B_status": b["status"],
            "assets": per_asset,
            "asset_count": len(ASSET_NAMES) * 2}


def execution_identity_status(selected_assets: dict, after_identity: dict) -> str:
    """Classify whether the run really consumed the selected A/B triple."""
    if not all(after_identity.get(n, {}).get("sha256") for n in ASSET_NAMES):
        return "missing_asset"
    if any(selected_assets.get(n, {}).get("sha256") != after_identity[n]["sha256"]
           for n in ASSET_NAMES):
        return "identity_mismatch"
    return "bound"


def scenario_outcome(record_status: str, binding: str, report_count: int) -> tuple[str, str | None]:
    """Verdict of one scenario phase: a green CLI is not evidence on its own.

    An exit-0 run that consumed different bytes than the selected round, or that produced
    no report, fails the gate — otherwise an unbound replay would be recorded as proof.
    """
    if record_status != "PASS":
        return record_status, None
    if binding != "bound":
        return "FAIL", binding
    if report_count == 0:
        return "FAIL", "missing_report"
    return "PASS", None


def scenario_files(directory: Path) -> list[dict]:
    """Enumerate the scenario inputs with raw hashes (the credential's input side)."""
    records = []
    for path in sorted(directory.glob("*.scenario.json")):
        header = json.loads(path.read_text(encoding="utf-8")).get("header") or {}
        records.append({"file": path.name,
                        "path": str(path).replace("\\", "/"),
                        "sha256": sha256_file(path),
                        "header_name": header.get("name"),
                        "template_id": header.get("templateId"),
                        "declared_steps": len(json.loads(path.read_text(encoding="utf-8")).get("steps") or [])})
    return records


def match_reports(report_path: Path, files: list[dict]) -> dict:
    """Tie every engine result row to exactly one scenario file, and every file to one row.

    ``run-report.json`` carries no scenario filename, only ``header.name``; an unpaired row
    or file means the ledger cannot claim that file was executed, so it is a failure rather
    than a rounding error.
    """
    if not report_path.is_file():
        return {"status": "FAIL", "failure": "missing_report", "report": str(report_path).replace("\\", "/"),
                "totals": None, "scenarios": [], "unmatched_rows": [], "unexecuted_files":
                [f["file"] for f in files]}
    data = json.loads(report_path.read_text(encoding="utf-8"))
    rows = data.get("results") or []
    by_name: dict = {}
    for record in files:
        by_name.setdefault(record["header_name"], []).append(record)
    paired, unmatched_rows, seen = [], [], set()
    for row in rows:
        name = (row.get("header") or {}).get("name")
        candidates = by_name.get(name) or []
        if len(candidates) != 1:
            unmatched_rows.append({"header_name": name, "candidates": len(candidates)})
            continue
        record = candidates[0]
        seen.add(record["file"])
        summary = row.get("summary") or {}
        failing = [s for s in (row.get("stepResults") or [])
                   if s.get("status") not in ("passed",)]
        paired.append({"scenario_file": record["file"], "scenario_sha256": record["sha256"],
                       "template_id": record["template_id"], "header_name": name,
                       "engine_status": row.get("status"), "ok": row.get("ok"),
                       "declared_steps": record["declared_steps"],
                       "executed_steps": summary.get("totalSteps"),
                       "passed_steps": summary.get("passedSteps"),
                       "skipped_steps": summary.get("skippedSteps"),
                       "failed_steps_detail": [
                           {"stepIndex": s.get("stepIndex"), "type": s.get("type"),
                            "status": s.get("status"), "expected": s.get("expected"),
                            "actual": s.get("actual")} for s in failing],
                       "total_virtual_us": (row.get("header") or {}).get("totalVirtualUs"),
                       "diagnostics": row.get("diagnostics") or []})
    unexecuted = [record["file"] for record in files if record["file"] not in seen]
    vacuous = [p["scenario_file"] for p in paired if not p["executed_steps"]]
    failed = ([p["scenario_file"] for p in paired if not p["ok"]] + unmatched_rows
              + unexecuted + vacuous)
    return {"status": "PASS" if not failed else "FAIL",
            "report": str(report_path).replace("\\", "/"),
            "report_sha256": sha256_file(report_path),
            "totals": {"total": data.get("total"), "passed": data.get("passed"),
                       "failed": data.get("failed"), "results_rows": len(rows)},
            "scenarios": paired, "unmatched_rows": unmatched_rows,
            "unexecuted_files": unexecuted, "vacuous_files": vacuous,
            "failure": ("scenario_pairing" if failed else None)}


def scenario_run(entry: dict, app_dir: Path, embedded_root: Path, run_root: Path,
                 app_name: str, app_tree: Path, selected: dict, timeout: int) -> dict:
    """Formal headless run, bound to the selected round by post-run raw-byte identity.

    The public ``sim run`` CLI always auto-builds (``--no-build`` exists only on
    ``consistency``) and reuses ``--out`` as the engine's app directory, so the run is
    staged in its own isolated copy without ``--out``: the engine then loads the triple
    from ``<staged app>/unisim-assets`` inside that copy. The selected A/B bytes are
    planted there first and re-hashed after the engine returned; binding is only claimed
    when the auto-build reproduced the selected identity.
    """
    iso_root = app_tree / "S"
    leaf = compile_root_for(iso_root, app_dir.relative_to(embedded_root))
    budget = iso_path_budget(leaf)
    if not iso_tree_fresh(iso_root):
        failure = "iso_tree_not_fresh"
    elif budget["status"] != "PASS":
        failure = "iso_path_budget"
    else:
        failure = None
    if failure:
        return {"status": "BLOCKED", "failure": failure, "path_budget": budget,
                "iso_root": str(iso_root).replace("\\", "/"),
                "compile_root": str(leaf).replace("\\", "/"),
                "selected_round": selected["round"], "selected_assets": selected["assets"],
                "execution_identity_status": "not_run", "pairing": {"status": "BLOCKED"},
                "scenario_file_count": 0, "report_files": []}
    stage = stage_isolated_copy(embedded_root, app_dir, iso_root, include_scenarios=True)
    staged_app = Path(stage["isolated_app_dir"])
    assets_dir = staged_app / "unisim-assets"
    assets_dir.mkdir(parents=True, exist_ok=True)
    for name in ASSET_NAMES:
        source = Path(selected["assets_dir"]) / name
        if source.is_file():
            shutil.copy2(source, assets_dir / name)
    staged_before = asset_identity(assets_dir)
    write_json(assets_dir / "asset-manifest.json", {
        "selected_round": selected["round"], "selected_from": selected["assets_dir"],
        "identity": selected["assets"],
        "note": "selected reproducible triple planted here; sim run always auto-builds over this "
                "directory, so binding is proven by the post-run re-hash, not by this file"})
    scenarios = staged_app / "unisim-scenarios"
    evidence = run_root / "apps" / app_name / "headless"
    evidence.mkdir(parents=True, exist_ok=True)
    command = entry["command_prefix"] + ["sim", "run", "--app", str(staged_app),
                                        "--mode", "headless",
                                        "--scenarios", str(scenarios),
                                        "--wasm-dir", str(assets_dir),
                                        "--artifacts", str(evidence), "--reporter", "json"]
    env = dict(entry["env"])
    env["WINK_AI_EMBEDDED_DIR"] = stage["isolated_embedded_root"]
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONUTF8"] = "1"
    record, _, _ = run_proc(command, cwd=embedded_root, env=env, timeout=timeout,
                            log_path=evidence / "run.log")
    after = asset_identity(assets_dir)
    binding = execution_identity_status(selected["assets"], after)
    files = scenario_files(scenarios)
    report_path = evidence / "run-report.json"
    pairing = match_reports(report_path, files)
    reports = sorted(str(p).replace("\\", "/") for p in evidence.rglob("*.json"))
    status, failure = scenario_outcome(record["status"], binding, len(reports))
    if status == "PASS" and pairing["status"] != "PASS":
        status, failure = "FAIL", pairing["failure"]
    result = {"status": status, "command": record, "selected_round": selected["round"],
              "isolation": stage, "entry_env": env,
              "selected_assets": selected["assets"], "staged_before_run": staged_before,
              "assets_after_run": after, "execution_identity_bound": binding == "bound",
              "execution_identity_status": binding,
              "compile_root": str(leaf).replace("\\", "/"), "path_budget": budget,
              "scenario_dir": str(scenarios).replace("\\", "/"),
              "scenario_file_count": len(files), "pairing": pairing,
              "report_files": reports}
    if failure:
        result["failure"] = failure
    result["path_leakage"] = path_leakage(assets_dir, stage["isolated_embedded_root"])
    result["load_evidence_status"] = "unverified"
    result["load_evidence_note"] = (
        "the engine was pointed at the staged asset directory and the triple there was re-hashed "
        "after it returned, but no public report field names the loaded device-tree/JS/Wasm objects, "
        "so the loaded-object binding stays unverified (design section 3)")
    return result


def write_json(path: Path, payload) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)
        handle.write("\n")


def run_root_of(raw: str) -> Path:
    """Evidence roots are absolute, so every CLI argument we forward is absolute too.

    A relative ``--run-root`` would leak relative ``--app``/``--out``/``--wasm-dir`` values
    into the public entry points, whose resolution base is the shared repository rather
    than the evidence tree.
    """
    return Path(raw).resolve()


def tool_fingerprint() -> dict:
    """Identify the exact driver bytes that produced a piece of evidence."""
    return {"tool": TOOL_ID, "driver": str(Path(__file__).resolve()).replace("\\", "/"),
            "driver_sha256": sha256_file(Path(__file__).resolve())}


COMPILE_EVIDENCE_FILES = (
    "CMakeCache.txt",
    "CMakeFiles/wink_simulator.dir/link.txt",
    "CMakeFiles/wink_simulator.dir/flags.make",
    "CMakeFiles/wink_simulator.dir/includes_C.rsp",
    "CMakeFiles/wink_simulator.dir/includes_CXX.rsp",
    "CMakeFiles/wink_simulator.dir/build.make",
)


def collect_compile_evidence(leaf: Path, destination: Path) -> dict:
    """Keep the real command chain, drop the multi-hundred-MB object tree.

    The isolated compile root is deliberately thrown away after each round; these are the
    files that show which generator, flags, includes and link line produced the bytes.
    """
    destination.mkdir(parents=True, exist_ok=True)
    copied, missing = {}, []
    for rel in COMPILE_EVIDENCE_FILES:
        source = leaf / rel
        if source.is_file():
            target = destination / Path(rel).name
            shutil.copy2(source, target)
            copied[rel] = {"path": str(target).replace("\\", "/"), "sha256": sha256_file(target)}
        else:
            missing.append(rel)
    generated = {}
    for header in sorted((leaf / "generated").glob("*.h")) if (leaf / "generated").is_dir() else []:
        target = destination / header.name
        shutil.copy2(header, target)
        generated[header.name] = {"path": str(target).replace("\\", "/"),
                                  "sha256": sha256_file(target)}
    return {"destination": str(destination).replace("\\", "/"), "copied": copied,
            "missing": missing, "generated_headers": generated}


def sdcc_gate_run(app_dir: Path, run_root: Path, app_name: str, timeout: int) -> dict:
    """Per-app Tier-S SDCC compile/link/capacity gate with captured argv.

    The gate resolves the vendor device header under ``docs/vendors`` in the shared tree,
    so it runs against the shared mirror (which ``audit_vendor_mirror.py --verify`` locks)
    rather than the staged copy; that scope is recorded instead of being glossed over.
    """
    tmp = run_root / "sdcc-tmp" / app_name
    tmp.mkdir(parents=True, exist_ok=True)
    commands_file = run_root / "apps" / app_name / "sdcc-commands.json"
    command = [sys.executable, str(HERE / "gate_app_hardware_capacity.py"), str(app_dir),
               "--commands-out", str(commands_file)]
    env = {"PYTHONIOENCODING": "utf-8", "PYTHONUTF8": "1", "TMPDIR": str(tmp)}
    record, _, _ = run_proc(command, cwd=EMBEDDED_ROOT, env=env, timeout=timeout,
                            log_path=run_root / "apps" / app_name / "sdcc.log")
    return {"status": record["status"], "command": record,
            "commands_file": str(commands_file).replace("\\", "/"),
            "commands_file_present": commands_file.is_file(),
            "source_tree": str(EMBEDDED_ROOT).replace("\\", "/"),
            "tmpdir": str(tmp).replace("\\", "/")}


def select_apps(apps: list[Path], args) -> tuple[list[Path], str]:
    """Subset selection for rehearsal chunks; the ledger still enumerates all 37 apps."""
    if args.only:
        wanted = [name.strip() for name in args.only.split(",") if name.strip()]
        known = {p.name: p for p in apps}
        unknown = [name for name in wanted if name not in known]
        if unknown:
            raise SystemExit(f"scope_error: unknown app(s): {', '.join(unknown)}")
        return [known[name] for name in wanted], f"only={args.only}"
    if args.shard:
        index, total = (int(x) for x in args.shard.split("/"))
        if not 1 <= index <= total:
            raise SystemExit(f"scope_error: bad --shard {args.shard}")
        chunk = [p for i, p in enumerate(apps) if i % total == index - 1]
        return chunk, f"shard={args.shard}"
    return apps, "full"


def assemble_apps_ledger(run_root: Path, all_apps: list[Path], entry: dict, scope: str) -> dict:
    """Roll every on-disk per-app summary up into the 222-identity ledger."""
    per_app, identities = [], 0
    for app_dir in all_apps:
        summary = run_root / "apps" / app_dir.name / "build-summary.json"
        if not summary.is_file():
            per_app.append({"app": app_dir.name, "status": "NOT_RUN", "summary": str(summary).replace("\\", "/")})
            continue
        record = json.loads(summary.read_text(encoding="utf-8"))
        for label in ("A", "B"):
            assets = record.get("rounds", {}).get(label, {}).get("assets", {})
            identities += sum(1 for name in ASSET_NAMES if assets.get(name, {}).get("sha256"))
        per_app.append({"app": app_dir.name, "status": record["status"],
                        "built_at": record.get("generated_at"),
                        "built_by": record.get("tool_fingerprint", {}).get("driver_sha256"),
                        "round_A": record["rounds"]["A"]["status"],
                        "round_B": record["rounds"]["B"]["status"],
                        "reproducibility": record["reproducibility"]["status"],
                        "sdcc": record["sdcc"]["status"],
                        "selected_round": record.get("selected_round"),
                        "assets": {label: {name: record["rounds"][label]["assets"][name].get("sha256")
                                           for name in ASSET_NAMES} for label in ("A", "B")},
                        "path_budget": {label: record["rounds"][label].get("path_budget", {}).get("status")
                                        for label in ("A", "B")},
                        "path_leakage": {label: record["rounds"][label].get("path_leakage")
                                         for label in ("A", "B")}})
    built = [p for p in per_app if p["status"] != "NOT_RUN"]
    status = ("PARTIAL" if len(built) != len(all_apps)
              else "PASS" if all(p["status"] == "PASS" for p in built)
              else "BLOCKED" if any(p["status"] == "BLOCKED" for p in built) else "FAIL")
    return {"tool": TOOL_ID, "tool_fingerprint": tool_fingerprint(),
            "generated_at": now_iso(), "entry": entry["launcher"], "scope": scope,
            "apps_enumerated": len(all_apps), "apps_built": len(built),
            "apps_blocked": [p["app"] for p in per_app if p["status"] == "BLOCKED"],
            "identity_count": identities, "identity_expected": len(all_apps) * len(ASSET_NAMES) * 2,
            "status": status, "apps": per_app}


def phase_apps(args) -> int:
    run_root = run_root_of(args.run_root)
    entry = resolve_entry(args.wink_ai_root)
    all_apps = enumerate_apps(EMBEDDED_ROOT / VENDOR_APPS_REL)
    selected, scope = select_apps(all_apps, args)
    iso_base = iso_base_of(run_root)
    for app_dir in selected:
        name = app_dir.name
        tree = iso_tree(iso_base, all_apps, name)
        rounds = {label: build_round(entry, app_dir, EMBEDDED_ROOT, run_root, name, tree, label,
                                     args.build_timeout) for label in ("A", "B")}
        for label, result in rounds.items():
            result["compile_evidence"] = collect_compile_evidence(
                Path(result["compile_root"]),
                run_root / "apps" / name / "builds" / label / "compile-evidence")
        reproducibility = compare_rounds(rounds["A"], rounds["B"])
        sdcc = sdcc_gate_run(app_dir, run_root, name, args.sdcc_timeout)
        selected_round = next((label for label in ("A", "B")
                               if rounds[label]["status"] == "PASS"), None)
        status = ("PASS" if rounds["A"]["status"] == "PASS" and rounds["B"]["status"] == "PASS"
                  and reproducibility["status"] == "PASS" and sdcc["status"] == "PASS"
                  else "BLOCKED" if any(rounds[label]["status"] == "BLOCKED"
                                        for label in ("A", "B")) else "FAIL")
        record = {"app": name, "app_dir": str(app_dir).replace("\\", "/"),
                  "generated_at": now_iso(), "tool_fingerprint": tool_fingerprint(),
                  "entry_fingerprint": entry_fingerprint(entry),
                  "iso_tree": str(tree).replace("\\", "/"),
                  "rounds": rounds, "reproducibility": reproducibility, "sdcc": sdcc,
                  "selected_round": selected_round, "scenario": {"status": "NOT_RUN"},
                  "status": status}
        write_json(run_root / "apps" / name / "build-summary.json", record)
        freed = tree.exists()
        shutil.rmtree(tree, ignore_errors=True)
        print(f"[apps] {name}: {status} (A={rounds['A']['status']} B={rounds['B']['status']} "
              f"repro={reproducibility['status']} sdcc={sdcc['status']} iso_cleaned={freed})",
              flush=True)
    ledger = assemble_apps_ledger(run_root, all_apps, entry, scope)
    write_json(run_root / "apps.json", ledger)
    print(json.dumps({k: ledger[k] for k in ("apps_enumerated", "apps_built", "identity_count",
                                             "identity_expected", "scope", "status")},
                     ensure_ascii=False))
    return EXIT_PASS if ledger["status"] == "PASS" else EXIT_FAIL


def assemble_scenarios_ledger(run_root: Path, all_apps: list[Path], entry: dict, scope: str) -> dict:
    """Roll the per-app scenario pairings up into the 38-scenario ledger."""
    per_app, rows = [], []
    for app_dir in all_apps:
        summary = run_root / "apps" / app_dir.name / "build-summary.json"
        if not summary.is_file():
            per_app.append({"app": app_dir.name, "status": "NOT_RUN"})
            continue
        record = json.loads(summary.read_text(encoding="utf-8"))
        scenario = record.get("scenario") or {"status": "NOT_RUN"}
        per_app.append({"app": app_dir.name, "status": scenario.get("status"),
                        "selected_round": scenario.get("selected_round"),
                        "execution_identity_status": scenario.get("execution_identity_status"),
                        "load_evidence_status": scenario.get("load_evidence_status"),
                        "scenario_file_count": scenario.get("scenario_file_count"),
                        "pairing_totals": (scenario.get("pairing") or {}).get("totals")})
        for item in ((scenario.get("pairing") or {}).get("scenarios") or []):
            rows.append({"app": app_dir.name, "scenario_file": item["scenario_file"],
                         "scenario_sha256": item["scenario_sha256"],
                         "engine_status": item["engine_status"],
                         "declared_steps": item["declared_steps"],
                         "executed_steps": item["executed_steps"],
                         "passed_steps": item["passed_steps"],
                         "skipped_steps": item["skipped_steps"],
                         "total_virtual_us": item["total_virtual_us"],
                         "bound_asset_identity": (scenario.get("assets_after_run") or {})
                         .get("wink_simulator.wasm", {}).get("sha256"),
                         "execution_identity_status": scenario.get("execution_identity_status")})
    built = [p for p in per_app if p["status"] != "NOT_RUN"]
    expected_files = sum(1 for app_dir in all_apps
                         for p in (EMBEDDED_ROOT / VENDOR_APPS_REL / app_dir.name
                                   / "unisim-scenarios").glob("*.scenario.json"))
    failed_pairs = [p["app"] for p in built if p["status"] != "PASS"]
    status = ("PARTIAL" if len(built) != len(all_apps)
              else "FAIL" if failed_pairs or len(rows) != expected_files else "PASS")
    return {"tool": TOOL_ID, "tool_fingerprint": tool_fingerprint(),
            "generated_at": now_iso(), "entry": entry["launcher"], "scope": scope,
            "apps_enumerated": len(all_apps), "apps_run": len(built),
            "scenario_files_expected": expected_files, "scenario_rows": len(rows),
            "selector_flags_used": [], "status": status,
            "apps": per_app, "scenarios": rows}


def phase_scenarios(args) -> int:
    run_root = run_root_of(args.run_root)
    entry = resolve_entry(args.wink_ai_root)
    all_apps = enumerate_apps(EMBEDDED_ROOT / VENDOR_APPS_REL)
    selected, scope = select_apps(all_apps, args)
    iso_base = iso_base_of(run_root)
    for app_dir in selected:
        name = app_dir.name
        tree = iso_tree(iso_base, all_apps, name)
        summary_path = run_root / "apps" / name / "build-summary.json"
        if not summary_path.is_file():
            raise SystemExit(f"dependency_error: run --phase apps for {name} first "
                             f"(missing {summary_path})")
        record = json.loads(summary_path.read_text(encoding="utf-8"))
        selected_round = record.get("selected_round")
        if record["reproducibility"]["status"] != "PASS" or selected_round is None:
            scenario = {"status": "BLOCKED",
                        "reason": f"build reproducibility is {record['reproducibility']['status']}"}
        else:
            scenario = scenario_run(entry, app_dir, EMBEDDED_ROOT, run_root, name, tree,
                                    record["rounds"][selected_round], args.run_timeout)
        record["scenario"] = scenario
        record["status"] = ("PASS" if record["rounds"]["A"]["status"] == "PASS"
                            and record["rounds"]["B"]["status"] == "PASS"
                            and record["reproducibility"]["status"] == "PASS"
                            and record["sdcc"]["status"] == "PASS"
                            and scenario["status"] == "PASS"
                            else "BLOCKED" if scenario["status"] == "BLOCKED"
                            or any(record["rounds"][label]["status"] == "BLOCKED"
                                   for label in ("A", "B")) else "FAIL")
        write_json(summary_path, record)
        shutil.rmtree(tree, ignore_errors=True)
        pairing = scenario.get("pairing") or {}
        print(f"[scenarios] {name}: {scenario['status']} "
              f"(bound={scenario.get('execution_identity_status')} "
              f"rows={len(pairing.get('scenarios') or [])})", flush=True)
    ledger = assemble_scenarios_ledger(run_root, all_apps, entry, scope)
    write_json(run_root / "scenarios.json", ledger)
    print(json.dumps({k: ledger[k] for k in ("apps_enumerated", "apps_run",
                                             "scenario_files_expected", "scenario_rows",
                                             "scope", "status")}, ensure_ascii=False))
    return EXIT_PASS if ledger["status"] == "PASS" else EXIT_FAIL


def read_ledger(run_root: Path, name: str) -> tuple[dict, dict]:
    handle = run_root / name
    if not handle.is_file():
        return {}, {"path": str(handle).replace("\\", "/"), "missing": True}
    return json.loads(handle.read_text(encoding="utf-8")), {
        "path": str(handle).replace("\\", "/"), "sha256": sha256_file(handle)}


def mirror_audit(flag: str, json_path: Path, run_root: Path) -> dict:
    """Re-run a read-only provenance gate so the delivery record cannot drift from S2."""
    command = [sys.executable, str(HERE / "audit_vendor_mirror.py"), flag, "--json", str(json_path)]
    env = {"PYTHONIOENCODING": "utf-8", "PYTHONUTF8": "1"}
    record, _, _ = run_proc(command, cwd=EMBEDDED_ROOT, env=env, timeout=600,
                            log_path=run_root / (json_path.stem + ".log"))
    report = json.loads(json_path.read_text(encoding="utf-8")) if json_path.is_file() else {}
    return {"status": record["status"], "command": record, "report": report,
            "report_sha256": sha256_file(json_path) if json_path.is_file() else None}


def phase_summary(args) -> int:
    run_root = run_root_of(args.run_root)
    entry = resolve_entry(args.wink_ai_root)
    preflight, preflight_input = read_ledger(run_root, "preflight.json")
    pilot, pilot_input = read_ledger(run_root, "pilot.json")
    apps, apps_input = read_ledger(run_root, "apps.json")
    scenarios, scenarios_input = read_ledger(run_root, "scenarios.json")
    mirror = mirror_audit("--verify", run_root / "mirror-verify.json", run_root)
    vectors = mirror_audit("--isr-audit", run_root / "isr-vector-audit.json", run_root)

    build_status = apps.get("status", "NOT_RUN")
    bound = [a for a in scenarios.get("apps", []) if a.get("execution_identity_status") == "bound"]
    leakage = [a for a in apps.get("apps", []) if any(a.get("path_leakage", {}).values())]
    execution_status = ("NOT_RUN" if not scenarios
                        else "PASS" if scenarios.get("apps_run") == scenarios.get("apps_enumerated")
                        and len(bound) == scenarios.get("apps_run") and not leakage else "FAIL")
    scenario_status = scenarios.get("status", "NOT_RUN")
    if scenario_status == "PASS" and scenarios.get("scenario_rows") != scenarios.get("scenario_files_expected"):
        scenario_status = "FAIL"
    automated = [build_status, execution_status, scenario_status,
                 "PASS" if mirror["status"] == "PASS" and vectors["status"] == "PASS" else "FAIL"]
    payload = {
        "tool": TOOL_ID, "tool_fingerprint": tool_fingerprint(), "generated_at": now_iso(),
        "run_root": str(run_root).replace("\\", "/"), "entry": entry,
        "inputs": {"preflight": preflight_input, "pilot": pilot_input, "apps": apps_input,
                   "scenarios": scenarios_input},
        "apps_frozen_at_preflight": preflight.get("apps", []),
        "statuses": {
            "build_baseline_status": build_status,
            "execution_evidence_status": execution_status,
            "scenario_suite_status": scenario_status,
            "functional_qualification_status": "NOT_RUN",
        },
        "status_definitions": {
            "build_baseline_status": "37 apps x 2 independently clean source-mode builds, all three "
                                     "emitted assets byte-identical per app, SDCC Tier-S gate green per app",
            "execution_evidence_status": "every scenario run consumed a triple byte-identical to its "
                                         "selected reproducible round and emitted no build path; the "
                                         "loaded-object identity itself stays unverified (no public "
                                         "report field names the loaded device-tree/JS/Wasm objects)",
            "scenario_suite_status": "every scenario file paired 1:1 with an executed engine row, tied "
                                     "to its raw sha256, with non-zero executed steps",
            "functional_qualification_status": "domain review of assertion strength and model gaps; a "
                                               "green pairing proves execution, not functional fitness",
        },
        "counts": {"apps_enumerated": apps.get("apps_enumerated"),
                   "apps_built": apps.get("apps_built"),
                   "identity_count": apps.get("identity_count"),
                   "identity_expected": apps.get("identity_expected"),
                   "scenario_files_expected": scenarios.get("scenario_files_expected"),
                   "scenario_rows": scenarios.get("scenario_rows"),
                   "bound_runs": len(bound), "pilot_status": pilot.get("status"),
                   "selector_flags_used": scenarios.get("selector_flags_used")},
        "provenance": {"mirror_verify": {"status": mirror["status"],
                                         "drift_counts": mirror["report"].get("drift_counts"),
                                         "report_sha256": mirror["report_sha256"]},
                       "isr_vector_audit": {"status": vectors["status"],
                                            "official_vector_count": vectors["report"].get(
                                                "official_vector_count"),
                                            "effective_registration_count": vectors["report"].get(
                                                "effective_registration_count"),
                                            "finding_count": len(vectors["report"].get("findings") or []),
                                            "report_sha256": vectors["report_sha256"]}},
        "apps": apps.get("apps", []), "scenarios": scenarios.get("scenarios", []),
        "scenario_apps": scenarios.get("apps", []),
    }
    payload["automated_status"] = "PASS" if all(s == "PASS" for s in automated) else "FAIL"
    write_json(run_root / "summary.json", payload)
    write_summary_markdown(run_root, payload)
    print(json.dumps(dict(payload["statuses"], automated=payload["automated_status"],
                          counts=payload["counts"]), ensure_ascii=False))
    return EXIT_PASS if payload["automated_status"] == "PASS" else EXIT_FAIL


def write_summary_markdown(run_root: Path, payload: dict) -> None:
    lines = [f"# CMS8S78xx build-baseline delivery — `{run_root.name}`", "",
             f"- driver: `{payload['tool_fingerprint']['driver_sha256'][:16]}`",
             f"- entry: `{payload['entry']['mode']}` `{payload['entry']['launcher']}`",
             f"- generated: {payload['generated_at']}", "", "| status axis | value |", "|---|---|"]
    for key, value in payload["statuses"].items():
        lines.append(f"| `{key}` | {value} |")
    lines += ["", f"`automated_status` = **{payload['automated_status']}**", "",
              "## Per-app ledger", "",
              "| app | A | B | reproducibility | sdcc | scenario | bound |", "|---|---|---|---|---|---|---|"]
    scenario_by_app = {a["app"]: a for a in payload["scenario_apps"]}
    for record in payload["apps"]:
        app = record["app"]
        scen = scenario_by_app.get(app, {})
        lines.append(f"| {app} | {record.get('round_A')} | {record.get('round_B')} | "
                     f"{record.get('reproducibility')} | {record.get('sdcc')} | "
                     f"{scen.get('status', 'NOT_RUN')} | {scen.get('execution_identity_status', '-')} |")
    lines += ["", "## Scenario ledger", "",
              "| app | scenario file | sha256 (first 12) | steps passed/declared | virtual us |",
              "|---|---|---|---|---|"]
    for row in payload["scenarios"]:
        lines.append(f"| {row['app']} | {row['scenario_file']} | `{row['scenario_sha256'][:12]}` | "
                     f"{row['passed_steps']}/{row['declared_steps']} | {row['total_virtual_us']} |")
    handle = run_root / "summary.md"
    handle.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def phase_preflight(args) -> int:
    run_root = run_root_of(args.run_root)
    entry = resolve_entry(args.wink_ai_root)
    apps = enumerate_apps(EMBEDDED_ROOT / VENDOR_APPS_REL)
    iso_base = iso_base_of(run_root)
    worst = max(apps, key=lambda app: len(app.name))
    budget = iso_path_budget(compile_root_for(iso_tree(iso_base, apps, worst.name) / "A",
                                              worst.relative_to(EMBEDDED_ROOT)))
    iso_base_text = str(iso_base).replace("\\", "/")
    payload = {"tool": TOOL_ID, "tool_fingerprint": tool_fingerprint(),
               "generated_at": now_iso(), "entry": entry,
               "entry_fingerprint": entry_fingerprint(entry),
               "app_count": len(apps), "apps": [p.name for p in apps],
               "isolation_method": "staged SDK/App copy bound through WINK_AI_EMBEDDED_DIR; "
                                   f"isolated trees under {iso_base_text}",
               "object_path_budget": {"worst_case_app": worst.name, **budget},
               "asset_names": list(ASSET_NAMES)}
    write_json(run_root / "preflight.json", payload)
    print(json.dumps({k: payload[k] for k in ("app_count", "isolation_method",
                                              "object_path_budget")}, ensure_ascii=False))
    print("entry:", entry["mode"], entry["launcher"])
    if budget["status"] != "PASS":
        print(f"[preflight] BLOCKED: the isolated tree for the longest app name would exceed "
              f"CMake's {OBJECT_PATH_LIMIT}-character object path "
              f"({budget['projected_length']}); shorten the run root before starting the batch")
        return EXIT_FAIL
    return EXIT_PASS


def phase_pilot(args) -> int:
    run_root = run_root_of(args.run_root)
    entry = resolve_entry(args.wink_ai_root)
    app_name = args.app
    app_dir = EMBEDDED_ROOT / VENDOR_APPS_REL / app_name
    if not app_dir.is_dir():
        raise SystemExit(f"app_error: {app_dir} is not a vendor app directory")
    all_apps = enumerate_apps(EMBEDDED_ROOT / VENDOR_APPS_REL)
    tree = iso_tree(iso_base_of(run_root), all_apps, app_name)
    rounds = {label: build_round(entry, app_dir, EMBEDDED_ROOT, run_root, app_name, tree, label,
                                 args.build_timeout) for label in ("A", "B")}
    for label, result in rounds.items():
        result["compile_evidence"] = collect_compile_evidence(
            Path(result["compile_root"]),
            run_root / "apps" / app_name / "builds" / label / "compile-evidence")
    reproducibility = compare_rounds(rounds["A"], rounds["B"])
    sdcc = sdcc_gate_run(app_dir, run_root, app_name, args.sdcc_timeout)
    selected_round = next((label for label in ("A", "B") if rounds[label]["status"] == "PASS"), "A")
    pilot = {"tool": TOOL_ID, "tool_fingerprint": tool_fingerprint(),
             "generated_at": now_iso(), "app": app_name, "iso_tree": str(tree).replace("\\", "/"),
             "entry_fingerprint": entry_fingerprint(entry), "rounds": rounds,
             "reproducibility": reproducibility, "sdcc": sdcc,
             "selected_round": selected_round}
    if reproducibility["status"] == "PASS":
        pilot["scenario"] = scenario_run(entry, app_dir, EMBEDDED_ROOT, run_root, app_name, tree,
                                         rounds[selected_round], args.run_timeout)
        pilot["scenario"]["selected_asset_identity"] = rounds[selected_round]["assets"]
    else:
        pilot["scenario"] = {"status": "BLOCKED",
                             "reason": f"A/B reproducibility is {reproducibility['status']}"}
    pilot["negatives"] = run_negatives(run_root, rounds, pilot.get("scenario"))
    pilot["status"] = ("PASS" if all(r["status"] == "PASS" for r in rounds.values())
                       and reproducibility["status"] == "PASS"
                       and sdcc["status"] == "PASS"
                       and pilot["scenario"]["status"] == "PASS"
                       and all(n["status"] == "PASS" for n in pilot["negatives"])
                       else "BLOCKED" if any(rounds[label]["status"] == "BLOCKED"
                                             for label in ("A", "B")) else "FAIL")
    shutil.rmtree(tree, ignore_errors=True)
    write_json(run_root / "pilot.json", pilot)
    print(json.dumps({"rounds": {k: v["status"] for k, v in rounds.items()},
                      "reproducibility": reproducibility["status"], "sdcc": sdcc["status"],
                      "scenario": pilot["scenario"]["status"],
                      "negatives": {n["case"]: n["status"] for n in pilot["negatives"]},
                      "status": pilot["status"]}, ensure_ascii=False))
    return EXIT_PASS if pilot["status"] == "PASS" else EXIT_FAIL


def fixture_identity(directory: Path) -> dict:
    """Write a complete asset triple so negatives never depend on a successful build."""
    directory.mkdir(parents=True, exist_ok=True)
    for name in ASSET_NAMES:
        (directory / name).write_bytes(b"cms8s78xx-baseline-negative-fixture\n")
    return asset_identity(directory)


def run_negatives(run_root: Path, rounds: dict, scenario) -> list[dict]:
    """The pilot must reject broken inputs, not only accept good ones."""
    results = []
    app_name = next((r["isolation"]["isolated_app_dir"].split("/")[-1] for r in rounds.values()), "app")
    missing_dir = run_root / "neg" / app_name / "neg-missing" / "assets"
    missing_dir.mkdir(parents=True, exist_ok=True)
    identity = asset_identity(missing_dir)
    results.append({"case": "missing_asset_rejected",
                    "status": "PASS" if not identity["complete"] and identity["device-tree.json"]["missing"]
                    else "FAIL", "observed": {"complete": identity["complete"]}})

    round_a = asset_identity(Path(rounds["A"]["assets_dir"]))
    basis = "round_A_assets"
    reference = round_a
    if not round_a["complete"]:
        basis = "synthetic_fixture"
        reference = fixture_identity(run_root / "neg" / app_name / "neg-assets")
    divergent = {name: dict(reference[name]) for name in ASSET_NAMES}
    divergent["wink_simulator.wasm"] = {"sha256": "0" * 64, "size_bytes": 1}
    mismatch = compare_rounds({"assets": divergent, "status": "PASS", "round": "A"},
                              {"assets": reference, "status": "PASS", "round": "B"})
    results.append({"case": "hash_mismatch_rejected",
                    "status": "PASS" if mismatch["status"] == "FAIL" else "FAIL",
                    "observed": {"basis": basis, "status": mismatch["status"],
                                 "divergent": [n for n, v in mismatch["assets"].items()
                                               if not v["identical"]]}})

    empty_after = {name: {"sha256": None, "size_bytes": None, "missing": True} for name in ASSET_NAMES}
    binding_missing = execution_identity_status(reference, empty_after)
    binding_divergent = execution_identity_status(reference, divergent)
    binding_bound = execution_identity_status(reference, dict(reference))
    results.append({"case": "executed_object_mismatch_rejected",
                    "status": "PASS" if (binding_missing == "missing_asset"
                                         and binding_divergent == "identity_mismatch"
                                         and binding_bound == "bound") else "FAIL",
                    "observed": {"basis": basis, "missing": binding_missing,
                                 "divergent": binding_divergent, "matching": binding_bound}})

    leaked = run_root / "neg" / app_name / "neg-leak"
    leaked.mkdir(parents=True, exist_ok=True)
    clean = run_root / "neg" / app_name / "neg-clean"
    clean.mkdir(parents=True, exist_ok=True)
    marker = str(leaked.parent).replace("\\", "/")
    for name in ASSET_NAMES:
        (clean / name).write_bytes(b"/wink-baseline/wink-micro-os/main.c\n")
    (leaked / "wink_simulator.wasm").write_bytes(b"x" + marker.replace("/", "\\").encode() + b"\x00")
    leak_hits = path_leakage(leaked, marker)
    clean_hits = path_leakage(clean, marker)
    results.append({"case": "embedded_build_path_rejected",
                    "status": "PASS" if (list(leak_hits) == ["wink_simulator.wasm"]
                                         and not clean_hits) else "FAIL",
                    "observed": {"leaked": leak_hits, "mapped": clean_hits}})

    pair_dir = run_root / "neg" / app_name / "neg-pairing"
    pair_dir.mkdir(parents=True, exist_ok=True)
    (pair_dir / "wanted.scenario.json").write_text(json.dumps(
        {"header": {"name": "wanted", "templateId": "t"}, "steps": [{"type": "ASSERT_POINT"}]}),
        encoding="utf-8", newline="\n")
    (pair_dir / "other.scenario.json").write_text(json.dumps(
        {"header": {"name": "other", "templateId": "t"}, "steps": [{"type": "ASSERT_POINT"}]}),
        encoding="utf-8", newline="\n")
    report = pair_dir / "run-report.json"
    report.write_text(json.dumps({"total": 1, "results": [{
        "ok": True, "status": "passed", "header": {"name": "ghost"},
        "summary": {"totalSteps": 1, "passedSteps": 1}, "stepResults": [
            {"stepIndex": 0, "status": "passed"}], "diagnostics": []}]}),
        encoding="utf-8", newline="\n")
    pairing = match_reports(report, scenario_files(pair_dir))
    results.append({"case": "scenario_pairing_rejected",
                    "status": "PASS" if (pairing["status"] == "FAIL"
                                         and pairing["unmatched_rows"]
                                         and sorted(pairing["unexecuted_files"])
                                         == ["other.scenario.json", "wanted.scenario.json"])
                    else "FAIL",
                    "observed": {"status": pairing["status"],
                                 "unmatched": pairing["unmatched_rows"],
                                 "unexecuted": pairing["unexecuted_files"]}})

    foreign = Path(run_root) / ".." / "outside-expected-tree"
    try:
        assert_clean_target(foreign.resolve(), run_root / "build")
        results.append({"case": "clean_outside_expected_tree_rejected", "status": "FAIL",
                        "observed": "no exception raised"})
    except SystemExit as exc:
        results.append({"case": "clean_outside_expected_tree_rejected", "status": "PASS",
                        "observed": str(exc)})

    deep = Path("D:/deep/" + "x" * 180) / "iso" / "00" / "A" / "build" / "wasm"
    over = iso_path_budget(deep)
    fitting = iso_path_budget(compile_root_for(iso_base_of(run_root) / "00" / "A",
                                               VENDOR_APPS_REL / "gpio"))
    results.append({"case": "iso_path_budget_rejected",
                    "status": "PASS" if (over["status"] == "BLOCKED"
                                         and over["projected_length"] > OBJECT_PATH_LIMIT
                                         and fitting["status"] == "PASS") else "FAIL",
                    "observed": {"over": {k: over[k] for k in ("projected_length", "status")},
                                 "batch": {k: fitting[k] for k in ("projected_length", "status")},
                                 "limit": OBJECT_PATH_LIMIT}})

    explicit_missing = None
    try:
        resolve_entry(str(run_root / "no-such-wink-root"))
    except SystemExit as exc:
        explicit_missing = str(exc)
    results.append({"case": "invalid_explicit_wink_ai_root_rejected",
                    "status": "PASS" if explicit_missing and "entry_error" in explicit_missing else "FAIL",
                    "observed": explicit_missing})

    no_reports, no_reports_failure = scenario_outcome("PASS", "bound", 0)
    unbound, unbound_failure = scenario_outcome("PASS", "identity_mismatch", 1)
    accepted, accepted_failure = scenario_outcome("PASS", "bound", 1)
    observed = {"no_reports": (no_reports, no_reports_failure),
                "unbound": (unbound, unbound_failure),
                "bound_with_report": (accepted, accepted_failure)}
    if scenario is not None:
        observed["formal_run_report_count"] = len(scenario.get("report_files", []))
    results.append({"case": "missing_report_rejected",
                    "status": "PASS" if (no_reports == "FAIL" and no_reports_failure == "missing_report"
                                         and unbound == "FAIL" and unbound_failure == "identity_mismatch"
                                         and accepted == "PASS" and accepted_failure is None) else "FAIL",
                    "observed": observed})
    return results


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--phase", required=True, choices=("preflight", "pilot", "apps",
                                                           "scenarios", "summary"))
    parser.add_argument("--run-root", required=True)
    parser.add_argument("--app", default="gpio")
    parser.add_argument("--only", default=None, help="comma-separated app names (rehearsal chunks)")
    parser.add_argument("--shard", default=None, help="k/N: process every Nth app, index k (1-based)")
    parser.add_argument("--wink-ai-root", default=None)
    parser.add_argument("--build-timeout", type=int, default=1800)
    parser.add_argument("--run-timeout", type=int, default=1200)
    parser.add_argument("--sdcc-timeout", type=int, default=600)
    args = parser.parse_args(argv[1:])
    if args.phase == "preflight":
        return phase_preflight(args)
    if args.phase == "apps":
        return phase_apps(args)
    if args.phase == "scenarios":
        return phase_scenarios(args)
    if args.phase == "summary":
        return phase_summary(args)
    return phase_pilot(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
