"""Arduino isolation pack (ADR-0035) — SSOT for ``wink lint --pack arduino``.

``tools/lint/check_arduino_isolation.py`` is a thin CLI shim over this module.
"""
from __future__ import annotations

import re
from pathlib import Path

from tools.lint.engine.models import Finding

ISOLATED_DIRS = ("pal", "dal", "trace", "runtime", "targets", "osal")

FORBIDDEN_PATTERNS = [
    (
        re.compile(r'#include\s*[<"](?:api/)?Arduino(?:API)?\.h[>"]'),
        "Forbidden Arduino.h or ArduinoAPI.h include",
    ),
    (
        re.compile(r'#include\s*[<"](?:api/)?Hardware(?:Serial|I2C|SPI|CAN)\.h[>"]'),
        "Forbidden Hardware bus headers",
    ),
    (re.compile(r"\bTwoWire\b"), "Forbidden TwoWire type reference"),
    (re.compile(r"\bHardwareSerial\b"), "Forbidden HardwareSerial type reference"),
    (re.compile(r"\bHardwareSPI\b"), "Forbidden HardwareSPI type reference"),
]


def check_arduino_isolation(root: Path) -> list[Finding]:
    """Scan kernel dirs under ``root`` for Arduino C++ bleed (ADR-0035)."""
    findings: list[Finding] = []
    for folder in ISOLATED_DIRS:
        folder_path = root / folder
        if not folder_path.is_dir():
            continue
        for path in folder_path.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".c", ".h"}:
                continue
            rel = path.relative_to(root).as_posix()
            findings.extend(_scan_file(rel, path))
    return findings


def _scan_file(rel: str, path: Path) -> list[Finding]:
    out: list[Finding] = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return out
    for line_no, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("/*"):
            continue
        for pattern, desc in FORBIDDEN_PATTERNS:
            if pattern.search(line):
                out.append(
                    Finding(
                        rule_id="ARDUINO-ISOLATION",
                        severity="error",
                        path=rel,
                        line=line_no,
                        column=None,
                        message=desc,
                        snippet=stripped,
                        help="ADR-0035: core kernel must not depend on Arduino C++ types/headers",
                        refs=("ADR-0035",),
                        allowlisted=False,
                        rule_source="sdk",
                    )
                )
    return out
