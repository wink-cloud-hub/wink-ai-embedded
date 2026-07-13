"""Core value types shared by all toolchain providers."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

# Default timeout (seconds) for external probe subprocesses (e.g. `--version`).
PROBE_TIMEOUT_SEC = 10


@dataclass
class DetectResult:
    """Outcome of a Provider.detect() probe.

    Attributes:
        found:   True if the tool is present and usable.
        path:    Absolute path to the executable / SDK root, if resolved.
        version: Parsed version string, if determinable.
        reason:  Short human-readable reason (mainly for the not-found case).
        source:  Where the tool was located (e.g. "PATH", "IDF_PATH", "emsdk").
    """

    found: bool
    path: Path | None
    version: str | None
    reason: str | None
    source: str | None


class UnsupportedError(Exception):
    """Raised when a provider cannot perform the requested capability.

    The canonical use is `Provider.install()` on providers that only support
    detection (host toolchains the user must install themselves).
    """
