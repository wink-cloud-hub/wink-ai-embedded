"""Regression tests for Binary SDK pack-time contracts."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_SDK_ROOT = _HERE.parent.parent
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from tools.pack import binary as pack_sdk_binary  # noqa: E402


def _write_valid_pack_cache(build_dir: Path) -> None:
    (build_dir / "CMakeCache.txt").write_text(
        "CMAKE_C_FLAGS:STRING="
        "-DWINK_MAX_SOFT_TIMERS=32 "
        "-DPAL_PWM_CHANNELS=16 "
        "-ffunction-sections "
        "-fdata-sections\n",
        encoding="utf-8",
    )


def _write_valid_wink_config_h(build_dir: Path) -> None:
    generated = build_dir / "generated"
    generated.mkdir(parents=True, exist_ok=True)
    (generated / "wink_config.h").write_text(
        "#ifndef WINK_CONFIG_H\n"
        "#define WINK_CONFIG_H\n"
        "#define WINK_MAX_SOFT_TIMERS    (32U)\n"
        "#define PAL_PWM_CHANNELS        (16U)\n"
        "#endif\n",
        encoding="utf-8",
    )


class BinarySdkPackTest(unittest.TestCase):
    def test_pack_stub_app_writes_binary_pack_ceilings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stub_dir = pack_sdk_binary.create_binary_pack_stub_app(Path(tmp))
            cfg = json.loads((stub_dir / "wink-app.json").read_text(encoding="utf-8"))
            cmake = (stub_dir / "CMakeLists.txt").read_text(encoding="utf-8")
            has_pack_stub = (stub_dir / "pack_stub.c").is_file()

        self.assertEqual(cfg["max_soft_timers"], 32)
        self.assertEqual(cfg["pwm_channels"], 16)
        self.assertEqual(cfg["app_name"], "binary_pack_stub")
        self.assertEqual(cfg["devices"], {})
        self.assertIn("set(WINK_APP_SOURCES", cmake)
        self.assertIn("PARENT_SCOPE", cmake)
        self.assertTrue(has_pack_stub)

    def test_gcc_skip_build_requires_function_and_data_sections(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / "CMakeCache.txt"
            cache.write_text(
                "CMAKE_C_FLAGS:STRING="
                "-DWINK_MAX_SOFT_TIMERS=32 "
                "-DPAL_PWM_CHANNELS=16 "
                "-ffunction-sections\n",
                encoding="utf-8",
            )

            with self.assertRaises(SystemExit) as cm:
                pack_sdk_binary.validate_binary_pack_build_flags(Path(tmp))

        self.assertIn("-fdata-sections", str(cm.exception))

    def test_gcc_skip_build_accepts_function_and_data_sections(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / "CMakeCache.txt"
            cache.write_text(
                "CMAKE_C_FLAGS:STRING="
                "-DWINK_MAX_SOFT_TIMERS=32 "
                "-DPAL_PWM_CHANNELS=16 "
                "-ffunction-sections "
                "-fdata-sections\n",
                encoding="utf-8",
            )

            pack_sdk_binary.validate_binary_pack_build_flags(Path(tmp))

    def test_skip_build_requires_generated_wink_config_h(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            _write_valid_pack_cache(build_dir)

            with self.assertRaises(SystemExit) as cm:
                pack_sdk_binary.validate_binary_pack_skip_build(build_dir)

        self.assertIn("wink_config.h", str(cm.exception))
        self.assertIn("Re-run without --skip-build", str(cm.exception))

    def test_skip_build_rejects_wrong_wink_config_ceilings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            _write_valid_pack_cache(build_dir)
            generated = build_dir / "generated"
            generated.mkdir(parents=True, exist_ok=True)
            (generated / "wink_config.h").write_text(
                "#define WINK_MAX_SOFT_TIMERS    (16U)\n"
                "#define PAL_PWM_CHANNELS        (8U)\n",
                encoding="utf-8",
            )

            with self.assertRaises(SystemExit) as cm:
                pack_sdk_binary.validate_binary_pack_skip_build(build_dir)

        msg = str(cm.exception)
        self.assertIn("WINK_MAX_SOFT_TIMERS=16 (expected 32)", msg)
        self.assertIn("PAL_PWM_CHANNELS=8 (expected 16)", msg)

    def test_skip_build_accepts_valid_wink_config_ceilings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            _write_valid_pack_cache(build_dir)
            _write_valid_wink_config_h(build_dir)

            pack_sdk_binary.validate_binary_pack_skip_build(build_dir)

    def test_wink_config_h_rejects_non_parenthesized_define(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            generated = build_dir / "generated"
            generated.mkdir(parents=True, exist_ok=True)
            (generated / "wink_config.h").write_text(
                "#define WINK_MAX_SOFT_TIMERS    32U\n"
                "#define PAL_PWM_CHANNELS        16U\n",
                encoding="utf-8",
            )

            with self.assertRaises(SystemExit) as cm:
                pack_sdk_binary.validate_binary_pack_wink_config_h(build_dir)

        self.assertIn("missing or not (NU) form", str(cm.exception))

    def test_copy_include_tree_skips_internal_and_preserves_hal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            sdk_root = Path(tmp) / "sdk"
            pal_include = sdk_root / "pal/include"
            (pal_include / "internal").mkdir(parents=True)
            (pal_include / "hal").mkdir(parents=True)
            (pal_include / "internal" / "secret.h").write_text("/* secret */\n", encoding="utf-8")
            (pal_include / "hal" / "pal_i2c.h").write_text("/* i2c */\n", encoding="utf-8")
            (pal_include / "pal.h").write_text("/* pal */\n", encoding="utf-8")

            out = Path(tmp) / "include"
            count = pack_sdk_binary.copy_include_tree(sdk_root, "pal/include", out)

            self.assertEqual(count, 2)
            self.assertFalse((out / "internal" / "secret.h").exists())
            self.assertTrue((out / "hal" / "pal_i2c.h").is_file())
            self.assertTrue((out / "pal.h").is_file())

    def test_binary_sdk_cmake_sets_platform_before_config_codegen(self) -> None:
        cmake = (
            _SDK_ROOT / "tools" / "binary_sdk_cmake" / "CMakeLists.txt"
        ).read_text(encoding="utf-8")

        self.assertLess(
            cmake.index('set(TARGET_PLATFORM "host"'),
            cmake.index("add_custom_command("),
        )


if __name__ == "__main__":
    unittest.main()
