# SPDX-License-Identifier: Apache-2.0
"""
tools.loop.agent - Backward Compatibility Shim for loop.agent
============================================================
Re-exports AgentSynthesizer and symbols from canonical loop.agent.
"""
from __future__ import annotations

from loop.agent import *
from loop.agent import AgentSynthesizer

__all__ = ["AgentSynthesizer"]
