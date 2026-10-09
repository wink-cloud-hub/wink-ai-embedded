# SPDX-License-Identifier: Apache-2.0
"""
loop.afg.build_sandbox - Build Sandbox & Dependency Closure Hash Cache
======================================================================
Implements Task T1.3 & AFG-R15:
- Full dependency closure cache key binding:
    app_source_digest + header_closure_digest + effective_sdkconfig_digest
    + toolchain_version + facade_git_sha + facade_source_digest + patch_content_digest
    + config_profile_id
- Working tree uncommitted changes explicitly incorporated via facade_source_digest.
- Cache hit dual verification: validates metadata.json and artifact SHA256 integrity.
- Atomic store_cache preventing partial writes under concurrent attempts.
- Write isolation verification guaranteeing upstream sources remain pristine.
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
    facade_source_digest: str,
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
        + facade_source_digest.encode("utf-8")
        + patch_content_digest.encode("utf-8")
        + config_profile_id.encode("utf-8")
    )
    return hashlib.sha256(raw).hexdigest()


def compute_directory_digest(
    dir_path: Path,
    extensions: Tuple[str, ...] = (
        ".c", ".h", ".cpp", ".hpp", ".txt", ".cmake", ".json", ".hwtimer"
    )
) -> str:
    """Computes a deterministic hash of all relevant source and config files in directory."""
    if not dir_path.is_dir():
        return hashlib.sha256(b"").hexdigest()

    hasher = hashlib.sha256()
    for root, _, files in sorted(os.walk(dir_path)):
        for f in sorted(files):
            if any(f.endswith(ext) for ext in extensions):
                file_path = Path(root) / f
                # Skip build outputs, temporary files and caches
                rel_parts = file_path.relative_to(dir_path).parts
                if any(p in ("build", "__pycache__", ".build_cache", "runs") for p in rel_parts):
                    continue
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
        """Calculates cache key and constituent digests including uncommitted working tree state."""
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
        facade_source_digest = compute_directory_digest(facade_path)
        patch_content_digest = hashlib.sha256(patch_content.encode("utf-8")).hexdigest()

        key = compute_build_cache_key(
            app_source_digest=app_source_digest,
            header_closure_digest=header_closure_digest,
            effective_sdkconfig_digest=effective_sdkconfig_digest,
            toolchain_version=toolchain_version,
            facade_git_sha=facade_git_sha,
            facade_source_digest=facade_source_digest,
            patch_content_digest=patch_content_digest,
            config_profile_id=config_profile_id,
        )

        digests = {
            "app_source_digest": app_source_digest,
            "header_closure_digest": header_closure_digest,
            "effective_sdkconfig_digest": effective_sdkconfig_digest,
            "toolchain_version": toolchain_version,
            "facade_git_sha": facade_git_sha,
            "facade_source_digest": facade_source_digest,
            "patch_content_digest": patch_content_digest,
            "config_profile_id": config_profile_id,
            "build_cache_key": key,
        }
        return key, digests

    def lookup_cache(self, cache_key: str) -> Optional[Path]:
        """Returns path to cached artifact if existing and mathematically verified."""
        target_dir = self.cache_dir / cache_key
        candidate = target_dir / "artifact.wasm"
        meta_file = target_dir / "metadata.json"

        if not candidate.is_file() or not meta_file.is_file():
            return None

        # Dual Verification: Check artifact size and sha256 against metadata
        try:
            content = candidate.read_bytes()
            if len(content) == 0:
                return None
            actual_sha = hashlib.sha256(content).hexdigest()
            meta = json.loads(meta_file.read_text(encoding="utf-8"))
            expected_sha = meta.get("artifact_sha256")
            if not expected_sha or actual_sha != expected_sha:
                # Corrupted or tampered cache entry; do not return
                return None
            return candidate
        except Exception:
            return None

    def store_cache(self, cache_key: str, artifact_path: Path, metadata: Dict[str, Any]) -> Path:
        """Stores built artifact in cache atomically with verified digest metadata."""
        target_dir = self.cache_dir / cache_key
        tmp_dir = self.cache_dir / f"{cache_key}.tmp.{os.getpid()}"
        if tmp_dir.exists():
            shutil.rmtree(tmp_dir)
        tmp_dir.mkdir(parents=True, exist_ok=True)

        artifact_bytes = artifact_path.read_bytes()
        artifact_sha256 = hashlib.sha256(artifact_bytes).hexdigest()

        dest_artifact = tmp_dir / "artifact.wasm"
        dest_artifact.write_bytes(artifact_bytes)

        full_meta = dict(metadata)
        full_meta["artifact_sha256"] = artifact_sha256
        full_meta["size_bytes"] = len(artifact_bytes)

        meta_file = tmp_dir / "metadata.json"
        meta_file.write_text(json.dumps(full_meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

        # Atomic replacement into target_dir
        if target_dir.exists():
            shutil.rmtree(target_dir)
        tmp_dir.replace(target_dir)

        return target_dir / "artifact.wasm"

    def create_isolated_sandbox(self, app_dir: Path, sandbox_target: Path) -> Path:
        """Creates an isolated copy of app directory for mutations (AFG-R05)."""
        if sandbox_target.exists():
            shutil.rmtree(sandbox_target)
        shutil.copytree(app_dir, sandbox_target)
        return sandbox_target

    @staticmethod
    def capture_directory_snapshot(dir_path: Path) -> Dict[str, str]:
        """Captures a snapshot mapping relative paths to file sha256."""
        snapshot = {}
        for root, _, files in os.walk(dir_path):
            for f in files:
                p = Path(root) / f
                try:
                    rel = str(p.relative_to(dir_path)).replace("\\", "/")
                    snapshot[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
                except Exception:
                    pass
        return snapshot

    @staticmethod
    def verify_write_isolation(
        dir_path: Path,
        baseline_snapshot: Dict[str, str]
    ) -> Tuple[bool, List[str]]:
        """Verifies that dir_path has suffered zero file mutations or additions."""
        current_snapshot = BuildSandbox.capture_directory_snapshot(dir_path)
        violations = []

        # Check modified or missing files
        for rel_path, old_hash in baseline_snapshot.items():
            if rel_path not in current_snapshot:
                violations.append(f"MISSING: File '{rel_path}' was removed from source directory")
            elif current_snapshot[rel_path] != old_hash:
                violations.append(f"TAINTED: File '{rel_path}' was modified during compilation/mutation")

        # Check newly created files
        for rel_path in current_snapshot:
            if rel_path not in baseline_snapshot:
                violations.append(f"POLLUTED: Untracked file '{rel_path}' was written to source directory")

        return len(violations) == 0, violations
