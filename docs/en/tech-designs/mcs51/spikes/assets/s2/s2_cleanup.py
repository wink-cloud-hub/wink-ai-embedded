#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Spike-S2 CMake regex-cleanup Pass prototype (throwaway).

Transforms UNMODIFIED Keil C51 user source into a C++17-clean copy in the
build dir. The ONLY transform is interrupt-syntax cleanup; every other dialect
(sfr/sbit/code/xdata/...) is eaten by REGX52.H macros at compile time.

    void Timer0_ISR(void) interrupt 1 using 1 { ... }
        -> WINK_ISR(1) { ... }

WINK_ISR(n) expands to a full extern "C" vector function declarator (the user
function NAME is intentionally discarded — ISRs are registered by vector
number, never called by name), so the ENTIRE match (return type/name/params/
interrupt/using) is replaced.

Strict mode: only the exact Keil ISR signature matches; anything else is left
untouched. Output is written as .cpp so GCC/MSVC/emcc all compile it natively
as C++ with NO -x c++ / LANGUAGE CXX override needed.
"""
import re
import sys
import pathlib

# Match: void <name>([void]) interrupt <num> [using <bank>]
ISR_RE = re.compile(
    r"void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?"
)


def cleanup(src_text: str) -> tuple[str, int]:
    count = 0

    def repl(m: re.Match) -> str:
        nonlocal count
        count += 1
        return f"WINK_ISR({m.group(2)})"

    return ISR_RE.sub(repl, src_text), count


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: s2_cleanup.py <input.c> <output.cpp>", file=sys.stderr)
        return 2
    inp, outp = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    text = inp.read_text(encoding="utf-8")
    cleaned, n = cleanup(text)
    outp.parent.mkdir(parents=True, exist_ok=True)
    outp.write_text(cleaned, encoding="utf-8")
    print(f"[s2-cleanup] {inp.name} -> {outp} ({n} ISR definition(s) rewritten)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
