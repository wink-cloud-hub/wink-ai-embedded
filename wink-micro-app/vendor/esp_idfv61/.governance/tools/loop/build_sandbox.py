# SPDX-License-Identifier: Apache-2.0
"""
build_sandbox.py - Build Sandbox & Dependency Closure Hash Cache
================================================================
Implements Task 4.2 & AFG-R15:
- Upgraded SHA-256 build cache key algorithm binding:
    app_source_digest + header_closure_digest + effective_sdkconfig_digest
    + toolchain_version + facade_git_sha + patch_content_digest + config_profile_id
- Isolates build sandbox to prevent tampering with upstream source tree (AFG-R05).
- Manages cached Wasm/ELF compilation artifacts.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def compute_build_cache_key(
    app_source_digest: str,
    header_closure_digest: str,
    effective_sdkconfig_digest: str,
    toolchain_version: str,
    facade_git_sha: str,
    patch_content_digest: str,
    config_profile_id: str,
) -> str:
    """Computes full dependency closure cache key per AFG-R15."""
    raw = (
        app_source_digest.encode("utf-8")
        + header_closure_digest.encode("utf-8")
        + effective_sdkconfig_digest.encode("utf-8")
        + toolchain_version.encode("utf-8")
        + facade_git_sha.encode("utf-8")
        + patch_content_digest.encode("utf-8")
        + config_profile_id.encode("utf-8")
    )
    return hashlib.sha256(raw).hexdigest()


def compute_directory_digest(dir_path: Path, extensions: Tuple[str, ...] = (".c", ".h", ".cpp", ".hpp", ".txt", ".cmake")) -> str:
    """Computes a deterministic hash of all relevant source files in directory."""
    if not dir_path.is_dir():
        return hashlib.sha256(b"").hexdigest()

    hasher = hashlib.sha256()
    for root, _, files in sorted(os.walk(dir_path)):
        for f in sorted(files):
            if any(f.endswith(ext) for ext in extensions):
                file_path = Path(root) / f
                try:
                    rel_p = str(file_path.relative_to(dir_path)).replace("\\", "/")
                    hasher.update(rel_p.encode("utf-8"))
                    with open(file_path, "rb") as fp:
                        while chunk := fp.read(65536):
                            hasher.update(chunk)
                except Exception:
                    pass
    return hasher.hexdigest()


def get_git_sha(repo_dir: Path) -> str:
    """Attempts to get current git SHA for repo, or deterministic fallback."""
    try:
        res = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(repo_dir),
            capture_output=True,
            text=True,
            check=True,
            timeout=5
        )
        return res.stdout.strip()
    except Exception:
        return "git-sha-unknown-fallback"


class BuildSandbox:
    """Manages isolated build environments and content-addressed compilation caches."""

    def __init__(self, workspace_root: Path, cache_dir: Optional[Path] = None):
        self.ws_root = workspace_root.resolve()
        self.gov_dir = self.ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
        self.cache_dir = (cache_dir or (self.gov_dir / "runs" / ".build_cache")).resolve()
        self.cache_dir.mkdir(parents=True, exist_ok=True)

    def calculate_key(
        self,
        app_dir: Path,
        config_profile_id: str = "default",
        patch_content: str = "",
        header_closure_files: Optional[List[Path]] = None,
        effective_sdkconfig: str = "",
        toolchain_version: str = "emscripten-3.1.56",
        facade_dir: Optional[Path] = None,
    ) -> Tuple[str, Dict[str, str]]:
        """Calculates cache key and constituent digests."""
        app_source_digest = compute_directory_digest(app_dir)

        # Header closure digest
        header_hasher = hashlib.sha256()
        if header_closure_files:
            for h in sorted(header_closure_files):
                if h.is_file():
                    header_hasher.update(h.name.encode("utf-8"))
                    header_hasher.update(h.read_bytes())
        header_closure_digest = header_hasher.hexdigest()

        effective_sdkconfig_digest = hashlib.sha256(effective_sdkconfig.encode("utf-8")).hexdigest()
        facade_path = facade_dir or (self.ws_root / "wink-micro-os" / "frameworks" / "esp_idf")
        facade_git_sha = get_git_sha(facade_path)
        patch_content_digest = hashlib.sha256(patch_content.encode("utf-8")).hexdigest()

        key = compute_build_cache_key(
            app_source_digest=app_source_digest,
            header_closure_digest=header_closure_digest,
            effective_sdkconfig_digest=effective_sdkconfig_digest,
            toolchain_version=toolchain_version,
            facade_git_sha=facade_git_sha,
            patch_content_digest=patch_content_digest,
            config_profile_id=config_profile_id,
        )

        digests = {
            "app_source_digest": app_source_digest,
            "header_closure_digest": header_closure_digest,
            "effective_sdkconfig_digest": effective_sdkconfig_digest,
            "toolchain_version": toolchain_version,
            "facade_git_sha": facade_git_sha,
            "patch_content_digest": patch_content_digest,
            "config_profile_id": config_profile_id,
            "build_cache_key": key,
        }
        return key, digests

    def lookup_cache(self, cache_key: str) -> Optional[Path]:
        """Returns path to cached artifact if existing."""
        candidate = self.cache_dir / cache_key / "artifact.wasm"
        if candidate.is_file():
            return candidate
        return None

    def store_cache(self, cache_key: str, artifact_path: Path, metadata: Dict[str, Any]) -> Path:
        """Stores built artifact in cache."""
        target_dir = self.cache_dir / cache_key
        target_dir.mkdir(parents=True, exist_ok=True)
        dest_artifact = target_dir / "artifact.wasm"
        shutil.copy2(artifact_path, dest_artifact)
        meta_file = target_dir / "metadata.json"
        meta_file.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
        return dest_artifact

    def create_isolated_sandbox(self, app_dir: Path, sandbox_target: Path) -> Path:
        """Creates an isolated copy of app directory for mutations (AFG-R05)."""
        if sandbox_target.exists():
            shutil.rmtree(sandbox_target)
        shutil.copytree(app_dir, sandbox_target)
        return sandbox_target
