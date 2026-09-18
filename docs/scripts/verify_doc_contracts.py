#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-only
"""verify_doc_contracts.py

全域文档黑盒契约与反情报泄露门禁检查（CI Gate）：
1. 扫描仓内所有公开文档（docs/ 全域、根目录 Markdown、子项目 docs），断言不得泄露闭源仓私有路径、
   内部文件、本地机器路径或未经抽象的源码引用。
2. 支持 --baseline 机制，冻结存量技术债，阻断新增违规，支持渐进式脱敏。
3. 校验关键导航节点（AGENTS.md, 00-quick-start, README 等）完整性。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Set, Tuple, Optional

# Windows GBK 控制台兼容：emoji/生僻字符输出不应导致 UnicodeEncodeError
for _stream in (getattr(sys, "stdout", None), getattr(sys, "stderr", None)):
    if _stream is not None and hasattr(_stream, "reconfigure"):
        try:
            _stream.reconfigure(errors="replace")
        except Exception:
            pass

# 零宽/不可见字符：可能被用于撕碎路径字符串以规避黑盒匹配
INVISIBLE_CHARS = re.compile(r"[\u200B-\u200F\u2060\uFEFF\u00AD\u180E]")

# 根目录推导
SCRIPT_PATH = Path(__file__).resolve()
WORKSPACE_DIR = SCRIPT_PATH.parents[2]
DOCS_DIR = WORKSPACE_DIR / "docs"

# 扫描目录与排除配置
DEFAULT_SCAN_DIRS = [
    DOCS_DIR,
    WORKSPACE_DIR / "wink-micro-os" / "docs",
    WORKSPACE_DIR / "wink-tools" / "docs",
    WORKSPACE_DIR / "wink-plugin-peripherals" / "docs",
]

DEFAULT_ROOT_DOCS = [
    WORKSPACE_DIR / "README.md",
    WORKSPACE_DIR / "README.zh-CN.md",
    WORKSPACE_DIR / "AGENTS.md",
    WORKSPACE_DIR / "CLAUDE.md",
]

# 严格跳过的物理目录与特征（防误入受保护本地通道与临时构建产物）
EXCLUDE_DIR_NAMES = {
    ".git",
    ".internals",
    "vendors",
    "node_modules",
    "build",
    "build_host",
    "dist",
    "artifacts",
    "__pycache__",
    ".pytest_cache",
}


@dataclass
class RedlineRule:
    rule_id: str
    severity: str  # "FATAL" (P0), "HIGH" (P1), "WARN" (P2)
    name: str
    pattern: re.Pattern
    remediation: str


# 核心反侦察与黑盒隔离红线矩阵
REDLINE_RULES: List[RedlineRule] = [
    RedlineRule(
        rule_id="RULE-01-SRC-ESCAPE",
        severity="FATAL",
        name="私有包内部源码路径泄露 (src/ escape)",
        pattern=re.compile(
            r"packages/(?:unisim|embedded-frontend|toy-studio|x-studio(?:-[a-z]+)?|shared)/src(?![A-Za-z0-9_-])",
            re.IGNORECASE,
        ),
        remediation="禁止在公开文档中直接引用外部闭源包的内部源码路径。请用逻辑模块名代替，如 `@wink-ai/unisim (PinArbiter)`。",
    ),
    RedlineRule(
        rule_id="RULE-02-MONOREPO-ESCAPE",
        severity="FATAL",
        name="兄弟 Monorepo 物理包相对路径泄露",
        pattern=re.compile(
            r"(?:\.\./)+|\bwink-ai/packages/",
            re.IGNORECASE,
        ),
        remediation="禁止暴露跨仓物理相对路径。请使用标准黑盒包名或契约规范代替物理文件路径。",
    ),
    RedlineRule(
        rule_id="RULE-03-FILE-URI-ESCAPE",
        severity="FATAL",
        name="本地机器绝对路径/URI 协议泄露",
        pattern=re.compile(
            r"file:///[^)\"\s]*(?:wink-ai/packages|\.internals)",
            re.IGNORECASE,
        ),
        remediation="严禁泄露开发者本地绝对盘符与工作区路径，防止信息泄露与链接失效。",
    ),
    RedlineRule(
        rule_id="RULE-04-CORE-INTERNAL-FILES",
        severity="HIGH",
        name="私有敏感核心源文件名泄露",
        pattern=re.compile(
            r"\b(?:unisim-bridge-factory|simulation-client|simulation-runtime|SimActuatorPanel|actuator-observation\.mapper)\.(?:ts|vue)\b",
            re.IGNORECASE,
        ),
        remediation="严禁暴露私有组件或服务的文件全名。请使用架构概念（如 SimulationClient、ActuatorMapper）进行功能性描述。",
    ),
    RedlineRule(
        rule_id="RULE-05-PRIV-TOOL-MODULES",
        severity="HIGH",
        name="内部私有工具链模块泄露",
        pattern=re.compile(
            r"packages/wink-tools/tools/(?:cli|vault|entitlement|toolchain|sim)\b",
            re.IGNORECASE,
        ),
        remediation="严禁泄露内部未开源的工具链私有模块源码路径。",
    ),
    RedlineRule(
        rule_id="RULE-06-INVISIBLE-CHARS",
        severity="HIGH",
        name="零宽/不可见字符混入词或路径 (invisible character evasion)",
        pattern=re.compile(
            r"[A-Za-z0-9_@/.\-][\u200B-\u200D\u2060\uFEFF\u00AD\u180E]+[A-Za-z0-9_@/.\-]"
        ),
        remediation="移除词或路径中的零宽字符/软连字符等不可见字符；它们会破坏黑盒路径匹配，并可能被用于规避门禁。",
    ),
]


@dataclass
class Finding:
    rel_path: str
    line_number: int
    rule: RedlineRule
    line_content: str
    fingerprint: str


def compute_fingerprint(rel_path: str, rule_id: str, line_content: str) -> str:
    """计算违规指纹，用于 Baseline 精确匹配"""
    normalized_content = line_content.strip()
    raw = f"{rel_path}|{rule_id}|{normalized_content}"
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:16]


def collect_markdown_files(root: Path) -> List[Path]:
    """全域收集需要扫描的公开 Markdown 文件"""
    md_files: List[Path] = []

    # 1. 检查根目录关键文档
    for root_doc in DEFAULT_ROOT_DOCS:
        if root_doc.is_file():
            md_files.append(root_doc)

    # 2. 遍历扫描目标目录
    for scan_dir in DEFAULT_SCAN_DIRS:
        if not scan_dir.exists():
            continue
        for current_root, dirs, files in os.walk(scan_dir):
            current_path = Path(current_root)
            # 动态剔除排除目录
            dirs[:] = [
                d for d in dirs
                if d not in EXCLUDE_DIR_NAMES
                and not d.startswith(".")
            ]

            for file in files:
                if file.endswith(".md"):
                    md_files.append(current_path / file)

    return sorted(list(set(md_files)))


def scan_file_for_violations(filepath: Path, base_dir: Path) -> List[Finding]:
    """对单文件按红线矩阵执行逐行扫描（含零宽字符归一化，防止匹配规避）"""
    findings: List[Finding] = []
    rel_path = filepath.relative_to(base_dir).as_posix()

    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            for line_idx, line in enumerate(f, start=1):
                normalized = INVISIBLE_CHARS.sub("", line)
                for rule in REDLINE_RULES:
                    if rule.pattern.search(line) or (
                        normalized != line and rule.pattern.search(normalized)
                    ):
                        fp = compute_fingerprint(rel_path, rule.rule_id, line)
                        findings.append(
                            Finding(
                                rel_path=rel_path,
                                line_number=line_idx,
                                rule=rule,
                                line_content=line.strip(),
                                fingerprint=fp,
                            )
                        )
    except Exception as e:
        print(f"⚠️ 读取文件出错 {rel_path}: {e}", file=sys.stderr)

    return findings


def check_essential_navigation(base_dir: Path) -> List[str]:
    """校验关键导航节点存在性"""
    essential_candidates = [
        [base_dir / "docs" / "AGENTS.md", base_dir / "AGENTS.md"],
        [
            base_dir / "docs" / "zh" / "design" / "00-quick-start" / "01-5min-getting-started.md",
            base_dir / "docs" / "design" / "00-quick-start" / "01-5min-getting-started.md",
        ],
        [
            base_dir / "docs" / "zh" / "design" / "README.md",
            base_dir / "docs" / "design" / "README.md",
        ],
    ]
    missing = []
    for group in essential_candidates:
        if not any(p.exists() for p in group):
            missing.append(group[0].relative_to(base_dir).as_posix())
    return missing


def load_baseline(baseline_path: Path) -> Tuple[Set[str], Set[str]]:
    """加载已承认的存量 Baseline (fingerprints 集合与 exempt_files 集合)"""
    if not baseline_path.is_file():
        return set(), set()
    try:
        data = json.loads(baseline_path.read_text(encoding="utf-8"))
        fingerprints = set(data.get("fingerprints", []))
        exempt_files = set(data.get("exempt_files", []))
        return fingerprints, exempt_files
    except Exception as e:
        print(f"⚠️ 解析 Baseline 文件失败 {baseline_path}: {e}", file=sys.stderr)
        return set(), set()


def save_baseline(baseline_path: Path, findings: List[Finding]) -> None:
    """将当前发现的存量违规生成为 Baseline 文件"""
    fps = sorted(list(set(f.fingerprint for f in findings)))
    files = sorted(list(set(f.rel_path for f in findings)))
    meta = {
        "version": 1,
        "total_violations": len(findings),
        "exempt_files_count": len(files),
        "unique_fingerprints": len(fps),
        "description": "Baseline of historical documentation contract violations for gradual remediation.",
        "exempt_files": files,
        "fingerprints": fps,
        "items": [
            {
                "file": f.rel_path,
                "line": f.line_number,
                "rule": f.rule.rule_id,
                "severity": f.rule.severity,
                "snippet": f.line_content[:100],
                "fingerprint": f.fingerprint,
            }
            for f in findings
        ],
    }
    baseline_path.parent.mkdir(parents=True, exist_ok=True)
    baseline_path.write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"💾 Baseline 已生成至: {baseline_path} (记录 {len(files)} 个历史文件共 {len(findings)} 处历史问题)")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Wink-AI 嵌入式全域文档黑盒契约与反情报泄露门禁检查器"
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=WORKSPACE_DIR,
        help="仓库根目录 (默认当前仓库根目录)",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=None,
        help="指定基线文件路径 (忽略基线内的历史存量违规)",
    )
    parser.add_argument(
        "--generate-baseline",
        type=Path,
        default=None,
        help="将当前扫描结果生成为指定路径的 Baseline 文件",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="严格模式：若发现任何非 Baseline 违规立刻返回退出码 1",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="详细输出扫描进度与结果",
    )

    args = parser.parse_args()
    root_dir: Path = args.root.resolve()

    print("=" * 80)
    print(" 🛡️  Wink-AI 全域文档黑盒隔离与契约安全审计 (CI Gate)")
    print(f" 📂 根目录: {root_dir}")
    print("=" * 80)

    # 1. 检查导航节点
    missing_nav = check_essential_navigation(root_dir)
    if missing_nav:
        print(f"❌ 关键导航节点缺失: {missing_nav}")
        if args.strict:
            return 1
    else:
        print("✅ 关键导航节点检查通过 (AGENTS.md, QuickStart, README)")

    # 2. 全域收集文档
    md_files = collect_markdown_files(root_dir)
    print(f"🔍 检索到 {len(md_files)} 个公开文档，正在执行全域黑盒红线分析...")

    all_findings: List[Finding] = []
    for md_file in md_files:
        findings = scan_file_for_violations(md_file, root_dir)
        all_findings.extend(findings)

    # 3. 处理 Baseline 生成
    if args.generate_baseline:
        save_baseline(args.generate_baseline, all_findings)
        return 0

    # 4. Baseline 过滤
    baseline_fps: Set[str] = set()
    exempt_files: Set[str] = set()
    if args.baseline:
        baseline_fps, exempt_files = load_baseline(args.baseline)
        if baseline_fps or exempt_files:
            print(f"📋 已载入 Baseline: {len(exempt_files)} 个历史文件，{len(baseline_fps)} 条指纹豁免")

    new_fatal_findings: List[Finding] = []
    new_high_findings: List[Finding] = []
    baseline_findings_count = 0

    for f in all_findings:
        if f.fingerprint in baseline_fps or f.rel_path in exempt_files:
            baseline_findings_count += 1
        else:
            if f.rule.severity == "FATAL":
                new_fatal_findings.append(f)
            else:
                new_high_findings.append(f)

    print("-" * 80)
    print(
        f"📊 扫描完成: 累计发现 {len(all_findings)} 处特征匹配 "
        f"(历史 Baseline 豁免: {baseline_findings_count} 处, "
        f"新增 P0 阻断: {len(new_fatal_findings)} 处, "
        f"新增 P1 告警: {len(new_high_findings)} 处)"
    )
    print("-" * 80)

    # 5. 输出违规详情
    if new_fatal_findings or new_high_findings:
        print("\n🚨 发现未在 Baseline 中登记的文档安全契约违规：\n")
        
        for idx, f in enumerate(new_fatal_findings + new_high_findings, start=1):
            sev_tag = "🔴 [P0 FATAL]" if f.rule.severity == "FATAL" else "🟡 [P1 HIGH]"
            print(f"{idx}. {sev_tag} {f.rule.name} ({f.rule.rule_id})")
            print(f"   位置: {f.rel_path}:{f.line_number}")
            print(f"   内容: {f.line_content}")
            print(f"   修复建议: {f.rule.remediation}\n")

    # 6. 判定退出码
    if new_fatal_findings or (args.strict and new_high_findings):
        print("❌ 门禁拦截触发：存在未经授权的外仓私有路径或敏感实现泄露！")
        print("💡 建议：请清理上述文件中的绝对路径/内部包私有引用；若为存量计划且已评审，请更新 Baseline。")
        return 1

    if baseline_findings_count > 0:
        print(f"⚠️  当前有 {baseline_findings_count} 处历史存量问题处于 Baseline 豁免状态，请按计划排期脱敏清洗。")

    print("🎉 全域文档契约与黑盒隔离校验通过！未发现新增私有资产泄露。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
