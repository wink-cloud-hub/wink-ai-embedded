#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ESP-IDF Headless Deterministic Replay Verifier (M3 Task M3-3, T-008).

Runs the same simulation binary multiple times and verifies that the filtered
stdout produces bit-exact identical hashes (zero scheduling jitter, identical
virtual timelines).

Usage: test_esp_idf_headless_replay.py <path_to_test_binary> [runs]
Exit code: 0 = deterministic, 1 = jitter/timeout/binary failure.
"""
from __future__ import annotations

import hashlib
import re
import subprocess
import sys

# Non-deterministic content removed before hashing:
#   1. wall-clock log prefix `[2026-09-25 23:16:45.033] [TID:0x07FC]`
#   2. pointer/thread ids (0x...)
#   3. process ids from host logs
# Virtual timestamps such as `I (1000)` are deterministic and intentionally kept.
_NON_DETERMINISTIC_PATTERNS = [
    re.compile(r"\[\d{4}-\d{2}-\d{2} [0-9:.]+\s*[^\]]*\]"),
    re.compile(r"0x[0-9a-fA-F]+"),
    re.compile(r"pid=\d+", re.IGNORECASE),
]


def sanitize(text: str) -> str:
    out = text.replace("\r\n", "\n")
    for pattern in _NON_DETERMINISTIC_PATTERNS:
        out = pattern.sub("<FILTERED>", out)
    return out


def run_simulation_and_hash(binary_path: str, timeout_sec: int = 30) -> str | None:
    try:
        res = subprocess.run(
            [binary_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_sec,
        )
    except subprocess.TimeoutExpired:
        print(f"Error: binary timed out after {timeout_sec}s - possible infinite loop")
        return None
    if res.returncode != 0:
        print(f"Error: binary exited with {res.returncode}\n{res.stderr}")
        return None
    return hashlib.sha256(sanitize(res.stdout).encode("utf-8")).hexdigest()


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: test_esp_idf_headless_replay.py <path_to_test_binary> [runs]")
        return 1

    binary = sys.argv[1]
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    print(f"Testing deterministic replay on: {binary} ({runs} runs)")

    hashes = [run_simulation_and_hash(binary) for _ in range(runs)]
    if any(h is None for h in hashes):
        print("Failed to obtain all run hashes")
        return 1

    for i, h in enumerate(hashes, 1):
        print(f"Run {i} Hash: {h}")

    if len(set(hashes)) == 1:
        print("SUCCESS: Deterministic replay verified! All runs bit-exact match.")
        return 0
    print("FAILURE: Scheduling jitter detected! Hash mismatch between runs.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
