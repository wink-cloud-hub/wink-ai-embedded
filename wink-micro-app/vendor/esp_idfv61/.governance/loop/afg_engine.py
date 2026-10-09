# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.afg.engine"""
try:
    from .afg.engine import *
except (ImportError, ValueError):
    from loop.afg.engine import *
