"""Negative validation tests for button ``event_drive`` / ``debounce_ms``.

Covers ADR-0031 codegen schema contract (S1 of the button event_drive
backends plan):

- Unknown ``event_drive`` value → SystemExit(2) with a clear message.
- ``event_drive == "soft_poll"`` missing ``auto_poll_ms`` → SystemExit(2).
- ``event_drive == "gpio_irq"`` without a ``pin`` (nor ``use_onboard``)
  → SystemExit(2).
- Default (no ``event_drive`` field) still requires ``auto_poll_ms``
  because default is soft_poll (Owner decision in ADR-0031).

Positive path with ``gpio_irq`` + valid ``pin`` must build_context OK,
so that S2/S3 can layer BAL start()/stop() on top without codegen
refusing.
"""
from __future__ import annotations

import contextlib
import io
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


def _run_codegen_expect_exit(cfg: dict) -> tuple[int, str]:
    """Run app_codegen on ``cfg``; return (exit_code, stderr_str)."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp_json = Path(tmp) / "app.json"
        tmp_json.write_text(json.dumps(cfg), encoding="utf-8")
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            try:
                rc = app_codegen.main([
                    "--config", str(tmp_json),
                    "--out-dir", str(Path(tmp) / "out"),
                ])
            except SystemExit as e:
                rc = int(e.code) if e.code is not None else 0
        return rc, buf.getvalue()


class ButtonEventDriveValidateTest(unittest.TestCase):
    def test_invalid_event_drive_value(self) -> None:
        cfg = {
            "app_name": "bad_drive",
            "devices": {
                "btn": {
                    "type": "button",
                    "gpio_pin": 0,
                    "event_drive": "bogus_backend",
                    "auto_poll_ms": 10,
                }
            },
        }
        rc, err = _run_codegen_expect_exit(cfg)
        self.assertNotEqual(rc, 0)
        self.assertIn("event_drive", err)
        self.assertIn("bogus_backend", err)

    def test_soft_poll_missing_auto_poll_ms(self) -> None:
        cfg = {
            "app_name": "no_poll_ms",
            "devices": {
                "btn": {
                    "type": "button",
                    "gpio_pin": 0,
                    "event_drive": "soft_poll",
                }
            },
        }
        rc, err = _run_codegen_expect_exit(cfg)
        self.assertNotEqual(rc, 0)
        self.assertIn("soft_poll", err)
        self.assertIn("auto_poll_ms", err)

    def test_default_drive_missing_auto_poll_ms(self) -> None:
        # No event_drive → default soft_poll → auto_poll_ms is still required.
        cfg = {
            "app_name": "default_no_poll_ms",
            "devices": {
                "btn": {
                    "type": "button",
                    "gpio_pin": 0,
                }
            },
        }
        rc, err = _run_codegen_expect_exit(cfg)
        self.assertNotEqual(rc, 0)
        self.assertIn("auto_poll_ms", err)

    def test_gpio_irq_missing_gpio_pin(self) -> None:
        # required_fields already enforces `gpio_pin`, but the message we care about
        # here is that codegen exits ≠ 0 with an actionable string. Since
        # ButtonDriver.required_fields = ["gpio_pin"], omit it entirely and let the
        # framework's missing-required-fields error fire — this is the actual
        # gate. If a future refactor drops `gpio_pin` from required_fields, the
        # gpio_irq-specific check must still block it.
        cfg = {
            "app_name": "irq_no_pin",
            "devices": {
                "btn": {
                    "type": "button",
                    "event_drive": "gpio_irq",
                }
            },
        }
        rc, err = _run_codegen_expect_exit(cfg)
        self.assertNotEqual(rc, 0)
        # Accept either the generic required-fields message or the
        # gpio_irq-specific message; both are correct rejections.
        self.assertTrue(
            "gpio_pin" in err,
            f"expected error to mention 'gpio_pin'; got: {err!r}",
        )

    def test_gpio_irq_with_gpio_pin_ok(self) -> None:
        # Positive path: gpio_irq + gpio_pin should validate cleanly (build_context
        # must not raise SystemExit). auto_poll_ms is only mandatory for
        # soft_poll; gpio_irq is allowed to omit it.
        cfg = {
            "app_name": "irq_ok",
            "devices": {
                "btn": {
                    "type": "button",
                    "gpio_pin": 0,
                    "event_drive": "gpio_irq",
                    "debounce_ms": 30,
                }
            },
        }
        # build_context is enough — no template rendering needed.
        ctx = app_codegen.build_context(cfg, "test:irq_ok")
        self.assertEqual(len(ctx["devices"]), 1)
        self.assertEqual(ctx["devices"][0]["name"], "btn")
        # Config macros must include debounce + gpio_irq selector.
        joined = "\n".join(ctx["config_macros"])
        self.assertIn("BTN_DEBOUNCE_MS 30u", joined)
        self.assertIn("BTN_EVENT_DRIVE_GPIO_IRQ", joined)

    def test_soft_poll_default_debounce_macro(self) -> None:
        # No debounce_ms → default 20u macro emitted (ADR-0031 doc default).
        cfg = {
            "app_name": "deb_default",
            "devices": {
                "btn": {
                    "type": "button",
                    "gpio_pin": 0,
                    "auto_poll_ms": 10,
                }
            },
        }
        ctx = app_codegen.build_context(cfg, "test:deb_default")
        joined = "\n".join(ctx["config_macros"])
        self.assertIn("BTN_DEBOUNCE_MS 20u", joined)
        self.assertIn("BTN_EVENT_DRIVE_SOFT_POLL", joined)


if __name__ == "__main__":
    unittest.main()
