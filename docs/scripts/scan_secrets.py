#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-only
"""scan_secrets.py — 公开仓密钥/内网信息卫生扫描器。

模式：
  --worktree  扫描工作树中所有被跟踪文件（CI 门禁，默认）
  --history   审计全部 ref 可达的每个 blob（人工全历史审计，较慢）

退出码：0 = 干净；1 = 命中（或 git 调用失败）。
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys

# Windows GBK 控制台兼容：emoji 输出不应导致 UnicodeEncodeError
for _stream in (getattr(sys, "stdout", None), getattr(sys, "stderr", None)):
    if _stream is not None and hasattr(_stream, "reconfigure"):
        try:
            _stream.reconfigure(errors="replace")
        except Exception:
            pass

MAX_BLOB_BYTES = 1024 * 1024

RULES = [
    ("NPM-AUTH-TOKEN", re.compile(rb"(?i)_authToken\s*[=:]\s*['\"]?[A-Za-z0-9\-_.]{16,}")),
    ("NPM-TOKEN", re.compile(rb"\bnpm_[A-Za-z0-9]{36}\b")),
    (
        "GITHUB-TOKEN",
        re.compile(rb"\b(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{36}\b|\bgithub_pat_[A-Za-z0-9_]{60,}\b"),
    ),
    ("AWS-KEY-ID", re.compile(rb"\bAKIA[0-9A-Z]{16}\b")),
    ("PRIVATE-KEY-BLOCK", re.compile(rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH |PGP )?PRIVATE KEY-----")),
    ("SLACK-TOKEN", re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{10,}\b")),
    (
        "GENERIC-API-SECRET",
        re.compile(
            rb"(?i)\b(?:api[_-]?key|apikey|secret[_-]?key|access[_-]?key|client[_-]?secret)\b"
            rb"\s*[=:]\s*['\"][A-Za-z0-9+/_\-]{16,}['\"]"
        ),
    ),
    ("INTERNAL-HOST", re.compile(rb"(?i)\bcodeup\.aliyun\.com\b|\b[a-z0-9.-]+\.corp\.[a-z0-9-]+\b")),
]

SENSITIVE_NAMES = re.compile(
    r"(^|/)(\.npmrc|\.pypirc|\.env(\..+)?|id_rsa|id_ed25519|credentials\.json|secrets\.(json|ya?ml)|"
    r".+\.(pem|p12|pfx|jks|keystore))$",
    re.IGNORECASE,
)

SENSITIVE_NAME_ALLOW = re.compile(r"(\.example$|\.sample$|\.template$|README|LICENSE)", re.IGNORECASE)


def run_git(args: list[str], binary: bool = False) -> bytes | str:
    proc = subprocess.run(
        ["git", *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode("utf-8", "replace").strip())
    return proc.stdout if binary else proc.stdout.decode("utf-8", "replace")


def is_binary(data: bytes) -> bool:
    return b"\x00" in data[:8192]


def scan_bytes(data: bytes, label: str, findings: list[str]) -> None:
    if is_binary(data) or len(data) > MAX_BLOB_BYTES:
        return
    for line_no, line in enumerate(data.split(b"\n"), start=1):
        for rule_id, pattern in RULES:
            if pattern.search(line):
                findings.append(f"{label}:{line_no}: {rule_id}")


def scan_worktree(findings: list[str]) -> int:
    names = run_git(["ls-files", "-z"]).split("\x00")
    count = 0
    for name in names:
        if not name:
            continue
        count += 1
        if SENSITIVE_NAMES.search(name) and not SENSITIVE_NAME_ALLOW.search(name):
            findings.append(f"{name}: filename policy: SENSITIVE-FILENAME")
        try:
            with open(name, "rb") as fh:
                scan_bytes(fh.read(), name, findings)
        except OSError:
            continue
    return count


def scan_history(findings: list[str]) -> int:
    out = run_git(["rev-list", "--objects", "--all"])
    blob_paths: dict[str, str] = {}
    for line in out.splitlines():
        sha, _, path = line.partition(" ")
        blob_paths.setdefault(sha, path)

    proc = subprocess.Popen(
        ["git", "cat-file", "--batch"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    scanned = 0
    assert proc.stdin and proc.stdout
    for sha, path in blob_paths.items():
        proc.stdin.write(sha.encode() + b"\n")
        proc.stdin.flush()
        header = proc.stdout.readline().split()
        if len(header) != 3:
            continue
        size = int(header[2])
        data = proc.stdout.read(size)
        proc.stdout.read(1)
        if header[1] != b"blob" or size > MAX_BLOB_BYTES:
            continue
        scanned += 1
        label = path or sha
        if path and SENSITIVE_NAMES.search(path) and not SENSITIVE_NAME_ALLOW.search(path):
            findings.append(f"{label}: filename policy: SENSITIVE-FILENAME")
        scan_bytes(data, label, findings)
    proc.stdin.close()
    proc.wait()
    return scanned


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", action="store_true", help="扫描全部 ref 可达 blob")
    parser.add_argument("--worktree", action="store_true", help="扫描工作树被跟踪文件（默认）")
    args = parser.parse_args()

    findings: list[str] = []
    try:
        if args.history:
            scanned = scan_history(findings)
            print(f"🔎 history scan: {scanned} blobs (≤ {MAX_BLOB_BYTES // 1024} KiB, non-binary)")
        else:
            scanned = scan_worktree(findings)
            print(f"🔎 worktree scan: {scanned} tracked files")
    except RuntimeError as exc:
        print(f"❌ git error: {exc}", file=sys.stderr)
        return 1

    if findings:
        print(f"🚨 {len(findings)} potential secret/internal-host findings:")
        for item in findings[:200]:
            print(f"  - {item}")
        if len(findings) > 200:
            print(f"  … {len(findings) - 200} more")
        return 1

    print("🎉 no secret or internal-host indicators found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
