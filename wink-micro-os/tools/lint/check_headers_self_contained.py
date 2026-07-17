#!/usr/bin/env python3
"""P1-B2: Header self-containment check for PAL/DAL public headers.

Every public header under `pal/include/` and `dal/include/` must compile as
the *first and only* `#include` in an empty translation unit. This catches
missing prerequisite includes (`<stdint.h>`, `<stdbool.h>`, `"wink_status.h"`,
etc.) that would otherwise silently rely on transitive inclusion at some
downstream caller — a footgun when a new consumer decides to include the
header directly.

Two probes per header:
  1. C mode   : `gcc  -fsyntax-only -std=c99   -x c   <tmp>.c`
  2. C++ mode : `g++  -fsyntax-only -std=c++11 -x c++ <tmp>.c`

The C++ probe verifies the `extern "C" { ... }` wrap is well-formed so the
header can be pulled into C++ TUs (e.g. tests, tooling) without link surprises.

Include paths mirror the target_include_directories() blocks in
`pal/CMakeLists.txt` and `dal/CMakeLists.txt`, so the "how a consumer would
see it" is faithfully reproduced.

Exit codes:
  0  all headers self-contained (or gcc not found — SKIPPED)
  1  one or more headers failed to compile standalone
  2  script usage / internal error

Flags:
  --verbose   print full compiler stderr on every failure
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile


# ── Repository layout ─────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))  # wink-micro-os/

# Header roots we scan. Every .h under these (recursively) is a check target.
HEADER_ROOTS = [
    os.path.join(REPO_ROOT, "pal", "include"),
    os.path.join(REPO_ROOT, "dal", "include"),
]

# Include search paths, mirroring pal/CMakeLists.txt + dal/CMakeLists.txt.
# Order matches how a downstream sees them (short-form includes like
# `#include "pal_hal.h"` must resolve because `pal/include/hal` is on -I).
INCLUDE_DIRS = [
    os.path.join(REPO_ROOT, "pal", "include"),
    os.path.join(REPO_ROOT, "pal", "include", "osal"),
    os.path.join(REPO_ROOT, "pal", "include", "hal"),
    os.path.join(REPO_ROOT, "pal", "include", "internal"),
    os.path.join(REPO_ROOT, "dal", "include"),
    os.path.join(REPO_ROOT, "dal", "include", "input"),
    os.path.join(REPO_ROOT, "dal", "include", "output"),
    os.path.join(REPO_ROOT, "dal", "include", "actuator"),
    os.path.join(REPO_ROOT, "dal", "include", "display"),
    os.path.join(REPO_ROOT, "dal", "include", "sensor"),
    os.path.join(REPO_ROOT, "dal", "include", "communication"),
    os.path.join(REPO_ROOT, "dal", "include", "storage"),
    os.path.join(REPO_ROOT, "runtime", "include"),
    os.path.join(REPO_ROOT, "trace", "include"),
    os.path.join(REPO_ROOT, "targets", "common", "include"),
]

# Explicit skip list. Headers here are intentionally NOT self-contained by
# design; we document the reason so the exception is discoverable.
#
# Keys are paths relative to REPO_ROOT (with forward slashes).
SKIP_HEADERS = {
    # ADR-0018: intentionally #error's unless WINK_ALLOW_ADVANCED_IRQ_APIS is
    # defined. That guard is the whole point of this header (physical
    # isolation of the restricted global-IRQ / SMP-sync API surface). Adding
    # the define here would defeat the check the header exists to enforce.
    "pal/include/pal_irq_advanced.h":
        "ADR-0018 guarded header: requires WINK_ALLOW_ADVANCED_IRQ_APIS",
}


# ── Helpers ───────────────────────────────────────────────────────────────────

def _is_private_header(name: str) -> bool:
    """Match common private-header conventions (currently none exist in-tree,
    but scanning defensively keeps the script honest as the codebase grows)."""
    stem = name[:-2] if name.endswith(".h") else name
    return (stem.endswith("_p") or
            stem.endswith("_priv") or
            stem.endswith("_private") or
            stem.endswith("_internal"))


def _find_headers():
    """Yield (abs_path, rel_from_include_root, rel_from_repo) for every
    public header under HEADER_ROOTS, honoring SKIP_HEADERS."""
    for root in HEADER_ROOTS:
        if not os.path.isdir(root):
            continue
        for dirpath, _dirnames, filenames in os.walk(root):
            for fname in filenames:
                if not fname.endswith(".h"):
                    continue
                if _is_private_header(fname):
                    continue
                abs_path = os.path.join(dirpath, fname)
                rel_from_root = os.path.relpath(abs_path, root).replace(os.sep, "/")
                rel_from_repo = os.path.relpath(abs_path, REPO_ROOT).replace(os.sep, "/")
                yield abs_path, rel_from_root, rel_from_repo


def _include_spelling(rel_from_root: str) -> str:
    """Pick how a consumer would `#include` this header.

    Because CMake exposes every subdir (`hal/`, `osal/`, `actuator/`, ...) on
    the -I search path (see pal/CMakeLists.txt + dal/CMakeLists.txt), the
    idiomatic spelling is the bare filename. Existing headers do exactly this
    (e.g. `#include "pal_hal.h"`, not `#include "hal/pal_hal.h"`). We follow
    the same convention here so the probe TU matches real-world callers."""
    return os.path.basename(rel_from_root)


def _build_probe_source(include_spelling: str) -> str:
    return (
        f'#include "{include_spelling}"\n'
        f"int main(void) {{ return 0; }}\n"
    )


def _compile_probe(compiler: str, lang_flag: str, std_flag: str,
                   probe_path: str) -> subprocess.CompletedProcess:
    """Run one syntax-only compile. Returns the CompletedProcess so the caller
    can inspect returncode + stderr."""
    cmd = [compiler, "-fsyntax-only", std_flag, "-x", lang_flag]
    for inc in INCLUDE_DIRS:
        cmd += ["-I", inc]
    cmd.append(probe_path)
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                          errors="replace")


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="print full compiler stderr on every failure")
    args = ap.parse_args()

    gcc = shutil.which("gcc")
    gxx = shutil.which("g++")
    if not gcc or not gxx:
        # Follow the run-tests.ps1 pattern: soft-skip rather than fail hard
        # when the toolchain isn't installed. CI images ship gcc; contributor
        # machines without it just don't run this specific check.
        print("[SKIP] gcc/g++ not found on PATH — header self-containment "
              "check skipped.")
        print("       Install WinLibs MinGW (or any gcc >= 9) to enable this "
              "lint locally.")
        return 0

    headers = sorted(_find_headers(), key=lambda t: t[2])
    if not headers:
        print("[FAIL] no headers discovered under pal/include or dal/include",
              file=sys.stderr)
        return 2

    print(f"[info] scanning {len(headers)} headers under pal/include, "
          f"dal/include")
    if SKIP_HEADERS:
        print(f"[info] skipping {len(SKIP_HEADERS)} intentionally guarded "
              f"header(s):")
        for k, why in SKIP_HEADERS.items():
            print(f"       - {k}  ({why})")

    failures = []  # list of (rel_path, mode, stderr)
    checked = 0
    skipped = 0

    # One temp dir for all probes; the .c file gets rewritten per header. We
    # use a stable filename inside the temp dir so gcc error output shows the
    # same TU path across probes — cleaner reporting than tempfile.mkstemp per
    # header (which would spam the report with random names).
    with tempfile.TemporaryDirectory(prefix="wink_hdr_probe_") as tmpdir:
        probe_path = os.path.join(tmpdir, "probe_tu.c")

        for abs_path, rel_from_root, rel_from_repo in headers:
            if rel_from_repo in SKIP_HEADERS:
                skipped += 1
                continue

            include_stmt = _include_spelling(rel_from_root)
            source = _build_probe_source(include_stmt)
            with open(probe_path, "w", encoding="utf-8", newline="\n") as f:
                f.write(source)

            checked += 1

            # C mode
            res_c = _compile_probe(gcc, "c", "-std=c99", probe_path)
            # C++ mode
            res_cxx = _compile_probe(gxx, "c++", "-std=c++11", probe_path)

            c_ok = (res_c.returncode == 0)
            cxx_ok = (res_cxx.returncode == 0)

            if c_ok and cxx_ok:
                print(f"  [ok]   {rel_from_repo}")
            else:
                tags = []
                if not c_ok:
                    tags.append("C")
                    failures.append((rel_from_repo, "C", res_c.stderr))
                if not cxx_ok:
                    tags.append("C++")
                    failures.append((rel_from_repo, "C++", res_cxx.stderr))
                print(f"  [FAIL] {rel_from_repo}  ({'+'.join(tags)} probe "
                      f"failed)")
                if args.verbose:
                    if not c_ok:
                        print("  --- C stderr ---")
                        print(res_c.stderr.rstrip())
                    if not cxx_ok:
                        print("  --- C++ stderr ---")
                        print(res_cxx.stderr.rstrip())

    print("")
    print("=" * 72)
    if failures:
        print(f"[FAIL] {len(failures)} probe failure(s) across "
              f"{checked} headers ({skipped} skipped).")
        if not args.verbose:
            # Always show at least one line of context per failure so the CI
            # log is diagnostic even without -v.
            print("       (re-run with --verbose for full compiler output)")
            print("")
            for rel, mode, stderr in failures:
                # First non-empty stderr line usually holds the fatal:
                first = next((ln for ln in stderr.splitlines() if ln.strip()),
                             "(no compiler output)")
                print(f"  {rel} [{mode}]: {first}")
        print("=" * 72)
        return 1
    else:
        print(f"[OK] {checked} headers self-contained "
              f"(C + C++ probes pass, {skipped} skipped by design)")
        print("=" * 72)
        return 0


if __name__ == "__main__":
    sys.exit(main())
