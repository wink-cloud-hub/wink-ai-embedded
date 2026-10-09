# SPDX-License-Identifier: Apache-2.0
"""
test_ssot_invariants.py
=======================
Unit tests for .github/scripts/check_ssot_invariants.py.
"""

import json
import re
import sys
from pathlib import Path

# Add repo root to sys.path to import check_ssot_invariants
REPO_ROOT = Path(__file__).resolve().parents[6]
SCRIPTS_DIR = REPO_ROOT / ".github" / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from check_ssot_invariants import verify_ssot_invariants


def test_real_workspace_ssot_invariants():
    """Verify that current live workspace satisfies all SSOT invariants."""
    errors = verify_ssot_invariants(REPO_ROOT)
    assert errors == [], f"Expected 0 errors, got: {errors}"


def test_summary_mismatch_detected(tmp_path):
    """Verify that any mismatch between summary and actual entries is flagged."""
    # Create minimal mock structure
    gov_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    data_dir = gov_dir / ".governance" / "data"
    data_dir.mkdir(parents=True)

    data = {
        "total_entries": 1,
        "summary": {
            "scope_in": 1,
            "scope_out": 0,
            "scope_unknown": 0,
            "in_scope_active": 1,
            "in_scope_deferred": 0,
            "audited": 99,  # Intentional error: 99 != 1
            "verified_configs": 1,
        },
        "entries": [
            {
                "id": "esp.test.blink",
                "display_id": 1,
                "scope": {"inclusion": "in_scope", "schedule": "active"},
                "audit": {"verdict": "audited"},
                "executions": [{"delivery_state": "verified"}],
            }
        ],
    }
    (data_dir / "checklist.data.json").write_text(json.dumps(data), encoding="utf-8")

    md_content = """
## 一、 总体适配进度统计
- **官方独立示例总数**：**1 个**
  - `[x]` **已完成六要素实证 (Verified)**：**1 项**
  - `[ ]` **规划中正常排期 (In-Scope Planned)**：**0 项**
  - `[-]` **明确产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**0 项**
  - `?` **待深度审定 (Pending Audit / Unknown Scope)**：**0 项**

| [x] | 001 | `test` | Level 1 | P0 | app | desc |
"""
    (gov_dir / "CHECKLIST.md").write_text(md_content, encoding="utf-8")

    errors = verify_ssot_invariants(tmp_path)
    assert any("audited (99) != computed audited (1)" in e for e in errors)


def test_broken_display_id_detected(tmp_path):
    """Verify that gaps or duplicate display_ids are flagged."""
    gov_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    data_dir = gov_dir / ".governance" / "data"
    data_dir.mkdir(parents=True)

    data = {
        "total_entries": 2,
        "summary": {
            "scope_in": 2,
            "scope_out": 0,
            "scope_unknown": 0,
            "in_scope_active": 2,
            "in_scope_deferred": 0,
            "audited": 0,
            "verified_configs": 0,
        },
        "entries": [
            {
                "id": "esp.test.one",
                "display_id": 1,
                "scope": {"inclusion": "in_scope", "schedule": "active"},
                "audit": {"verdict": "pending"},
                "executions": [],
            },
            {
                "id": "esp.test.two",
                "display_id": 3,  # Missing display_id 2!
                "scope": {"inclusion": "in_scope", "schedule": "active"},
                "audit": {"verdict": "pending"},
                "executions": [],
            },
        ],
    }
    (data_dir / "checklist.data.json").write_text(json.dumps(data), encoding="utf-8")
    (gov_dir / "CHECKLIST.md").write_text("", encoding="utf-8")

    errors = verify_ssot_invariants(tmp_path)
    assert any("display_id sequence broken" in e for e in errors)


def test_checklist_md_mismatch_detected(tmp_path):
    """Verify that tampered figures in CHECKLIST.md are detected."""
    gov_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    data_dir = gov_dir / ".governance" / "data"
    data_dir.mkdir(parents=True)

    data = {
        "total_entries": 1,
        "summary": {
            "scope_in": 1,
            "scope_out": 0,
            "scope_unknown": 0,
            "in_scope_active": 1,
            "in_scope_deferred": 0,
            "audited": 1,
            "verified_configs": 1,
        },
        "entries": [
            {
                "id": "esp.test.blink",
                "display_id": 1,
                "scope": {"inclusion": "in_scope", "schedule": "active"},
                "audit": {"verdict": "audited"},
                "executions": [{"delivery_state": "verified"}],
            }
        ],
    }
    (data_dir / "checklist.data.json").write_text(json.dumps(data), encoding="utf-8")

    # Tampered markdown claim: 5 verified instead of 1
    md_content = """
## 一、 总体适配进度统计
- **官方独立示例总数**：**1 个**
  - `[x]` **已完成六要素实证 (Verified)**：**5 项**
  - `[ ]` **规划中正常排期 (In-Scope Planned)**：**0 项**
  - `[-]` **明确产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**0 项**
  - `?` **待深度审定 (Pending Audit / Unknown Scope)**：**0 项**

| [x] | 001 | `test` | Level 1 | P0 | app | desc |
"""
    (gov_dir / "CHECKLIST.md").write_text(md_content, encoding="utf-8")

    errors = verify_ssot_invariants(tmp_path)
    assert any("CHECKLIST.md verified (5) != 1" in e for e in errors)
