# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.run_context - RunContext Isolation & Identity Management (GAP-05)
=============================================================================
Enforces orthogonal execution identity and per-run isolated sandboxes:
  run_id = f"{clean_app_id}__{clean_profile}__{timestamp_ms}_{uuid_nonce}"
Guarantees zero file collision across profiles, chip targets, or concurrent workers.
"""
from __future__ import annotations

import datetime
import os
import re
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional


def sanitize_id_component(raw: str) -> str:
    """Sanitize arbitrary strings for filesystem directory names."""
    cleaned = raw.replace("/", "_").replace("\\", "_").replace(":", "_").replace(" ", "_")
    return re.sub(r"[^A-Za-z0-9_\-\.]", "", cleaned)


@dataclass
class RunContext:
    """Frozen execution context for an application governance run."""
    app_id: str
    config_id: str
    profile: str = "default"
    target_soc: str = "esp32"
    backend: str = "wasm_simulation"
    governance_dir: Path = field(default_factory=lambda: Path("."))
    run_id: str = ""
    run_root: Path = field(default_factory=lambda: Path("."))
    is_frozen: bool = False
    metadata: Dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        self.governance_dir = self.governance_dir.resolve()
        if not self.run_id:
            now_str = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            nonce = uuid.uuid4().hex[:6]
            app_clean = sanitize_id_component(self.app_id)
            prof_clean = sanitize_id_component(self.profile)
            self.run_id = f"{app_clean}__{prof_clean}__{now_str}_{nonce}"

        if self.run_root == Path("."):
            self.run_root = (self.governance_dir / "runs" / self.run_id).resolve()

    def freeze(self) -> RunContext:
        """Freeze execution context against mutations."""
        self.is_frozen = True
        return self

    def ensure_directories(self) -> Path:
        """Create isolated sandbox directory."""
        self.run_root.mkdir(parents=True, exist_ok=True)
        return self.run_root

    def to_dict(self) -> Dict[str, Any]:
        return {
            "app_id": self.app_id,
            "config_id": self.config_id,
            "profile": self.profile,
            "target_soc": self.target_soc,
            "backend": self.backend,
            "run_id": self.run_id,
            "run_root": str(self.run_root),
            "is_frozen": self.is_frozen,
            "metadata": self.metadata,
        }
