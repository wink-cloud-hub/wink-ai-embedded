# SPDX-License-Identifier: Apache-2.0
"""
loop.domains - Domain Plugin Registry & Loader (GAP-01)
========================================================
Decouples domain-specific test harnesses, causality assertions, and injection
logic from the core loop orchestration engine.
"""
from __future__ import annotations

from typing import Any, Dict, Optional, Type
from .base import DomainPlugin, DomainFixture, CausalityGraph
from .uart import UartDomainPlugin, UartCausalityPipeline, UartEventsFaultPipeline
from .system import TwdtDomainPlugin, TwdtTimeoutPipeline

_DOMAIN_REGISTRY: Dict[str, Any] = {
    "uart": UartDomainPlugin(),
    "system": TwdtDomainPlugin(),
}


def register_domain_plugin(domain_id: str, plugin: Any) -> None:
    """Register a domain plugin conforming to DomainPlugin protocol."""
    _DOMAIN_REGISTRY[domain_id] = plugin


def get_domain_plugin(domain_id: str) -> Optional[Any]:
    """Retrieve a registered domain plugin by domain_id."""
    return _DOMAIN_REGISTRY.get(domain_id)


def resolve_domain_fixture(domain_name: str, ctx: Any = None) -> Any:
    """Resolve and build domain fixture for a given domain name."""
    plugin = _DOMAIN_REGISTRY.get(domain_name)
    if plugin and hasattr(plugin, "build_fixture"):
        return plugin.build_fixture(ctx)
    return None


__all__ = [
    "DomainPlugin",
    "DomainFixture",
    "CausalityGraph",
    "register_domain_plugin",
    "get_domain_plugin",
    "resolve_domain_fixture",
    "UartDomainPlugin",
    "UartCausalityPipeline",
    "UartEventsFaultPipeline",
    "TwdtDomainPlugin",
    "TwdtTimeoutPipeline",
]
