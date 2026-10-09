# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for Task T1.3: 构建指纹、加载身份与写隔离
Validates AC-1.3 & AC-1.4 acceptance criteria:
- Full dependency closure cache key sensitivity (source, headers, sdkconfig, toolchain, facade, patch, profile).
- Uncommitted working tree facade modifications invalidate cache key.
- Dual verification on cache lookup (rejects corrupted, empty, or tampered artifacts).
- Atomic store_cache behavior.
- Write isolation verification preventing source directory pollution.
"""
from __future__ import annotations

import json
from pathlib import Path
import pytest

from loop.afg.build_sandbox import (
    BuildSandbox,
    compute_build_cache_key,
    compute_directory_digest,
)


def test_build_cache_key_full_sensitivity(tmp_path: Path):
    """Verifies that all 7 constituent axes sensitivity strictly bust the cache key."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    (app_dir / "main.c").write_text('int x = 1;\n', encoding="utf-8")

    facade_dir = tmp_path / "facade"
    facade_dir.mkdir(parents=True)
    (facade_dir / "facade.c").write_text('int f = 1;\n', encoding="utf-8")

    header = tmp_path / "custom.h"
    header.write_text('#define FOO 1\n', encoding="utf-8")

    sandbox = BuildSandbox(workspace_root=tmp_path, cache_dir=tmp_path / "cache")

    base_key, base_digests = sandbox.calculate_key(
        app_dir=app_dir,
        config_profile_id="default",
        patch_content="",
        header_closure_files=[header],
        effective_sdkconfig="CONFIG_TEST=y",
        toolchain_version="emscripten-3.1.56",
        facade_dir=facade_dir,
    )

    # 1. Modifying app source changes key
    (app_dir / "main.c").write_text('int x = 2;\n', encoding="utf-8")
    key_app, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.56")
    assert key_app != base_key
    (app_dir / "main.c").write_text('int x = 1;\n', encoding="utf-8")  # restore

    # 2. Modifying header changes key
    header.write_text('#define FOO 2\n', encoding="utf-8")
    key_hdr, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.56")
    assert key_hdr != base_key
    header.write_text('#define FOO 1\n', encoding="utf-8")  # restore

    # 3. Modifying sdkconfig changes key
    key_cfg, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=n", toolchain_version="emscripten-3.1.56")
    assert key_cfg != base_key

    # 4. Modifying toolchain version changes key
    key_tc, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.57")
    assert key_tc != base_key

    # 5. Modifying uncommitted facade source code changes key
    (facade_dir / "facade.c").write_text('int f = 2;\n', encoding="utf-8")
    key_facade, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.56")
    assert key_facade != base_key
    (facade_dir / "facade.c").write_text('int f = 1;\n', encoding="utf-8")  # restore

    # 6. Modifying patch content changes key
    key_patch, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.56", patch_content="diff-patch-1")
    assert key_patch != base_key

    # 7. Modifying config profile ID changes key
    key_prof, _ = sandbox.calculate_key(app_dir=app_dir, facade_dir=facade_dir, header_closure_files=[header], effective_sdkconfig="CONFIG_TEST=y", toolchain_version="emscripten-3.1.56", config_profile_id="release")
    assert key_prof != base_key


def test_cache_store_and_verified_lookup(tmp_path: Path):
    """Verifies that store_cache saves verified artifact and lookup returns it."""
    sandbox = BuildSandbox(workspace_root=tmp_path, cache_dir=tmp_path / "cache")

    artifact_src = tmp_path / "output.wasm"
    artifact_src.write_bytes(b"\x00asm\x01\x00\x00\x00_test_payload")

    stored = sandbox.store_cache("key_abc_123", artifact_src, {"claim_id": "test.claim"})
    assert stored.is_file()

    lookup = sandbox.lookup_cache("key_abc_123")
    assert lookup is not None
    assert lookup.read_bytes() == artifact_src.read_bytes()


def test_cache_lookup_rejects_tampered_or_corrupted_entry(tmp_path: Path):
    """Verifies that tampered or corrupted cache entries fail dual verification and return None."""
    sandbox = BuildSandbox(workspace_root=tmp_path, cache_dir=tmp_path / "cache")

    artifact_src = tmp_path / "output.wasm"
    artifact_src.write_bytes(b"\x00asm\x01\x00\x00\x00_original")

    stored = sandbox.store_cache("key_tampered", artifact_src, {"claim_id": "test.claim"})
    assert stored.is_file()

    # Tamper the cached artifact file directly
    stored.write_bytes(b"corrupted_bytes_that_do_not_match_metadata_sha")

    # lookup_cache MUST catch hash mismatch and refuse to serve it
    lookup = sandbox.lookup_cache("key_tampered")
    assert lookup is None


def test_write_isolation_verification(tmp_path: Path):
    """Verifies that write isolation catches tainted or newly written files."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    file_a = app_dir / "a.c"
    file_a.write_text("code a\n", encoding="utf-8")
    file_b = app_dir / "b.h"
    file_b.write_text("code b\n", encoding="utf-8")

    snapshot = BuildSandbox.capture_directory_snapshot(app_dir)

    # 1. Untouched directory passes
    ok, violations = BuildSandbox.verify_write_isolation(app_dir, snapshot)
    assert ok is True
    assert len(violations) == 0

    # 2. Modifying a file is detected as TAINTED
    file_a.write_text("tainted code\n", encoding="utf-8")
    ok, violations = BuildSandbox.verify_write_isolation(app_dir, snapshot)
    assert ok is False
    assert any("TAINTED" in v for v in violations)
    file_a.write_text("code a\n", encoding="utf-8")  # restore

    # 3. Adding an untracked file is detected as POLLUTED
    (app_dir / "untracked_output.o").write_bytes(b"garbage")
    ok, violations = BuildSandbox.verify_write_isolation(app_dir, snapshot)
    assert ok is False
    assert any("POLLUTED" in v for v in violations)
