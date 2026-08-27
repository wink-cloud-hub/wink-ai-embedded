#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""MCS-51 Keil C51 dialect cleanup pass (Axis B, ADR-0070 / Spike-S2).

Transforms an UNMODIFIED Keil C51 user source (``.c``) into a C++ translation
unit (``.cpp``) in the build tree. The user source is never edited in place —
the cleaned copy is what the sandbox compiles (Spike-S2 §6).

M1 scope:
  * Rewrite ISR signatures ``void f(void) interrupt N [using M]`` -> ``WINK_ISR(N)``.
    All other dialect (sfr/sbit/code/xdata/main/...) is erased by REGX52.H at
    compile time, not by this pass.
  * Comment- and string-literal-aware: the ISR pattern is only matched inside
    real code, so a signature appearing in a comment or string is left untouched
    (Spike-S2 §4.5 residual closed).

Usage:
    python mcs51_cleanup.py <input.c> <output.cpp>
"""
import re
import sys

# Strict Keil ISR signature: void name( void | () ) interrupt N [using M].
# Non-void parameters are illegal for a Keil ISR and intentionally do NOT match
# (left for the compiler to reject) — Spike-S2 §4.5.
ISR_RE = re.compile(
    r"void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?"
)


def build_code_mask(source: str) -> str:
    """Return a copy with comments and string/char-literal contents blanked to
    spaces (newlines preserved), so regex offsets stay aligned with `source`
    but only real code can match."""
    mask = list(source)
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        # Line comment
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
                if source[i] != "\n":
                    mask[i] = " "
                i += 1
            continue
        # Block comment
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
        # String literal
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
                i += 1
            continue
        # Char literal
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
                i += 1
            continue
        i += 1
    return "".join(mask)


def cleanup(source: str) -> tuple[str, int]:
    """Return (cleaned_cpp_source, isr_rewrite_count)."""
    mask = build_code_mask(source)
    matches = list(ISR_RE.finditer(mask))
    if not matches:
        return source, 0

    out = []
    prev = 0
    for m in matches:
        out.append(source[prev:m.start()])
        vector = m.group(2)
        out.append(f"WINK_ISR({vector})")
        prev = m.end()
    out.append(source[prev:])
    return "".join(out), len(matches)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        sys.stderr.write("usage: mcs51_cleanup.py <input.c> <output.cpp>\n")
        return 2
    inp, outp = argv[1], argv[2]
    with open(inp, "r", encoding="utf-8") as f:
        source = f.read()
    cleaned, count = cleanup(source)
    with open(outp, "w", encoding="utf-8", newline="\n") as f:
        f.write(cleaned)
    print(f"[mcs51_cleanup] {inp} -> {outp}: {count} ISR signature(s) rewritten")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
