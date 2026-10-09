# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.afg.archetype_resolver"""
try:
    from .afg.archetype_resolver import *
except (ImportError, ValueError):
    from loop.afg.archetype_resolver import *
