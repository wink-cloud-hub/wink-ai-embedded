"""Codegen tests for motor and encoder driver plugins."""
from __future__ import annotations

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_SDK_ROOT = _HERE.parent.parent.parent
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from tools.codegen import app_codegen  # noqa: E402

MOTOR_ONLY_JSON = _HERE / "golden_motor_only.json"

ALL_WINK_USE = [
    "WINK_USE_LED",
    "WINK_USE_BUTTON",
    "WINK_USE_SERVO",
    "WINK_USE_SSD1306",
    "WINK_USE_ULTRASONIC",
    "WINK_USE_GPS",
    "WINK_USE_EEPROM",
    "WINK_USE_MOTOR",
    "WINK_USE_ENCODER",
]


def _parse_app_options(text: str) -> dict[str, bool]:
    states: dict[str, bool] = {}
    for opt in ALL_WINK_USE:
        m = re.search(rf"set\({opt} (ON|OFF)", text)
        if not m:
            raise AssertionError(f"missing {opt} in app_options.cmake")
        states[opt] = m.group(1) == "ON"
    return states


class MotorEncoderCodegenTest(unittest.TestCase):
    def test_motor_only_wink_use_matrix(self) -> None:
        self.assertTrue(MOTOR_ONLY_JSON.exists())
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            rc = app_codegen.main([
                "--config", str(MOTOR_ONLY_JSON),
                "--out-dir", str(out),
            ])
            self.assertEqual(rc, 0)
            cmake = (out / "app_options.cmake").read_text(encoding="utf-8")
            states = _parse_app_options(cmake)
            self.assertTrue(states["WINK_USE_MOTOR"])
            for opt in ALL_WINK_USE:
                if opt == "WINK_USE_MOTOR":
                    continue
                self.assertFalse(states[opt], f"expected {opt} OFF")

    def test_motor_config_init_defaults(self) -> None:
        cfg = {
            "app_name": "motor_defaults",
            "board": "esp32_devkitc",
            "devices": {
                "left_motor": {
                    "type": "motor",
                    "pwm_channel": 1,
                    "dir_pin_a": 4,
                },
            },
        }
        ctx = app_codegen.build_context(cfg, "test.json")
        init = ctx["devices"][0]["config_init"]
        self.assertIn(".dir_pin_b = -1", init)
        self.assertIn(".pwm_freq_hz = 20000u", init)
        self.assertIn("dal_motor_init", init)

    def test_encoder_config_init_pull_default(self) -> None:
        cfg = {
            "app_name": "encoder_defaults",
            "board": "esp32_devkitc",
            "devices": {
                "wheel_encoder": {
                    "type": "encoder",
                    "pin_a": 18,
                },
            },
        }
        ctx = app_codegen.build_context(cfg, "test.json")
        init = ctx["devices"][0]["config_init"]
        self.assertIn(".pin_b = -1", init)
        self.assertIn(".pull = PAL_GPIO_INPUT_PULLUP", init)
        self.assertIn("dal_encoder_init", init)

    def test_encoder_and_motor_both_on(self) -> None:
        cfg = {
            "app_name": "chassis_pair",
            "board": "esp32_devkitc",
            "devices": {
                "left_motor": {
                    "type": "motor",
                    "pwm_channel": 0,
                    "dir_pin_a": 12,
                },
                "left_encoder": {
                    "type": "encoder",
                    "pin_a": 34,
                    "pin_b": 35,
                    "pull": "down",
                },
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_json = Path(tmp) / "app.json"
            tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
            out = Path(tmp) / "out"
            rc = app_codegen.main(["--config", str(tmp_json), "--out-dir", str(out)])
            self.assertEqual(rc, 0)
            states = _parse_app_options(
                (out / "app_options.cmake").read_text(encoding="utf-8")
            )
            self.assertTrue(states["WINK_USE_MOTOR"])
            self.assertTrue(states["WINK_USE_ENCODER"])
            init = app_codegen.build_context(cfg, str(tmp_json))["devices"]
            enc_init = next(d["config_init"] for d in init if d["type"] == "encoder")
            self.assertIn(".pull = PAL_GPIO_INPUT_PULLDOWN", enc_init)


if __name__ == "__main__":
    unittest.main()
