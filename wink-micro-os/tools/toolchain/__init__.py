"""Toolchain provisioning package (Phase 2).

Pure-stdlib providers for detecting and (optionally) installing host and SDK
toolchains. The top-level entry point for CLI callers is :func:`ensure_for`.
"""
from __future__ import annotations

from .check import ensure_for

__all__ = ["ensure_for"]
