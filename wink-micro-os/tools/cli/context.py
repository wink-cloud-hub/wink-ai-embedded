"""AppContext — immutable execution context for Wink Micro OS CLI commands."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Optional


@dataclass(frozen=True)
class AppContext:
    """CLI runtime context containing resolved paths, workspace config, and environment."""

    sdk_root: Path
    workspace_root: Path
    app_dir: Optional[Path]
    config: Dict[str, Any]
    env: Dict[str, str]
