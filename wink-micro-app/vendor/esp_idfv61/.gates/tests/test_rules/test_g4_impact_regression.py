# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g4_impact_regression.py.
"""

from pathlib import Path
from rules import g4_impact_regression

FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def test_g4_no_changed_files_empty():
    context = {"changed_files": []}
    findings = g4_impact_regression.run(context)
    assert len(findings) == 0


def test_g4_inline_regression_passed(tmp_path):
    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"],
        "catalog": {
            "capabilities": {
                "cap.core.fiber_task": {
                    "owned_paths": ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"],
                }
            }
        },
        "manifest": {
            "entries": [
                {
                    "id": "esp.get_started.blink",
                    "display_id": 1,
                    "required_capabilities": ["cap.core.fiber_task"],
                    "executions": [
                        {
                            "config_id": "cfg1",
                            "acceptance": {
                                "scenario_path": None,
                            },
                        }
                    ],
                }
            ]
        },
    }
    findings = g4_impact_regression.run(context)
    assert len(findings) == 0


def test_g4_exceed_pr_inline_threshold_marks_stale_warning(tmp_path):
    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"],
        "catalog": {
            "capabilities": {
                "cap.core.fiber_task": {
                    "owned_paths": ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"],
                }
            }
        },
        "manifest": {
            "entries": [
                {
                    "id": f"esp.entry.{i}",
                    "display_id": i,
                    "required_capabilities": ["cap.core.fiber_task"],
                    "executions": [],
                }
                for i in range(1, 35)
            ]
        },
    }
    findings = g4_impact_regression.run(context, {"max_inline_entries": 30})
    assert len(findings) == 1
    assert findings[0]["severity"] == "warning"
    assert "exceeds PR threshold" in findings[0]["message"]
    assert "stale" in findings[0]["message"]
