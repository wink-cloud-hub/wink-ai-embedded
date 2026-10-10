# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.idf_paths - ESP-IDF SDK Path Resolution Engine
===========================================================
Shared resolver for ESP-IDF repository roots, examples, components, and local configurations.
Extracted per ADR-0092 & Governance Redundancy Cleanup Plan Phase 3.
"""
from __future__ import annotations

import argparse
import io
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

GOV_DIR = Path(__file__).resolve().parents[2]
LOCAL_CONFIG_FILE = GOV_DIR / "local_config.json"

WELL_KNOWN_CANDIDATES: List[Path] = [
    Path(r"D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf"),
    Path(r"C:\Espressif\frameworks\esp-idf-v6.1"),
    Path(r"C:\Espressif\frameworks\esp-idf"),
    Path.home() / ".espressif" / "v6.1" / "esp-idf",
    Path.home() / "esp" / "esp-idf",
]


def load_local_config() -> Dict[str, Any]:
    """Read local config file (if present)."""
    if LOCAL_CONFIG_FILE.is_file():
        try:
            with open(LOCAL_CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}


def save_local_config(idf_path: Path | str) -> Path:
    """Persist local ESP-IDF path to local_config.json."""
    path_obj = Path(idf_path).resolve()
    if not path_obj.is_dir():
        raise FileNotFoundError(f"Specified path does not exist or is not a directory: {path_obj}")

    if not (path_obj / "examples").is_dir() and not (path_obj / "components").is_dir():
        if path_obj.name == "examples" and (path_obj.parent / "components").is_dir():
            path_obj = path_obj.parent

    config = load_local_config()
    config["idf_path"] = str(path_obj)

    with open(LOCAL_CONFIG_FILE, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)

    return path_obj


def validate_idf_root(p: Path) -> bool:
    """Validate whether directory is a valid ESP-IDF root directory."""
    if not p.is_dir():
        return False
    return (p / "examples").is_dir() or (p / "components").is_dir()


def get_idf_root(explicit_path: Optional[Path | str] = None) -> Tuple[Path, str]:
    """Resolve ESP-IDF root directory, returning (Path, source_description)."""
    if explicit_path:
        p = Path(explicit_path).resolve()
        if validate_idf_root(p):
            return p, "CLI explicit parameter"
        if p.name == "examples" and validate_idf_root(p.parent):
            return p.parent, "CLI explicit parameter (parent of examples)"
        if p.is_dir():
            return p, "CLI explicit parameter"

    for env_var in ("IDF_PATH", "ESP_IDF_PATH"):
        env_val = os.environ.get(env_var, "").strip()
        if env_val:
            p = Path(env_val).resolve()
            if validate_idf_root(p):
                return p, f"Environment variable {env_var}"

    examples_env = os.environ.get("ESP_IDF_EXAMPLES_DIR", "").strip()
    if examples_env:
        p = Path(examples_env).resolve()
        if p.is_dir():
            parent = p.parent
            if validate_idf_root(parent):
                return parent, "Derived from ESP_IDF_EXAMPLES_DIR parent"
            return p, "Direct ESP_IDF_EXAMPLES_DIR (as root fallback)"

    local_cfg = load_local_config()
    cfg_path = local_cfg.get("idf_path", "").strip()
    if cfg_path:
        p = Path(cfg_path).resolve()
        if validate_idf_root(p):
            return p, f"Local config ({LOCAL_CONFIG_FILE.name})"

    for cand in WELL_KNOWN_CANDIDATES:
        if validate_idf_root(cand):
            return cand.resolve(), "Well-known system candidate"

    searched_hints = [
        "1. CLI: --idf-path <DIR>",
        "2. Env: set IDF_PATH=<DIR>",
        f"3. Config: save_local_config(<DIR>)",
        "4. Standard paths: " + ", ".join(str(c) for c in WELL_KNOWN_CANDIDATES[:2]),
    ]
    raise FileNotFoundError(
        "Could not locate valid ESP-IDF source directory!\nSpecify using:\n"
        + "\n".join("   " + h for h in searched_hints)
    )


def get_idf_examples_dir(
    explicit_examples: Optional[Path | str] = None,
    explicit_root: Optional[Path | str] = None,
) -> Path:
    """Get ESP-IDF official examples directory."""
    if explicit_examples:
        p = Path(explicit_examples).resolve()
        if p.is_dir():
            return p

    env_examples = os.environ.get("ESP_IDF_EXAMPLES_DIR", "").strip()
    if env_examples:
        p = Path(env_examples).resolve()
        if p.is_dir():
            return p

    idf_root, _ = get_idf_root(explicit_root)
    examples_dir = idf_root / "examples"
    if examples_dir.is_dir():
        return examples_dir.resolve()

    if idf_root.name == "examples" and idf_root.is_dir():
        return idf_root.resolve()

    raise FileNotFoundError(f"examples subdirectory not found in ESP-IDF: {idf_root}")


def get_idf_components_dir(explicit_root: Optional[Path | str] = None) -> Path:
    """Get ESP-IDF official components directory."""
    idf_root, _ = get_idf_root(explicit_root)
    comp_dir = idf_root / "components"
    if comp_dir.is_dir():
        return comp_dir.resolve()
    raise FileNotFoundError(f"components subdirectory not found in ESP-IDF: {idf_root}")


def get_idf_version(idf_root: Path) -> str:
    """Detect ESP-IDF version string."""
    version_file = idf_root / "version.txt"
    if version_file.is_file():
        try:
            return version_file.read_text(encoding="utf-8").strip()
        except Exception:
            pass

    if (idf_root / ".git").exists():
        try:
            res = subprocess.run(
                ["git", "-C", str(idf_root), "describe", "--tags", "--always"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=3,
            )
            if res.returncode == 0 and res.stdout.strip():
                return res.stdout.strip()
        except Exception:
            pass

    return "unknown (no git tag or version.txt)"


def find_example(pattern: str, explicit_root: Optional[Path | str] = None) -> List[Path]:
    """Search for official example project directory."""
    examples_dir = get_idf_examples_dir(explicit_root=explicit_root)
    matched = []
    pat = pattern.lower().replace("\\", "/")

    for cm in examples_dir.rglob("CMakeLists.txt"):
        ex_dir = cm.parent
        if any(part in ("build", "managed_components", "main") for part in ex_dir.parts):
            continue
        rel = ex_dir.relative_to(examples_dir).as_posix().lower()
        if pat in rel:
            matched.append(ex_dir)

    return matched


def find_component(name: str, explicit_root: Optional[Path | str] = None) -> Optional[Path]:
    """Search for official component directory."""
    comp_dir = get_idf_components_dir(explicit_root=explicit_root)
    target = comp_dir / name
    if target.is_dir():
        return target
    for d in comp_dir.iterdir():
        if d.is_dir() and d.name.lower() == name.lower():
            return d
    return None


def add_idf_cli_arguments(parser: argparse.ArgumentParser) -> None:
    """Add standard ESP-IDF path CLI arguments to parser."""
    parser.add_argument(
        "--idf-path",
        type=Path,
        default=None,
        help="ESP-IDF root directory",
    )
    parser.add_argument(
        "--idf-examples",
        type=Path,
        default=None,
        help="ESP-IDF official examples directory",
    )


__all__ = [
    "LOCAL_CONFIG_FILE",
    "WELL_KNOWN_CANDIDATES",
    "load_local_config",
    "save_local_config",
    "validate_idf_root",
    "get_idf_root",
    "get_idf_examples_dir",
    "get_idf_components_dir",
    "get_idf_version",
    "find_example",
    "find_component",
    "add_idf_cli_arguments",
]
