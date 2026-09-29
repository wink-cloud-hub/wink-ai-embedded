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


def _scenario_context(tmp_path, delivery_state):
    return {
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
            "entries": [{
                "id": "esp.core.sched",
                "display_id": 1,
                "required_capabilities": ["cap.core.fiber_task"],
                "executions": [{
                    "config_id": "wasm_browser",
                    "delivery_state": delivery_state,
                    "acceptance": {
                        "scenario_path": "get-started/nope/unisim-scenarios/nope.scenario.json",
                        "positive_cases": [{"name": "c1"}],
                    },
                }],
            }],
        },
    }


def test_g4_planned_execution_may_declare_absent_scenario(tmp_path):
    """A scenario_path on a `planned` execution is a planned declaration, not a
    claim of existence. 468 of 469 declared scenarios are absent because that
    work has not been done; enforcing presence would block all real progress."""
    findings = g4_impact_regression.run(
        _scenario_context(tmp_path, "planned"), {"max_inline_entries": 30})
    assert findings == [], findings


def test_g4_built_execution_requires_scenario_on_disk(tmp_path):
    """Once an execution claims to be built, a declared-but-missing scenario is a
    broken declaration. This mirrors g1_can_check_mark's evidence rule."""
    for state in ("building", "verified", "stale", "regressed"):
        findings = g4_impact_regression.run(
            _scenario_context(tmp_path, state), {"max_inline_entries": 30})
        assert len(findings) == 1, (state, findings)
        assert findings[0]["severity"] == "error"
        assert "not on disk" in findings[0]["message"]


def test_g4_never_claims_to_execute_regression():
    """The rule performs static structural validation only. Its public surface
    must not be named in a way that implies a headless run happened."""
    assert not hasattr(g4_impact_regression, "verify_scenario_headless")
    assert hasattr(g4_impact_regression, "validate_scenario_manifest")
    assert "headless" in g4_impact_regression.__doc__.lower()
    assert "does not execute" in g4_impact_regression.__doc__.lower()


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
    # The rule must not claim a regression was scheduled: nothing in CI consumes
    # the pending list, and this gate never marks entries stale or executes them.
    assert "NOT regression-tested by this gate" in findings[0]["message"]
    assert "No CI job consumes" in findings[0]["message"]
