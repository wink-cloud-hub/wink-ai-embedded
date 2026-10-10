# SPDX-License-Identifier: Apache-2.0
"""
tools.loop - Backward Compatibility Package
===========================================
Re-exports symbols from the canonical loop package (.governance/loop).
"""
from __future__ import annotations

import loop

for _attr in getattr(loop, "__all__", []):
    globals()[_attr] = getattr(loop, _attr)

__all__ = getattr(loop, "__all__", [])
