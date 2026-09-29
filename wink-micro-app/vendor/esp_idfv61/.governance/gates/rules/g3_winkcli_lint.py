# SPDX-License-Identifier: Apache-2.0
"""
g3_winkcli_lint.py
==================
Gate 3 Rule: Delegates layering and architecture boundary checks to winkcli lint.
"""

import os
import sys
import json
import shutil
import hashlib
import subprocess
from pathlib import Path

import yaml

RULE_ID = "g3.winkcli_lint"

# Severities the gate engine is able to bucket. run_gates.py counts by exact
# string equality, so an unrecognized value would land in no bucket and would
# not influence the exit code at all.
SEVERITIES = frozenset({"error", "warning", "info"})

DEFAULT_PACKS = ["layering", "api"]

# Canonical local location of the toolchain. Per project convention the linter is
# always invoked through wink-ai/packages/wink-tools/wink.py, never through a
# `winkcli` binary on PATH: the packaged executable can be a stale build and would
# silently disagree with the rule data in this repo's wink-tools/tools/lint/rules.
CANONICAL_TOOLCHAIN_RELPATH = Path("wink-ai") / "packages" / "wink-tools" / "wink.py"


def find_winkcli_executable(ws_root: Path) -> list[str] | None:
    """Resolves the wink-tools entry point.

    Deliberately does NOT probe `shutil.which("winkcli")`: a packaged binary on
    PATH may be an older build whose rule set diverges from the one committed here.
    """
    # 1. Explicit override (used by CI, which checks out the toolchain at a pinned ref)
    tools_root_env = os.environ.get("WINK_TOOLS_ROOT")
    if tools_root_env:
        wink_py = Path(tools_root_env) / "wink.py"
        if wink_py.exists():
            return [sys.executable, str(wink_py), "lint"]

    # 2. Canonical sibling-repository path: wink-ai/packages/wink-tools/wink.py
    candidates = [
        ws_root.parent / CANONICAL_TOOLCHAIN_RELPATH,
        ws_root / CANONICAL_TOOLCHAIN_RELPATH,
        ws_root.parent / "packages" / "wink-tools" / "wink.py",
    ]
    for c in candidates:
        if c.exists():
            return [sys.executable, str(c), "lint"]

    return None


def resolve_overlays(ws_root: Path, config: dict) -> list[Path]:
    """Locates project-local lint overlay YAMLs.

    wink lint already supports overlays natively: a YAML carrying `overrides` or
    `disable_rules` is treated as an overlay rather than a pack, and
    `add_allow_paths` is appended to the built-in rule. Overlays are passed last,
    so they apply to the already-loaded built-in packs.

    This is how a project customises lint WITHOUT forking the toolchain's rule
    packs -- necessary here because the engine and the built-in rule data live in
    a private repository, while the paths that must be exempted are specific to
    this repository's layout.
    """
    out: list[Path] = []
    for entry in config.get("overlays", []):
        p = Path(entry)
        if not p.is_absolute():
            p = ws_root / p
        if p.exists():
            out.append(p)
    return out


def _build_cmd(cmd_base: list[str], ws_root: Path, packs: list[str],
               overlays: list[Path]) -> list[str]:
    """Single command builder so every branch carries packs and overlays alike."""
    cmd = list(cmd_base)
    cmd.extend([
        "--skip-toolchain-check",
        "--skip-auth-check",
        "--root", str(ws_root / "wink-micro-os"),
        "--format", "json",
    ])
    for p in packs:
        cmd.extend(["--pack", p])
    for ov in overlays:
        cmd.extend(["--config", str(ov)])
    return cmd


def toolchain_search_hint(ws_root: Path) -> str:
    """Human-actionable message listing where the toolchain was looked for.

    Deliberately describes the toolchain by its local path only. The engine lives
    in a private, commercial-secret repository; naming its remote, organisation
    or commit here would publish that infrastructure in this open-source repo.
    """
    probed = [str(ws_root.parent / CANONICAL_TOOLCHAIN_RELPATH),
              str(ws_root / CANONICAL_TOOLCHAIN_RELPATH)]
    return (
        f"Looked for: {', '.join(probed)}, or $WINK_TOOLS_ROOT/wink.py. "
        f"By convention the linter is invoked through "
        f"'wink-ai/packages/wink-tools/wink.py' from the sibling private "
        f"toolchain checkout, not through a 'winkcli' binary on PATH. "
        f"If the sibling checkout is absent, restore it and re-run; do not "
        f"point this gate at a PATH binary."
    )


# --- Toolchain fingerprint ---------------------------------------------------
# Gate 3 executes an engine that lives in a private, non-vendored repository.
# The lock makes that coupling auditable. Drift is a WARNING by design: the
# private toolchain is under active development, and hard-blocking on every
# engine change would train people to ignore the signal.

GOV_DIR = Path(__file__).resolve().parent.parent.parent
TOOLCHAIN_LOCK = GOV_DIR / "toolchain.lock.yaml"


def _sha256(p: Path) -> str | None:
    try:
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return None


def check_toolchain_fingerprint(entry_point: Path, ws_root: Path) -> list[dict]:
    """Compares the resolved engine against .governance/toolchain.lock.yaml.

    Returns WARNING findings on drift. A missing lock is tolerated (first run),
    but an UNREADABLE lock is reported: silently returning [] there would turn a
    corrupt baseline into a permanently disabled check that looks identical to a
    clean run.
    """
    def warn(message: str) -> list[dict]:
        return [{
            "rule_id": f"{RULE_ID}.toolchain_drift",
            "severity": "warning",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": str(TOOLCHAIN_LOCK.name),
            "message": message,
        }]

    if not TOOLCHAIN_LOCK.exists():
        return []
    try:
        with open(TOOLCHAIN_LOCK, "r", encoding="utf-8") as f:
            lock = yaml.safe_load(f) or {}
    except Exception as exc:
        return warn(
            f"Toolchain lock '{TOOLCHAIN_LOCK.name}' could not be parsed ({exc}). "
            f"Fingerprint checking is therefore NOT running for this gate; lint "
            f"verdicts cannot be traced to a recorded toolchain baseline. "
            f"Regenerate with: python .governance/tools/refresh_toolchain_lock.py"
        )
    if not isinstance(lock, dict) or "toolchain" not in lock:
        return warn(
            f"Toolchain lock '{TOOLCHAIN_LOCK.name}' is missing the required "
            f"'toolchain' key. Fingerprint checking is NOT running for this gate. "
            f"Regenerate with: python .governance/tools/refresh_toolchain_lock.py"
        )

    findings: list[dict] = []

    def warn(message: str) -> None:
        findings.append({
            "rule_id": f"{RULE_ID}.toolchain_drift",
            "severity": "warning",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": str(TOOLCHAIN_LOCK.name),
            "message": message,
        })

    expected = ((lock.get("toolchain") or {}).get("entry_sha256") or "").lower()
    if expected:
        actual = _sha256(entry_point)
        if actual and actual != expected:
            warn(
                f"Toolchain drift: engine at '{entry_point}' has sha256 "
                f"{actual[:12]} but toolchain.lock.yaml records {expected[:12]}. "
                f"Lint verdicts from this run are not comparable to the recorded "
                f"baseline. Refresh with: python .governance/tools/refresh_toolchain_lock.py"
            )

    # Rule-data mirror check: Gate 3 executes the private repo's rule copies, so
    # the mirror committed in this repo can silently stop describing what is linted.
    expected_rules = lock.get("rule_data") or {}

    # Overlay drift: the overlay is the only channel through which this project
    # customises lint, so its drift matters at least as much as the engine's.
    for oname, oexp in (lock.get("overlays") or {}).items():
        oexp = (oexp or "").lower()
        if not oexp:
            continue
        ofile = ws_root / "wink-tools" / "tools" / "lint" / "rules" / oname
        if not ofile.exists():
            warn(
                f"Overlay '{oname}' recorded in toolchain.lock.yaml is missing from "
                f"this repo. Lint exemptions recorded in the baseline are no longer "
                f"being applied."
            )
            continue
        oact = _sha256(ofile)
        if oact and oact != oexp:
            warn(
                f"Overlay drift: '{oname}' has sha256 {oact[:12]} but "
                f"toolchain.lock.yaml records {oexp[:12]}. Lint verdicts from this "
                f"run are not comparable to the recorded baseline."
            )

    if not expected_rules:
        return findings
    priv_root = entry_point.parent
    mirror_root = ws_root / "wink-tools" / "tools" / "lint" / "rules"
    for fname, exp in expected_rules.items():
        exp = (exp or "").lower()
        if not exp:
            continue
        priv_file = priv_root / "tools" / "lint" / "rules" / fname
        actual = _sha256(priv_file) if priv_file.exists() else None
        if actual and actual != exp:
            warn(
                f"Toolchain drift: rule '{fname}' in the executed private copy has "
                f"sha256 {actual[:12]} but toolchain.lock.yaml records {exp[:12]}."
            )
        mirror = mirror_root / fname
        if not mirror.exists():
            warn(f"Rule mirror missing in this repo: {mirror}")
        else:
            m = _sha256(mirror)
            if m and actual and m != actual:
                warn(
                    f"Rule mirror divergence: this repo's committed '{fname}' "
                    f"(sha256 {m[:12]}) does not match the copy Gate 3 actually "
                    f"executes ({actual[:12]}). The mirror is a reference copy -- it "
                    f"is never loaded; project customisation goes through the "
                    f"overlay. Sync with refresh_toolchain_lock.py --sync-mirror."
                )

    return findings


def _run_lint(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    config = config or {}
    packs = config.get("packs", DEFAULT_PACKS)
    strict_env = config.get("strict_environment", True)
    timeout_sec = config.get("timeout_seconds", 120)

    ws_root = Path(context.get("workspace_root", "."))
    cmd_base = find_winkcli_executable(ws_root)

    # In CI or strict_environment, missing toolchain is a blocking ERROR
    is_ci = os.environ.get("CI") == "true" or os.environ.get("GITHUB_ACTIONS") == "true"
    if not cmd_base:
        if strict_env or is_ci:
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": (
                    "Gate 3 Fail-Closed: wink-tools toolchain not found. "
                    "In CI or strict mode, skipping is strictly forbidden. "
                    + toolchain_search_hint(ws_root)
                ),
            }]
        else:
            return [{
                "rule_id": RULE_ID,
                "severity": "warning",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": "Gate 3: 'winkcli' toolchain not found, skipping lint checks in non-strict mode.",
            }]

    # Filter changed files under wink-micro-os
    changed_files = context.get("changed_files", [])
    os_changed_files = []
    for f in changed_files:
        if f.startswith("wink-micro-os/"):
            rel_to_os = f[len("wink-micro-os/"):]
            if rel_to_os.endswith((".c", ".h", ".cpp", ".hpp")):
                os_changed_files.append(rel_to_os)

    # A PR that does not touch wink-micro-os C code still gets the layering/api
    # pass, but the pack list must come from config rather than being hardcoded,
    # otherwise any config-driven change (e.g. added overlays or a new pack) is
    # silently dropped on exactly this branch.
    if context.get("mode") == "pr" and changed_files and not os_changed_files:
        packs = DEFAULT_PACKS

    cmd = _build_cmd(cmd_base, ws_root, packs, resolve_overlays(ws_root, config))

    if context.get("mode") == "pr" and os_changed_files:
        cmd.append("--paths")
        cmd.extend(os_changed_files)

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ws_root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_sec,
        )
    except subprocess.TimeoutExpired:
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": f"Gate 3: winkcli lint timed out after {timeout_sec} seconds.",
        }]
    except Exception as e:
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": f"Gate 3: winkcli lint failed to execute: {e}",
        }]

    # Parse JSON output
    stdout = proc.stdout.strip()
    if not stdout:
        if proc.returncode != 0:
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Gate 3: winkcli lint returned exit code {proc.returncode} with no output. Stderr: {proc.stderr[:200]}",
            }]
        return findings

    try:
        items = json.loads(stdout)
    except json.JSONDecodeError:
        # Non-JSON output (maybe plain text or error)
        if proc.returncode != 0:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Gate 3: winkcli lint returned error output:\n{stdout[:500]}",
            })
        return findings

    # A non-zero exit code means the tool did not complete its analysis. Reporting
    # whatever partial output it managed to emit (or an empty set) as success would
    # silently downgrade a hard toolchain failure into a clean pass.
    if proc.returncode != 0:
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": (
                f"Gate 3: winkcli lint exited with code {proc.returncode}; "
                f"its output must not be trusted as a clean result. "
                f"Stderr: {proc.stderr[:300]}"
            ),
        }]

    # Envelope drift guard: a non-list payload means the tool changed its output
    # contract, so we would silently discard every finding it reported.
    if not isinstance(items, list):
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": (
                f"Gate 3: winkcli lint returned a {type(items).__name__}, expected a JSON array "
                f"of findings. Output contract drift would silently drop all violations. "
                f"Payload head: {stdout[:300]}"
            ),
        }]

    for item in items:
        if not isinstance(item, dict):
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": (
                    f"Gate 3: winkcli lint emitted a non-object array element "
                    f"({type(item).__name__}); refusing to certify a partial result."
                ),
            }]
        if item.get("allowlisted"):
            continue  # Suppress allowlisted violations
        sev = str(item.get("severity", "error")).lower()
        if sev not in SEVERITIES:
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": (
                    f"Gate 3: winkcli lint reported unknown severity {item.get('severity')!r}; "
                    f"expected one of {sorted(SEVERITIES)}."
                ),
            }]
        rule_name = item.get("rule_id", "lint")
        path = item.get("path")
        line = item.get("line")
        msg = item.get("message", "")
        loc = f"{path}:{line}" if line else str(path)

        findings.append({
            "rule_id": f"g3.winkcli.{rule_name}",
            "severity": sev,
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": path,
            "message": f"{loc} - {msg}",
        })

    return findings


def run(context: dict, config: dict | None = None) -> list[dict]:
    """Runs the delegated lint, then appends toolchain fingerprint warnings.

    Fingerprint findings are merged in here rather than at each return site so
    that every exit path -- clean, error, or timeout -- still reports which
    engine produced the verdict and whether it has drifted from the baseline.
    """
    findings = _run_lint(context, config)

    ws_root = Path(context.get("workspace_root", "."))
    cmd_base = find_winkcli_executable(ws_root)
    if cmd_base and len(cmd_base) > 1 and Path(cmd_base[1]).exists():
        findings = findings + check_toolchain_fingerprint(Path(cmd_base[1]), ws_root)

    return findings
