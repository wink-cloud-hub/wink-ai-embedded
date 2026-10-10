# SPDX-License-Identifier: Apache-2.0
"""
Tests for Rollout Manager, Defect Feedback, and Observability Tracker (V1-T2, V1-T3, V1-T4)
========================================================================================
Verifies:
- V1-T2: Pilot slicing, failure domain freeze, 100% pass expansion authorization.
- V1-T3: Defect registry updates and reverse transitive impact propagation on verified items.
- V1-T4: Observability metrics recording and Stop-the-Line circuit breaker tripping.
"""

import json
import tempfile
from pathlib import Path

import pytest

from loop.services.defect_feedback import DefectFeedbackManager
from loop.batch_observability import BatchObservabilityTracker
from loop.batch_rollout import RolloutManager


@pytest.fixture
def temp_dir():
    with tempfile.TemporaryDirectory() as td:
        yield Path(td)


# =========================================================================
# V1-T2: Batch Rollout Tests
# =========================================================================

def test_rollout_manager_pilot_creation(temp_dir):
    mgr = RolloutManager()
    batch = mgr.create_pilot_batch(lane=2, pilot_size=3)
    assert batch.get("status") == "SCHEDULED"
    assert len(batch.get("entries", [])) == 3
    assert batch.get("domain_key") == "lane_2"
    assert "timeout_per_entry_sec" in batch.get("budget", {})


def test_rollout_manager_freezes_domain_on_pilot_failure(temp_dir):
    mgr = RolloutManager()
    domain_key = "test_lane_failure"

    # Simulate pilot results with one failure
    results = [
        {"app_id": "app_1", "success": True},
        {"app_id": "app_2", "success": False, "message": "Assertion failed: expected 1 got 0"},
    ]
    decision = mgr.evaluate_pilot(domain_key, results)
    assert decision.status == "DOMAIN_FROZEN"
    assert decision.expansion_authorized is False
    assert len(decision.failed_items) == 1

    # Attempting to create pilot on frozen domain must be blocked
    mgr.state_file = temp_dir / "rollout_state.json"
    mgr._save_state({"domains": {domain_key: "FROZEN"}, "frozen_reasons": {domain_key: "Pilot failed"}})
    blocked_batch = mgr.create_pilot_batch(domain_keyword=domain_key)
    assert blocked_batch.get("status") == "BLOCKED"

    # Clean up / unfreeze
    mgr.unfreeze_domain(domain_key, "Remediated")


def test_rollout_manager_authorizes_expansion_on_all_passed(temp_dir):
    mgr = RolloutManager()
    domain_key = "test_lane_success"

    results = [
        {"app_id": "app_1", "success": True},
        {"app_id": "app_2", "success": True},
        {"app_id": "app_3", "success": True},
    ]
    decision = mgr.evaluate_pilot(domain_key, results)
    assert decision.status == "PILOT_PASSED"
    assert decision.expansion_authorized is True
    assert decision.max_expansion_size > 0


# =========================================================================
# V1-T3: Defect Feedback & Impact Closure Tests
# =========================================================================

def test_defect_feedback_registry_and_recording(temp_dir):
    mgr = DefectFeedbackManager(vendor_root=temp_dir)
    defects = mgr.load_registry().get("defects", [])
    assert len(defects) >= 5
    assert any(d.get("defect_id") == "S-01" for d in defects)

    # Record a new defect
    res = mgr.record_defect(
        defect_id="DEF-TEST-01",
        category="DRIVER_RUNTIME",
        title="Test defect recording",
        affected_components=["wink-micro-os/test/test.c"],
        regression_test_id="AT-99",
        status="TRACKING",
        resolution="Fix in progress",
    )
    assert res.get("status") == "SUCCESS"

    reg = mgr.load_registry()
    assert any(d.get("defect_id") == "DEF-TEST-01" for d in reg.get("defects", []))


def test_defect_feedback_evaluates_impact_and_invalidates_stale(temp_dir):
    mgr = DefectFeedbackManager()
    # Evaluating impact on gptimer driver must pinpoint timer entry
    res = mgr.evaluate_impact(["wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c"])
    assert res.get("hit_global") is False
    assert res.get("total_impacted_entries") >= 1
    assert res.get("stale_count") >= 1
    assert any(item.get("id") == "esp.peripherals.timer_group.gptimer" for item in res.get("verified_stale_entries", []))
    assert res.get("directive") == "INVALIDATE_STALE_EVIDENCE_PACKAGES"


# =========================================================================
# V1-T4: Observability Tracker & Circuit Breaker Tests
# =========================================================================

def test_observability_tracker_normal_metrics(temp_dir):
    tracker = BatchObservabilityTracker(
        batch_id="TEST-BATCH-01",
        expected_apps=["app1", "app2"],
    )
    tracker.record_app_result(
        "app1",
        True,
        "CANDIDATE",
        "Passed",
        candidate_meta={"firmware_mutation": {"operator_id": "MUT-GPIO-INV"}},
    )
    tracker.record_app_result("app2", True, "CANDIDATE", "Passed")

    assert tracker.circuit_breaker_tripped is False
    assert tracker.coverage_stats["baseline_passed"] == 2
    assert tracker.coverage_stats["firmware_mutations_killed"] == 1

    summary_file = tracker.finalize_summary(temp_dir)
    assert summary_file.is_file()
    summary = json.loads(summary_file.read_text(encoding="utf-8"))
    assert summary["execution_rate_pct"] == 100.0
    assert summary["pass_rate_pct"] == 100.0
    assert summary["circuit_breaker"]["tripped"] is False


def test_observability_tracker_circuit_breaker_mutation_survived(temp_dir):
    tracker = BatchObservabilityTracker(
        batch_id="TEST-BATCH-SURVIVED",
        expected_apps=["app1", "app2", "app3"],
        max_allowed_survived=0,
    )
    tracker.record_app_result("app1", True, "CANDIDATE", "Passed")
    assert tracker.circuit_breaker_tripped is False

    # Second app has mutation survived!
    tracker.record_app_result("app2", False, "CANARY_KILL", "Canary assertion survived uncaught")
    should_abort, reason = tracker.should_abort_batch()
    assert should_abort is True
    assert "STOP-THE-LINE" in reason
    assert tracker.circuit_breaker_tripped is True

    summary_file = tracker.finalize_summary(temp_dir)
    summary = json.loads(summary_file.read_text(encoding="utf-8"))
    assert summary["circuit_breaker"]["tripped"] is True
    assert "app3" in summary["unexecuted_apps"]


def test_observability_tracker_circuit_breaker_infra_failures(temp_dir):
    tracker = BatchObservabilityTracker(
        batch_id="TEST-BATCH-INFRA",
        expected_apps=["app1", "app2", "app3"],
        max_allowed_infra=2,
    )
    # First infra failure: not tripped yet
    tracker.record_app_result("app1", False, "EXECUTION", "Subprocess timeout after 120s")
    assert tracker.circuit_breaker_tripped is False

    # Second infra failure: trips circuit breaker!
    tracker.record_app_result("app2", False, "EXECUTION", "Process terminated with SIGKILL")
    should_abort, reason = tracker.should_abort_batch()
    assert should_abort is True
    assert "Infrastructure failure count" in reason
