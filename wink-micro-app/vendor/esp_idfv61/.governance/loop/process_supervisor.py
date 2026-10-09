# SPDX-License-Identifier: Apache-2.0
"""Shim re-exporting loop.harness.process_supervisor"""
try:
    from .harness.process_supervisor import *
except (ImportError, ValueError):
    from loop.harness.process_supervisor import *
