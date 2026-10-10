# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.paths - Single Authority for Workspace & Governance Root Resolution
================================================================================
Implements Plan Phase 2 (T2.1):
- Resolves workspace root (anchored to file location or explicitly passed).
- Resolves governance root and validates mandatory marker files.
- Provides PathGuard topological validation to prevent recursive nesting.
"""
from __future__ import annotations

import os
from pathlib import Path
from typing import Optional


class PathTopologyViolationError(ValueError):
    """Raised when a resolved path violates workspace topology (e.g. recursive nesting)."""
    pass


class GovernanceRootNotFoundError(FileNotFoundError):
    """Raised when the governance root or its mandatory marker files cannot be found."""
    pass


class WorkspaceRootNotFoundError(FileNotFoundError):
    """Raised when the workspace root or its mandatory markers cannot be found."""
    pass


def resolve_workspace_root(start_path: Optional[str | Path] = None) -> Path:
    """
    Finds and validates the repository workspace root.
    
    If explicit start_path is provided:
        - If relative, resolved relative to caller's CWD.
        - Must contain 'wink-micro-os' and 'wink-micro-app/vendor/esp_idfv61/.governance'.
    If start_path is None:
        - Anchors to this file's location (checkout location) and searches upwards.
    
    Fail-Loud if not found or invalid. Never falls back to guessing or silent current dir.
    """
    if start_path is not None:
        candidate = Path(start_path).resolve()
        _validate_workspace_markers(candidate)
        return candidate

    # Anchor to __file__ location
    curr = Path(__file__).resolve()
    for parent in [curr] + list(curr.parents):
        if _is_workspace_root(parent):
            return parent

    raise WorkspaceRootNotFoundError(
        f"Unable to locate workspace root from anchor {curr}. "
        "Must contain 'wink-micro-os' and 'wink-micro-app/vendor/esp_idfv61/.governance'."
    )


def _is_workspace_root(p: Path) -> bool:
    has_os = (p / "wink-micro-os").is_dir()
    has_gov = (p / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance").is_dir()
    return has_os and has_gov


def _validate_workspace_markers(p: Path) -> None:
    if not p.is_dir():
        raise WorkspaceRootNotFoundError(f"Specified workspace root is not a directory: {p}")
    if not (p / "wink-micro-os").is_dir():
        raise WorkspaceRootNotFoundError(f"Missing mandatory marker 'wink-micro-os' in: {p}")
    gov = p / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
    if not gov.is_dir():
        raise WorkspaceRootNotFoundError(f"Missing mandatory marker '{gov}' in: {p}")


def resolve_governance_root(workspace_root: Optional[str | Path] = None) -> Path:
    """
    Finds and validates the .governance root.
    Validates root-level 'toolchain.lock.yaml' and 'data/checklist.data.json'.
    """
    ws = resolve_workspace_root(workspace_root)
    gov = ws / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
    
    if not gov.is_dir():
        raise GovernanceRootNotFoundError(f"Governance directory does not exist: {gov}")
        
    lock_file = gov / "toolchain.lock.yaml"
    data_file = gov / "data" / "checklist.data.json"
    
    if not lock_file.is_file():
        raise GovernanceRootNotFoundError(f"Missing governance marker 'toolchain.lock.yaml' in {gov}")
    if not data_file.is_file():
        raise GovernanceRootNotFoundError(f"Missing governance marker 'data/checklist.data.json' in {gov}")
        
    return gov


def validate_no_recursive_nesting(candidate_path: str | Path, base_root: Optional[Path] = None) -> Path:
    """
    PathGuard: Topological validation ensuring candidate_path does not erroneously
    nest repo relative paths into sub-roots.
    
    Example violation:
        base_root: .../wink-micro-app/vendor/esp_idfv61/.governance
        candidate_path: .../.governance/wink-micro-app/vendor/esp_idfv61
        
    Checks that relative to base_root (or within the path), duplicate repo segments
    do not form recursive layout loops.
    """
    resolved = Path(candidate_path).resolve()
    
    if base_root is not None:
        base_res = Path(base_root).resolve()
        try:
            rel = resolved.relative_to(base_res)
            rel_parts = rel.parts
            if "wink-micro-app" in rel_parts or ".governance" in rel_parts:
                raise PathTopologyViolationError(
                    f"PathGuard violation: Attempted to append workspace relative path '{rel}' "
                    f"onto sub-root '{base_res}'. Final path: {resolved}"
                )
        except ValueError:
            pass

    parts = list(resolved.parts)
    gov_indices = [i for i, part in enumerate(parts) if part == ".governance"]
    if len(gov_indices) > 1:
        raise PathTopologyViolationError(
            f"PathGuard violation: Recursive '.governance' segments detected in path: {resolved}"
        )
    app_indices = [i for i, part in enumerate(parts) if part == "wink-micro-app"]
    if len(app_indices) > 1:
        if app_indices[1] > app_indices[0] + 1:
            raise PathTopologyViolationError(
                f"PathGuard violation: Recursive 'wink-micro-app' segments detected in path: {resolved}"
            )

    return resolved


# Convenience backward compatibility aliases
find_workspace_root = resolve_workspace_root
find_governance_root = resolve_governance_root
