"""Providers subpackage: one Provider subclass per host tool or SDK.

Exposes ``REGISTRY``, a mapping from capability id to a singleton
:class:`Provider` instance. The registry is built at import time and
consumed by higher-level orchestration (``ensure_for``, ``wink doctor``).

Phase A registers nine capabilities:
    - Host tools:  python, jinja2, cmake, make, gcc
    - SDKs:        emsdk, idf, node, powershell
"""
from __future__ import annotations

from .base import Provider
from .cmake import CMakeProvider
from .emsdk import EmsdkProvider
from .gcc import GccProvider
from .idf import IdfProvider
from .make import MakeProvider
from .node import NodeProvider
from .powershell import PowerShellProvider
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
        EmsdkProvider(),
        IdfProvider(),
        NodeProvider(),
        PowerShellProvider(),
    )
}

__all__ = [
    "REGISTRY",
    "Provider",
    "CMakeProvider",
    "EmsdkProvider",
    "GccProvider",
    "IdfProvider",
    "MakeProvider",
    "NodeProvider",
    "PowerShellProvider",
    "PythonProvider",
    "Jinja2Provider",
]
