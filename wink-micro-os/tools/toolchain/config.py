"""tools.json config loading, merging, and persistence.

Schema (version 1):

    {
      "version": 1,
      "tools_home": "C:/tools",     // optional; base dir for managed installs
      "paths": {                     // optional; per-tool path overrides
        "gcc": "C:/tools/gcc/bin/gcc.exe",
        "emsdk": "D:/emsdk"
      }
    }

Two files participate, merged in this order (later wins):

    1. User config:      ~/.wink/tools.json
    2. Workspace config: <workspace_root>/.wink/tools.json  (optional)
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

SCHEMA_VERSION = 1
_CONFIG_DIR_NAME = ".wink"
_CONFIG_FILE_NAME = "tools.json"


class UnsupportedToolsJsonVersionError(Exception):
    """Raised when a tools.json file declares an unsupported schema version."""


@dataclass
class ToolsConfig:
    """In-memory representation of the merged tools.json config."""

    version: int = SCHEMA_VERSION
    tools_home: Path | None = None
    paths: dict[str, str | None] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, data: dict) -> "ToolsConfig":
        """Build a ToolsConfig from a raw JSON-decoded dict.

        Raises UnsupportedToolsJsonVersionError if `version` is present and not
        equal to SCHEMA_VERSION. A missing `version` key is treated as v1 to
        keep hand-authored files forgiving; write-side always emits `version`.
        """
        version = data.get("version", SCHEMA_VERSION)
        if version != SCHEMA_VERSION:
            raise UnsupportedToolsJsonVersionError(
                f"tools.json version {version!r} is not supported "
                f"(expected {SCHEMA_VERSION})"
            )
        tools_home_raw = data.get("tools_home")
        tools_home = Path(tools_home_raw) if tools_home_raw else None
        paths = dict(data.get("paths") or {})
        return cls(version=SCHEMA_VERSION, tools_home=tools_home, paths=paths)


def _user_config_path() -> Path:
    return Path.home() / _CONFIG_DIR_NAME / _CONFIG_FILE_NAME


def _workspace_config_path(workspace_root: Path) -> Path:
    return workspace_root / _CONFIG_DIR_NAME / _CONFIG_FILE_NAME


def _load_config_file(path: Path) -> dict | None:
    """Read one tools.json file. Returns None if the file does not exist.

    JSON decode errors propagate to the caller.
    """
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8")
    return json.loads(text)


def load_tools_config(workspace_root: Path | None) -> ToolsConfig:
    """Load and merge user + workspace tools.json configs.

    Merge order: user first, then workspace overrides user (per-key for
    `paths`; whole-value for `tools_home`).
    """
    user_data = _load_config_file(_user_config_path())
    user_cfg = ToolsConfig.from_dict(user_data) if user_data is not None else ToolsConfig()

    ws_cfg: ToolsConfig | None = None
    if workspace_root is not None:
        ws_data = _load_config_file(_workspace_config_path(workspace_root))
        if ws_data is not None:
            ws_cfg = ToolsConfig.from_dict(ws_data)

    if ws_cfg is None:
        return user_cfg

    merged_paths: dict[str, str | None] = dict(user_cfg.paths)
    merged_paths.update(ws_cfg.paths)
    tools_home = ws_cfg.tools_home if ws_cfg.tools_home is not None else user_cfg.tools_home

    return ToolsConfig(version=SCHEMA_VERSION, tools_home=tools_home, paths=merged_paths)


def _save_path_to(config_file: Path, key: str, value: str) -> Path:
    """Merge a single `paths[key] = value` update into `config_file`.

    Loads the file if it exists (must be v1), merges the key, writes back
    with version=1, indent=2, UTF-8. Creates parent dirs as needed.
    """
    existing = _load_config_file(config_file)
    if existing is not None:
        cfg = ToolsConfig.from_dict(existing)
    else:
        cfg = ToolsConfig()

    cfg.paths[key] = value

    config_file.parent.mkdir(parents=True, exist_ok=True)
    payload: dict = {"version": SCHEMA_VERSION}
    if cfg.tools_home is not None:
        payload["tools_home"] = str(cfg.tools_home)
    payload["paths"] = dict(cfg.paths)
    config_file.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    return config_file


def save_user_path(key: str, value: str) -> Path:
    """Persist `paths[key] = value` into the user config (~/.wink/tools.json)."""
    return _save_path_to(_user_config_path(), key, value)


def save_workspace_path(workspace_root: Path, key: str, value: str) -> Path:
    """Persist `paths[key] = value` into `<workspace_root>/.wink/tools.json`."""
    return _save_path_to(_workspace_config_path(workspace_root), key, value)
