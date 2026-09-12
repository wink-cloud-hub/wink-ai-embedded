#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
gate_app_hardware_capacity.py — Tier-S SDCC compile gate for mcs51 carrier sources
# Renamed from mcs51_sdcc_gate.py (2026-09 canonical gate naming).
(GAP-03 implementation plan PLAN-20260910-GAP03-SDCC-GATE, Task 0/1).

For each application directory:
  1. reads wink-app.json and resolves the MCU family through the chip
     manifests (tools/manifests/chips/*.yaml; stage6 S6-2, CPL-16)
  2. runs transpile_app_keil_c51.py --target=sdcc on every top-level .c source
     (multi-TU vendor examples supported)
  3. transpiles the family's vendor Keil device header when the manifest
     declares one (vendor_device_header)
  4. compiles + LINKS with sdcc -mmcs51 and the manifest part limits
     (.mem is only produced at link time; SDCC does not know the part,
     so --code-size/--iram-size/--xram-size are mandatory)
  5. parses the .mem budget report and compares with the manifest limits

This gate does NOT imply Keil acceptance or flashability: it catches
dialect/SFR/type/interrupt/size errors with a real 8051 compiler. Keil
(Tier-K) remains the final capacity/toolchain authority.

Fail-fast (stage6): an unknown mcu or a missing chip manifest is an error —
budgets and vendor rules live only in tools/manifests/chips/*.yaml (an
omitted mcu still resolves to the historical at89c52 board default, through
its manifest).

Usage:
    python gate_app_hardware_capacity.py APP_DIR [APP_DIR ...] [--sdcc PATH]
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mcs51_manifest import ManifestError, manifest_for_mcu  # noqa: E402

# Repo root = parent of the wink-micro-os tree, found by name so the script
# survives MSYS/Git-Bash forward-slash paths and relocation (S6-H6a).
_MICRO_OS_DIR = next(
    (p for p in pathlib.Path(HERE).resolve().parents if p.name == "wink-micro-os"),
    None)
EMBEDDED_ROOT = (str(_MICRO_OS_DIR.parent) if _MICRO_OS_DIR is not None
                 else os.path.abspath(os.path.join(HERE, "..", "..", "..", "..")))

# GAP-25 coarse stack headroom: SDCC .mem "bytes available" (free internal
# RAM for the stack) must clear this floor. 32 B covers one ISR frame
# (ACC/PSW/B/DPL/DPH + a few locals) with margin; it is a tripwire, not a
# WCET proof — Keil overlay/IDATA packing (Tier-K) remains authoritative.
STACK_MIN_FREE = 32


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def app_manifest(app_dir):
    """(mcu, manifest) for the app's wink-app.json; fail-fast when unknown."""
    cfg = os.path.join(app_dir, "wink-app.json")
    mcu = "at89c52"
    if os.path.exists(cfg):
        with open(cfg, encoding="utf-8") as f:
            data = json.load(f)
        mcu = str(data.get("mcu", mcu)).lower()
    return mcu, manifest_for_mcu(mcu)


def repo_path(rel_path):
    return os.path.join(EMBEDDED_ROOT, rel_path)


def transpile_devhdr(vendor_devhdr, out_path):
    r = run([sys.executable,
             os.path.join(HERE, "mcs51_sdcc_devhdr.py"),
             vendor_devhdr, out_path])
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


def gate_app(app_dir, sdcc, stack_min):
    mcu, manifest = app_manifest(app_dir)
    fam = manifest["family"]
    gate = manifest["sdcc_gate"]
    budget = gate["mem_limits"]
    sources = sorted(f for f in os.listdir(app_dir) if f.endswith(".c"))
    if not sources:
        return False, "no top-level .c sources"
    work = tempfile.mkdtemp(prefix="mcs51_sdcc_")
    try:
        includes = [work,
                    app_dir,  # multi-TU vendor examples include local "xxx.h"
                    os.path.join(HERE, "sdcc_gate"),
                    os.path.join(HERE, "sdcc_gate", fam)]
        vendor_header = gate["vendor_device_header"]
        if vendor_header:
            vendor_header = repo_path(vendor_header)
            transpile_devhdr(vendor_header,
                             os.path.join(work,
                                          os.path.basename(vendor_header)))

        vendor_std = gate["vendor_stddriver_dir"]
        vendor_src_dir = None
        if gate["stddriver_link"] and vendor_std:
            vendor_std = repo_path(vendor_std)
            vendor_src_dir = os.path.join(vendor_std, "src")
            if os.path.isdir(os.path.join(vendor_std, "inc")):
                includes.append(os.path.join(vendor_std, "inc"))

        rels = []
        # Families declaring stddriver_link link against the UNMODIFIED vendor
        # StdDriver (Keil projects do the same): compile every vendor .c through
        # cleanup (GB18030 transcode + dialect rewrite) so BUZ_*/ADC_*/...
        # symbols resolve.
        compile_sources = list(sources)
        if vendor_src_dir and os.path.isdir(vendor_src_dir):
            compile_sources += sorted(
                f for f in os.listdir(vendor_src_dir) if f.endswith(".c"))
        for idx, src in enumerate(compile_sources):
            if src in sources:
                src_path = os.path.join(app_dir, src)
                out_base = src.replace(".c", "_sdcc.c")
            else:
                src_path = os.path.join(vendor_src_dir, src)
                out_base = f"vendor_{idx}_{src.replace('.c', '_sdcc.c')}"
            cleaned = os.path.join(work, out_base)
            r = run([sys.executable,
                     os.path.join(HERE, "transpile_app_keil_c51.py"), "--target=sdcc",
                     src_path, cleaned])
            if r.returncode != 0:
                return False, f"transpile failed for {src}: {r.stderr or r.stdout}"
            obj = out_base.replace(".c", ".rel")
            cmd = [sdcc, "-mmcs51"] + [f"-I{os.path.abspath(p)}" for p in includes] + \
                  ["-c", out_base, "-o", obj]
            r = run(cmd, cwd=work)
            if r.returncode != 0:
                return False, f"compile error in {src}:\n" + \
                              (r.stderr or r.stdout)[-1500:]
            rels.append(os.path.join(work, obj))

        link = [sdcc, "-mmcs51",
                f"--code-size", str(budget["code_max"]),
                f"--iram-size", str(budget["iram_max"])]
        if budget["xdata_max"] is not None:
            link += [f"--xram-size", str(budget["xdata_max"])]
        link += rels + ["-o", os.path.join(work, "app.ihx")]
        r = run(link, cwd=work)
        if r.returncode != 0:
            return False, "link/budget error:\n" + (r.stderr or r.stdout)[-1500:]

        rom, free_i = parse_mem(os.path.join(work, "app.mem"))
        detail = f"CODE={rom if rom is not None else '?'}B " \
                 f"(limit {budget['code_max']}), stack free={free_i}B"
        if rom is not None and rom > budget["code_max"]:
            return False, f"CODE over budget: {detail}"
        if free_i is not None and free_i < stack_min:
            return False, f"stack headroom {free_i}B < {stack_min}B floor: {detail}"
        return True, detail
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("apps", nargs="+")
    ap.add_argument("--sdcc", default=shutil.which("sdcc") or "sdcc")
    ap.add_argument("--stack-min", type=int, default=STACK_MIN_FREE,
                    help="minimum free internal-RAM bytes for the stack "
                         f"(default {STACK_MIN_FREE}; GAP-25 coarse floor)")
    args = ap.parse_args(argv[1:])

    failures = 0
    for app in args.apps:
        app_abs = os.path.abspath(app)
        try:
            mcu, _manifest = app_manifest(app_abs)
        except ManifestError as exc:
            print(f"[FAIL] {os.path.basename(app)}: {exc}")
            failures += 1
            continue
        ok, detail = gate_app(app_abs, args.sdcc, args.stack_min)
        tag = "PASS" if ok else "FAIL"
        print(f"[{tag}] {os.path.basename(app)} ({mcu}): {detail}")
        failures += 0 if ok else 1
    print(f"\n{len(args.apps) - failures}/{len(args.apps)} apps passed Tier-S SDCC gate")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
