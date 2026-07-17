"""Shared helpers for ADR-0034 progressive-disclosure ``advanced.*`` JSON.

Only ``advanced.*`` is accepted as L2. Top-level aliases (``pull``,
``resolution_bits``, ``clock_requirement``) are rejected so codegen never
emits dual-write forms.
"""
from __future__ import annotations

import sys
from typing import Any, FrozenSet, Optional


def _die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(2)


def parse_advanced(
    dev_name: str,
    spec: dict,
    *,
    allowed_keys: FrozenSet[str],
    top_level_aliases: FrozenSet[str] = frozenset(),
) -> dict:
    """Validate and return the ``advanced`` object (possibly empty).

    - ``advanced`` must be absent or a dict.
    - Unknown keys inside ``advanced`` → SystemExit(2).
    - Top-level aliases listed in ``top_level_aliases`` → SystemExit(2)
      (use ``advanced.<key>`` instead; dual-write is never accepted).
    """
    for alias in top_level_aliases:
        if alias in spec:
            _die(
                f"device '{dev_name}': top-level '{alias}' is not supported; "
                f"use 'advanced.{alias}' (ADR-0034)"
            )

    adv = spec.get("advanced")
    if adv is None:
        return {}
    if not isinstance(adv, dict):
        _die(
            f"device '{dev_name}': 'advanced' must be an object "
            f"(got {type(adv).__name__})"
        )

    unknown = sorted(set(adv.keys()) - set(allowed_keys))
    if unknown:
        _die(
            f"device '{dev_name}': unknown advanced key(s): {unknown} "
            f"(allowed: {sorted(allowed_keys)})"
        )
    return adv


def require_string_enum(
    dev_name: str,
    field: str,
    value: Any,
    allowed: FrozenSet[str],
) -> str:
    """Require ``value`` to be an exact lowercase string from ``allowed``."""
    if not isinstance(value, str):
        _die(
            f"device '{dev_name}': advanced.{field} must be a string "
            f"(got {type(value).__name__})"
        )
    if value not in allowed:
        _die(
            f"device '{dev_name}': invalid advanced.{field}: {value!r} "
            f"(allowed: {sorted(allowed)}; case-sensitive)"
        )
    return value


def require_int(
    dev_name: str,
    field: str,
    value: Any,
    *,
    min_v: Optional[int] = None,
    max_v: Optional[int] = None,
) -> int:
    """Require a real JSON integer (reject bool / float / string)."""
    # bool is a subclass of int in Python — reject explicitly.
    if isinstance(value, bool) or not isinstance(value, int):
        _die(
            f"device '{dev_name}': advanced.{field} must be an integer "
            f"(got {type(value).__name__})"
        )
    if min_v is not None and value < min_v:
        _die(
            f"device '{dev_name}': advanced.{field}={value} below minimum {min_v}"
        )
    if max_v is not None and value > max_v:
        _die(
            f"device '{dev_name}': advanced.{field}={value} above maximum {max_v}"
        )
    return value
