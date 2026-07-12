"""Golden-file regression test for app_codegen.

Runs codegen against ``golden_devkitc.json`` into a temp directory, then
compares each generated file to a checked-in expected file under
``golden_expected/``. When an expected file is missing, the test fails with
a message pointing at ``--regen-golden`` — this forces explicit human
acceptance of new output rather than silent creation.

Usage (workspace root, SDK on PYTHONPATH):
    $env:PYTHONPATH = "wink-micro-os"   # PowerShell
    python wink-micro-os/tools/codegen/tests/test_golden.py
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

# Ensure wink-micro-os (SDK root) is importable when run as a script.
_HERE = Path(__file__).resolve().parent
_SDK_ROOT = _HERE.parent.parent.parent
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from tools.codegen import app_codegen  # noqa: E402

GOLDEN_JSON = _HERE / "golden_devkitc.json"
GOLDEN_EXPECTED = _HERE / "golden_expected"


class GoldenTest(unittest.TestCase):
    def tearDown(self) -> None:
        import shutil
        docs_dir = _HERE / "docs"
        if docs_dir.exists():
            shutil.rmtree(docs_dir)

    def test_devkitc_golden(self) -> None:
        self.assertTrue(GOLDEN_JSON.exists(),
                        f"golden config missing: {GOLDEN_JSON}")
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            rc = app_codegen.main([
                "--config", str(GOLDEN_JSON),
                "--out-dir", str(out),
            ])
            self.assertEqual(rc, 0, "codegen exited non-zero")

            generated = sorted(out.iterdir())
            self.assertTrue(generated, "codegen produced no output files")

            missing = []
            mismatched = []
            for gen in generated:
                exp = GOLDEN_EXPECTED / gen.name
                if not exp.exists():
                    missing.append(gen.name)
                    continue
                got = gen.read_text(encoding="utf-8")
                want = exp.read_text(encoding="utf-8")
                if got.rstrip("\n") != want.rstrip("\n"):
                    mismatched.append(gen.name)

            if missing or mismatched:
                lines = []
                if missing:
                    lines.append(
                        "missing golden expected files: " + ", ".join(missing)
                    )
                if mismatched:
                    lines.append(
                        "output differs from golden: " + ", ".join(mismatched)
                    )
                lines.append(
                    "regenerate with: python wink-micro-os/tools/codegen/app_codegen.py "
                    f"--config {GOLDEN_JSON} --out-dir {GOLDEN_EXPECTED}"
                )
                self.fail("\n".join(lines))

    def test_multi_device_golden(self) -> None:
        """Test servo + ssd1306 drivers + I2C bus-owner codegen."""
        golden_json = _HERE / "golden_multi_device.json"
        golden_expected = _HERE / "golden_multi_expected"
        self.assertTrue(golden_json.exists(),
                        f"golden config missing: {golden_json}")
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            rc = app_codegen.main([
                "--config", str(golden_json),
                "--out-dir", str(out),
            ])
            self.assertEqual(rc, 0, "codegen exited non-zero")

            generated = sorted(out.iterdir())
            self.assertTrue(generated, "codegen produced no output files")

            missing = []
            mismatched = []
            for gen in generated:
                exp = golden_expected / gen.name
                if not exp.exists():
                    missing.append(gen.name)
                    continue
                got = gen.read_text(encoding="utf-8")
                want = exp.read_text(encoding="utf-8")
                if got.rstrip("\n") != want.rstrip("\n"):
                    mismatched.append(gen.name)

            if missing or mismatched:
                lines = []
                if missing:
                    lines.append(
                        "missing golden expected files: " + ", ".join(missing)
                    )
                if mismatched:
                    lines.append(
                        "output differs from golden: " + ", ".join(mismatched)
                    )
                lines.append(
                    "regenerate with: python wink-micro-os/tools/codegen/app_codegen.py "
                    f"--config {golden_json} --out-dir {golden_expected}"
                )
                self.fail("\n".join(lines))

    def test_invalid_board(self) -> None:
        # Invalid board name should exit with 2 (validation error)
        cfg = {
            "app_name": "invalid_board_test",
            "board": "non_existent_board_name_xyz",
            "devices": {}
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            with self.assertRaises(SystemExit) as cm:
                app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            self.assertEqual(cm.exception.code, 2)

    def test_pin_conflict(self) -> None:
        # Reusing pin 2 on both status_led and another led should cause a conflict and exit with 2
        cfg = {
            "app_name": "conflict_test",
            "board": "esp32_devkitc",
            "devices": {
                "led1": {
                    "use_onboard": "status_led"
                },
                "led2": {
                    "type": "led",
                    "pin": 2
                }
            }
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            with self.assertRaises(SystemExit) as cm:
                app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            self.assertEqual(cm.exception.code, 2)

    def test_invalid_reference(self) -> None:
        # Resolving a non-existent board header path should exit with 2
        cfg = {
            "app_name": "invalid_ref_test",
            "board": "esp32_devkitc",
            "devices": {
                "led1": {
                    "type": "led",
                    "pin": "$board.headers.NON_EXISTENT_HEADER"
                }
            }
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            with self.assertRaises(SystemExit) as cm:
                app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            self.assertEqual(cm.exception.code, 2)

    def test_invalid_role(self) -> None:
        # Configuring an invalid role for a driver should exit with code 2
        cfg = {
            "app_name": "invalid_role_test",
            "board": "esp32_devkitc",
            "devices": {
                "led1": {
                    "type": "led",
                    "pin": 2,
                    "role": "binary_sensor"
                }
            }
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            with self.assertRaises(SystemExit) as cm:
                app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            self.assertEqual(cm.exception.code, 2)

    def test_onboard_type_conflict(self) -> None:
        # Explicit type declaration conflicting with onboard device should exit with code 2
        cfg = {
            "app_name": "onboard_type_conflict_test",
            "board": "esp32_devkitc",
            "devices": {
                "board_led": {
                    "use_onboard": "status_led",
                    "type": "servo"  # Conflicts with onboard "led" type!
                }
            }
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            with self.assertRaises(SystemExit) as cm:
                app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            self.assertEqual(cm.exception.code, 2)

    def test_escaping(self) -> None:
        # A value starting with $$board. should strip the first $ and return a literal string,
        # bypassing the board lookup completely.
        cfg = {
            "app_name": "escaping_test",
            "board": "esp32_devkitc",
            "devices": {
                "led1": {
                    "type": "led",
                    "pin": "$$board.non_existent_header"
                }
            }
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            
            # This should NOT raise SystemExit because $$board. is escaped
            ctx = app_codegen.build_context(cfg, str(tmp_json))
            dev = ctx["devices"][0]
            self.assertIn('.pin = $board.non_existent_header', dev["config_init"])


if __name__ == "__main__":
    unittest.main()
