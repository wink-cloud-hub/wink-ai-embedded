# SPDX-License-Identifier: Apache-2.0
"""
Tests for loop.harness.paths and PathGuard anti-recursive topology validation (Phase 2, INV-10).
"""
import pytest
from pathlib import Path

from loop.harness.paths import (
    resolve_workspace_root,
    resolve_governance_root,
    validate_no_recursive_nesting,
    PathTopologyViolationError,
    WorkspaceRootNotFoundError,
    GovernanceRootNotFoundError,
)


def test_resolve_workspace_root_auto():
    ws = resolve_workspace_root()
    assert ws.is_dir()
    assert (ws / "wink-micro-os").is_dir()
    assert (ws / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance").is_dir()


def test_resolve_governance_root_auto():
    gov = resolve_governance_root()
    assert gov.is_dir()
    assert (gov / "toolchain.lock.yaml").is_file()
    assert (gov / "data" / "checklist.data.json").is_file()


def test_resolve_workspace_root_invalid():
    with pytest.raises(WorkspaceRootNotFoundError):
        resolve_workspace_root(Path.home())


def test_pathguard_valid_path():
    ws = resolve_workspace_root()
    gov = resolve_governance_root()
    # A valid path should pass without error
    valid_path = gov / "runs" / "output_123"
    assert validate_no_recursive_nesting(valid_path, gov) == valid_path.resolve()


def test_pathguard_rejects_recursive_subpath():
    gov = resolve_governance_root()
    # Attempting to append repo relative path onto .governance
    bad_nested = gov / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
    with pytest.raises(PathTopologyViolationError):
        validate_no_recursive_nesting(bad_nested, gov)


def test_pathguard_rejects_duplicate_governance_segments():
    bad_path = Path("/some/path/wink-micro-app/vendor/esp_idfv61/.governance/nested/.governance")
    with pytest.raises(PathTopologyViolationError):
        validate_no_recursive_nesting(bad_path)
