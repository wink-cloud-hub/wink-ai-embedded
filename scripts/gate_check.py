# SPDX-License-Identifier: GPL-3.0-only
"""
gate_check.py
=============
Single local entry point for the ESP-IDF classification gate system.

This is the local counterpart to the GitHub Actions jobs. It exists because the
gate rules only enforce anything when something actually invokes them; without
an entry point, every fail-closed guard silently decays back into decoration.

It runs, in order:
  1. pytest over the gate engine's own tests (meta: does the enforcer still work?)
  2. run_gates.py over the SSOT corpus
  3. the checklist generator in --dry-run mode (SSOT -> dashboard agreement)

Toolchain convention: the linter is invoked through
``wink-ai/packages/wink-tools/wink.py`` (sibling repository), never a
``winkcli`` binary on PATH.

Usage:
    python scripts/gate_check.py                 # staged + unstaged changes
    python scripts/gate_check.py --all           # full sweep, ignore diff
    python scripts/gate_check.py --no-tests      # skip the engine self-test
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
GATES_DIR = REPO_ROOT / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance" / "gates"
RUN_GATES = GATES_DIR / "run_gates.py"
GATE_TESTS = GATES_DIR / "tests"
CHECKLIST_DATA = (REPO_ROOT / "wink-micro-app" / "vendor" / "esp_idfv61"
                  / ".governance" / "data" / "checklist.data.json")
GENERATOR = (REPO_ROOT / "wink-micro-app" / "vendor" / "esp_idfv61"
             / ".governance" / "tools" / "generate_checklist_v1_1.py")
CHECKLIST_MD = REPO_ROOT / "wink-micro-app" / "vendor" / "esp_idfv61" / "CHECKLIST.md"

# Gate 1 (10 rules) + Gate 3 (1 rule) always run. Gate 2 / Gate 4 are
# git_diff-triggered and legitimately skip when nothing they watch changed.
MIN_RULES_EXECUTED = 11


def _banner(text: str) -> None:
    print(f"\n{'=' * 72}\n  {text}\n{'=' * 72}")


def _run(cmd, cwd=REPO_ROOT, quiet=False):
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    if quiet:
        return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True).returncode
    return subprocess.run(cmd, cwd=str(cwd)).returncode


def changed_files_path(tmp: Path, all_files: bool) -> Path | None:
    """Builds the changed-files list. Returns None to signal --allow-empty-diff.

    Untracked files are included deliberately: a brand-new `dal_ws2812.h` is
    exactly the kind of violation these gates exist to catch, and `git diff`
    alone never reports it.
    """
    if all_files:
        return None
    names: set[str] = set()
    for cmd in (
        ["git", "diff", "--name-only", "HEAD"],
        ["git", "diff", "--name-only", "--cached"],
        ["git", "ls-files", "--others", "--exclude-standard"],
    ):
        proc = subprocess.run(cmd, cwd=str(REPO_ROOT), capture_output=True, text=True)
        names |= {ln.strip() for ln in proc.stdout.splitlines() if ln.strip()}
    if not names:
        return None
    tmp.write_text("\n".join(sorted(names)) + "\n", encoding="utf-8")
    print(f"  {len(names)} changed/untracked file(s) detected")
    return tmp


def main() -> int:
    ap = argparse.ArgumentParser(description="Local ESP-IDF governance gate check")
    ap.add_argument("--all", action="store_true",
                    help="Full sweep; do not restrict to a git diff")
    ap.add_argument("--no-tests", action="store_true",
                    help="Skip the gate engine self-test")
    args = ap.parse_args()

    import tempfile
    tmp = Path(tempfile.gettempdir()) / "wink_gate_changed_files.txt"

    # ---- Step 1: does the enforcer itself still work? -------------------------
    failures = []
    if not args.no_tests:
        _banner("Step 1/3  Gate engine self-test (meta-guard)")
        rc = _run([sys.executable, "-m", "pytest", str(GATE_TESTS), "-q"])
        if rc != 0:
            failures.append("gate engine self-test")
    else:
        _banner("Step 1/3  Gate engine self-test  [skipped by --no-tests]")

    # ---- Step 2: the actual gates ---------------------------------------------
    _banner("Step 2/3  Gate system (SSOT corpus)")
    cf = changed_files_path(tmp, args.all)
    cmd = [sys.executable, str(RUN_GATES), "--mode", "pr",
           "--require-executed", str(MIN_RULES_EXECUTED)]
    if cf is None:
        print("  no changes detected; running with --allow-empty-diff")
        cmd.append("--allow-empty-diff")
    else:
        cmd += ["--changed-files", str(cf)]
    if _run(cmd) != 0:
        failures.append("gate system")

    # ---- Step 3: is the generated dashboard still in sync? --------------------
    _banner("Step 3/3  Checklist regeneration (SSOT -> dashboard)")
    if GENERATOR.exists():
        rc = _run([sys.executable, str(GENERATOR), "--dry-run"], quiet=True)
        if rc != 0:
            failures.append("checklist generator dry-run")
        else:
            print("  SSOT -> dashboard regeneration is consistent (dry-run OK)")
    else:
        print(f"  generator not found: {GENERATOR}")
        failures.append("checklist generator missing")

    # ---- Verdict --------------------------------------------------------------
    _banner("Verdict")
    if failures:
        print("  FAILED:")
        for f in failures:
            print(f"    - {f}")
        return 1
    print("  All governance gates passed.")
    print(f"  Toolchain convention: linter invoked via "
          f"wink-ai/packages/wink-tools/wink.py (not a PATH winkcli).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
