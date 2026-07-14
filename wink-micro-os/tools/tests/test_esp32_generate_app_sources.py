"""Tests for :mod:`tools.esp32.generate_app_sources`.

These tests exercise the Python port of ``generate_app_sources.ps1``. They
build a self-contained fake repo tree in a temp dir (fake app dir + fake
``wink-micro-os/samples/common/src``) so we don't depend on the real
workspace layout — and can still assert the BAL-migrated filter fires
even though ``common/src`` no longer exists in the live tree.
"""
from __future__ import annotations

import re
import sys
import tempfile
import unittest
from pathlib import Path


SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))


def _make_fake_repo(tmp: Path, *, with_common: bool = True) -> tuple[Path, Path, Path]:
    """Build a minimal fake repo tree.

    Layout::

        tmp/
          esp32_firmware/
            main/                  (may be missing — generator creates it)
          wink-micro-os/
            samples/
              common/
                include/           (exists)
                src/               (only if with_common=True)
                  wink_blink_helper.c   (BAL-migrated — must be filtered)
                  wink_extra_helper.c   (kept)
          wink-micro-app/
            devkitc_smoke/
              app_callbacks.c
              board_config.c
              sub/
                extra.c
              test_devkitc_smoke_e2e.c  (excluded)
              TEST_SHOUTY.C             (excluded — case-insensitive)

    Returns ``(repo_root, esp32_firmware_dir, app_dir)``.
    """
    repo_root = tmp
    esp32_fw = repo_root / "esp32_firmware"
    esp32_fw.mkdir()
    # Intentionally do NOT precreate main/ — the generator should mkdir it.

    micro_os = repo_root / "wink-micro-os" / "samples" / "common"
    (micro_os / "include").mkdir(parents=True)
    if with_common:
        src_dir = micro_os / "src"
        src_dir.mkdir()
        (src_dir / "wink_blink_helper.c").write_text("// BAL-migrated\n")
        (src_dir / "wink_extra_helper.c").write_text("// kept\n")

    app_dir = repo_root / "wink-micro-app" / "devkitc_smoke"
    (app_dir / "sub").mkdir(parents=True)
    (app_dir / "app_callbacks.c").write_text("// app\n")
    (app_dir / "board_config.c").write_text("// board\n")
    (app_dir / "sub" / "extra.c").write_text("// nested\n")
    (app_dir / "test_devkitc_smoke_e2e.c").write_text("// e2e test — excluded\n")
    (app_dir / "TEST_SHOUTY.C").write_text("// case-insensitive test exclusion\n")

    return repo_root, esp32_fw, app_dir


def _extract_sources_block(content: str) -> list[str]:
    """Return the token list inside ``set(WINK_APP_SOURCES … CACHE INTERNAL "…")``.

    Strips whitespace and empty lines; keeps everything else so callers can
    assert both presence and absence of entries.
    """
    # Non-greedy match up to the ``CACHE INTERNAL`` keyword — the source
    # list is the payload between the opening line and the CACHE marker.
    m = re.search(
        r"set\(WINK_APP_SOURCES\s*(.*?)\s*CACHE INTERNAL",
        content,
        flags=re.DOTALL,
    )
    if not m:
        return []
    body = m.group(1)
    return [line.strip() for line in body.splitlines() if line.strip()]


class GenerateBehaviourTests(unittest.TestCase):
    def test_writes_output_to_main_app_sources_cmake(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            self.assertEqual(result.out_path, esp32_fw / "main" / "app_sources.cmake")
            self.assertTrue(result.out_path.exists(), "generator did not write output")

    def test_excludes_test_c_files_case_insensitively(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            sources = _extract_sources_block(content)
            # Excluded (case-insensitively).
            joined = "\n".join(sources).lower()
            self.assertNotIn("test_devkitc_smoke_e2e.c", joined)
            self.assertNotIn("test_shouty.c", joined)

    def test_includes_non_test_c_recursively(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            sources = _extract_sources_block(content)
            joined = "\n".join(sources)
            self.assertTrue(any("app_callbacks.c" in s for s in sources))
            self.assertTrue(any("board_config.c" in s for s in sources))
            self.assertTrue(any("sub/extra.c" in s for s in sources),
                            f"missing nested source in:\n{joined}")
            self.assertEqual(result.app_count, 3)
            self.assertEqual(result.common_count, 1)
            self.assertEqual(result.total_count, 4)

    def test_bal_migrated_names_filtered_from_common(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp), with_common=True)
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            sources = _extract_sources_block(content)
            joined = "\n".join(sources)
            # BAL-migrated must be filtered out.
            self.assertNotIn("wink_blink_helper.c", joined)
            # Non-BAL common source must be included.
            self.assertTrue(any("wink_extra_helper.c" in s for s in sources),
                            f"missing non-BAL common source in:\n{joined}")

    def test_missing_common_src_dir_is_silently_skipped(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp), with_common=False)
            # Should not raise even though wink-micro-os/samples/common/src is absent.
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            sources = _extract_sources_block(content)
            # Three app sources (nested + two top-level); no common entries.
            self.assertEqual(len(sources), 3, f"unexpected sources:\n{sources}")
            self.assertEqual(result.app_count, 3)
            self.assertEqual(result.common_count, 0)

    def test_paths_under_repo_root_use_cmake_prefix(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            sources = _extract_sources_block(content)
            for s in sources:
                self.assertTrue(
                    s.startswith("${CMAKE_CURRENT_LIST_DIR}/../../"),
                    f"expected repo-relative prefix, got: {s!r}",
                )
                self.assertNotIn("\\", s, f"backslash leaked: {s!r}")

    def test_cache_variables_present(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            self.assertIn('set(WINK_APP_NAME "devkitc_smoke"', content)
            self.assertIn('set(WINK_APP_DIR "${CMAKE_CURRENT_LIST_DIR}/../../wink-micro-app/devkitc_smoke"', content)
            # Common include falls back to wink-micro-os/samples/common/include
            # because <parent-of-app>/common/include (wink-micro-app/common/include)
            # doesn't exist in this fake tree.
            self.assertIn(
                'set(WINK_APP_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../wink-micro-os/samples/common/include"',
                content,
            )
            self.assertIn("message(STATUS", content)

    def test_common_include_prefers_sibling_when_present(self):
        """When ``<parent-of-app>/common/include`` exists, prefer it."""
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            # Create wink-micro-app/common/include as a sibling of the app.
            sibling = repo / "wink-micro-app" / "common" / "include"
            sibling.mkdir(parents=True)

            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            content = result.out_path.read_text(encoding="utf-8-sig")
            self.assertIn(
                'set(WINK_APP_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../wink-micro-app/common/include"',
                content,
            )

    def test_output_encoding_is_utf8_bom_and_crlf(self):
        from tools.esp32.generate_app_sources import generate

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            result = generate(
                app_dir=app_dir,
                esp32_firmware_dir=esp32_fw,
                repo_root=repo,
            )
            raw = result.out_path.read_bytes()
            # UTF-8 BOM.
            self.assertTrue(raw.startswith(b"\xef\xbb\xbf"),
                            f"missing UTF-8 BOM; head={raw[:6]!r}")
            # CRLF line endings.
            self.assertIn(b"\r\n", raw)
            self.assertNotIn(b"\r\r", raw)


class GenerateErrorTests(unittest.TestCase):
    def test_missing_app_dir_returns_nonzero_from_main(self):
        from tools.esp32 import generate_app_sources as mod

        with tempfile.TemporaryDirectory() as tmp:
            esp32_fw = Path(tmp) / "esp32_firmware"
            esp32_fw.mkdir()
            missing = Path(tmp) / "does_not_exist"
            rc = mod.main([
                "--esp32-firmware-dir", str(esp32_fw),
                "--app-dir", str(missing),
                "--repo-root", str(tmp),
            ])
            self.assertNotEqual(rc, 0)


class CliTests(unittest.TestCase):
    def test_main_with_app_dir_writes_file(self):
        from tools.esp32 import generate_app_sources as mod

        with tempfile.TemporaryDirectory() as tmp:
            repo, esp32_fw, app_dir = _make_fake_repo(Path(tmp))
            rc = mod.main([
                "--esp32-firmware-dir", str(esp32_fw),
                "--app-dir", str(app_dir),
                "--repo-root", str(repo),
            ])
            self.assertEqual(rc, 0)
            self.assertTrue((esp32_fw / "main" / "app_sources.cmake").exists())

    def test_main_with_app_name_uses_legacy_samples_path(self):
        """--app-name (no --app-dir) resolves to wink-micro-os/samples/<name>.

        Legacy path preserved verbatim per PS1; this test creates that layout
        as a fake app so the module can find it.
        """
        from tools.esp32 import generate_app_sources as mod

        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            esp32_fw = repo / "esp32_firmware"
            esp32_fw.mkdir()
            legacy_app = repo / "wink-micro-os" / "samples" / "myapp"
            legacy_app.mkdir(parents=True)
            (legacy_app / "hello.c").write_text("// hi\n")
            (repo / "wink-micro-os" / "samples" / "common" / "include").mkdir(parents=True)

            rc = mod.main([
                "--esp32-firmware-dir", str(esp32_fw),
                "--app-name", "myapp",
                "--repo-root", str(repo),
            ])
            self.assertEqual(rc, 0)
            content = (esp32_fw / "main" / "app_sources.cmake").read_text(encoding="utf-8-sig")
            self.assertIn('set(WINK_APP_NAME "myapp"', content)
            self.assertIn("wink-micro-os/samples/myapp", content)


if __name__ == "__main__":
    unittest.main()
