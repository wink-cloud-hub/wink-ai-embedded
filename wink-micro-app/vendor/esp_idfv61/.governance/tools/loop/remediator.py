# SPDX-License-Identifier: Apache-2.0
"""
tools.loop.remediator - Backward Compatibility Shim for loop.remediator
======================================================================
Re-exports Remediator, RemediatorState, and symbols from canonical loop.remediator.
"""
from __future__ import annotations

from loop.remediator import *
from loop.remediator import (
    DOMAIN_APP_MAP,
    DOMAIN_FILE_MAP,
    InvestigationWorkspace,
    Remediator,
    RemediatorState,
    TransactionalGitTracker,
    ZeroRegressionRunner,
)

__all__ = [
    "DOMAIN_APP_MAP",
    "DOMAIN_FILE_MAP",
    "InvestigationWorkspace",
    "Remediator",
    "RemediatorState",
    "TransactionalGitTracker",
    "ZeroRegressionRunner",
]
