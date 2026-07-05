"""Golden-file regression test for app_codegen.

Runs codegen against ``golden_devkitc.json`` into a temp directory, then
compares each generated file to a checked-in expected file under
``golden_expected/``. When an expected file is missing, the test fails with
a message pointing at ``--regen-golden`` — this forces explicit human
acceptance of new output rather than silent creation.

Usage (repo root):
    python -m tools.codegen.tests.test_golden
"""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

# Ensure repo root is importable when run as ``python path/to/test_golden.py``.
_HERE = Path(__file__).resolve().parent
_REPO_ROOT = _HERE.parent.parent.parent
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

from tools.codegen import app_codegen  # noqa: E402

GOLDEN_JSON = _HERE / "golden_devkitc.json"
GOLDEN_EXPECTED = _HERE / "golden_expected"


class GoldenTest(unittest.TestCase):
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
                    "regenerate with: python tools/codegen/app_codegen.py "
                    f"--config {GOLDEN_JSON} --out-dir {GOLDEN_EXPECTED}"
                )
                self.fail("\n".join(lines))


if __name__ == "__main__":
    unittest.main()
