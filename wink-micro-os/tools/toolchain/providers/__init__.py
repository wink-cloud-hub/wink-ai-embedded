"""Providers subpackage: one Provider subclass per host tool or SDK.

Exposes ``REGISTRY``, a mapping from capability id to a singleton
:class:`Provider` instance. The registry is built at import time and
consumed by higher-level orchestration (``ensure_for``, ``wink doctor``).

Phase A registers the five host capabilities: python, jinja2, cmake, make,
gcc. SDK providers (emsdk, idf, node, powershell) are added by Task 7.
"""
from __future__ import annotations

from .base import Provider
from .cmake import CMakeProvider
from .gcc import GccProvider
from .make import MakeProvider
from .python_interp import PythonProvider
from .python_pkgs import Jinja2Provider

REGISTRY: dict[str, Provider] = {
    p.id: p
    for p in (
        PythonProvider(),
        Jinja2Provider(),
        CMakeProvider(),
        MakeProvider(),
        GccProvider(),
    )
}

__all__ = [
    "REGISTRY",
    "Provider",
    "CMakeProvider",
    "GccProvider",
    "MakeProvider",
    "PythonProvider",
    "Jinja2Provider",
]
