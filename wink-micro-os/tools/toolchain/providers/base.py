"""Abstract base class for all toolchain providers."""
from __future__ import annotations

from abc import ABC, abstractmethod

from ..types import DetectResult, UnsupportedError


class Provider(ABC):
    """Contract for a toolchain component (host tool or SDK).

    Subclasses set the class-level ``id`` (stable identifier used in reports
    and config keys) and implement ``detect``/``hint``. Providers that can
    self-install override ``install``; the default raises ``UnsupportedError``.
    """

    # Stable identifier, e.g. "python", "emsdk", "idf". Subclasses must set.
    id: str = ""

    @abstractmethod
    def detect(self, ctx) -> DetectResult:
        """Probe the environment and report whether the tool is available."""

    @abstractmethod
    def hint(self, ctx) -> str:
        """Return an actionable one-line hint for the not-found case."""

    def install(self, ctx) -> None:
        """Install the tool. Default: not supported.

        Providers that manage installable SDKs (emsdk, idf, node) override
        this. Providers for host prerequisites (python, gcc, make, cmake)
        leave the default in place so the caller surfaces ``hint()`` instead.
        """
        raise UnsupportedError(
            f"provider {self.id!r} does not support install; see hint()"
        )
