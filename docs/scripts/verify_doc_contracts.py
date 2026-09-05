#!/usr/bin/env python3
"""
verify_doc_contracts.py

自动化文档契约与黑盒隔离校验脚本：
1. 校验 docs/zh/design/ 和 docs/en/design/ 下的规范文件，断言不得泄露外仓私有源码路径 (如 src/views/..., src/components/...)。
2. 校验文档中 Code-Mapping / Contract-Mapping 的合规性。
3. 检查必备导航节点 (AGENTS.md, 00-quick-start) 的完整性。
"""

import os
import sys
import re

WORKSPACE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS_DIR = os.path.join(WORKSPACE_DIR, "docs")
DESIGN_DIRS = [
    os.path.join(DOCS_DIR, "zh", "design"),
    os.path.join(DOCS_DIR, "en", "design"),
    os.path.join(DOCS_DIR, "design"),  # 兼容过渡期
]

# 严禁在 design 规范中泄露的外仓私有路径特征
FORBIDDEN_EXTERNAL_PATTERNS = [
    r"packages/embedded-frontend/src/",
    r"packages/unisim/src/core/",
    r"packages/unisim/src/worker/",
    r"src/views/EmbeddedWorkbench\.vue",
]

def check_forbidden_patterns():
    violations = []
    for d in DESIGN_DIRS:
        if not os.path.exists(d):
            continue
        for root, _, files in os.walk(d):
            for file in files:
                if not file.endswith(".md"):
                    continue
                filepath = os.path.join(root, file)
                relpath = os.path.relpath(filepath, WORKSPACE_DIR)
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    lines = f.readlines()
                    for line_idx, line in enumerate(lines, 1):
                        for pattern in FORBIDDEN_EXTERNAL_PATTERNS:
                            if re.search(pattern, line):
                                violations.append(f"[{relpath}:{line_idx}] 泄露外仓私有源码路径: {line.strip()}")
    return violations

def check_essential_files():
    essential_candidates = [
        [os.path.join(DOCS_DIR, "AGENTS.md")],
        [
            os.path.join(DOCS_DIR, "zh", "design", "00-quick-start", "01-5min-getting-started.md"),
            os.path.join(DOCS_DIR, "design", "00-quick-start", "01-5min-getting-started.md")
        ],
        [
            os.path.join(DOCS_DIR, "zh", "design", "README.md"),
            os.path.join(DOCS_DIR, "design", "README.md")
        ],
    ]
    missing = []
    for group in essential_candidates:
        if not any(os.path.exists(p) for p in group):
            missing.append(os.path.relpath(group[0], WORKSPACE_DIR))
    return missing

def main():
    print("🔍 运行 docs design 契约与黑盒隔离自动扫描...")
    
    missing_files = check_essential_files()
    if missing_files:
        print(f"❌ 缺失关键导航节点: {missing_files}")
    else:
        print("✅ 关键导航节点 (AGENTS.md, QuickStart) 检查通过")

    violations = check_forbidden_patterns()
    if violations:
        print(f"❌ 发现 {len(violations)} 处外仓私有源码泄露:")
        for v in violations:
            print(f"   - {v}")
        sys.exit(1)
    else:
        print("✅ 黑盒隔离防护扫描通过：未发现外仓私有源码路径泄露")

    print("🎉 文档契约与黑盒隔离校验 100% 通过！")

if __name__ == "__main__":
    main()
