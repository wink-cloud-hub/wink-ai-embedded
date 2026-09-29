# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g2_pal_naming.py.
"""

from rules import g2_pal_naming


def test_pal_naming_clean_file(tmp_path):
    pal_dir = tmp_path / "pal"
    pal_dir.mkdir(parents=True, exist_ok=True)
    pal_file = pal_dir / "pal_clean.h"
    pal_file.write_text("void pal_gpio_set(int pin, int val);\n", encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["pal/pal_clean.h"],
    }
    findings = g2_pal_naming.run(context)
    assert len(findings) == 0


def test_pal_naming_forbidden_keyword_interception(tmp_path):
    pal_dir = tmp_path / "pal"
    pal_dir.mkdir(parents=True, exist_ok=True)
    pal_file = pal_dir / "pal_polluted.h"
    pal_file.write_text(
        "// Driver for ws2812 pixel led\n"
        "void pal_ws2812_send(void);\n",
        encoding="utf-8",
    )

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["pal/pal_polluted.h"],
    }
    findings = g2_pal_naming.run(context)
    assert len(findings) >= 1
    assert findings[0]["severity"] == "error"
    msgs = " ".join(f["message"] for f in findings)
    assert "ws2812" in msgs
