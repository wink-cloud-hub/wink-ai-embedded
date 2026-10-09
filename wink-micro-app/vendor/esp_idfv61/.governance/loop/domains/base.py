# SPDX-License-Identifier: Apache-2.0
"""
loop.domains.base - Formal Domain Plugin Protocol Specification (GAP-01)
========================================================================
Defines the formal typed protocol for domain-specific testing fixtures,
causality graphs, and fault injection plugins.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Protocol, Tuple, runtime_checkable
from pathlib import Path


@runtime_checkable
class DomainFixture(Protocol):
    """Protocol for fixtures prepared by domain plugins."""
    fixture_id: str
    target_app_dir: str

    def setup(self, workspace_root: Path) -> bool:
        """Sets up domain-specific testing resources."""
        ...

    def teardown(self) -> None:
        """Cleans up domain-specific testing resources."""
        ...


@runtime_checkable
class CausalityGraph(Protocol):
    """Protocol for domain causality graphs and causal assertions."""
    domain_id: str
    nodes: List[Dict[str, Any]]
    edges: List[Dict[str, Any]]

    def validate_invariants(self, execution_trace: List[Dict[str, Any]]) -> Tuple[bool, List[str]]:
        """Validates causal invariants against an execution trace."""
        ...


@runtime_checkable
class DomainPlugin(Protocol):
    """Formal protocol for domain-specific verification plugins (GAP-01)."""
    domain_id: str
    archetype_ref: str              # Referenced archetype template (e.g. archetype_uart_stream)
    required_ctx_fields: List[str]  # Required context fields in RunContext

    def build_fixture(self, ctx: Any) -> Any:
        """Constructs domain-specific fixture for target app execution."""
        ...

    def get_causality_graph(self) -> Any:
        """Returns the formal causality dependency graph for this domain."""
        ...

    def list_supported_injection_modes(self) -> List[str]:
        """Returns list of supported fault injection modes (e.g. ['parity_err', 'frame_err'])."""
        ...
