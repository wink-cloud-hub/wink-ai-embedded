# SPDX-License-Identifier: Apache-2.0
# Stage0 (PLAN-20260911-MCS51-S0, CPL-24): core/vendor layering gate.
# Renamed from lint_mcs51_layering.py (2026-09 canonical gate naming).
#
# Standalone equivalent of the `wink lint arch` layering pack (the wink.py
# engine wiring lands in stage6 with the tooling manifests; this script is
# the executable gate until then). Stdlib only; exit 0 = PASS, 1 = FAIL.
#
# Two checks:
#   1. Schema freeze (v2 fields + caps_cache snapshot + STRICT numbers).
#   2. Vendor-residue gate: core-owned files (include/mcs51_*.h,
#      include/wink_mcs51_*.h, src/mcs51_*.cpp) must not gain NEW vendor
#      residue beyond BASELINE below. Stage6 S6-2: the residue patterns come
#      from tools/manifests/chips/*.yaml (headers.forbid_in_core_regex); a
#      missing manifest fails the gate.
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

sys.path.insert(0, str(HERE.parents[1]))  # tools/ (manifest loader)
from mcs51_manifest import ManifestError, forbid_patterns  # noqa: E402

# Vendor-residue patterns (core scope must be clean of these).
# Stage6 S6-2 (CPL-16): family regex facts come from
# tools/manifests/chips/*.yaml (headers.forbid_in_core_regex) — no family name
# is hardcoded here. The stage2 extbus rename guard (classicBus/at89_/stc)
# lives in the at89c52 manifest; prose ("classic 8051") stays allowed via the
# BASELINE waiver rows below. Missing manifests are fail-fast: no facts, no
# verdict.
try:
    FAMILY_PATTERNS = [(family, re.compile(rx))
                       for family, rx in forbid_patterns()]
except ManifestError as exc:
    print(f"LAYER-GATE FAIL: chip manifests unavailable: {exc}")
    sys.exit(1)

# (file-rel, line-regex, removal-stage)
# Line-regexes pin the KNOWN pre-stage0 constructs; any new construct fails.
# HYGIENE IRON RULE (S3-H6): prune a row the moment its lines are gone;
# retag (never delete) rows whose lines roll to a later stage with the code.
# A stale row silently re-admits the exact regression the gate exists to
# catch; deleting a live row turns the gate red. Verify dead/alive with the
# family patterns above (word boundaries matter), not substrings.
BASELINE: list[tuple[str, str, str]] = [
    # mcs51_adc.h -> stage1 (physical-pin rail + ADCLDO sink)
    ("include/mcs51_adc.h", r"CMS8S", "stage1"),
    ("include/mcs51_adc.h", r"ADCLDO", "stage1"),
    # mcs51_context.h -> stage2 (context purify; soc_priv + macro sink).
    # S2-1/S2-2 done: only the XRAM comment row + PS_ADET doc row below
    # remain by content (XRAM block + PS seeds sunk); entries pruned with
    # the lines.
    ("include/mcs51_context.h", r"PS_ADET", "stage2"),
    # mcs51_family.h: family-id rows + schema docs are BY DESIGN (the
    # descriptor table is the single place allowed to name families).
    ("include/mcs51_family.h", r".*", "by-design"),
    # mcs51_family_route.h: 51-only routing table is BY DESIGN (the single
    # place inside frameworks/mcs51/ allowed to name families + vendor
    # shim headers — mirrors the mcs51_family.h descriptor treatment).
    ("include/mcs51_family_route.h", r".*", "by-design"),
    # sfr_map/trap/core-header scrubs (S3-H6/S4-C) are pruned with the lines:
    # re-adding a vendor name must fail, not be waived.
    # (stage7 pruned the mcs51_xsfr_allowlist.h shim waiver with the file.)
    # src/mcs51_adc.cpp comment -> stage1
    ("src/mcs51_adc.cpp", r"CMS8S78xx", "stage1"),
    # src/mcs51_bridge.cpp: the S4-D5 transitional family-register call was
    # removed in stage7 (link-time self-registration); the row is pruned with
    # the lines, so a vendor symbol must never reappear in the bridge.
    # src/mcs51_context.cpp: WINK_MCU_* build routing + sentinel tables are
    # by-design (seed values sunk in stage2: ADCLDO/PS_ rows pruned).
    ("src/mcs51_context.cpp",
     r"WINK_MCU_CMS8S78XX|MCS51_FAMILY_CMS8S78XX|CMS8S 24 MHz", "by-design"),
    ("src/mcs51_context.cpp", r"WINK_MCU_AT89C52|CLASSIC", "by-design"),
    # src/mcs51_family.cpp descriptor rows + refs -> by-design
    ("src/mcs51_family.cpp", r".*", "by-design"),
    # extbus rename (stage2, S2-1 done): family-named symbols in generic
    # code are gone; the two prose waivers below stay (prose is allowed).
    ("src/mcs51_xdata.cpp", r"classic AT89C52", "prose"),
    ("src/mcs51_timer.cpp", r"classic STC/AT89", "prose"),
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
    ("cap MCS51_CAP_CHIP_MODELS", "include/mcs51_family.h",
     r"#define MCS51_CAP_CHIP_MODELS"),
    ("pin validity accessor", "include/mcs51_family.h",
     r"mcs51_family_pin_valid"),
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
]

# Absence assertions (name, file, regex): S6-3 sank the XDATA aperture knob
# to board scope — a framework-level cache or compile definition must fail.
NEGATIVE_SCHEMA_CHECKS = [
    ("no XDATA framework cache", "CMakeLists.txt",
     r"set\(WINK_MCS51_XDATA_SIZE"),
    ("no XDATA framework define", "CMakeLists.txt",
     r"WINK_MCS51_XDATA_SIZE="),
]


def core_files() -> list[Path]:
    # Stage5 review follow-up (S5-H1): the gate must cover EVERY generic
    # include surface. The original glob skipped C++ proxy headers (*.hpp)
    # and the ABSACC shim, so vendor comment residue could hide there and
    # "delete waiver -> PASS" proved nothing for those files.
    files: list[Path] = []
    for pat in ("mcs51_*.h", "mcs51_*.hpp",
                "wink_mcs51_*.h", "wink_mcs51_*.hpp", "absacc.h"):
        files += sorted(INC.glob(pat))
    files += sorted(SRC.glob("mcs51_*.cpp"))
    return files


def check_schema() -> list[str]:
    failures: list[str] = []
    for name, rel, rx in SCHEMA_CHECKS:
        text = (FW / rel).read_text(encoding="utf-8", errors="replace")
        if not re.search(rx, text):
            failures.append(f"schema: missing {name} ({rel} /{rx}/)")
    for name, rel, rx in NEGATIVE_SCHEMA_CHECKS:
        text = (FW / rel).read_text(encoding="utf-8", errors="replace")
        if re.search(rx, text):
            failures.append(f"schema: forbidden {name} ({rel} /{rx}/)")
    return failures


def check_residue() -> list[str]:
    base: dict[str, list[tuple[re.Pattern, str]]] = {}
    for rel, line_rx, stage in BASELINE:
        base.setdefault(rel, []).append((re.compile(line_rx), stage))
    failures: list[str] = []
    for path in core_files():
        rel = path.relative_to(FW).as_posix()
        for i, line in enumerate(
                path.read_text(encoding="utf-8",
                               errors="replace").splitlines(), 1):
            for family, rx in FAMILY_PATTERNS:
                if not rx.search(line):
                    continue
                waived = any(w.search(line) for w, _ in base.get(rel, []))
                if not waived:
                    failures.append(
                        f"residue: {rel}:{i} [{family}] {line.strip()[:100]}")
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
