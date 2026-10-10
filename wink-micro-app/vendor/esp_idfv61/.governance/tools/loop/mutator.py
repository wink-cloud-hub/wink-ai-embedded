# SPDX-License-Identifier: Apache-2.0
"""
tools.loop.mutator - Backward Compatibility Shim for loop.mutator
================================================================
Re-exports CanaryMutator and symbols from canonical loop.mutator.
"""
from __future__ import annotations

from loop.mutator import *
from loop.mutator import (
    DIM_ASSERTION,
    DIM_PLATFORM_FAULT,
    DIM_STIMULUS,
    CanaryMutator,
)

__all__ = [
    "CanaryMutator",
    "DIM_ASSERTION",
    "DIM_PLATFORM_FAULT",
    "DIM_STIMULUS",
]
