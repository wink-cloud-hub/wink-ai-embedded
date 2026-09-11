# SPDX-License-Identifier: Apache-2.0
# S1-0b "测试不丢"基线工具：ctest 注册列表的捕获与比对（标准库 only）。
#
# 为什么必须是脚本 + 为什么只数总数不够：
#   1. 总数相同也可能一增一减（改名/拆分会瞒过计数）——必须比对排序后的名字集合。
#   2. ctest 注册列表随 configure 条件变化（有无 Python3/emcc、生成器、平台），
#      所以基线必须连同 configure 上下文一起记录，否则换台机器就对不上。
#   3. "不丢"（集合相等）与"全绿"（结果门禁）是两件事：前者只管 move 本身
#      是否弄丢用例，后者看 verdict（已知例外见计划 S1-0b，如 wasm 断链、
#      2 个 MinGW 链接失败）。
#
# 用法（仓库根目录执行；比对前先重跑 cmake configure + 全量构建，否则列表是旧的）：
#   捕获基线： python <this> --capture --test-dir build-host/wink-micro-os/test
#   搬迁后比对：python <this> --compare --test-dir build-host/wink-micro-os/test
# 自定义基线路径： --baseline <path>（默认：本脚本同目录 test-baseline-N.txt）
from __future__ import annotations

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_BASELINE = HERE / "test-baseline-N.txt"

_CTEST_LINE = re.compile(r"Test\s+#\d+:\s+(.*?)\s*$")
_META_LINE = re.compile(r"\s*([\w_]+)\s*:\s*(.*)")  # applied AFTER lstrip("#")


def run(cmd: list[str], cwd: Path) -> str:
    r = subprocess.run(cmd, capture_output=True, text=True,
                       errors="replace", cwd=str(cwd))
    if r.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} failed:\n{r.stdout}\n{r.stderr}")
    return r.stdout


def list_tests(test_dir: Path) -> list[str]:
    """Raw test names in capture order. Prefers structured JSON (immune to
    spaces/localization in names); falls back to -N text parsing."""
    try:
        data = json.loads(run(["ctest", "--show-only=json-v1"], cwd=test_dir))
        return [t["name"] for t in data["tests"]]
    except Exception:
        out = run(["ctest", "-N"], cwd=test_dir)
        return [m.group(1) for m in _CTEST_LINE.finditer(out)]


def classify(name: str) -> str:
    if name.startswith(("test_mcs51", "wasm_mcs51")):
        if name.endswith("_strict"):
            return "mcs51-strict"
        if name.startswith("wasm_"):
            return "mcs51-wasm"
        return "mcs51-host"
    if name.startswith("wasm_"):
        return "wasm-other"
    return "other"


def find_cache(test_dir: Path) -> Path | None:
    if (test_dir / "CMakeCache.txt").is_file():
        return test_dir / "CMakeCache.txt"
    # test_dir is usually a configured SUBDIR (e.g. build-host/.../test);
    # the cache lives at the build root — walk up until found.
    for parent in test_dir.parents:
        if (parent / "CMakeCache.txt").is_file():
            return parent / "CMakeCache.txt"
    return None


def capture(test_dir: Path) -> tuple[list[str], dict]:
    if not test_dir.is_dir():
        raise RuntimeError(f"test dir not found: {test_dir} (configure first?)")
    raw = list_tests(test_dir)
    if not raw:
        raise RuntimeError("ctest returned no tests; is the build dir configured?")
    if len(raw) != len(set(raw)):
        dupes = sorted({n for n in raw if raw.count(n) > 1})
        raise RuntimeError(f"ctest emitted duplicate names (build anomaly): {dupes}")
    names = sorted(set(raw))
    cats: dict[str, int] = {}
    for n in names:
        cats[classify(n)] = cats.get(classify(n), 0) + 1
    meta: dict = {"total": len(names), "cats": cats}
    try:
        meta["cmake"] = run(["cmake", "--version"], cwd=test_dir).splitlines()[0]
    except Exception:
        meta["cmake"] = "unknown"
    meta["generator"] = "unknown"
    meta["target_platform"] = "unknown"
    cache = find_cache(test_dir)
    if cache is not None:
        txt = cache.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"CMAKE_GENERATOR:INTERNAL=(.+)", txt)
        if m:
            meta["generator"] = m.group(1).strip()
        m = re.search(r"TARGET_PLATFORM:STRING=(.+)", txt)
        if m:
            meta["target_platform"] = m.group(1).strip()
    try:
        repo = HERE
        while not (repo / ".git").exists() and repo.parent != repo:
            repo = repo.parent
        meta["head"] = run(["git", "rev-parse", "--short", "HEAD"],
                           cwd=repo).strip()
        dirty = run(["git", "status", "--short"], cwd=repo).strip()
        meta["dirty_files"] = len(dirty.splitlines()) if dirty else 0
    except Exception:
        meta["head"] = "unknown"
        meta["dirty_files"] = -1
    meta["captured"] = datetime.datetime.now().isoformat(timespec="seconds")
    return names, meta


def render(names: list[str], meta: dict) -> str:
    # One machine-readable `# key: value` per line (parsed back on compare);
    # the human summary line deliberately has no colon so it never parses.
    lines = [
        "# mcs51 migration test baseline: REGISTERED ctest list (names, sorted).",
        "# Criterion 'nothing lost' = set equality with this list after the move.",
        "# (Greenness is a separate gate with known exceptions; see stage1 S1-0b.)",
        f"# total: {meta['total']}",
        f"# generator: {meta['generator']}",
        f"# target_platform: {meta['target_platform']}",
        f"# cmake: {meta['cmake']}",
        f"# captured: {meta['captured']}",
        f"# head: {meta['head']}",
        f"# dirty_files: {meta['dirty_files']}",
        "# cats " + " ".join(f"{k}={v}" for k, v in sorted(meta["cats"].items())),
        "",
    ]
    return "\n".join(lines) + "\n".join(names) + "\n"


def load_baseline(path: Path) -> tuple[dict, list[str]]:
    if not path.is_file():
        raise RuntimeError(f"baseline not found: {path} (run --capture first?)")
    meta: dict[str, str] = {}
    names: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            m = _META_LINE.match(line.lstrip("#"))
            if m:
                meta[m.group(1)] = m.group(2).strip()
        elif line.strip():
            names.append(line.strip())
    return meta, sorted(names)


def warn_context(old: dict, new: dict) -> None:
    """Baseline context drift never fails the gate (different machine/config
    is legitimate) but must be visible so a mismatch isn't misread."""
    for key in ("generator", "target_platform", "cmake"):
        a, b = old.get(key, ""), new.get(key, "")
        if a and b and a != b and "unknown" not in (a, b):
            print(f"WARNING: baseline context drift [{key}]: "
                  f"'{a}' -> '{b}' (set equality still enforced).")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--test-dir", required=True,
                    help="configured ctest dir, e.g. build-host/wink-micro-os/test")
    ap.add_argument("--baseline", default=str(DEFAULT_BASELINE))
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--capture", action="store_true")
    mode.add_argument("--compare", action="store_true")
    args = ap.parse_args()

    test_dir = Path(args.test_dir)
    baseline = Path(args.baseline)
    if args.capture:
        names, meta = capture(test_dir)
        baseline.parent.mkdir(parents=True, exist_ok=True)
        baseline.write_text(render(names, meta), encoding="utf-8")
        print(f"CAPTURED {len(names)} tests -> {baseline}")
        for k, v in sorted(meta["cats"].items()):
            print(f"  {k}: {v}")
        if meta["dirty_files"] > 0:
            print("WARNING: tree is dirty; re-capture on the clean pre-move "
                  "tree before the move (S1-0b).")
        elif meta["dirty_files"] < 0:
            print("NOTE: git info unavailable; HEAD/dirty not recorded.")
        return 0

    old_meta, want = load_baseline(baseline)
    if str(old_meta.get("total", "")) != str(len(want)):
        print(f"WARNING: baseline file internally inconsistent (header total "
              f"{old_meta.get('total')} vs {len(want)} names); using names.")
    got, meta = capture(test_dir)
    print(f"baseline: {len(want)} tests | current: {len(got)} tests "
          f"[{meta['generator']} / {meta['target_platform']}]")
    warn_context(old_meta, meta)
    lost = sorted(set(want) - set(got))
    added = sorted(set(got) - set(want))
    if not lost and not added:
        print("BASELINE MATCH: nothing lost, nothing silently added.")
        return 0
    for n in lost:
        print(f"  LOST:  {n}")
    for n in added:
        print(f"  ADDED: {n}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
