#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
mcs51_sdcc_gate.py — Tier-S SDCC compile gate for mcs51 carrier sources
(GAP-03 implementation plan PLAN-20260910-GAP03-SDCC-GATE, Task 0/1).

For each application directory:
  1. reads wink-app.json to select the MCU family
  2. runs mcs51_cleanup.py --target=sdcc on every top-level .c source
     (multi-TU vendor examples supported)
  3. transpiles the vendor Keil device header for CMS8S-family apps
  4. compiles + LINKS with sdcc -mmcs51 and the device size limits
     (.mem is only produced at link time; SDCC does not know the part,
     so --code-size/--iram-size/--xram-size are mandatory)
  5. parses the .mem budget report and compares with the part limits

This gate does NOT imply Keil acceptance or flashability: it catches
dialect/SFR/type/interrupt/size errors with a real 8051 compiler. Keil
(Tier-K) remains the final capacity/toolchain authority.

Usage:
    python mcs51_sdcc_gate.py APP_DIR [APP_DIR ...] [--sdcc PATH]
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
EMBEDDED_ROOT = HERE.split(os.sep + "wink-micro-os" + os.sep)[0]
VENDOR_BASE = os.path.join(
    EMBEDDED_ROOT,
    "docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Demo/Libary")
VENDOR_DEVHDR = os.path.join(VENDOR_BASE, "Device/CMS8S78xx/Include/cms8s78xx.h")
VENDOR_STDRIVER_INC = os.path.join(VENDOR_BASE, "StdDriver/inc")

# Part budgets (bytes): code (FLASH), iram (DATA+IDATA), xram.
BUDGETS = {
    "cms8s78xx": {"code": 16384, "iram": 256, "xram": 1024},
    "at89c52":   {"code": 8192,  "iram": 256, "xram": None},  # external MOVX, part-agnostic
}
FAMILY_BY_MCU = {
    "cms8s78xx": "cms8s78xx",
    "at89c52": "at89c52",
    "stc89c52": "at89c52",
}


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def app_family(app_dir):
    cfg = os.path.join(app_dir, "wink-app.json")
    mcu = "at89c52"
    if os.path.exists(cfg):
        data = json.load(open(cfg, encoding="utf-8"))
        mcu = str(data.get("mcu", mcu)).lower()
    fam = FAMILY_BY_MCU.get(mcu)
    if fam is None:
        print(f"  WARNING: unknown mcu '{mcu}', defaulting to at89c52")
        return "at89c52", mcu
    return fam, mcu


def transpile_devhdr(out_path):
    r = run([sys.executable,
             os.path.join(HERE, "mcs51_sdcc_devhdr.py"),
             VENDOR_DEVHDR, out_path])
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        raise RuntimeError("device header transpile failed")


def parse_mem(mem_path):
    """Return (rom_bytes, internal stack free bytes) from SDCC .mem."""
    text = open(mem_path, encoding="utf-8", errors="replace").read()
    rom = None
    m = re.search(r"ROM/EPROM/FLASH\s+0x[0-9A-Fa-f]+\s+0x[0-9A-Fa-f]+\s+(\d+)",
                  text)
    if m:
        rom = int(m.group(1))
    free_i = None
    m = re.search(r"(\d+) bytes available", text)
    if m:
        free_i = int(m.group(1))
    return rom, free_i


def gate_app(app_dir, sdcc):
    fam, mcu = app_family(app_dir)
    sources = sorted(f for f in os.listdir(app_dir) if f.endswith(".c"))
    if not sources:
        return False, "no top-level .c sources"
    work = tempfile.mkdtemp(prefix="mcs51_sdcc_")
    try:
        includes = [work,
                    app_dir,  # multi-TU vendor examples include local "xxx.h"
                    os.path.join(HERE, "sdcc_gate"),
                    os.path.join(HERE, "sdcc_gate", fam)]
        if fam == "cms8s78xx":
            transpile_devhdr(os.path.join(work, "cms8s78xx.h"))
            includes.append(VENDOR_STDRIVER_INC)

        rels = []
        # CMS8S links against the UNMODIFIED vendor StdDriver (Keil projects
        # do the same): compile every vendor .c through cleanup (GB18030
        # transcode + dialect rewrite) so BUZ_*/ADC_*/... symbols resolve.
        compile_sources = list(sources)
        if fam == "cms8s78xx" and os.path.isdir(
                os.path.join(VENDOR_BASE, "StdDriver/src")):
            compile_sources += sorted(
                f for f in os.listdir(os.path.join(VENDOR_BASE, "StdDriver/src"))
                if f.endswith(".c"))
        vendor_src_dir = os.path.join(VENDOR_BASE, "StdDriver/src")
        for idx, src in enumerate(compile_sources):
            if src in sources:
                src_path = os.path.join(app_dir, src)
                out_base = src.replace(".c", "_sdcc.c")
            else:
                src_path = os.path.join(vendor_src_dir, src)
                out_base = f"vendor_{idx}_{src.replace('.c', '_sdcc.c')}"
            cleaned = os.path.join(work, out_base)
            r = run([sys.executable,
                     os.path.join(HERE, "mcs51_cleanup.py"), "--target=sdcc",
                     src_path, cleaned])
            if r.returncode != 0:
                return False, f"cleanup failed for {src}: {r.stderr or r.stdout}"
            obj = out_base.replace(".c", ".rel")
            cmd = [sdcc, "-mmcs51"] + [f"-I{os.path.abspath(p)}" for p in includes] + \
                  ["-c", out_base, "-o", obj]
            r = run(cmd, cwd=work)
            if r.returncode != 0:
                return False, f"compile error in {src}:\n" + \
                              (r.stderr or r.stdout)[-1500:]
            rels.append(os.path.join(work, obj))

        budget = BUDGETS[fam]
        link = [sdcc, "-mmcs51",
                f"--code-size", str(budget["code"]),
                f"--iram-size", str(budget["iram"])]
        if budget["xram"] is not None:
            link += [f"--xram-size", str(budget["xram"])]
        link += rels + ["-o", os.path.join(work, "app.ihx")]
        r = run(link, cwd=work)
        if r.returncode != 0:
            return False, "link/budget error:\n" + (r.stderr or r.stdout)[-1500:]

        rom, free_i = parse_mem(os.path.join(work, "app.mem"))
        detail = f"CODE={rom if rom is not None else '?'}B " \
                 f"(limit {budget['code']}), stack free={free_i}B"
        if rom is not None and rom > budget["code"]:
            return False, f"CODE over budget: {detail}"
        return True, detail
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("apps", nargs="+")
    ap.add_argument("--sdcc", default=shutil.which("sdcc") or "sdcc")
    args = ap.parse_args(argv[1:])

    failures = 0
    for app in args.apps:
        ok, detail = gate_app(os.path.abspath(app), args.sdcc)
        fam, mcu = app_family(os.path.abspath(app))
        tag = "PASS" if ok else "FAIL"
        print(f"[{tag}] {os.path.basename(app)} ({mcu}): {detail}")
        failures += 0 if ok else 1
    print(f"\n{len(args.apps) - failures}/{len(args.apps)} apps passed Tier-S SDCC gate")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
