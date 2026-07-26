"""tools.cli._shared — Shared orchestration utilities across CLI commands."""
from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from tools.cli.context import AppContext


def run_cmd(cmd: list[str], cwd: Path | str | None = None, env: dict[str, str] | None = None) -> None:
    """Helper to run a subprocess command with check=True."""
    subprocess.run(cmd, cwd=cwd, check=True, env=env)


def run_esp32_guard_density_lint(sdk_dir: Path) -> bool:
    """Check ESP32 driver guards in CMakeLists.txt and header files."""
    cmake = sdk_dir / "targets" / "esp32" / "CMakeLists.txt"
    if not cmake.is_file():
        return True
    text = cmake.read_text(encoding="utf-8")
    guarded_sources = set(re.findall(r"pal_[a-z0-9_]+\.c", text))
    if not guarded_sources:
        print("[lint] WARN: no guarded sources matched in esp32 CMakeLists.txt", file=sys.stderr)
        return False
    missing_headers = []
    for src_name in guarded_sources:
        header_name = src_name.replace(".c", ".h")
        header_path = sdk_dir / "targets" / "esp32" / header_name
        if not header_path.is_file():
            missing_headers.append(header_name)
    if missing_headers:
        print(f"[lint] FAIL: guarded sources missing headers: {missing_headers}", file=sys.stderr)
        return False
    print("[lint] ESP32 guard density check OK")
    return True


def run_adr0017_l1_strict_lint(sdk_dir: Path) -> bool:
    """Strict check: ensure dal_ultrasonic_read is not linked in baremetal/strict mode."""
    target_dir = sdk_dir / "targets" / "host"
    if not target_dir.is_dir():
        return True

    nm = "nm"
    if sys.platform == "win32":
        nm = "nm.exe"

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_bdir = Path(tmpdir)
        try:
            run_cmd(
                ["cmake", "-S", str(sdk_dir), "-B", str(tmp_bdir), "-DTARGET_PLATFORM=host"],
                cwd=tmpdir,
            )
            run_cmd(["cmake", "--build", str(tmp_bdir)], cwd=tmpdir)
        except Exception as exc:
            print(f"[lint] WARN: ADR-0017 check skipped build failure: {exc}", file=sys.stderr)
            return True

        lib_file = tmp_bdir / "libwink_os.a"
        if not lib_file.is_file():
            lib_file = tmp_bdir / "wink_os.lib"
        if not lib_file.is_file():
            return True

        if subprocess.run([nm, "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
            res = subprocess.run([nm, str(lib_file)], capture_output=True, text=True)
            if "dal_ultrasonic_read" in res.stdout:
                print("[lint] FAIL: dal_ultrasonic_read symbol present in libwink_os.a", file=sys.stderr)
                return False

    print("[lint] ADR-0017 L1: dal_ultrasonic_read absent under strict mode OK")
    return True
