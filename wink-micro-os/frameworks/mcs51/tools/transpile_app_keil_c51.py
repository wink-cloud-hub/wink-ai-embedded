#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MCS-51 Keil C51 dialect transpile pass (Axis B, ADR-0070 / Task R4).
# Renamed from mcs51_cleanup.py (2026-09 canonical gate naming).

Transforms an UNMODIFIED Keil C51 user source (``.c``) into a C++ translation
unit (``.cpp``) for Tier 2 native simulation, or an SDCC C source for Tier 3 ISS.
The user source is never edited in place — the cleaned copy is what the sandbox compiles.

Features:
  * Preprocessor conditional tracking: `#if 0` and unselected branches are masked
    so disabled code (e.g. inactive ISRs) is never rewritten.
  * Target gating:
    - `--target=native` (default): rewrites ISRs to `WINK_ISR(N)`, normalizes MCU headers
      to `<wink_mcu.h>`, rewrites empty superloops to inject `_nop_()`, normalizes main
      signature, and rewrites delay call-sites (`delay_ms`/`delay_us`) to step-pumped
      runtime services unless locally defined.
    - `--target=sdcc`: rewrites `interrupt N` -> `__interrupt(N)`, `code` -> `__code`,
      and `_at_ 0xNN` -> `__at(0xNN)`. Strictly disables delay stubs, superloop injections,
      and native header substitutions.
  * Local Definition Guard: if `delay_ms` / `delay_us` has a local function body,
    call-site rewriting is skipped to prevent hijacking user implementations.
  * Comment- and string-literal-aware masking.
  * UTF-8 with GBK fallback decoding (transcode mode supported).

Usage:
    python transpile_app_keil_c51.py [--transcode] [--target=native|sdcc] <input.c> <output.cpp>
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcs51_manifest import cleanup_header_patterns  # noqa: E402  (stage6 S6-2)


def read_source(path: str) -> str:
    """Read a source file as text: UTF-8 first, GBK fallback (vendor fixtures)."""
    with open(path, "rb") as f:
        data = f.read()
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        # GB18030 is a superset of GBK and decodes every vendor fixture
        # byte sequence encountered (plain GBK rejects some extension chars
        # found in vendor extint.c/spi.c).
        try:
            return data.decode("gb18030")
        except UnicodeDecodeError:
            return data.decode("gbk", errors="replace")


# Strict Keil ISR signature: [static] void name( void | () ) interrupt N [using M].
# Vector can be a bare decimal integer or a symbolic macro (e.g. TMR0_VECTOR),
# optionally enclosed in parentheses (e.g. `interrupt (1)`).
# Register bank `using` can be an integer or a symbolic macro (e.g. `using BANK1`).
ISR_RE = re.compile(
    r"\b(?:static\s+)?void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(?:\(\s*([a-zA-Z0-9_]+)\s*\)|([a-zA-Z0-9_]+))(?:\s+using\s+([a-zA-Z0-9_]+))?"
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

# Legacy Keil C51 MCU register headers to normalize to <wink_mcu.h>.
# Supports optional relative paths (e.g. `inc/cms8s78xx.h`, `../reg52.h`).
# Restricts model matching so peripheral drivers (e.g. `cms8s_flash.h`,
# `stcuart.h`) are NOT touched.
# Stage6 S6-2 (CPL-16): the vendor-name alternation comes from
# tools/manifests/chips/*.yaml (headers.cleanup_alias_regex) — no family name
# is hardcoded here; only the standard Intel reg51/reg52/regx52 dialect stays.
MCU_HEADER_RE = re.compile(
    r'#\s*include\s*[<"](?:[^\r\n<">]*[/\\])?(?:'
    + "|".join(cleanup_header_patterns())
    + r')\.h[>"]',
    re.IGNORECASE,
)

# Normalize `int main(` to `void main(` for bare-metal MCS-51 entry ABI
MAIN_RE = re.compile(r"\bint\s+main\s*\(")

# Empty infinite loop regex (e.g. `while(1) { ; }`, `while(1) {}`, `while(1);`, `for(;;);`, `for(;;) {}`).
# In cooperative fiber simulation (ASYNCIFY), an empty infinite loop without SFR access
# or _nop_() never yields and never charges virtual time, starving timers and freezing execution.
# Injecting _nop_() charges functional microsteps and allows cooperative scheduling and catch-up.
EMPTY_SUPERLOOP_RE = re.compile(
    r"\b(?:while\s*\(\s*(?:1[uUlL]?|true|!0)\s*\)|for\s*\(\s*;\s*;\s*\))\s*(?:\{\s*(?:;\s*)*\}|;)"
)

# SDCC dialect mappings (Task R4)
SDCC_CODE_RE = re.compile(r"\bcode\b")
SDCC_XDATA_RE = re.compile(r"\bxdata\b")
SDCC_AT_RE = re.compile(r"\b_at_\s+((?:0x[0-9a-fA-F]+|\d+))\b")

# Delay function definitions and call sites (Task R4 & Task R5)
LOCAL_DELAY_DEF_RE = re.compile(
    r"\b(?:void|int|unsigned\s+int|uint16_t|uint8_t)\s+(delay_ms|delay_us|DelayMs|DelayUs)\s*\([^)]*\)\s*\{"
)
DELAY_CALL_RE = re.compile(r"\b(delay_ms|DelayMs|delay_us|DelayUs)\s*\(")


def is_do_while_tail(mask: str, while_pos: int) -> bool:
    """Check if the `while` at `while_pos` is the closing condition of a `do ... while` statement."""
    i = while_pos - 1
    while i >= 0 and mask[i].isspace():
        i -= 1
    if i < 0:
        return False
    if mask[i] == "}":
        depth = 1
        i -= 1
        while i >= 0 and depth > 0:
            if mask[i] == "}":
                depth += 1
            elif mask[i] == "{":
                depth -= 1
            i -= 1
        if depth != 0:
            return False
    else:
        while i >= 0 and mask[i] not in ";{}":
            i -= 1
    while i >= 0 and mask[i].isspace():
        i -= 1
    if i >= 1 and mask[i - 1:i + 1] == "do":
        if i - 2 < 0 or not (mask[i - 2].isalnum() or mask[i - 2] == "_"):
            return True
    return False


def mask_inactive_preprocessor_branches(mask: list[str]) -> None:
    """Scan code mask for preprocessor directives (#if 0, #ifdef, #else, etc.)
    and blank out inactive branches with spaces (preserving newlines)."""
    text = "".join(mask)
    n = len(text)
    # Stack entries: [parent_active, branch_taken, current_branch_active]
    stack: list[list[bool]] = []
    directive_re = re.compile(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b(.*)")

    pos = 0
    inactive_start = None

    while pos < n:
        next_nl = text.find("\n", pos)
        line_end = n if next_nl == -1 else next_nl
        next_pos = n if next_nl == -1 else next_nl + 1

        line = text[pos:line_end]
        m = directive_re.match(line)
        if m:
            directive = m.group(1)
            arg = m.group(2).strip()

            if stack and not stack[-1][2] and inactive_start is not None:
                for idx in range(inactive_start, pos):
                    if mask[idx] != "\n":
                        mask[idx] = " "
                inactive_start = None

            if directive in ("if", "ifdef", "ifndef"):
                parent_active = (len(stack) == 0 or stack[-1][2])
                if not parent_active:
                    stack.append([False, True, False])
                else:
                    if directive == "if":
                        is_false = bool(re.match(r"^(?:0[uUlL]?|false)\s*$", arg))
                        active = not is_false
                    else:
                        active = True
                    stack.append([True, active, active])
            elif directive == "elif":
                if stack:
                    parent_active, branch_taken, _ = stack[-1]
                    if not parent_active or branch_taken:
                        stack[-1][2] = False
                    else:
                        is_false = bool(re.match(r"^(?:0[uUlL]?|false)\s*$", arg))
                        if not is_false:
                            stack[-1][1] = True
                            stack[-1][2] = True
                        else:
                            stack[-1][2] = False
            elif directive == "else":
                if stack:
                    parent_active, branch_taken, _ = stack[-1]
                    if not parent_active or branch_taken:
                        stack[-1][2] = False
                    else:
                        stack[-1][1] = True
                        stack[-1][2] = True
            elif directive == "endif":
                if stack:
                    stack.pop()

            if stack and not stack[-1][2]:
                inactive_start = next_pos

        pos = next_pos

    if stack and not stack[-1][2] and inactive_start is not None:
        for idx in range(inactive_start, n):
            if mask[idx] != "\n":
                mask[idx] = " "


def build_code_mask(source: str) -> str:
    """Return a copy with comments, string/char-literals, and inactive preprocessor
    branches blanked to spaces (newlines preserved), keeping offsets 1:1 aligned with source."""
    mask = list(source)
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        # Line comment
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
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
            line_start = source.rfind("\n", 0, i)
            line_prefix = source[0 if line_start == -1 else line_start + 1:i].strip()
            if line_prefix.startswith("#") and "include" in line_prefix:
                i += 1
                while i < n and source[i] != '"':
                    if source[i] == "\n":
                        break
                    i += 1
                if i < n and source[i] == '"':
                    i += 1
                continue

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

    # Apply preprocessor conditional branch masking (#if 0, etc.)
    mask_inactive_preprocessor_branches(mask)

    return "".join(mask)


# sbit declarations (SDCC target only — native keeps the C++ WinkSbit proxy):
#   sbit NAME = 0x90;            (absolute bit address)
#   sbit NAME = P2^0;            (relative to a bit-addressable SFR)
# SDCC form: __sbit __at(bitaddr) NAME;  (bitaddr = SFR_base + bit)
SDCC_SBIT_ABS_RE = re.compile(r'\bsbit\s+([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;')
SDCC_SBIT_REL_RE = re.compile(r'\bsbit\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\^\s*(\d)\s*;')

# Base SFR addresses of the bit-addressable SFRs (bit-address space base).
# Standard 8052 + CMS8S alias names used in carrier sources.
BIT_ADDRESSABLE_SFR = {
    "P0": 0x80, "TCON": 0x88, "P1": 0x90, "SCON": 0x98, "SCON0": 0x98,
    "P2": 0xA0, "IE": 0xA8, "P3": 0xB0, "IP": 0xB8, "T2CON": 0xC8,
    "PSW": 0xD0, "ACC": 0xE0, "B": 0xF0,
}


def cleanup(source: str, target: str = "native") -> tuple[str, dict[str, int]]:
    """Clean source code according to the chosen target ('native' or 'sdcc').
    Returns (cleaned_source, counts_dict)."""
    mask = build_code_mask(source)
    regions: list[tuple[int, int, str, str]] = []
    counts = {"isr": 0, "header": 0, "loop": 0, "delay": 0, "sdcc": 0,
              "sbit_unresolved": 0}

    if target == "sdcc":
        # ── SDCC Target Mode (Tier 3 ISS) ───────────────────────────────────
        # 1. ISR rewrites: void f(void) __interrupt(N) [__using(M)]
        for m in ISR_RE.finditer(mask):
            fn_name = m.group(1)
            vector = m.group(2) or m.group(3)
            vector = KNOWN_VECTORS.get(vector, vector)
            using_bank = m.group(4)
            if using_bank:
                repl = f"void {fn_name}(void) __interrupt({vector}) __using({using_bank})"
            else:
                repl = f"void {fn_name}(void) __interrupt({vector})"
            regions.append((m.start(), m.end(), repl, "isr"))

        # 2. code -> __code
        for m in SDCC_CODE_RE.finditer(mask):
            regions.append((m.start(), m.end(), "__code", "sdcc"))

        # 2b. xdata -> __xdata (PLAN-20260912-SIM-FIDELITY: app-level XDATA
        # placement for the 16-slot slope ring and debounce state; the native
        # target erases xdata via REGX52.H, SDCC needs its __xdata spelling)
        for m in SDCC_XDATA_RE.finditer(mask):
            regions.append((m.start(), m.end(), "__xdata", "sdcc"))

        # 3. _at_ 0xNN -> __at(0xNN)
        for m in SDCC_AT_RE.finditer(mask):
            addr = m.group(1)
            regions.append((m.start(), m.end(), f"__at({addr})", "sdcc"))

        # 4. sbit declarations:
        #    relative form `sbit N = REG^b;` resolves REG's SFR base from
        #    the bit-addressable table; unknown REG is left untouched and
        #    counted as sbit_unresolved (gate headers may extend the table).
        for m in SDCC_SBIT_REL_RE.finditer(mask):
            name, reg, bit = m.group(1), m.group(2), int(m.group(3))
            base = BIT_ADDRESSABLE_SFR.get(reg)
            if base is not None:
                repl = f"__sbit __at(0x{base + bit:02X}) {name};"
                regions.append((m.start(), m.end(), repl, "sdcc"))
            else:
                counts["sbit_unresolved"] = counts.get("sbit_unresolved", 0) + 1
        #    absolute form `sbit N = 0xNN;`
        for m in SDCC_SBIT_ABS_RE.finditer(mask):
            name, addr = m.group(1), m.group(2)
            regions.append((m.start(), m.end(),
                            f"__sbit __at({addr}) {name};", "sdcc"))

    else:
        # ── Native Target Mode (Tier 2 C++ Sandbox) ────────────────────────
        # 1. ISR rewrites: WINK_ISR(N)
        for m in ISR_RE.finditer(mask):
            vector = m.group(2) or m.group(3)
            vector = KNOWN_VECTORS.get(vector, vector)
            rest = mask[m.end():].lstrip()
            if rest.startswith(";"):
                repl = f'extern "C" void wink_isr_vector_{vector}(void)'
            else:
                repl = f"WINK_ISR({vector})"
            regions.append((m.start(), m.end(), repl, "isr"))

        # 2. Legacy MCU Header normalizations
        for m in MCU_HEADER_RE.finditer(mask):
            regions.append((m.start(), m.end(), "#include <wink_mcu.h>", "header"))

        # 3. main signature normalizations (int main -> void main)
        for m in MAIN_RE.finditer(mask):
            regions.append((m.start(), m.end(), "void main(", "main"))
            open_brace = mask.find("{", m.end())
            if open_brace != -1:
                depth = 1
                i = open_brace + 1
                n = len(mask)
                while i < n and depth > 0:
                    if mask[i] == "{":
                        depth += 1
                    elif mask[i] == "}":
                        depth -= 1
                    i += 1
                if depth == 0:
                    close_brace = i - 1
                    body = mask[open_brace:close_brace]
                    for ret_m in re.finditer(r"\breturn\s+[^;]+;", body):
                        r_start = open_brace + ret_m.start()
                        r_end = open_brace + ret_m.end()
                        regions.append((r_start, r_end, "return;", "main_ret"))

        # 4. Empty super-loops: inject _nop_() to prevent fiber freeze
        for m in EMPTY_SUPERLOOP_RE.finditer(mask):
            match_str = m.group(0).lstrip()
            if match_str.startswith("while") and is_do_while_tail(mask, m.start()):
                continue
            regions.append((m.start(), m.end(), "while(1) { _nop_(); }", "loop"))

        # 5. Local Definition Guard & Delay Call-Site Stubs (Task R4 & Task R5)
        local_delays = set()
        for m in LOCAL_DELAY_DEF_RE.finditer(mask):
            func_name = m.group(1)
            local_delays.add(func_name)
            print(f"[transpile_app_keil_c51] Skipped locally-defined delay function: {func_name}")

        for m in DELAY_CALL_RE.finditer(mask):
            func_name = m.group(1)
            if func_name in local_delays:
                continue
            prefix = mask[:m.start()].rstrip()
            last_token = prefix.split()[-1] if prefix.split() else ""
            if last_token in ("void", "int", "char", "unsigned", "extern", "static", "uint16_t", "uint8_t"):
                continue

            if func_name in ("delay_ms", "DelayMs"):
                regions.append((m.start(), m.end(), "wink_mcs51_delay_ms(", "delay"))
            elif func_name in ("delay_us", "DelayUs"):
                regions.append((m.start(), m.end(), "wink_delay_us(", "delay"))

    if not regions:
        return source, counts

    regions.sort(key=lambda r: r[0])

    out = []
    prev = 0
    for start, end, repl, kind in regions:
        if start < prev:
            continue
        out.append(source[prev:start])
        out.append(repl)
        prev = end
        if kind in counts:
            counts[kind] += 1
    out.append(source[prev:])
    return "".join(out), counts


def main(argv: list[str]) -> int:
    transcode = False
    target = "native"
    args = []
    for a in argv[1:]:
        if a == "--transcode":
            transcode = True
        elif a.startswith("--target="):
            target = a.split("=", 1)[1]
        else:
            args.append(a)

    if len(args) != 2:
        sys.stderr.write("usage: transpile_app_keil_c51.py [--transcode] [--target=native|sdcc] <input> <output>\n")
        return 2

    inp, outp = args
    source = read_source(inp)
    if transcode:
        cleaned = source
        counts = {"isr": 0, "header": 0, "loop": 0, "delay": 0, "sdcc": 0}
    else:
        cleaned, counts = cleanup(source, target=target)

    with open(outp, "w", encoding="utf-8", newline="\n") as f:
        f.write(cleaned)

    if transcode:
        print(f"[transpile_app_keil_c51] {inp} -> {outp}: transcoded to UTF-8")
    else:
        info = [f"{counts['isr']} ISR"]
        if target == "native":
            info.extend([
                f"{counts['header']} MCU header(s)",
                f"{counts['loop']} loop(s)",
                f"{counts['delay']} delay(s)",
            ])
        else:
            info.append(f"{counts['sdcc']} SDCC dialect rewrite(s)")
        print(f"[transpile_app_keil_c51] {inp} -> {outp} ({target}): {', '.join(info)} rewritten")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
