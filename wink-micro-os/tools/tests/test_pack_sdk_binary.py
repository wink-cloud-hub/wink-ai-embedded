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

from tools import pack_sdk_binary  # noqa: E402


class BinarySdkPackTest(unittest.TestCase):
    def test_pack_stub_app_writes_binary_pack_ceilings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stub_dir = pack_sdk_binary.create_binary_pack_stub_app(Path(tmp))
            cfg = json.loads((stub_dir / "wink-app.json").read_text(encoding="utf-8"))

        self.assertEqual(cfg["max_soft_timers"], 32)
        self.assertEqual(cfg["pwm_channels"], 16)
        self.assertEqual(cfg["app_name"], "binary_pack_stub")
        self.assertEqual(cfg["devices"], {})

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
