#!/usr/bin/env python3
"""Pack wink-micro-os as a Phase-1 Source SDK tarball (faithful tree subset).

Excludes test/, build-*, .git, and never pulls monorepo siblings.
Writes VERSION / NOTICE / SDK_MANIFEST.txt into the archive root.
"""
from __future__ import annotations

import argparse
import hashlib
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

SDK_ROOT = Path(__file__).resolve().parent.parent

# Top-level names to copy when present (faithful export; no include/src remap).
INCLUDE_TOP = [
    "pal",
    "dal",
    "bal",
    "runtime",
    "trace",
    "targets",
    "tools",
    "CMakeLists.txt",
    "core_sources.cmake",
    "VERSION",
    "NOTICE",
    "SDK_MANIFEST.in.txt",
    "CHANGELOG.md",
    ".clang-tidy",
    "run-tests.ps1",
    "idf_component.yml",
]

SKIP_DIR_NAMES = {"test", "__pycache__", ".git", "dist"}

# host PAL compiles against test/stubs/host_test_ctrl.h — keep stubs even when
# the rest of test/ is omitted from the Source SDK.
KEEP_UNDER_TEST = {"stubs"}


def should_skip(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    parts = rel.parts
    if parts and parts[0] == "test":
        # Allow test/stubs/** only
        if len(parts) >= 2 and parts[1] in KEEP_UNDER_TEST:
            return False
        return True
    if any(p in SKIP_DIR_NAMES for p in parts):
        return True
    if any(p.startswith("build-") for p in parts):
        return True
    return False


def read_version(sdk_root: Path) -> tuple[str, str]:
    version_file = sdk_root / "VERSION"
    if not version_file.is_file():
        raise SystemExit(f"[pack] VERSION not found: {version_file}")
    lines = [ln.strip() for ln in version_file.read_text(encoding="utf-8").splitlines() if ln.strip()]
    if not lines:
        raise SystemExit("[pack] VERSION is empty")
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


def copy_tree(src: Path, dst: Path, sdk_root: Path) -> None:
    if src.is_file():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        return
    for path in src.rglob("*"):
        if path.is_dir():
            continue
        if should_skip(path, sdk_root):
            continue
        # Also skip if any parent under src is a skip dir name already handled
        rel = path.relative_to(src)
        out = dst / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, out)


def build_manifest(staging: Path, ver: str, abi: str, sdk_root: Path) -> str:
    template = (sdk_root / "SDK_MANIFEST.in.txt").read_text(encoding="utf-8")
    toolchain = detect_toolchain()
    content_hashes = compute_content_hashes(staging)

    files = sorted(content_hashes.keys())

    lines = [
        template.rstrip(),
        f"version={ver}",
        f"abi={abi}",
        "",
        "toolchain:",
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


def pack(out_dir: Path) -> Path:
    ver, abi = read_version(SDK_ROOT)
    pkg_name = f"wink-micro-os-sdk-source-v{ver}"
    out_dir.mkdir(parents=True, exist_ok=True)
    tarball = out_dir / f"{pkg_name}.tar.gz"

    with tempfile.TemporaryDirectory(prefix="wink-sdk-pack-") as tmp:
        staging_root = Path(tmp) / pkg_name
        staging_root.mkdir(parents=True)

        for name in INCLUDE_TOP:
            src = SDK_ROOT / name
            if not src.exists():
                continue
            copy_tree(src, staging_root / name, SDK_ROOT)

        # Always ship test/stubs (host PAL dependency); never ship the rest of test/.
        stubs_src = SDK_ROOT / "test" / "stubs"
        if stubs_src.is_dir():
            copy_tree(stubs_src, staging_root / "test" / "stubs", SDK_ROOT)

        # Ensure VERSION / NOTICE always present at package root
        for meta in ("VERSION", "NOTICE"):
            src = SDK_ROOT / meta
            if src.is_file():
                shutil.copy2(src, staging_root / meta)

        build_manifest(staging_root, ver, abi, SDK_ROOT)

        # Negative checks before archive
        test_root = staging_root / "test"
        if test_root.is_dir():
            for child in test_root.iterdir():
                if child.name != "stubs":
                    raise SystemExit(f"[pack] refused: unexpected test/ entry {child.name}")
        for banned in ("wink-micro-app", "embedded-frontend", "esp32_firmware"):
            if (staging_root / banned).exists():
                raise SystemExit(f"[pack] refused: sibling dir {banned} leaked")

        if tarball.exists():
            tarball.unlink()
        with tarfile.open(tarball, "w:gz") as tar:
            tar.add(staging_root, arcname=pkg_name)

    tarball_hash = hash_file(tarball)
    print(f"[pack] Wrote {tarball}")
    print(f"[pack] SHA-256: {tarball_hash}")
    return tarball


def main() -> int:
    p = argparse.ArgumentParser(description="Pack wink-micro-os Source SDK (Phase 1)")
    p.add_argument(
        "--out-dir",
        type=Path,
        default=SDK_ROOT / "dist",
        help="Directory for the .tar.gz (default: wink-micro-os/dist)",
    )
    args = p.parse_args()
    pack(args.out_dir.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
