"""Driver plugin registry for app_codegen.

Auto-discovers every ``*.py`` sibling module (except ``base.py``) and imports
it once; ``DriverBase.__init_subclass__`` fills ``_REGISTRY`` on import.
"""
from __future__ import annotations

import importlib
import pathlib
import sys
from typing import Dict, List

from .base import DriverBase

_REGISTRY: Dict[str, DriverBase] = {}
_LOADED = False


def _register_all() -> None:
    global _LOADED
    if _LOADED:
        return
    _LOADED = True  # set early so failed import doesn't re-scan on every call

    pkg_dir = pathlib.Path(__file__).parent
    # Allow the flat-import fallback (when this package is loaded as
    # ``drivers`` rather than ``tools.codegen.drivers`` — e.g. when a caller
    # inserted the codegen dir onto sys.path).
    if str(pkg_dir) not in sys.path:
        sys.path.insert(0, str(pkg_dir))

    for f in sorted(pkg_dir.glob("*.py")):
        if f.name in ("base.py", "__init__.py"):
            continue
        mod_name = f.stem
        try:
            importlib.import_module(f"tools.codegen.drivers.{mod_name}")
        except ImportError:
            # Fallback: direct import when the package is not addressable
            # from repo root (e.g. app_codegen.py run as __main__ inside
            # tools/codegen/).
            importlib.import_module(mod_name)


def get_driver(type_: str) -> DriverBase:
    _register_all()
    if type_ not in _REGISTRY:
        known = sorted(_REGISTRY)
        raise ValueError(
            f"unknown device type '{type_}' (known: {known})"
        )
    return _REGISTRY[type_]


def all_drivers() -> List[DriverBase]:
    _register_all()
    return list(_REGISTRY.values())


def known_types() -> List[str]:
    _register_all()
    return sorted(_REGISTRY)
