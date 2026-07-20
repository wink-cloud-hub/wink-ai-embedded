"""Parity probe: wink lint fail/pass set must match CMake-replay gates."""
from __future__ import annotations

import sys
import unittest
from datetime import date
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import load_configs  # noqa: E402
from tools.lint.engine.runner import run_lint  # noqa: E402
from tools.lint.testdata.parity_probe.cmake_replay import (  # noqa: E402
    run_cmake_replay,
)

PROBE = SDK / "tools" / "lint" / "testdata" / "parity_probe"
BAD_ROOT = PROBE / "bad"
GOOD_ROOT = PROBE / "good"
RULES = SDK / "tools" / "lint" / "rules" / "layering.yaml"

# Rules covered by the CMake-replay + DAL probe (Task 7.5).
_PARITY_RULES = frozenset(
    {
        "BAL-HDR-NO-PAL",
        "BAL-NAME-1",
        "BAL-NAME-2",
        "BAL-MATH-1",
        "DAL-HDR-NO-HAL",
    }
)


def run_wink_lint_for_parity(root: Path) -> dict[str, set[str]]:
    cfg = load_configs([RULES])
    findings = run_lint(
        root,
        cfg,
        packs=["layering"],
        today=date(2026, 7, 20),
    )
    out: dict[str, set[str]] = {}
    for f in findings:
        if f.allowlisted:
            continue
        if f.rule_id not in _PARITY_RULES:
            continue
        out.setdefault(f.path.replace("\\", "/"), set()).add(f.rule_id)
    return out


class TestLintParity(unittest.TestCase):
    def test_parity_bad_tree_matches(self):
        cmake_fails = run_cmake_replay(BAD_ROOT)
        lint_fails = run_wink_lint_for_parity(BAD_ROOT)
        self.assertEqual(
            cmake_fails,
            lint_fails,
            f"diff cmake-only={_symdiff(cmake_fails, lint_fails)[0]} "
            f"lint-only={_symdiff(cmake_fails, lint_fails)[1]}",
        )
        # Sanity: every listed bad sample appears.
        self.assertIn("BAL-HDR-NO-PAL", cmake_fails.get(
            "bal/include/motion/bad_include_pal_hal.h", set()
        ))
        self.assertIn("BAL-NAME-1", cmake_fails.get(
            "bal/include/motion/motion_helper.h", set()
        ))
        self.assertIn("BAL-NAME-1", cmake_fails.get(
            "bal/include/motion/motion_controller.h", set()
        ))
        self.assertIn("BAL-MATH-1", cmake_fails.get(
            "bal/include/math/leaky.h", set()
        ))
        self.assertIn("BAL-NAME-2", cmake_fails.get(
            "bal/include/motion/sonar_probe.h", set()
        ))
        self.assertIn("DAL-HDR-NO-HAL", cmake_fails.get(
            "dal/include/actuator/dal_leaky.h", set()
        ))

    def test_parity_good_tree_matches(self):
        self.assertEqual(run_cmake_replay(GOOD_ROOT), {})
        self.assertEqual(run_wink_lint_for_parity(GOOD_ROOT), {})


def _symdiff(
    a: dict[str, set[str]], b: dict[str, set[str]]
) -> tuple[dict[str, set[str]], dict[str, set[str]]]:
    only_a: dict[str, set[str]] = {}
    only_b: dict[str, set[str]] = {}
    keys = set(a) | set(b)
    for k in keys:
        sa, sb = a.get(k, set()), b.get(k, set())
        if sa - sb:
            only_a[k] = sa - sb
        if sb - sa:
            only_b[k] = sb - sa
    return only_a, only_b


if __name__ == "__main__":
    unittest.main()
