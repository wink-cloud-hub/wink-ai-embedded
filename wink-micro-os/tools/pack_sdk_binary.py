#!/usr/bin/env python3
"""Pack wink-micro-os as a Phase-2 Binary SDK tarball (host target).

Builds the OS in SOURCE mode, merges all component static libraries +
pal_host objects into a single libwink_micro_os.a, copies the public
header whitelist into include/, and produces a .tar.gz.

Usage:
    python tools/pack_sdk_binary.py [--build-dir BUILD_DIR] [--out-dir OUT_DIR]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

SDK_ROOT = Path(__file__).resolve().parent.parent

BINARY_PACK_GCC_C_FLAGS = (
    "-DWINK_MAX_SOFT_TIMERS=32 "
    "-DPAL_PWM_CHANNELS=16 "
    "-ffunction-sections "
    "-fdata-sections"
)
BINARY_PACK_MSVC_C_FLAGS = (
    "/DWINK_MAX_SOFT_TIMERS=32 "
    "/DPAL_PWM_CHANNELS=16 "
    "/Gy"
)

# ── Public header whitelist ──────────────────────────────────────────────────
# Each entry: (source_relative_to_SDK_ROOT, destination_relative_to_include_root)
# DAL and BAL headers preserve their subdirectory structure.

PAL_PUBLIC_HEADERS = [
    "pal/include/wink_status.h",
    "pal/include/pal_log.h",
    "pal/include/pal.h",
    "pal/include/osal/pal_osal.h",
    "pal/include/pal_irq.h",
]

RUNTIME_PUBLIC_HEADERS = [
    "runtime/include/wink_app.h",
    "runtime/include/wink_runtime.h",
    "runtime/include/wink_event.h",
    "runtime/include/wink_tasks.h",
    "runtime/include/wink_soft_timer.h",
    "runtime/include/wink_actuator_registry.h",
    "runtime/include/wink_fault.h",
    "runtime/include/wink_log.h",
    "runtime/include/wink_blocking_region.h",
    "runtime/include/wink_selftest.h",
]

TRACE_PUBLIC_HEADERS = [
    "trace/include/wink_trace.h",
]

# DAL and BAL: copy entire include/ trees (all public headers by convention).
DAL_INCLUDE_ROOT = "dal/include"
BAL_INCLUDE_ROOT = "bal/include"

# ── Files/dirs to copy verbatim into the binary SDK ──────────────────────────
SDK_BRIDGE_FILES = [
    "targets/host/wink_binary_import.cmake",
]

SDK_WASM_BRIDGE_FILES = [
    "targets/wasm/wink_binary_import.cmake",
    "targets/wasm/wink_sim_js.js",
    "targets/wasm/exported_runtime_functions.json",
]

# Source file basenames compiled into the wasm PAL (targets/wasm/ + targets/common/src/).
# Used to locate .o files in the build tree for archive merging.
_WASM_PAL_SOURCE_BASENAMES = [
    # targets/wasm/
    "pal_hal_wasm.c", "pal_log_wasm.c", "pal_irq_wasm.c",
    "pal_osal_wasm.c", "pal_storage_wasm.c",
    "pal_wasm_physical.c", "pal_wasm_fault.c", "pal_wasm_fault_domain.c",
    "sim_ctx_emscripten_fiber.c",
    # targets/common/src/
    "pal_osal_ringbuf.c", "pal_resource.c",
    "wink_sim_physical.c", "wink_sim_scheduler.c",
]

SDK_TOOL_ROOTS = [
    "tools/wink.py",
    "tools/__init__.py",
    "tools/codegen",
    "tools/lint",
]

# pal_host .obj files that duplicate symbols already in component .a files.
# wink_dev_config.c is in libwink_runtime.a; pal_pwm_router.c is in pal_common
# (which is compiled into pal_host).  Skip these when merging to avoid
# duplicate symbol warnings.
_DUPLICATE_OBJ_NAMES = {"wink_dev_config.c.obj", "wink_dev_config.c.o"}

# Evidence that build_host configured the binary-pack ABI ceilings and section splitting.
_BINARY_PACK_REQUIRED_DEFINES = (
    "WINK_MAX_SOFT_TIMERS=32",
    "PAL_PWM_CHANNELS=16",
)
_BINARY_PACK_GCC_SECTION_FLAGS = ("-ffunction-sections", "-fdata-sections")
_BINARY_PACK_MSVC_SECTION_FLAG = "/Gy"
_BINARY_PACK_WINK_CONFIG_CEILINGS = {
    "WINK_MAX_SOFT_TIMERS": 32,
    "PAL_PWM_CHANNELS": 16,
}
# Matches config_h.py output: #define MACRO    (32U)
_BINARY_PACK_DEFINE_RE = re.compile(
    r"^#define\s+(\w+)\s+\((\d+)U\)\s*$",
    re.MULTILINE,
)


def create_binary_pack_stub_app(parent_dir: Path) -> Path:
    """Create the app config used to compile Binary SDK archives."""
    stub_dir = parent_dir / "binary_pack_stub_app"
    stub_dir.mkdir(parents=True, exist_ok=True)
    config = {
        "app_name": "binary_pack_stub",
        "board": "esp32_devkitc",
        "tick_ms": 10,
        "max_soft_timers": 32,
        "pwm_channels": 16,
        "devices": {},
    }
    (stub_dir / "wink-app.json").write_text(
        json.dumps(config, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (stub_dir / "CMakeLists.txt").write_text(
        "# Binary SDK pack stub: config-only app for pack-time ceilings.\n",
        encoding="utf-8",
    )
    return stub_dir


def read_version(sdk_root: Path) -> tuple[str, str]:
    version_file = sdk_root / "VERSION"
    if not version_file.is_file():
        raise SystemExit(f"[pack-binary] VERSION not found: {version_file}")
    lines = [ln.strip() for ln in version_file.read_text(encoding="utf-8").splitlines() if ln.strip()]
    if not lines:
        raise SystemExit("[pack-binary] VERSION is empty")
    ver = lines[0]
    abi = "0"
    for ln in lines[1:]:
        if ln.startswith("ABI="):
            abi = ln.split("=", 1)[1].strip()
    return ver, abi


def detect_toolchain() -> dict[str, str]:
    """Detect available toolchain versions for manifest pinning."""
    info: dict[str, str] = {}
    info["python"] = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    info["platform"] = platform.system()

    for tool, flag in [("gcc", "--version"), ("cmake", "--version")]:
        try:
            result = subprocess.run(
                [tool, flag],
                capture_output=True,
                text=True,
                timeout=10,
            )
            first_line = result.stdout.strip().splitlines()[0] if result.stdout.strip() else ""
            if first_line:
                info[tool] = first_line
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            pass

    return info


def hash_file(path: Path) -> str:
    """Compute SHA-256 hex digest of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def compute_content_hashes(staging: Path) -> dict[str, str]:
    """Compute SHA-256 for every file in the staging tree, keyed by relative posix path."""
    hashes: dict[str, str] = {}
    for p in staging.rglob("*"):
        if p.is_file():
            rel = p.relative_to(staging).as_posix()
            hashes[rel] = hash_file(p)
    return hashes


def _collect_cmake_cache_flag_text(build_dir: Path) -> str:
    """Return concatenated CMAKE_*FLAGS values from CMakeCache.txt."""
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.is_file():
        raise SystemExit(
            f"[pack-binary] CMakeCache.txt not found in {build_dir}. "
            "Re-run without --skip-build to configure and build with binary pack flags."
        )
    parts: list[str] = []
    for line in cache_file.read_text(encoding="utf-8").splitlines():
        if "CMAKE_C_FLAGS" not in line:
            continue
        _, _, value = line.partition("=")
        if value:
            parts.append(value)
    return " ".join(parts)


def _binary_pack_wink_config_h_path(build_dir: Path) -> Path:
    """Return the pack build's generated wink_config.h path."""
    return build_dir / "generated" / "wink_config.h"


def _parse_wink_config_ceiling(header_text: str, macro: str) -> int | None:
    """Parse a (NNU) define value from generated wink_config.h."""
    for name, value in _BINARY_PACK_DEFINE_RE.findall(header_text):
        if name == macro:
            return int(value)
    return None


def validate_binary_pack_wink_config_h(build_dir: Path) -> None:
    """Require pack build's generated wink_config.h to match binary pack ceilings."""
    config_h = _binary_pack_wink_config_h_path(build_dir)
    if not config_h.is_file():
        raise SystemExit(
            f"[pack-binary] Generated config header not found: {config_h}. "
            "Re-run without --skip-build to configure and build with binary pack flags."
        )

    header_text = config_h.read_text(encoding="utf-8")
    problems: list[str] = []
    for macro, expected in _BINARY_PACK_WINK_CONFIG_CEILINGS.items():
        actual = _parse_wink_config_ceiling(header_text, macro)
        if actual is None:
            problems.append(f"{macro} missing or not (NU) form")
        elif actual != expected:
            problems.append(f"{macro}={actual} (expected {expected})")

    if problems:
        raise SystemExit(
            "[pack-binary] Build dir lacks binary pack wink_config.h ceilings: "
            + ", ".join(problems)
            + f". Re-run without --skip-build (header: {config_h})."
        )


def validate_binary_pack_build_flags(build_dir: Path) -> None:
    """Require evidence that the build dir was configured for binary pack ABI ceilings."""
    flags = _collect_cmake_cache_flag_text(build_dir)
    missing: list[str] = [
        define for define in _BINARY_PACK_REQUIRED_DEFINES if define not in flags
    ]
    if _BINARY_PACK_MSVC_SECTION_FLAG not in flags:
        missing.extend(
            flag for flag in _BINARY_PACK_GCC_SECTION_FLAGS if flag not in flags
        )
    if missing:
        raise SystemExit(
            "[pack-binary] Build dir lacks binary pack ABI ceiling flags: "
            + ", ".join(missing)
            + ". Re-run without --skip-build (or reconfigure with pack flags: "
            + "-ffunction-sections and -fdata-sections for GCC, /Gy for MSVC)."
        )


def validate_binary_pack_skip_build(build_dir: Path) -> None:
    """Validate an existing pack build dir before --skip-build merge."""
    validate_binary_pack_build_flags(build_dir)
    validate_binary_pack_wink_config_h(build_dir)


def build_host(sdk_root: Path, build_dir: Path) -> None:
    """Configure and build the OS in SOURCE mode for host."""
    print(f"[pack-binary] Configuring host build in {build_dir} ...")
    pack_stub_app = create_binary_pack_stub_app(build_dir)
    configure_cmd = [
        "cmake",
        "-S", str(sdk_root),
        "-B", str(build_dir),
        "-DTARGET_PLATFORM=host",
        f"-DWINK_APP_DIR={pack_stub_app}",
        f"-DCMAKE_C_FLAGS={BINARY_PACK_GCC_C_FLAGS}",
    ]
    if os.name == "nt":
        gcc = shutil.which("gcc")
        ninja = shutil.which("ninja")
        mingw_make = shutil.which("mingw32-make")
        if gcc and ninja:
            configure_cmd[1:1] = ["-G", "Ninja"]
            configure_cmd.append(f"-DCMAKE_C_COMPILER={gcc}")
        elif gcc and mingw_make:
            configure_cmd[1:1] = ["-G", "MinGW Makefiles"]
            configure_cmd.append(f"-DCMAKE_C_COMPILER={gcc}")
        else:
            configure_cmd[-1] = f"-DCMAKE_C_FLAGS={BINARY_PACK_MSVC_C_FLAGS}"
    subprocess.run(configure_cmd, check=True)
    validate_binary_pack_build_flags(build_dir)

    print("[pack-binary] Building component libraries ...")
    subprocess.run(
        ["cmake", "--build", str(build_dir)],
        check=True,
    )


def find_component_libs(build_dir: Path) -> dict[str, Path]:
    """Locate the component .a files in the build tree."""
    libs = {}
    for name in ("dal", "runtime", "trace", "bal"):
        candidates = list(build_dir.glob(f"{name}/lib{name}*.*"))
        # Filter: prefer .a (MinGW) or .lib (MSVC)
        for c in candidates:
            if c.suffix in (".a", ".lib") and ("wink_" in c.name or name in c.name):
                libs[name] = c
                break
        if name not in libs:
            # Fallback: direct path
            direct = build_dir / name / f"libwink_{name}.a" if name != "dal" else build_dir / name / "libdal.a"
            if direct.exists():
                libs[name] = direct
            else:
                # Try .lib for MSVC
                lib_name = f"wink_{name}.lib" if name != "dal" else "dal.lib"
                direct_lib = build_dir / name / lib_name
                if direct_lib.exists():
                    libs[name] = direct_lib
    return libs


def require_component_libs(libs: dict[str, Path]) -> None:
    if len(libs) < 4:
        raise SystemExit(
            f"[pack-binary] Expected 4 component libs, found {len(libs)}: "
            + ", ".join(f"{k}={v}" for k, v in libs.items())
        )


def find_pal_host_objs(build_dir: Path) -> list[Path]:
    """Locate all pal_host .obj/.o files."""
    pal_host_dir = build_dir / "targets" / "host" / "CMakeFiles" / "pal_host.dir"
    if not pal_host_dir.exists():
        raise SystemExit(f"[pack-binary] pal_host build dir not found: {pal_host_dir}")
    objs = []
    for ext in ("*.obj", "*.o"):
        objs.extend(pal_host_dir.rglob(ext))
    return objs


def build_wasm(sdk_root: Path, build_dir: Path) -> None:
    """Configure and build the OS in SOURCE mode for wasm (Emscripten)."""
    emcmake = shutil.which("emcmake")
    if not emcmake:
        raise SystemExit(
            "[pack-binary] emcmake not found on PATH. "
            "Install Emscripten SDK and activate it before packing wasm."
        )

    print(f"[pack-binary] Configuring wasm build in {build_dir} ...")
    pack_stub_app = create_binary_pack_stub_app(build_dir)
    configure_cmd = [
        "emcmake", "cmake",
        "-S", str(sdk_root),
        "-B", str(build_dir),
        "-DTARGET_PLATFORM=wasm",
        f"-DWINK_APP_DIR={pack_stub_app}",
    ]
    subprocess.run(configure_cmd, check=True)

    print("[pack-binary] Building wasm component libraries ...")
    subprocess.run(
        ["cmake", "--build", str(build_dir)],
        check=True,
    )


def find_wasm_pal_objs(build_dir: Path) -> list[Path]:
    """Locate wasm PAL .o files in the build tree by matching known source basenames."""
    objs: list[Path] = []
    stem_to_src = {Path(s).stem: s for s in _WASM_PAL_SOURCE_BASENAMES}

    targets_build = build_dir / "targets"
    if not targets_build.is_dir():
        raise SystemExit(f"[pack-binary] wasm targets build dir not found: {targets_build}")

    for o_file in targets_build.rglob("*.o"):
        if o_file.stem in stem_to_src:
            objs.append(o_file)

    found_stems = {o.stem for o in objs}
    missing = set(stem_to_src) - found_stems
    if missing:
        print(f"[pack-binary] WARNING: wasm PAL objects not found for: {', '.join(sorted(missing))}")

    return objs


def merge_libraries(
    libs: dict[str, Path],
    pal_host_objs: list[Path],
    output: Path,
    archiver: str | None = None,
) -> None:
    """Merge component .a/.lib files + pal_host objects into a single archive."""
    print(f"[pack-binary] Merging {len(libs)} libs + {len(pal_host_objs)} objects → {output.name}")

    with tempfile.TemporaryDirectory(prefix="wink-merge-") as tmp:
        tmp_path = Path(tmp)

        # Extract all component archives
        for name, lib_path in sorted(libs.items()):
            print(f"  extracting {lib_path.name}")
            if lib_path.suffix == ".a":
                subprocess.run(
                    ["ar", "x", str(lib_path)],
                    cwd=tmp,
                    check=True,
                )
            else:
                # MSVC .lib — use lib.exe /LIST + /EXTRACT
                subprocess.run(
                    ["lib", "/NOLOGO", f"/LIST:{lib_path}"],
                    cwd=tmp,
                    check=True,
                )
                # Fallback: just copy the .lib as-is
                shutil.copy2(lib_path, tmp_path / lib_path.name)

        # Copy pal_host objects (skip duplicates)
        for obj in pal_host_objs:
            if obj.name in _DUPLICATE_OBJ_NAMES:
                continue
            dest = tmp_path / obj.name
            if not dest.exists():
                shutil.copy2(obj, dest)

        # Collect all object files
        all_objs = sorted(
            p for p in tmp_path.iterdir()
            if p.suffix in (".obj", ".o") and p.is_file()
        )
        if not all_objs:
            raise SystemExit("[pack-binary] No object files found to merge")

        print(f"  merging {len(all_objs)} objects ...")

        # Determine archiver
        if archiver:
            cmd = [archiver, "rcs", str(output)] + [str(o) for o in all_objs]
            subprocess.run(cmd, check=True)
        else:
            ar = shutil.which("ar")
            lib_exe = shutil.which("lib")

            if ar and all_objs[0].suffix == ".o":
                # MinGW/GCC
                cmd = ["ar", "rcs", str(output)] + [str(o) for o in all_objs]
                subprocess.run(cmd, check=True)
            elif lib_exe:
                # MSVC
                cmd = ["lib", "/NOLOGO", f"/OUT:{output}"] + [str(o) for o in all_objs]
                subprocess.run(cmd, check=True)
            elif ar:
                # Fallback to ar even for .obj (COFF)
                cmd = ["ar", "rcs", str(output)] + [str(o) for o in all_objs]
                subprocess.run(cmd, check=True)
            else:
                raise SystemExit("[pack-binary] No archiver found (need 'ar' or 'lib')")

    print(f"[pack-binary] Wrote {output} ({output.stat().st_size:,} bytes)")


def copy_public_headers(sdk_root: Path, include_dir: Path) -> None:
    """Copy whitelisted public headers into include/."""
    print(f"[pack-binary] Copying public headers → {include_dir}")

    # Flat PAL headers
    for hdr_rel in PAL_PUBLIC_HEADERS:
        src = sdk_root / hdr_rel
        if not src.exists():
            print(f"  WARNING: header not found: {hdr_rel}")
            continue
        dest = include_dir / src.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)

    # Flat runtime headers
    for hdr_rel in RUNTIME_PUBLIC_HEADERS:
        src = sdk_root / hdr_rel
        if not src.exists():
            print(f"  WARNING: header not found: {hdr_rel}")
            continue
        dest = include_dir / src.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)

    # Flat trace headers
    for hdr_rel in TRACE_PUBLIC_HEADERS:
        src = sdk_root / hdr_rel
        if not src.exists():
            print(f"  WARNING: header not found: {hdr_rel}")
            continue
        dest = include_dir / src.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)

    # DAL headers (preserve subdirectory structure)
    dal_src = sdk_root / DAL_INCLUDE_ROOT
    if dal_src.is_dir():
        for hdr in dal_src.rglob("*.h"):
            rel = hdr.relative_to(dal_src)
            dest = include_dir / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(hdr, dest)

    # BAL headers (preserve subdirectory structure)
    bal_src = sdk_root / BAL_INCLUDE_ROOT
    if bal_src.is_dir():
        for hdr in bal_src.rglob("*.h"):
            rel = hdr.relative_to(bal_src)
            dest = include_dir / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(hdr, dest)


def copy_sdk_bridge(staging: Path, sdk_root: Path) -> None:
    """Copy CMake bridge, tools, and test stubs into the staging tree."""
    # Consumer-facing CMakeLists.txt (binary SDK entry point)
    consumer_cmake = sdk_root / "tools" / "binary_sdk_cmake" / "CMakeLists.txt"
    if consumer_cmake.exists():
        shutil.copy2(consumer_cmake, staging / "CMakeLists.txt")

    # Smoke test source (verifies headers + .a link)
    smoke_src = sdk_root / "tools" / "binary_sdk_cmake" / "smoke_test.c"
    if smoke_src.exists():
        shutil.copy2(smoke_src, staging / "smoke_test.c")

    # CMake bridge files (host)
    for f in SDK_BRIDGE_FILES:
        src = sdk_root / f
        if src.exists():
            dest = staging / f
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)

    # Wasm bridge bridge files
    for f in SDK_WASM_BRIDGE_FILES:
        src = sdk_root / f
        if src.exists():
            dest = staging / f
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)

    # Tools (codegen, lint, wink.py)
    for tool_path in SDK_TOOL_ROOTS:
        src = sdk_root / tool_path
        if not src.exists():
            continue
        dest = staging / tool_path
        if src.is_file():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
        elif src.is_dir():
            shutil.copytree(
                src,
                dest,
                dirs_exist_ok=True,
                ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "tests"),
            )

    # test/stubs (host PAL dependency)
    stubs_src = sdk_root / "test" / "stubs"
    if stubs_src.is_dir():
        stubs_dst = staging / "test" / "stubs"
        shutil.copytree(stubs_src, stubs_dst, dirs_exist_ok=True)


ALLOWED_ROOT_C = {"smoke_test.c"}


def assert_staging_clean(staging: Path) -> None:
    """Refuse to ship sibling trees or implementation sources that leaked."""
    for banned in ("wink-micro-app", "embedded-frontend", "esp32_firmware"):
        if (staging / banned).exists():
            raise SystemExit(f"[pack-binary] refused: sibling dir {banned} leaked")

    for c_file in staging.rglob("*.c"):
        rel = c_file.relative_to(staging)
        rel_posix = rel.as_posix()
        if rel_posix in ALLOWED_ROOT_C:
            continue
        if rel_posix.startswith("test/"):
            continue
        raise SystemExit(f"[pack-binary] refused: implementation source leaked: {rel_posix}")


def assert_wasm_no_host_objs(wasm_objs: list[Path]) -> None:
    """Refuse to ship pal_host objects inside the wasm archive."""
    for obj in wasm_objs:
        obj_posix = obj.as_posix()
        if "targets/host" in obj_posix or "pal_host" in obj.name:
            raise SystemExit(
                f"[pack-binary] refused: host object in wasm archive: {obj_posix}"
            )


def _compute_aggregate_content_hash(content_hashes: dict[str, str]) -> str:
    """Compute a single SHA-256 over all per-file hashes in sorted key order."""
    h = hashlib.sha256()
    for key in sorted(content_hashes):
        h.update(f"{content_hashes[key]}  {key}\n".encode("utf-8"))
    return h.hexdigest()


def build_manifest(staging: Path, ver: str, abi: str, targets: list[str], primary_build_dir: Path) -> str:
    """Write SDK_MANIFEST.txt into the staging tree with toolchain + content hashes."""
    toolchain = detect_toolchain()
    content_hashes = compute_content_hashes(staging)
    aggregate_hash = _compute_aggregate_content_hash(content_hashes)
    cflags = _collect_cmake_cache_flag_text(primary_build_dir)
    files = sorted(content_hashes.keys())

    lines = [
        "mode=binary",
        f"targets={','.join(targets)}",
        f"version={ver}",
        f"abi={abi}",
        f"toolchain={toolchain.get('gcc', toolchain.get('platform', 'unknown'))}",
        f"cflags={cflags}",
        f"content_sha256={aggregate_hash}",
        "",
        "toolchain_detail:",
    ]
    for key in ("platform", "python", "gcc", "cmake"):
        if key in toolchain:
            lines.append(f"  {key}={toolchain[key]}")

    lines.append("")
    lines.append("files:")
    for f in files:
        lines.append(f"  {content_hashes[f]}  {f}")

    body = "\n".join(lines) + "\n"
    (staging / "SDK_MANIFEST.txt").write_text(body, encoding="utf-8")
    return body


def pack(targets: list[str], base_build_dir: Path, out_dir: Path) -> Path:
    ver, abi = read_version(SDK_ROOT)
    pkg_name = f"wink-micro-os-sdk-binary-v{ver}"
    out_dir.mkdir(parents=True, exist_ok=True)
    tarball = out_dir / f"{pkg_name}.tar.gz"

    with tempfile.TemporaryDirectory(prefix="wink-sdk-binary-") as tmp:
        staging = Path(tmp) / pkg_name
        staging.mkdir(parents=True)

        primary_build_dir = None

        for target in targets:
            target_build_dir = base_build_dir.parent / f"{base_build_dir.name}-{target}"

            if target == "host":
                build_host(SDK_ROOT, target_build_dir)
                libs = find_component_libs(target_build_dir)
                require_component_libs(libs)
                pal_objs = find_pal_host_objs(target_build_dir)
                libs_dir = staging / "libs" / "host" / "release"
                libs_dir.mkdir(parents=True)
                merge_libraries(libs, pal_objs, libs_dir / "libwink_micro_os.a")
            elif target == "wasm":
                build_wasm(SDK_ROOT, target_build_dir)
                libs = find_component_libs(target_build_dir)
                require_component_libs(libs)
                wasm_objs = find_wasm_pal_objs(target_build_dir)
                assert_wasm_no_host_objs(wasm_objs)
                libs_dir = staging / "libs" / "wasm" / "release"
                libs_dir.mkdir(parents=True)
                merge_libraries(libs, wasm_objs, libs_dir / "libwink_micro_os.a", archiver="emar")
            else:
                raise SystemExit(f"[pack-binary] unknown target: {target}")

            if primary_build_dir is None:
                primary_build_dir = target_build_dir

        # Copy public headers (shared across targets)
        include_dir = staging / "include"
        include_dir.mkdir(parents=True)
        copy_public_headers(SDK_ROOT, include_dir)

        # Copy SDK bridge files + tools + stubs
        copy_sdk_bridge(staging, SDK_ROOT)

        # Metadata
        for meta in ("VERSION", "NOTICE"):
            src = SDK_ROOT / meta
            if src.is_file():
                shutil.copy2(src, staging / meta)

        build_manifest(staging, ver, abi, targets, primary_build_dir)

        # Negative checks
        assert_staging_clean(staging)

        # Create tarball
        if tarball.exists():
            tarball.unlink()
        with tarfile.open(tarball, "w:gz") as tar:
            tar.add(staging, arcname=pkg_name)

    tarball_hash = hash_file(tarball)
    print(f"[pack-binary] Wrote {tarball}")
    print(f"[pack-binary] SHA-256: {tarball_hash}")
    return tarball


def main() -> int:
    p = argparse.ArgumentParser(description="Pack wink-micro-os Binary SDK (Phase 2)")
    p.add_argument(
        "--build-dir",
        type=Path,
        default=SDK_ROOT / "build-pack",
        help="Base build directory (default: build-pack; per-target subdirs created automatically)",
    )
    p.add_argument(
        "--out-dir",
        type=Path,
        default=SDK_ROOT / "dist",
        help="Directory for the .tar.gz (default: wink-micro-os/dist)",
    )
    p.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip cmake configure/build (use existing build-dirs)",
    )
    p.add_argument(
        "--targets",
        type=str,
        default="host",
        help="Comma-separated target list (default: host; e.g. host,wasm)",
    )
    args = p.parse_args()

    targets = [t.strip() for t in args.targets.split(",") if t.strip()]
    if not targets:
        raise SystemExit("[pack-binary] --targets must not be empty")

    base_build_dir = args.build_dir.resolve()
    out_dir = args.out_dir.resolve()

    if args.skip_build:
        ver, abi = read_version(SDK_ROOT)
        pkg_name = f"wink-micro-os-sdk-binary-v{ver}"
        out_dir.mkdir(parents=True, exist_ok=True)
        tarball = out_dir / f"{pkg_name}.tar.gz"

        primary_build_dir = None

        with tempfile.TemporaryDirectory(prefix="wink-sdk-binary-") as tmp:
            staging = Path(tmp) / pkg_name
            staging.mkdir(parents=True)

            for target in targets:
                target_build_dir = base_build_dir.parent / f"{base_build_dir.name}-{target}"

                if target == "host":
                    validate_binary_pack_skip_build(target_build_dir)
                    libs = find_component_libs(target_build_dir)
                    require_component_libs(libs)
                    pal_objs = find_pal_host_objs(target_build_dir)
                    libs_dir = staging / "libs" / "host" / "release"
                    libs_dir.mkdir(parents=True)
                    merge_libraries(libs, pal_objs, libs_dir / "libwink_micro_os.a")
                elif target == "wasm":
                    wasm_objs = find_wasm_pal_objs(target_build_dir)
                    assert_wasm_no_host_objs(wasm_objs)
                    libs = find_component_libs(target_build_dir)
                    require_component_libs(libs)
                    libs_dir = staging / "libs" / "wasm" / "release"
                    libs_dir.mkdir(parents=True)
                    merge_libraries(libs, wasm_objs, libs_dir / "libwink_micro_os.a", archiver="emar")
                else:
                    raise SystemExit(f"[pack-binary] unknown target: {target}")

                if primary_build_dir is None:
                    primary_build_dir = target_build_dir

            include_dir = staging / "include"
            include_dir.mkdir(parents=True)
            copy_public_headers(SDK_ROOT, include_dir)
            copy_sdk_bridge(staging, SDK_ROOT)

            for meta in ("VERSION", "NOTICE"):
                src = SDK_ROOT / meta
                if src.is_file():
                    shutil.copy2(src, staging / meta)
            build_manifest(staging, ver, abi, targets, primary_build_dir)
            assert_staging_clean(staging)

            if tarball.exists():
                tarball.unlink()
            with tarfile.open(tarball, "w:gz") as tar:
                tar.add(staging, arcname=pkg_name)

        tarball_hash = hash_file(tarball)
        print(f"[pack-binary] Wrote {tarball}")
        print(f"[pack-binary] SHA-256: {tarball_hash}")
    else:
        pack(targets, base_build_dir, out_dir)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
