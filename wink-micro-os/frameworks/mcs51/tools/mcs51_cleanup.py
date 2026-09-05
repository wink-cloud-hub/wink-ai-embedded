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

Encoding:
  Vendor SDK fixtures (e.g. the Cmsemicon StdDriver) are GBK-encoded with
  Chinese comments. The input is decoded as UTF-8 first and falls back to GBK,
  so a vendor .c normalizes to UTF-8 on output (the host /utf-8 and emcc
  builds both expect UTF-8). No hardcoded input charset is assumed for the
  project's own UTF-8 sources.

Vendor headers (e.g. StdDriver/inc/adc.h) are GBK as well and are #included
directly by the cleaned TU, so they must also be normalized to UTF-8 in the
build tree -- the `--transcode` mode copies a file byte-for-byte after the
UTF-8/GBK decode (no ISR rewrite), e.g.:

    python mcs51_cleanup.py --transcode <input.h> <output.h>

Usage:
    python mcs51_cleanup.py <input.c> <output.cpp>
"""
import re
import sys


def read_source(path: str) -> str:
    """Read a source file as text: UTF-8 first, GBK fallback (vendor fixtures)."""
    with open(path, "rb") as f:
        data = f.read()
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("gbk")

# Strict Keil ISR signature: void name( void | () ) interrupt N [using M].
# Vector can be a bare decimal integer or a symbolic macro (e.g. TMR0_VECTOR).
ISR_RE = re.compile(
    r"void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+([a-zA-Z0-9_]+)(?:\s+using\s+\d+)?"
)

# Standard and vendor symbolic interrupt vector names normalized to vector index
KNOWN_VECTORS = {
    "INT0_VECTOR": "0",
    "TMR0_VECTOR": "1",
    "INT1_VECTOR": "2",
    "TMR1_VECTOR": "3",
    "UART0_VECTOR": "4",
    "TMR2_VECTOR": "5",
    "P0EI_VECTOR": "7",
    "P1EI_VECTOR": "8",
    "P2EI_VECTOR": "9",
    "P3EI_VECTOR": "10",
    "ACMP_VECTOR": "14",
    "TMR3_VECTOR": "15",
    "TMR4_VECTOR": "16",
    "EPWM_VECTOR": "18",
    "ADC_VECTOR": "19",
    "WDT_VECTOR": "20",
    "I2C_VECTOR": "21",
    "SPI_VECTOR": "22",
    "LSE_SCM_VECTOR": "25",
    "LVD_VECTOR": "26",
}

# Legacy Keil C51 MCU register headers to normalize to <wink_mcu.h>
# Enables zero-touch migration for unmodified legacy user code.
MCU_HEADER_RE = re.compile(
    r'#\s*include\s*[<"](?:regx?5[12]|cms8s[0-9a-z]*|reg_cms[0-9a-z]*|stc[0-9a-z]*)\.h[>"]',
    re.IGNORECASE
)

# Normalize `int main(` to `void main(` for bare-metal MCS-51 entry ABI
MAIN_RE = re.compile(r"\bint\s+main\s*\(")

# Empty infinite loop regex (e.g. `while(1) { ; }`, `while(1) {}`, `while(1);`, `for(;;);`, `for(;;) {}`).
# In cooperative fiber simulation (ASYNCIFY), an empty infinite loop without SFR access
# or _nop_() never yields and never charges virtual time, starving timers and freezing execution.
# Injecting _nop_() charges functional microsteps and allows cooperative scheduling and catch-up.
EMPTY_SUPERLOOP_RE = re.compile(
    r"\b(?:while\s*\(\s*1\s*\)|for\s*\(\s*;\s*;\s*\))\s*(?:\{\s*;?\s*\}|;)"
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


def cleanup(source: str) -> tuple[str, int, int]:
    """Return (cleaned_cpp_source, isr_rewrite_count, header_rewrite_count)."""
    mask = build_code_mask(source)

    regions: list[tuple[int, int, str, str]] = []

    # 1. Collect ISR rewrites
    for m in ISR_RE.finditer(mask):
        vector = m.group(2)
        vector = KNOWN_VECTORS.get(vector, vector)
        rest = mask[m.end():]
        rest_stripped = rest.lstrip()
        if rest_stripped.startswith(";"):
            repl = f'extern "C" void wink_isr_vector_{vector}(void)'
        else:
            repl = f"WINK_ISR({vector})"
        regions.append((m.start(), m.end(), repl, "isr"))

    # 2. Collect Legacy MCU Header normalizations
    for m in MCU_HEADER_RE.finditer(mask):
        regions.append((m.start(), m.end(), "#include <wink_mcu.h>", "header"))

    # 3. Collect main signature normalizations (int main -> void main)
    for m in MAIN_RE.finditer(mask):
        regions.append((m.start(), m.end(), "void main(", "main"))

    # 4. Collect empty super-loops and inject _nop_() to prevent fiber freeze
    for m in EMPTY_SUPERLOOP_RE.finditer(mask):
        regions.append((m.start(), m.end(), "while(1) { _nop_(); }", "loop"))

    if not regions:
        return source, 0, 0, 0

    regions.sort(key=lambda r: r[0])

    out = []
    prev = 0
    isr_count = 0
    header_count = 0
    loop_count = 0
    for start, end, repl, kind in regions:
        out.append(source[prev:start])
        out.append(repl)
        prev = end
        if kind == "isr":
            isr_count += 1
        elif kind == "header":
            header_count += 1
        elif kind == "loop":
            loop_count += 1
    out.append(source[prev:])
    return "".join(out), isr_count, header_count, loop_count


def main(argv: list[str]) -> int:
    transcode = False
    args = argv[1:]
    if args and args[0] == "--transcode":
        transcode = True
        args = args[1:]
    if len(args) != 2:
        sys.stderr.write(
            "usage: mcs51_cleanup.py [--transcode] <input> <output>\n")
        return 2
    inp, outp = args
    source = read_source(inp)
    if transcode:
        cleaned, isr_c, hdr_c, loop_c = source, 0, 0, 0
    else:
        cleaned, isr_c, hdr_c, loop_c = cleanup(source)
    with open(outp, "w", encoding="utf-8", newline="\n") as f:
        f.write(cleaned)
    if transcode:
        print(f"[mcs51_cleanup] {inp} -> {outp}: transcoded to UTF-8")
    else:
        print(f"[mcs51_cleanup] {inp} -> {outp}: {isr_c} ISR, {hdr_c} MCU header(s), {loop_c} loop(s) rewritten")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
