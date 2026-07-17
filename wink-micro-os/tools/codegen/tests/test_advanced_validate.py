"""ADR-0034 progressive disclosure: ``advanced.*`` codegen validation + emit.

Positive:
  - button advanced.pull=none → emits DAL_BUTTON_PULL_NONE
  - servo advanced.resolution_bits + clock_requirement → emits C fields
  - default (no advanced) → no .pull / .resolution_bits / .clock_requirement lines

Negative:
  - advanced non-object; unknown keys; bad pull / bits / clock types & values
  - top-level pull / resolution_bits / clock_requirement aliases rejected
"""
from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_SDK_ROOT = _HERE.parent.parent.parent
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from tools.codegen import app_codegen  # noqa: E402


def _run_codegen(cfg: dict) -> tuple[int, str, str | None]:
    """Return (exit_code, stderr, device_tree_c_text_or_None)."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        tmp_json = tmp_path / "app.json"
        out = tmp_path / "out"
        tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            try:
                rc = app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(out),
                ])
            except SystemExit as e:
                rc = int(e.code) if e.code is not None else 0
        tree_c = None
        if rc == 0 and (out / "device_tree.c").exists():
            tree_c = (out / "device_tree.c").read_text(encoding="utf-8")
        return rc, buf.getvalue(), tree_c


def _run_expect_fail(cfg: dict) -> tuple[int, str]:
    rc, err, _ = _run_codegen(cfg)
    return rc, err


def _device_tree_c(cfg: dict) -> str:
    rc, err, tree_c = _run_codegen(cfg)
    if rc != 0 or tree_c is None:
        raise AssertionError(f"codegen failed rc={rc}: {err}")
    return tree_c


class AdvancedValidateTest(unittest.TestCase):
    def test_button_default_omits_pull(self) -> None:
        cfg = {
            "app_name": "btn_default",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                }
            },
        }
        c = _device_tree_c(cfg)
        self.assertNotIn(".pull", c)

    def test_button_advanced_pull_none_emits(self) -> None:
        cfg = {
            "app_name": "btn_none",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": {"pull": "none"},
                }
            },
        }
        c = _device_tree_c(cfg)
        self.assertIn(".pull = DAL_BUTTON_PULL_NONE", c)

    def test_servo_default_omits_advanced(self) -> None:
        cfg = {
            "app_name": "servo_default",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                }
            },
        }
        c = _device_tree_c(cfg)
        self.assertNotIn(".resolution_bits", c)
        self.assertNotIn(".clock_requirement", c)

    def test_servo_advanced_emits_bits_and_clock(self) -> None:
        cfg = {
            "app_name": "servo_adv",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 1,
                    "advanced": {
                        "resolution_bits": 10,
                        "clock_requirement": "stable_required",
                    },
                }
            },
        }
        c = _device_tree_c(cfg)
        self.assertIn(".resolution_bits = 10u", c)
        self.assertIn(".clock_requirement = DAL_SERVO_CLOCK_STABLE_REQUIRED", c)

    def test_advanced_must_be_object(self) -> None:
        cfg = {
            "app_name": "adv_str",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": "none",
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("advanced", err)
        self.assertIn("object", err)

    def test_unknown_advanced_key(self) -> None:
        cfg = {
            "app_name": "adv_unknown",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": {"pull": "up", "system_clock_hz": 80000000},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("unknown advanced", err)

    def test_pull_case_sensitive(self) -> None:
        cfg = {
            "app_name": "pull_case",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": {"pull": "NONE"},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("pull", err)

    def test_pull_non_string(self) -> None:
        cfg = {
            "app_name": "pull_int",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": {"pull": 3},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("pull", err)
        self.assertIn("string", err)

    def test_top_level_pull_rejected(self) -> None:
        cfg = {
            "app_name": "top_pull",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "pull": "none",
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("top-level", err)
        self.assertIn("advanced.pull", err)

    def test_resolution_bits_rejects_bool(self) -> None:
        cfg = {
            "app_name": "bits_bool",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                    "advanced": {"resolution_bits": True},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("resolution_bits", err)

    def test_resolution_bits_rejects_float(self) -> None:
        cfg = {
            "app_name": "bits_float",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                    "advanced": {"resolution_bits": 10.5},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("resolution_bits", err)

    def test_resolution_bits_out_of_range(self) -> None:
        cfg = {
            "app_name": "bits_oor",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                    "advanced": {"resolution_bits": 99},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("resolution_bits", err)

    def test_clock_requirement_unknown(self) -> None:
        cfg = {
            "app_name": "clock_bad",
            "devices": {
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                    "advanced": {"clock_requirement": "fixed"},
                }
            },
        }
        rc, err = _run_expect_fail(cfg)
        self.assertEqual(rc, 2)
        self.assertIn("clock_requirement", err)

    def test_generated_c_compiles_with_host_headers(self) -> None:
        """Gate: advanced fixture emits designated initializers that match DAL enums."""
        cfg = {
            "app_name": "adv_compile",
            "board": "esp32_devkitc",
            "devices": {
                "btn": {
                    "type": "button",
                    "pin": 0,
                    "auto_poll_ms": 10,
                    "advanced": {"pull": "none"},
                },
                "neck": {
                    "type": "servo",
                    "pwm_channel": 0,
                    "advanced": {
                        "resolution_bits": 13,
                        "clock_requirement": "auto",
                    },
                },
            },
        }
        c = _device_tree_c(cfg)
        self.assertIn(".pull = DAL_BUTTON_PULL_NONE", c)
        self.assertIn(".resolution_bits = 13u", c)
        self.assertIn(".clock_requirement = DAL_SERVO_CLOCK_AUTO", c)
        # Structural sanity: both devices init in one TU.
        self.assertIn("dal_button_init(&btn", c)
        self.assertIn("dal_servo_init(&neck", c)


if __name__ == "__main__":
    unittest.main()
