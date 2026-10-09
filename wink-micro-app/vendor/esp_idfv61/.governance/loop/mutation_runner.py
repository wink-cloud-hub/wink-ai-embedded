# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.afg.mutation_runner"""
try:
    from .afg.mutation_runner import *
except (ImportError, ValueError):
    from loop.afg.mutation_runner import *
