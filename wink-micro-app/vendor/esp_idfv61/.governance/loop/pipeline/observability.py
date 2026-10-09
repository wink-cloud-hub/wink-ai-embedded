# SPDX-License-Identifier: Apache-2.0
"""
Continuous Observability & Batch Summary Engine (V1-T4)
======================================================
Tracks granular execution metrics across loop batches:
- Mandatory check coverage (baseline, canary kill, firmware mutation, recovery)
- Anomaly statistics (invalid observations, mutation survived, infra failures, CAS conflicts)
- Stop-the-Line Circuit Breaker: triggers immediate halt upon anomalous survival/failure rate.
"""

from __future__ import annotations

import datetime
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


@dataclass
class AnomalyEvent:
    app_id: str
    kind: str  # INVALID_OBSERVATION | MUTATION_SURVIVED | INFRA_FAILURE | PROMOTION_CONFLICT
    detail: str
    timestamp_utc: str = field(
        default_factory=lambda: datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    )


@dataclass
class MutationBudgetMetrics:
    """Metrics tracking L1/L2 mutation consumption, budgets, and backlog (GAP-06)."""
    batch_id: str
    l1_injections_executed: int = 0
    l1_injections_passed: int = 0
    l2_mutations_executed: int = 0
    l2_mutations_killed: int = 0
    l2_budget_exceeded_count: int = 0
    mutants_survived_count: int = 0
    equivalent_mutant_backlog: int = 0

    def record_l1_result(self, witness_status: str) -> None:
        self.l1_injections_executed += 1
        if witness_status in ("FAULT_HANDLED_PASS", "PASS"):
            self.l1_injections_passed += 1

    def record_l2_result(self, witness_status: str) -> None:
        self.l2_mutations_executed += 1
        if witness_status == "MUTANT_KILLED":
            self.l2_mutations_killed += 1
        elif witness_status == "MUTANT_SURVIVED":
            self.mutants_survived_count += 1
        elif witness_status == "EQUIVALENT_MUTANT_EXEMPT":
            self.equivalent_mutant_backlog += 1

    def to_dict(self) -> Dict[str, Any]:
        return {
            "batch_id": self.batch_id,
            "l1_injections_executed": self.l1_injections_executed,
            "l1_injections_passed": self.l1_injections_passed,
            "l2_mutations_executed": self.l2_mutations_executed,
            "l2_mutations_killed": self.l2_mutations_killed,
            "l2_budget_exceeded_count": self.l2_budget_exceeded_count,
            "mutants_survived_count": self.mutants_survived_count,
            "equivalent_mutant_backlog": self.equivalent_mutant_backlog,
            "l1_pass_rate": (self.l1_injections_passed / self.l1_injections_executed) if self.l1_injections_executed > 0 else 1.0,
            "l2_kill_rate": (self.l2_mutations_killed / self.l2_mutations_executed) if self.l2_mutations_executed > 0 else 1.0,
        }


class BatchObservabilityTracker:
    """Collects real-time observability metrics and triggers Stop-the-Line if needed."""

    def __init__(
        self,
        batch_id: str,
        expected_apps: List[str],
        max_allowed_survived: int = 0,
        max_allowed_infra: int = 2,
    ):
        self.batch_id = batch_id
        self.start_time = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        self.expected_apps = expected_apps
        self.executed_apps: List[str] = []
        self.successful_apps: List[str] = []
        self.failed_apps: List[str] = []
        self.mutation_budget_metrics = MutationBudgetMetrics(batch_id=batch_id)


        self.coverage_stats = {
            "baseline_passed": 0,
            "assertion_self_checks_killed": 0,
            "firmware_mutations_killed": 0,
            "recoveries_passed": 0,
        }

        self.anomaly_events: List[AnomalyEvent] = []
        self.max_allowed_survived = max_allowed_survived
        self.max_allowed_infra = max_allowed_infra
        self.circuit_breaker_tripped = False
        self.trip_reason: Optional[str] = None

    def record_app_result(
        self,
        app_id: str,
        success: bool,
        stage: str,
        message: str,
        candidate_meta: Optional[Dict[str, Any]] = None,
    ) -> None:
        """Record the outcome of a single application execution."""
        self.executed_apps.append(app_id)
        if success:
            self.successful_apps.append(app_id)
            self.coverage_stats["baseline_passed"] += 1
            self.coverage_stats["assertion_self_checks_killed"] += 1
            self.coverage_stats["recoveries_passed"] += 1
            if candidate_meta and "firmware_mutation" in candidate_meta:
                self.coverage_stats["firmware_mutations_killed"] += 1
        else:
            self.failed_apps.append(app_id)
            # Classify anomaly kind
            msg_lower = message.lower()
            if "invalid" in msg_lower or "abi" in msg_lower or "null actual" in msg_lower:
                self.anomaly_events.append(AnomalyEvent(app_id, "INVALID_OBSERVATION", message))
            elif "survived" in msg_lower or "uncaught" in msg_lower or stage == "CANARY_KILL":
                self.anomaly_events.append(AnomalyEvent(app_id, "MUTATION_SURVIVED", message))
            elif "conflict" in msg_lower or "cas" in msg_lower:
                self.anomaly_events.append(AnomalyEvent(app_id, "PROMOTION_CONFLICT", message))
            else:
                self.anomaly_events.append(AnomalyEvent(app_id, "INFRA_FAILURE", message))

            # Check circuit breaker thresholds
            self._evaluate_circuit_breaker()

    def _evaluate_circuit_breaker(self) -> None:
        survived_count = sum(1 for e in self.anomaly_events if e.kind == "MUTATION_SURVIVED")
        infra_count = sum(1 for e in self.anomaly_events if e.kind == "INFRA_FAILURE")

        if survived_count > self.max_allowed_survived:
            self.circuit_breaker_tripped = True
            self.trip_reason = (
                f"STOP-THE-LINE: Mutation survival count ({survived_count}) "
                f"exceeded threshold ({self.max_allowed_survived}). Halting batch."
            )
        elif infra_count >= self.max_allowed_infra:
            self.circuit_breaker_tripped = True
            self.trip_reason = (
                f"STOP-THE-LINE: Infrastructure failure count ({infra_count}) "
                f"exceeded threshold ({self.max_allowed_infra}). Halting batch."
            )

    def should_abort_batch(self) -> Tuple[bool, Optional[str]]:
        return self.circuit_breaker_tripped, self.trip_reason

    def finalize_summary(self, output_dir: Path) -> Path:
        """Produce the batch observability summary artifact."""
        output_dir.mkdir(parents=True, exist_ok=True)
        end_time = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

        total_exp = len(self.expected_apps)
        total_exec = len(self.executed_apps)
        exec_rate = (total_exec / total_exp * 100.0) if total_exp > 0 else 0.0
        pass_rate = (len(self.successful_apps) / total_exec * 100.0) if total_exec > 0 else 0.0

        survived_count = sum(1 for e in self.anomaly_events if e.kind == "MUTATION_SURVIVED")
        invalid_obs_count = sum(1 for e in self.anomaly_events if e.kind == "INVALID_OBSERVATION")
        infra_count = sum(1 for e in self.anomaly_events if e.kind == "INFRA_FAILURE")
        conflicts_count = sum(1 for e in self.anomaly_events if e.kind == "PROMOTION_CONFLICT")

        summary_data = {
            "schema_version": "1.0",
            "batch_id": self.batch_id,
            "start_time_utc": self.start_time,
            "end_time_utc": end_time,
            "total_expected": total_exp,
            "total_executed": total_exec,
            "execution_rate_pct": round(exec_rate, 2),
            "total_passed": len(self.successful_apps),
            "total_failed": len(self.failed_apps),
            "pass_rate_pct": round(pass_rate, 2),
            "coverage_stats": self.coverage_stats,
            "anomaly_summary": {
                "invalid_observations": invalid_obs_count,
                "mutation_survived": survived_count,
                "infra_failures": infra_count,
                "promotion_conflicts": conflicts_count,
            },
            "circuit_breaker": {
                "tripped": self.circuit_breaker_tripped,
                "reason": self.trip_reason,
            },
            "anomalies": [
                {
                    "app_id": e.app_id,
                    "kind": e.kind,
                    "detail": e.detail,
                    "timestamp_utc": e.timestamp_utc,
                }
                for e in self.anomaly_events
            ],
            "mutation_budget_metrics": self.mutation_budget_metrics.to_dict(),
            "unexecuted_apps": [app for app in self.expected_apps if app not in self.executed_apps],
        }


        out_file = output_dir / "batch_observability_summary.json"
        out_file.write_text(json.dumps(summary_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return out_file
