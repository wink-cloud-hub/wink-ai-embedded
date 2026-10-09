# SPDX-License-Identifier: Apache-2.0
"""
loop - WinkMicroOS Governance Closed-Loop Package v2.0
======================================================
Autonomous, anti-false-green closed loop driver and verification architecture
for ESP-IDF example governance.
"""
from .mutator import CanaryMutator
from .agent import AgentSynthesizer
from .pipeline.pipeline import LoopPipeline, PipelineResult
from .pipeline.runner import LoopRunner, main
from .harness.safety_checker import HeuristicSafetyChecker, TieredCParser
from .harness.process_supervisor import ProcessSupervisor, safe_file_retry
from .harness.run_context import RunContext
from .harness.lock_lease import FileLockLease
from .remediator import (
    Remediator,
    InvestigationWorkspace,
    TransactionalGitTracker,
    ZeroRegressionRunner,
    RemediatorState,
)
from .afg.engine import AFGEngine, AFGReceipt, ExecutionIdentity
from .afg.error_matcher import match_error_assertion
from .afg.archetype_resolver import ArchetypeResolver
from .afg.build_sandbox import BuildSandbox
from .afg.mutation_runner import MutationRunner

__all__ = [
    "CanaryMutator",
    "AgentSynthesizer",
    "LoopPipeline",
    "PipelineResult",
    "LoopRunner",
    "main",
    "HeuristicSafetyChecker",
    "TieredCParser",
    "ProcessSupervisor",
    "safe_file_retry",
    "RunContext",
    "FileLockLease",
    "Remediator",
    "InvestigationWorkspace",
    "TransactionalGitTracker",
    "ZeroRegressionRunner",
    "RemediatorState",
    "AFGEngine",
    "AFGReceipt",
    "ExecutionIdentity",
    "match_error_assertion",
    "ArchetypeResolver",
    "BuildSandbox",
    "MutationRunner",
]
