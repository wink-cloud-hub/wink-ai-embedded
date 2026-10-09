# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.afg.build_sandbox"""
try:
    from .afg.build_sandbox import *
except (ImportError, ValueError):
    from loop.afg.build_sandbox import *
