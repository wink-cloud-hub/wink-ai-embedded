# SPDX-License-Identifier: Apache-2.0
"""
gate_context.py
===============
Shared context builder for WinkMicroOS ESP-IDF Classification Gate System.

Loads and normalizes:
- checklist.data.json (Manifest SSOT, Schema v2.0)
- capability-catalog.yaml (Capability Catalog SSOT)
- quarantine.yaml (Quarantine debt allowlist with 14-day TTL)
- changed_files (POSIX normalized paths with deterministic CI handling)
- now_utc (UTC timestamp reference to prevent runner drift)
- workspace_root (Absolute path to repository root)
"""

import os
import sys
import re
import json
import subprocess
from pathlib import Path
from datetime import datetime, timezone
import yaml


class ManifestSchemaError(ValueError):
    """Raised when the checklist manifest is structurally unusable.

    Fail-closed by design: a truncated or half-written manifest must abort the
    gate run, never silently pass as an empty (violation-free) corpus.
    """


def validate_manifest_schema(manifest, source_path) -> str:
    """Validates the structural integrity of checklist.data.json at load time.

    Every downstream rule reads ``context["manifest"].get("entries", [])``, which
    returns ``[]`` both for an absent key and for a present-but-empty list. Without
    this guard a botched regeneration (crash, merge conflict, bad sed) produces a
    fully green gate run over zero entries -- the worst possible failure mode,
    because it is indistinguishable from a clean pass.

    Returns the manifest's declared ``spec_version``.
    Raises ManifestSchemaError (subclass of ValueError) on any violation.
    """
    if not isinstance(manifest, dict):
        raise ManifestSchemaError(
            f"Manifest root must be a JSON object, got {type(manifest).__name__} ({source_path})"
        )

    if "spec_version" not in manifest:
        raise ManifestSchemaError(f"Manifest is missing required key 'spec_version' ({source_path})")
    spec_version = manifest["spec_version"]
    if not isinstance(spec_version, str) or not re.match(r"^\d+\.\d+\.\d+$", spec_version):
        raise ManifestSchemaError(
            f"Manifest 'spec_version' must be a MAJOR.MINOR.PATCH string, got {spec_version!r} ({source_path})"
        )

    if "entries" not in manifest:
        raise ManifestSchemaError(
            f"Manifest is missing required key 'entries' ({source_path}). "
            "A truncated manifest must never be reported as a clean gate run."
        )
    entries = manifest["entries"]
    if not isinstance(entries, list):
        raise ManifestSchemaError(
            f"Manifest 'entries' must be a JSON array, got {type(entries).__name__} ({source_path})"
        )
    if not entries:
        raise ManifestSchemaError(
            f"Manifest 'entries' is empty ({source_path}). "
            "Refusing to certify a zero-entry corpus as fully compliant."
        )

    declared_total = manifest.get("total_entries")
    if declared_total is not None:
        if not isinstance(declared_total, int):
            raise ManifestSchemaError(
                f"Manifest 'total_entries' must be an integer, got {type(declared_total).__name__} ({source_path})"
            )
        if declared_total != len(entries):
            raise ManifestSchemaError(
                f"Manifest 'total_entries' ({declared_total}) disagrees with the actual entry "
                f"count ({len(entries)}) ({source_path}). Data file is likely partially written."
            )

    bad = [i for i, e in enumerate(entries) if not isinstance(e, dict)]
    if bad:
        raise ManifestSchemaError(
            f"Manifest has {len(bad)} non-object entry element(s) at indices {bad[:5]} ({source_path})"
        )

    return spec_version


def find_workspace_root(start_path: Path | None = None) -> Path:
    """Finds the workspace root by looking for wink-micro-os or .git."""
    curr = (start_path or Path(__file__)).resolve()
    for parent in [curr] + list(curr.parents):
        if (parent / "wink-micro-os").exists() and (parent / "wink-micro-app").exists():
            return parent
        if (parent / ".git").exists():
            return parent
    return curr.parents[4] if len(curr.parents) >= 5 else curr.parent


def normalize_posix_path(p: str | Path) -> str:
    """Normalizes path to clean POSIX style (forward slashes, normpath)."""
    norm = os.path.normpath(str(p)).replace("\\", "/")
    # Remove leading './' if present
    if norm.startswith("./"):
        norm = norm[2:]
    return norm


def load_changed_files(
    changed_files_path: str | Path | None,
    workspace_root: Path,
    mode: str,
    allow_empty_diff: bool = False,
) -> list[str]:
    """
    Deterministically resolves changed files list.
    Enforces strict defenses: in CI mode, empty diff without --allow-empty-diff raises ValueError.
    """
    is_ci = os.environ.get("CI") == "true" or os.environ.get("GITHUB_ACTIONS") == "true"
    changed_files: list[str] = []

    if changed_files_path:
        cfp = Path(changed_files_path)
        if not cfp.is_absolute():
            cfp = workspace_root / cfp
        if not cfp.exists():
            raise FileNotFoundError(f"Changed files file does not exist: {cfp}")
        with open(cfp, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    changed_files.append(normalize_posix_path(line))
        if not changed_files and mode == "pr" and not allow_empty_diff:
            raise ValueError(
                f"Changed files file '{changed_files_path}' contains 0 files in PR mode. "
                "Pass --allow-empty-diff if this PR deliberately contains zero file changes."
            )
        return changed_files

    # If no file provided and running in CI
    if is_ci and not allow_empty_diff and mode == "pr":
        raise ValueError(
            "CI environment detected without --changed-files specified. "
            "To prevent silent pass on shallow clones, CI must explicitly pass --changed-files "
            "or --allow-empty-diff."
        )

    # Local debugging fallback: attempt git status/diff
    try:
        res = subprocess.run(
            ["git", "diff", "--name-only", "HEAD"],
            cwd=str(workspace_root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        if res.returncode == 0 and res.stdout.strip():
            for line in res.stdout.splitlines():
                line = line.strip()
                if line:
                    changed_files.append(normalize_posix_path(line))

        # Also include untracked/staged files
        res_st = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=str(workspace_root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        if res_st.returncode == 0 and res_st.stdout.strip():
            for line in res_st.stdout.splitlines():
                if len(line) > 3:
                    fpath = line[3:].strip()
                    # Handle renames e.g. R old -> new
                    if " -> " in fpath:
                        fpath = fpath.split(" -> ")[1].strip()
                    norm = normalize_posix_path(fpath)
                    if norm not in changed_files:
                        changed_files.append(norm)
    except Exception:
        # Git unavailable or failed
        pass

    return changed_files


def build_context(
    manifest_path: str | Path | None = None,
    catalog_path: str | Path | None = None,
    quarantine_path: str | Path | None = None,
    changed_files_path: str | Path | None = None,
    mode: str = "pr",
    allow_empty_diff: bool = False,
    workspace_root: str | Path | None = None,
    now_utc: datetime | None = None,
) -> dict:
    """
    Constructs the shared context dict passed to each gate rule.
    """
    ws_root = Path(workspace_root).resolve() if workspace_root else find_workspace_root()

    default_esp_dir = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
    default_gov_dir = default_esp_dir / ".governance"

    if manifest_path:
        m_path = Path(manifest_path).resolve()
    else:
        if (default_gov_dir / "data" / "checklist.data.json").exists():
            m_path = default_gov_dir / "data" / "checklist.data.json"
        elif (default_gov_dir / "checklist.data.json").exists():
            m_path = default_gov_dir / "checklist.data.json"
        else:
            m_path = default_esp_dir / "checklist.data.json"

    if catalog_path:
        c_path = Path(catalog_path).resolve()
    else:
        if (default_gov_dir / "catalog" / "capability-catalog.yaml").exists():
            c_path = default_gov_dir / "catalog" / "capability-catalog.yaml"
        elif (default_gov_dir / "capability-catalog.yaml").exists():
            c_path = default_gov_dir / "capability-catalog.yaml"
        else:
            c_path = default_esp_dir / "capability-catalog.yaml"

    if quarantine_path:
        q_path = Path(quarantine_path).resolve()
    else:
        if (default_gov_dir / "gates" / "quarantine.yaml").exists():
            q_path = default_gov_dir / "gates" / "quarantine.yaml"
        elif (default_gov_dir / "quarantine.yaml").exists():
            q_path = default_gov_dir / "quarantine.yaml"
        else:
            q_path = default_gov_dir / "gates" / "quarantine.yaml"

    if not m_path.exists():
        raise FileNotFoundError(f"Checklist manifest file not found: {m_path}")
    if not c_path.exists():
        raise FileNotFoundError(f"Capability catalog file not found: {c_path}")
    if not q_path.exists():
        raise FileNotFoundError(f"Quarantine file not found: {q_path}")

    with open(m_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    manifest_spec_version = validate_manifest_schema(manifest, m_path)

    with open(c_path, "r", encoding="utf-8") as f:
        catalog = yaml.safe_load(f) or {}

    with open(q_path, "r", encoding="utf-8") as f:
        quarantine = yaml.safe_load(f) or {}

    changed_files = load_changed_files(
        changed_files_path=changed_files_path,
        workspace_root=ws_root,
        mode=mode,
        allow_empty_diff=allow_empty_diff,
    )

    current_utc = now_utc if now_utc is not None else datetime.now(timezone.utc)

    return {
        "manifest": manifest,
        "catalog": catalog,
        "quarantine": quarantine,
        "changed_files": changed_files,
        "workspace_root": str(ws_root).replace("\\", "/"),
        "spec_version": manifest_spec_version,
        "mode": mode,
        "now_utc": current_utc,
    }
