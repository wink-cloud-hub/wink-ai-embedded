# SPDX-License-Identifier: GPL-3.0-only
"""ESP-IDF simulation interception safety pack (external lint pack, group ``esp_idf_all``).

Enforces architectural red lines from PLAN-20260923-ESP-IDF-SIM-M0:
    ESPIDF-RESOURCE-CLAIM       (error) Red Line 3: facade calling pal_resource_claim (ADR-0065)
    ESPIDF-RUNTIME-MALLOC       (error) Red Line 4: zero runtime dynamic memory allocation
    ESPIDF-FLOAT-PWM            (error) Red Line 5: forbidden float PWM duty (ADR-0066)
    ESPIDF-SPDX                 (error) Red Line 7: license compliance (ADR-0083/ADR-0084)
    ESPIDF-DOWNGRADE-UNLOGGED   (error) R-005 Floor: ESP_LOGE in drivers without matrix entry
"""
from __future__ import annotations

import re
from pathlib import Path

from tools.lint.engine.base import LintContext, register_pack
from tools.lint.engine.models import Finding

GROUP = "esp_idf_all"

_SPDX_RE = re.compile(r"SPDX-License-Identifier:\s*([A-Za-z0-9.\-+]+)")
_CLAIM_RE = re.compile(r"\bpal_resource_claim\s*\(")
_MALLOC_RE = re.compile(r"\b(malloc|free|calloc|realloc)\s*\(")
_FLOAT_PWM_RE = re.compile(r"\bpal_pwm_set_duty\s*\(")
_LOGE_RE = re.compile(r"\bESP_LOGE\s*\(")


def strip_c_comments_and_strings(source: str) -> str:
    """Mask comments and string/char literals with spaces (offsets and lines kept)."""
    mask = list(source)
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
                if source[i] != "\n":
                    mask[i] = " "
                i += 1
            continue
        if c == "/" and i + 1 < n and source[i + 1] == "*":
            mask[i] = " "
            mask[i + 1] = " "
            i += 2
            while i < n and not (source[i] == "*" and i + 1 < n and source[i + 1] == "/"):
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = " "
                mask[i + 1] = " "
                i += 2
            continue
        if c == '"':
            mask[i] = " "
            i += 1
            while i < n and source[i] != '"':
                if source[i] == "\\" and i + 1 < n:
                    mask[i] = " "
                    mask[i + 1] = " " if source[i + 1] != "\n" else "\n"
                    i += 2
                    continue
                mask[i] = "\n" if source[i] == "\n" else " "
                i += 1
            if i < n:
                mask[i] = " "
                mask[i + 1] = " "
                i += 1
            continue
        if c == "'":
            mask[i] = " "
            i += 1
            while i < n and source[i] != "'":
                if source[i] == "\\" and i + 1 < n:
                    mask[i] = " "
                    mask[i + 1] = " "
                    i += 2
                    continue
                mask[i] = " "
                i += 1
            if i < n:
                mask[i] = " "
                mask[i + 1] = " "
                i += 1
            continue
        i += 1
    return "".join(mask)


@register_pack("esp_idf_isolation", group=GROUP, default_enabled=False)
class EspIdfIsolationPack:
    """FilePack for ESP-IDF simulation interception layer safety and red lines."""

    def applies_to(self, rel: str, layer_id: str | None) -> bool:
        norm = rel.replace("\\", "/")
        if norm.startswith("docs/") or "/docs/" in norm:
            return False
        return "frameworks/esp_idf/" in norm or norm.startswith("frameworks/esp_idf/")

    def run_on_file(
        self,
        rel: str,
        text: str,
        layer_id: str | None,
        kind: str | None,
        ctx: LintContext,
    ) -> list[Finding]:
        findings: list[Finding] = []
        norm = rel.replace("\\", "/")

        def emit(line: int, rule_id: str, severity: str, message: str, help_text: str):
            findings.append(
                Finding(
                    rule_id=rule_id,
                    severity=severity,
                    path=rel,
                    line=line,
                    column=None,
                    message=message,
                    snippet=None,
                    help=help_text,
                    refs=("esp-idf-sim",),
                    allowlisted=False,
                    rule_source="sdk",
                )
            )

        # 1. License Check (ESPIDF-SPDX)
        m = _SPDX_RE.search(text)
        found_license = m.group(1) if m else None
        line_num = 1
        if m:
            line_num = text[: m.start()].count("\n") + 1

        is_c_or_py = norm.endswith((".c", ".h", ".cpp", ".hpp", ".py"))
        if is_c_or_py:
            if "/test/corpus/" in norm:
                # Upstream official corpus retains its original license (Public Domain / CC0-1.0)
                pass
            elif "/test/" in norm:
                if found_license != "GPL-3.0-only":
                    emit(
                        line_num,
                        "ESPIDF-SPDX",
                        "error",
                        f"File in test/ must carry 'SPDX-License-Identifier: GPL-3.0-only' (got '{found_license}').",
                        "Update SPDX identifier to GPL-3.0-only.",
                    )
            elif "/tools/" in norm and norm.endswith(".py"):
                if found_license != "GPL-3.0-only":
                    emit(
                        line_num,
                        "ESPIDF-SPDX",
                        "error",
                        f"Python tool must carry 'SPDX-License-Identifier: GPL-3.0-only' (got '{found_license}').",
                        "Update SPDX identifier to GPL-3.0-only.",
                    )
            elif any(sub in norm for sub in ("/src/", "/include/", "/chips/")):
                if found_license != "LGPL-3.0-only":
                    emit(
                        line_num,
                        "ESPIDF-SPDX",
                        "error",
                        f"Runtime file must carry 'SPDX-License-Identifier: LGPL-3.0-only' (got '{found_license}').",
                        "Update SPDX identifier to LGPL-3.0-only.",
                    )

        # Only C source files in src/** (excluding targets/ and osal/) check Red Lines 3, 4, 5
        if norm.endswith(".c") and "/src/" in norm and "/targets/" not in norm and "/osal/" not in norm:
            stripped = strip_c_comments_and_strings(text)
            lines = stripped.splitlines()
            for idx, line in enumerate(lines, start=1):
                # Red Line 3: No pal_resource_claim
                if _CLAIM_RE.search(line):
                    emit(
                        idx,
                        "ESPIDF-RESOURCE-CLAIM",
                        "error",
                        "Facade layer must not call pal_resource_claim() (ADR-0065 Red Line 3).",
                        "Delegate hardware resource lifecycle exclusively to underlying PAL.",
                    )

                # Red Line 4: Zero runtime dynamic heap allocation
                if _MALLOC_RE.search(line):
                    emit(
                        idx,
                        "ESPIDF-RUNTIME-MALLOC",
                        "error",
                        "Facade layer must not perform dynamic heap allocation (Red Line 4).",
                        "Use static pools or caller-provided buffers.",
                    )

                # Red Line 5: Forbidden floating point PWM
                if _FLOAT_PWM_RE.search(line):
                    emit(
                        idx,
                        "ESPIDF-FLOAT-PWM",
                        "error",
                        "Calling floating point pal_pwm_set_duty is forbidden (ADR-0066 Red Line 5).",
                        "Use pal_pwm_set_duty_bp() with integer basis points.",
                    )

        # R-005 Floor: Drivers containing ESP_LOGE must be recorded in 02-api-coverage-matrix.md downgrade table
        if norm.endswith(".c") and "/src/drivers/" in norm:
            if _LOGE_RE.search(text):
                # Check matrix file
                matrix_path = ctx.root / "frameworks" / "esp_idf" / "docs" / "02-api-coverage-matrix.md"
                matrix_content = ""
                if matrix_path.is_file():
                    matrix_content = matrix_path.read_text(encoding="utf-8", errors="replace")
                if "降级条目" not in matrix_content and "Downgrade" not in matrix_content:
                    emit(
                        1,
                        "ESPIDF-DOWNGRADE-UNLOGGED",
                        "error",
                        f"Driver '{rel}' logs errors via ESP_LOGE but downgrade registry in 02-api-coverage-matrix.md is empty.",
                        "Document error/degrade behavior in docs/02-api-coverage-matrix.md (ADR-0012).",
                    )

        return findings
