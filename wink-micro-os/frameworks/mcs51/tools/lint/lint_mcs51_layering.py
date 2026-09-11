# SPDX-License-Identifier: Apache-2.0
# Stage0 (PLAN-20260911-MCS51-S0, CPL-24): core/vendor layering gate.
#
# Standalone equivalent of the `wink lint arch` layering pack (the wink.py
# engine wiring lands in stage6 with the tooling manifests; this script is
# the executable gate until then). Stdlib only; exit 0 = PASS, 1 = FAIL.
#
# Two checks:
#   1. Schema freeze (v2 fields + caps_cache snapshot + STRICT numbers).
#   2. Vendor-residue gate: core-owned files (include/mcs51_*.h,
#      include/wink_mcs51_*.h, src/mcs51_*.cpp) must not gain NEW
#      cms8s/0xF0xx/ADCLDO/FUNCCR/PS_ residue beyond BASELINE below.
#      Each baseline entry names the stage that removes it; later stages
#      only delete lines, so the gate stays green while residue shrinks.
from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve()
FW = HERE.parents[2]  # frameworks/mcs51
INC = FW / "include"
SRC = FW / "src"

# Vendor-residue patterns (core scope must be clean of these).
# familynam pins the stage2 extbus rename: family-specific *symbols* in
# generic code (classicBus/at89_/stc) fail; prose ("classic 8051") stays
# allowed — classic IS the generic family, only its name in symbols is not.
PATTERNS = {
    "cms8s": re.compile(r"cms8s", re.IGNORECASE),
    "xsfr_addr": re.compile(r"0xF0[0-9A-Fa-f]{2}"),
    "adcldo": re.compile(r"ADCLDO"),
    "funccr": re.compile(r"FUNCCR"),
    "ps_sel": re.compile(r"\bPS_[A-Z0-9]+\b"),
    "familynam": re.compile(
        r"classicBus|ClassicBus|CLASSIC_BUS|Mcs51ClassicBus|"
        r"mcs51_classic_bus|at89_|AT89|stc89|STC89"),
}

# (file-rel, pattern-key, line-regex, removal-stage)
# Line-regexes pin the KNOWN pre-stage0 constructs; any new construct fails.
BASELINE: list[tuple[str, str, str, str]] = [
    # mcs51_adc.h -> stage1 (physical-pin rail + ADCLDO sink)
    ("include/mcs51_adc.h", "cms8s", r"CMS8S", "stage1"),
    ("include/mcs51_adc.h", "adcldo", r"ADCLDO", "stage1"),
    # mcs51_context.h -> stage2 (context purify; soc_priv + macro sink).
    # S2-1/S2-2 done: only the XRAM comment row + PS_ADET doc row below
    # remain by content (XRAM block + PS seeds sunk); entries pruned with
    # the lines.
    ("include/mcs51_context.h", "ps_sel", r"PS_ADET", "stage2"),
    # mcs51_family.h: family-id rows + schema docs are BY DESIGN (the
    # descriptor table is the single place allowed to name families).
    ("include/mcs51_family.h", "cms8s", r".*", "by-design"),
    ("include/mcs51_family.h", "familynam", r".*", "by-design"),
    ("include/mcs51_family.h", "cms8s", r".*", "by-design"),
    ("include/mcs51_family.h", "funccr", r"FUNCCR", "by-design"),
    ("include/mcs51_family.h", "ps_sel", r"PS_", "by-design"),
    # mcs51_peripheral.h comment -> stage4 (self-registration)
    ("include/mcs51_peripheral.h", "cms8s", r"cms8s_\*", "stage4"),
    # mcs51_sfr_map.h vendor block -> stage3 (header homing)
    ("include/mcs51_sfr_map.h", "xsfr_addr", r"0xF0", "stage3"),
    ("include/mcs51_sfr_map.h", "ps_sel", r"PS_", "stage3"),
    ("include/mcs51_sfr_map.h", "cms8s", r"cms8s", "stage3"),
    # mcs51_trap.h doc comments -> stage4
    ("include/mcs51_trap.h", "cms8s", r"CMS8S|cms8s_sys", "stage4"),
    # mcs51_xsfr_allowlist.h whole file -> stage5 (XSFR parametrize)
    ("include/mcs51_xsfr_allowlist.h", "cms8s", r".*", "stage5"),
    ("include/mcs51_xsfr_allowlist.h", "xsfr_addr", r"0xF", "stage5"),
    # wink_mcs51_* public headers -> stage3 (WDT hard export, UART remap
    # docs, extint mux docs, ISR width docs, clock/strict grouping notes)
    ("include/wink_mcs51_clock.h", "cms8s", r"CMS8S", "stage3"),
    ("include/wink_mcs51_extint.h", "cms8s", r"CMS8S78xx", "stage3"),
    ("include/wink_mcs51_extint.h", "xsfr_addr", r"0xF0C[01]", "stage3"),
    ("include/wink_mcs51_extint.h", "ps_sel", r"PS_INT", "stage3"),
    ("include/wink_mcs51_isr.h", "cms8s", r"CMS8S78xx", "stage5"),
    ("include/wink_mcs51_strict.h", "cms8s", r"CMS8S", "stage3"),
    ("include/wink_mcs51_uart.h", "funccr", r"FUNCCR", "stage4"),
    ("include/wink_mcs51_wdt.h", "cms8s",
     r"CMS8S78xx|cms8s_sys", "stage3"),
    # src/mcs51_adc.cpp comment -> stage1
    ("src/mcs51_adc.cpp", "cms8s", r"CMS8S78xx", "stage1"),
    # src/mcs51_adc0832.cpp comment -> stage3 (devices/ sink)
    ("src/mcs51_adc0832.cpp", "cms8s", r"CMS8S", "stage3"),
    # src/mcs51_bridge.cpp hard include + hard call -> stage3
    ("src/mcs51_bridge.cpp", "cms8s",
     r'#include "cms8s_adc\.h"|cms8s_sys_notify_sfr_write', "stage3"),
    # src/mcs51_clock.cpp comment -> stage4
    ("src/mcs51_clock.cpp", "cms8s", r"CMS8S78xx", "stage4"),
    # src/mcs51_context.cpp: WINK_MCU_* build routing is by-design;
    # ADCLDO seeding + PS_* seeds -> stage2.
    ("src/mcs51_context.cpp", "cms8s",
     r"WINK_MCU_CMS8S78XX|MCS51_FAMILY_CMS8S78XX|CMS8S 24 MHz", "by-design"),
    ("src/mcs51_context.cpp", "familynam",
     r"WINK_MCU_AT89C52|CLASSIC", "by-design"),
    # S2-1 done: A-02 seeding lines (ADCLDO comment + 3000/3000) removed.
    ("src/mcs51_context.cpp", "ps_sel", r"PS_", "stage2"),
    # S3-1 transition (expires stage4): extint/timer/uart code still lives in
    # core but reads the sunk CMS8S_ addresses; the usages leave core WITH
    # the code when stage4 strips the models to chips/.
    ("src/mcs51_extint.cpp", "cms8s",
     r"CMS8S_|cms8s_sfr_map", "stage4"),
    ("src/mcs51_timer.cpp", "cms8s",
     r"CMS8S_|cms8s_sfr_map", "stage4"),
    ("src/mcs51_uart.cpp", "cms8s",
     r"CMS8S_|cms8s_sfr_map", "stage4"),
    ("src/mcs51_extint.cpp", "xsfr_addr", r"0xF08|0xF09", "stage4"),
    ("src/mcs51_extint.cpp", "cms8s", r"cms8s_sys", "stage4"),
    ("src/mcs51_extint.cpp", "ps_sel", r"PS_RESET|XSFR_PS_|MCS51_XSFR_PS_",
     "stage4"),
    # src/mcs51_family.cpp descriptor rows + refs -> by-design
    ("src/mcs51_family.cpp", "cms8s", r".*", "by-design"),
    ("src/mcs51_family.cpp", "xsfr_addr", r".*", "by-design"),
    ("src/mcs51_family.cpp", "familynam", r".*", "by-design"),
    # extbus rename (stage2, S2-1 done): family-named symbols in generic
    # code are gone; the two prose waivers below stay (prose is allowed).
    ("src/mcs51_xdata.cpp", "familynam",
     r"classic AT89C52", "prose"),
    ("src/mcs51_timer.cpp", "familynam",
     r"classic STC/AT89", "prose"),
    # src/mcs51_gpio.cpp TRIS/OD/CFG tables + has_cms8s_io -> stage4
    ("src/mcs51_gpio.cpp", "xsfr_addr", r"0xF0|0xF00", "stage4"),
    ("src/mcs51_gpio.cpp", "cms8s",
     r"mcs51_has_cms8s_io|CMS8S|cms8s", "stage4"),
    # src/mcs51_isr.cpp default map + comments -> stage5
    ("src/mcs51_isr.cpp", "cms8s",
     r"CMS8S78xx|cms8s78xx", "stage5"),
    # src/mcs51_peripheral.cpp cms8s_* table -> stage4
    ("src/mcs51_peripheral.cpp", "cms8s",
     r"cms8s|MCS51_FAMILY_MASK_CMS8S78XX", "stage4"),
    # src/mcs51_timer.cpp CKCON/W0C/PS_* -> stage4
    ("src/mcs51_timer.cpp", "cms8s", r"CMS8S78xx", "stage4"),
    ("src/mcs51_timer.cpp", "xsfr_addr", r"0xF0C6", "stage4"),
    ("src/mcs51_timer.cpp", "ps_sel", r"PS_T", "stage4"),
    # src/mcs51_uart.cpp FUNCCR/remap block -> stage4
    ("src/mcs51_uart.cpp", "xsfr_addr", r"0xF01|0xF02", "stage4"),
    ("src/mcs51_uart.cpp", "cms8s", r"CMS8S78xx", "stage4"),
    ("src/mcs51_uart.cpp", "funccr", r"FUNCCR|SFR_FUNCCR", "stage4"),
    ("src/mcs51_uart.cpp", "ps_sel", r"PS_RXD", "stage4"),
    # src/mcs51_xdata.cpp XSFR window model + comments -> stage5
    ("src/mcs51_xdata.cpp", "cms8s",
     r"CMS8S78xx|REG_CMS8S78XX", "stage5"),
    ("src/mcs51_xdata.cpp", "xsfr_addr",
     r"0xF000|0xF692", "stage5"),
    ("src/mcs51_xdata.cpp", "adcldo", r"ADCLDO", "stage5"),
]

# Schema-freeze assertions (name, file, regex).
SCHEMA_CHECKS = [
    ("cap MCS51_CAP_ENHANCED_IO", "include/mcs51_family.h",
     r"#define MCS51_CAP_ENHANCED_IO"),
    ("cap MCS51_CAP_TIMER34", "include/mcs51_family.h",
     r"#define MCS51_CAP_TIMER34"),
    ("cap MCS51_CAP_PORT_EXTINT", "include/mcs51_family.h",
     r"#define MCS51_CAP_PORT_EXTINT"),
    ("cap MCS51_CAP_UART_REMAP", "include/mcs51_family.h",
     r"#define MCS51_CAP_UART_REMAP"),
    ("timer caps T0..CAPTURE", "include/mcs51_family.h",
     r"#define MCS51_TIMER_CAP_CAPTURE"),
    ("field capabilities", "include/mcs51_family.h",
     r"capabilities;"),
    ("field port_pin_masks[4]", "include/mcs51_family.h",
     r"port_pin_masks\[4\]"),
    ("field irq_vector_table", "include/mcs51_family.h",
     r"irq_vector_table;"),
    ("field irq_count", "include/mcs51_family.h",
     r"irq_count;"),
    ("field wdt_present", "include/mcs51_family.h",
     r"wdt_present;"),
    ("field iap_present", "include/mcs51_family.h",
     r"iap_present;"),
    ("field uart_count", "include/mcs51_family.h",
     r"uart_count;"),
    ("field timer_caps", "include/mcs51_family.h",
     r"timer_caps;"),
    ("classic pins {8,8,8,8}", "src/mcs51_family.cpp",
     r"\{ 8u, 8u, 8u, 8u \}"),
    ("cms8s pins {8,8,6,4}", "src/mcs51_family.cpp",
     r"\{ 8u, 8u, 6u, 4u \}"),
    ("cms8s irq_count 28", "src/mcs51_family.cpp",
     r"28u,         // irq_count"),
    ("context caps_cache", "include/mcs51_context.h",
     r"caps_cache;"),
    ("snapshot caps_cache", "src/mcs51_context.cpp",
     r"ctx->caps_cache = d->capabilities;"),
    ("STRICT IAP_FLASH=11", "include/wink_mcs51_strict.h",
     r"MCS51_FEAT_IAP_FLASH\s+= 11,"),
    ("XDATA TODO(stage6)", "CMakeLists.txt",
     r"TODO\(stage6\)"),
]


def core_files() -> list[Path]:
    files: list[Path] = []
    for pat in ("mcs51_*.h", "wink_mcs51_*.h"):
        files += sorted(INC.glob(pat))
    files += sorted(SRC.glob("mcs51_*.cpp"))
    return files


def check_schema() -> list[str]:
    failures: list[str] = []
    for name, rel, rx in SCHEMA_CHECKS:
        text = (FW / rel).read_text(encoding="utf-8", errors="replace")
        if not re.search(rx, text):
            failures.append(f"schema: missing {name} ({rel} /{rx}/)")
    return failures


def check_residue() -> list[str]:
    base: dict[tuple[str, str], list[tuple[re.Pattern, str]]] = {}
    for rel, key, line_rx, stage in BASELINE:
        base.setdefault((rel, key), []).append(
            (re.compile(line_rx), stage))
    failures: list[str] = []
    for path in core_files():
        rel = path.relative_to(FW).as_posix()
        for i, line in enumerate(
                path.read_text(encoding="utf-8",
                               errors="replace").splitlines(), 1):
            for key, rx in PATTERNS.items():
                if not rx.search(line):
                    continue
                waived = any(w.search(line)
                             for w, _ in base.get((rel, key), []))
                if not waived:
                    failures.append(
                        f"residue: {rel}:{i} [{key}] {line.strip()[:100]}")
    return failures


def main() -> int:
    failures = check_schema() + check_residue()
    if failures:
        print(f"LAYER-GATE FAIL ({len(failures)}):")
        for f in failures:
            print(f"  {f}")
        return 1
    print("LAYER-GATE PASS: schema frozen, no new vendor residue.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
