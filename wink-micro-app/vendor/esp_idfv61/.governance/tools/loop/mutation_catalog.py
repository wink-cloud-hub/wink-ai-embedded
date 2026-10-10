# SPDX-License-Identifier: Apache-2.0
"""
tools.loop.mutation_catalog - Backward Compatibility Shim for loop.mutation_catalog
===================================================================================
Re-exports CATALOG_OPERATORS and mutation functions from canonical loop.mutation_catalog.
"""
from __future__ import annotations

from loop.mutation_catalog import *
from loop.mutation_catalog import (
    CATALOG_OPERATORS,
    apply_catalog_mutation,
    classify_mutation_verdict,
)

__all__ = [
    "CATALOG_OPERATORS",
    "apply_catalog_mutation",
    "classify_mutation_verdict",
]
