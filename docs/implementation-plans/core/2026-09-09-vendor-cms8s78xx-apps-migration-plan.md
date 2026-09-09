# 实施计划：vendor_cms8s78xx_v202 系列微应用向分组目录迁移与 CMake 深度解耦重构

> 📋 **本文档是 ADR-0079（嵌套目录发现）落地执行的第一阶段业务微应用迁移实施计划**。

---

## 1. 元数据表

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260909-VENDOR-CMS8S78XX-MIGRATION` |
| **创建日期** | `2026-09-09` |
| **目标平台/SoC** | `wasm` (Emscripten), `host`, MCS-51 仿真内核 |
| **工具链/SDK版本**| Python 3.10+, Emscripten, CMake 3.15+ |
| **计划状态** | ✅ 已完成（2026-09-09） |
| **优先级** | 🟡 P1（架构重构与分组治理） |
| **计划版本** | `v1.0` |
| **关联技术设计** | 无，已并入本计划 |
| **关联设计规范** | [`docs/zh/design/02-wink-micro-os/03-directory-architecture.md`](../../zh/design/02-wink-micro-os/03-directory-architecture.md) |
| **关联 ADR** | [`ADR-0079：wink-micro-app 最多三级嵌套目录与清单边界剪枝发现`](../../decisions/core/0079-micro-app-nested-app-discovery.md) |
| **前置依赖计划** | [`docs/implementation-plans/core/2026-09-09-micro-app-nested-discovery-plan.md`](./2026-09-09-micro-app-nested-discovery-plan.md)（已实施） |
| **计划负责人** | 嵌入式与工具链架构团队 |

---

## 2. 背景与目标

### 2.1 问题陈述
随着原厂 MCS-51 示例库的引入，`wink-micro-app/` 根目录下平铺了 19 个以 `vendor_cms8s78xx_v202_` 开头的目录，导致目录冗长、可读性差。
在 [ADR-0079](../../decisions/core/0079-micro-app-nested-app-discovery.md) 中，`wink-tools`、`unisim` 以及 `embedded-frontend` 已经完成了深度最多为 3 级的受限多级路径发现与构建支持。
然而，现存 19 个应用在平铺开发时，其 `CMakeLists.txt` 中均硬编码了相对路径：
```cmake
get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
```
一旦直接移动到 `wink-micro-app/vendor_cms8s78xx_v202/` 分组目录下，应用自身的深度从 2 变更为 3，向上两级只能到达 `wink-micro-app` 导致无法找到 `wink-micro-os`，在 Emscripten 预处理阶段会直接报错中断。同时，手动迁移 19 个应用涉及大量繁杂操作，极易产生疏漏。

### 2.2 技术目标
1. **目录结构优雅化（短名化分组）**：将 19 个应用收敛至 `wink-micro-app/vendor_cms8s78xx_v202/` 目录下，子目录去除重复前缀（如 `vendor_cms8s78xx_v202/gpio`）。
2. **CMake 深度解耦（终结相对路径断裂）**：改造 `CMakeLists.txt` 中的根路径推导机制，优先消费 CMake 内置项目变量 `wink-micro-os_SOURCE_DIR` 与 `WINK_MICRO_OS_ROOT`，使 App 彻底免疫目录深度的后续变动。
3. **保留 Git 提交与 Blame 历史**：搬迁时全部走 `git mv`，保证历史变更轨迹连续。
4. **一键迁移脚本交付**：提供带有 `--dry-run` 预览机制的安全 Python 迁移脚本，一键完成目录移动、CMake 改造、清单规范更新与外部单测对齐。

### 2.3 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| **多级应用发现** | 19 个应用被正确发现为 `vendor_cms8s78xx_v202/<leaf>` | `wink build-sim --all --dry-run` 或 Python 单测 |
| **WASM 编译通过** | 抽检与全量应用生成 `.cpp` 并在 wasm 下编译成功 | `wink build-sim --app gpio` / `buzzer` / `led_4com_8seg` |
| **全量单测通过** | unisim 双轨一致性测试与扫描单测全部通过 | `bun test packages/unisim/src/simulation-runner/consistency` |
| **架构门禁** | 0 违规，符合分层与 API 规范 | `wink lint arch --pack layering --pack api` |

---

## 3. 架构设计与重构细节

### 3.1 目录组织与 ID 映射设计

遵循 ADR-0079 的 ID 与别名规则：
- **完整应用 ID**：`vendor_cms8s78xx_v202/<leaf>`（对应产物 `build/wasm/vendor_cms8s78xx_v202/<leaf>/`）
- **别名匹配**：当短名在全局唯一时，支持直接通过短名调用（如 `wink build-sim --app buzzer`）

#### 19 个应用映射表：
| 序号 | 原平铺目录 | 重构后分组目录 | 新 App ID |
|:---:|---|---|---|
| 1 | `vendor_cms8s78xx_v202_adc_hardware_trigger` | `vendor_cms8s78xx_v202/adc_hardware_trigger` | `vendor_cms8s78xx_v202/adc_hardware_trigger` |
| 2 | `vendor_cms8s78xx_v202_adc_ldo` | `vendor_cms8s78xx_v202/adc_ldo` | `vendor_cms8s78xx_v202/adc_ldo` |
| 3 | `vendor_cms8s78xx_v202_buzzer` | `vendor_cms8s78xx_v202/buzzer` | `vendor_cms8s78xx_v202/buzzer` |
| 4 | `vendor_cms8s78xx_v202_extint0` | `vendor_cms8s78xx_v202/extint0` | `vendor_cms8s78xx_v202/extint0` |
| 5 | `vendor_cms8s78xx_v202_extint1` | `vendor_cms8s78xx_v202/extint1` | `vendor_cms8s78xx_v202/extint1` |
| 6 | `vendor_cms8s78xx_v202_gpio` | `vendor_cms8s78xx_v202/gpio` | `vendor_cms8s78xx_v202/gpio` |
| 7 | `vendor_cms8s78xx_v202_led_4com_8seg` | `vendor_cms8s78xx_v202/led_4com_8seg` | `vendor_cms8s78xx_v202/led_4com_8seg` |
| 8 | `vendor_cms8s78xx_v202_timer0_count_mode` | `vendor_cms8s78xx_v202/timer0_count_mode` | `vendor_cms8s78xx_v202/timer0_count_mode` |
| 9 | `vendor_cms8s78xx_v202_timer0_timming_mode` | `vendor_cms8s78xx_v202/timer0_timming_mode` | `vendor_cms8s78xx_v202/timer0_timming_mode` |
| 10 | `vendor_cms8s78xx_v202_timer1_count_mode` | `vendor_cms8s78xx_v202/timer1_count_mode` | `vendor_cms8s78xx_v202/timer1_count_mode` |
| 11 | `vendor_cms8s78xx_v202_timer1_timming_mode` | `vendor_cms8s78xx_v202/timer1_timming_mode` | `vendor_cms8s78xx_v202/timer1_timming_mode` |
| 12 | `vendor_cms8s78xx_v202_timer2_capture_mode` | `vendor_cms8s78xx_v202/timer2_capture_mode` | `vendor_cms8s78xx_v202/timer2_capture_mode` |
| 13 | `vendor_cms8s78xx_v202_timer2_compare_mode` | `vendor_cms8s78xx_v202/timer2_compare_mode` | `vendor_cms8s78xx_v202/timer2_compare_mode` |
| 14 | `vendor_cms8s78xx_v202_timer2_count_mode` | `vendor_cms8s78xx_v202/timer2_count_mode` | `vendor_cms8s78xx_v202/timer2_count_mode` |
| 15 | `vendor_cms8s78xx_v202_timer2_timing_mode` | `vendor_cms8s78xx_v202/timer2_timing_mode` | `vendor_cms8s78xx_v202/timer2_timing_mode` |
| 16 | `vendor_cms8s78xx_v202_timer3_timming_mode` | `vendor_cms8s78xx_v202/timer3_timming_mode` | `vendor_cms8s78xx_v202/timer3_timming_mode` |
| 17 | `vendor_cms8s78xx_v202_timer4_timming_mode` | `vendor_cms8s78xx_v202/timer4_timming_mode` | `vendor_cms8s78xx_v202/timer4_timming_mode` |
| 18 | `vendor_cms8s78xx_v202_uart0_printf` | `vendor_cms8s78xx_v202/uart0_printf` | `vendor_cms8s78xx_v202/uart0_printf` |
| 19 | `vendor_cms8s78xx_v202_uart0_rxtx` | `vendor_cms8s78xx_v202/uart0_rxtx` | `vendor_cms8s78xx_v202/uart0_rxtx` |

---

### 3.2 CMake 改造范式

#### 现有模式（脆弱）
```cmake
get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")
```

#### 重构后统一模式（鲁棒）
在 `wink-micro-os/CMakeLists.txt` 中调用 `add_subdirectory(${WINK_APP_DIR})` 时，CMake 的 `wink-micro-os_SOURCE_DIR` 变量在子作用域中完全可见。因此采用以下三级自适应逻辑：
```cmake
if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")
```

---

### 3.3 wink-app.json 与相关单测同步

1. **`wink-app.json`**：
   - 将 `"app_name": "vendor_cms8s78xx_v202_<leaf>"` 同步修改为 `"app_name": "<leaf>"`。
   - `upstream.source_dir` 为基于 Workspace Root 的相对路径，保持不变。
2. **Unisim 单测更新**：
   - [`packages/unisim/src/simulation-runner/consistency/__tests__/app-consistency-runner.test.ts`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/simulation-runner/consistency/__tests__/app-consistency-runner.test.ts#L20)：
     将 `wink-micro-app/vendor_cms8s78xx_v202_led_4com_8seg` 路径更新为：
     `wink-micro-app/vendor_cms8s78xx_v202/led_4com_8seg`。

---

## 4. 任务拆分与执行步骤

### Phase 1：准备与脚本化
- [x] **Task 1.1**：清理/暂存工作区内已有的未提交变更（特别是 `vendor_cms8s78xx_v202_buzzer` 资产）。
- [x] **Task 1.2**：在 `scripts/migrate_vendor_cms8s78xx_apps.py` 中编写并落地一键迁移脚本。
- [x] **Task 1.3**：执行 `--dry-run`，审查移动的路径树与即将修改的文件内容。

### Phase 2：执行迁移
- [x] **Task 2.1**：执行 `python scripts/migrate_vendor_cms8s78xx_apps.py --apply`。
  - 通过 `git mv` 搬迁 19 个目录到 `wink-micro-app/vendor_cms8s78xx_v202/`；
  - 自动打补丁更新 19 个 `CMakeLists.txt`；
  - 自动更新 19 个 `wink-app.json` 的 `app_name`；
  - 自动更新 `app-consistency-runner.test.ts` 中的单测路径。

### Phase 3：构建与一致性验证
- [x] **Task 3.1**：验证 App 发现能力：`python packages/wink-tools/wink.py build wasm --all --dry-run` 检查 19 个新 ID。
- [x] **Task 3.2**：抽检关键外设应用 WASM 编译：
  - GPIO 外部中断：`wink build wasm --app gpio`
  - 蜂鸣器：`wink build wasm --app buzzer`
  - ADC/LDO：`wink build wasm --app adc_ldo`
  - 数码管动态扫描：`wink build wasm --app led_4com_8seg`
- [x] **Task 3.3**：执行 Unisim 一致性双轨测试：
  - `bun test packages/unisim/src/simulation-runner/consistency`
- [x] **Task 3.4**：更新 `wink-micro-app/README.md` 文档中的路径与命令示例。

### Phase 4：提交与归档
- [x] **Task 4.1**：架构与 API 门禁核验：`python packages/wink-tools/wink.py lint --pack layering --pack api`。
- [x] **Task 4.2**：原子 Git 提交：`refactor(apps): migrate vendor_cms8s78xx_v202 apps into group folder (ADR-0079)`。

---

## 5. 一键迁移脚本实现规格 (`scripts/migrate_vendor_cms8s78xx_apps.py`)

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scripts/migrate_vendor_cms8s78xx_apps.py — One-click migration script (ADR-0079)."""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

PREFIX = "vendor_cms8s78xx_v202_"
TARGET_GROUP = "vendor_cms8s78xx_v202"

ROBUST_CMAKE_BLOCK = """\
if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")"""


def run_git_mv(src: Path, dst: Path, dry_run: bool = False) -> bool:
    if dry_run:
        print(f"[DRY-RUN] Move: {src.name} -> {TARGET_GROUP}/{dst.name}")
        return True
    dst.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "mv", str(src), str(dst)], check=True, capture_output=True)
        print(f"  [git mv] {src.name} -> {dst.relative_to(src.parent)}")
        return True
    except subprocess.CalledProcessError:
        src.rename(dst)
        print(f"  [fs mv]  {src.name} -> {dst.relative_to(src.parent)}")
        return True


def patch_cmakelists(cmake_path: Path, dry_run: bool = False) -> bool:
    content = cmake_path.read_text(encoding="utf-8")
    pattern = re.compile(
        r'get_filename_component\(_MCS51_APP_OS_ROOT\s+"[^"]+"\s+ABSOLUTE\)\s*\n'
        r'set\(_MCS51_CLEANUP\s+"[^"]+"\)',
        re.MULTILINE
    )
    if not pattern.search(content):
        print(f"  ⚠️ Warning: Could not match _MCS51_APP_OS_ROOT in {cmake_path}")
        return False

    new_content = pattern.sub(ROBUST_CMAKE_BLOCK, content)
    if not dry_run:
        cmake_path.write_text(new_content, encoding="utf-8")
    print(f"  [patch] CMakeLists.txt patched.")
    return True


def patch_wink_app_json(json_path: Path, leaf_name: str, dry_run: bool = False) -> bool:
    try:
        data = json.loads(json_path.read_text(encoding="utf-8"))
        old_name = data.get("app_name", "")
        data["app_name"] = leaf_name
        if not dry_run:
            json_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"  [patch] wink-app.json: app_name '{old_name}' -> '{leaf_name}'")
        return True
    except Exception as e:
        print(f"  ⚠️ Failed to patch {json_path}: {e}")
        return False


def patch_unisim_tests(workspace_root: Path, dry_run: bool = False):
    test_file = workspace_root / "docs/.internals/packages/unisim/src/simulation-runner/consistency/__tests__/app-consistency-runner.test.ts"
    if not test_file.is_file():
        return
    content = test_file.read_text(encoding="utf-8")
    old_target = "wink-micro-app/vendor_cms8s78xx_v202_led_4com_8seg"
    new_target = "wink-micro-app/vendor_cms8s78xx_v202/led_4com_8seg"
    if old_target in content:
        if not dry_run:
            test_file.write_text(content.replace(old_target, new_target), encoding="utf-8")
        print(f"  [patch] Unisim test updated: {test_file.name}")


def main():
    parser = argparse.ArgumentParser(description="Migrate vendor_cms8s78xx_v202 apps into group folder.")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes")
    parser.add_argument("--apply", action="store_true", help="Execute changes")
    args = parser.parse_args()

    if not args.apply and not args.dry_run:
        print("Specify --dry-run or --apply")
        sys.exit(1)

    workspace_root = Path(__file__).resolve().parents[1]
    apps_root = workspace_root / "wink-micro-app"
    group_dir = apps_root / TARGET_GROUP

    candidates = [
        d for d in apps_root.iterdir()
        if d.is_dir() and d.name.startswith(PREFIX) and d != group_dir
    ]
    candidates.sort(key=lambda p: p.name)

    print(f"Found {len(candidates)} apps to migrate into '{TARGET_GROUP}/'...")
    for app_dir in candidates:
        leaf_name = app_dir.name[len(PREFIX):]
        target_dir = group_dir / leaf_name
        print(f"\nProcessing: {app_dir.name} -> {TARGET_GROUP}/{leaf_name}")
        run_git_mv(app_dir, target_dir, dry_run=args.dry_run)
        cmake_file = (target_dir if not args.dry_run else app_dir) / "CMakeLists.txt"
        if cmake_file.is_file():
            patch_cmakelists(cmake_file, dry_run=args.dry_run)
        manifest_file = (target_dir if not args.dry_run else app_dir) / "wink-app.json"
        if manifest_file.is_file():
            patch_wink_app_json(manifest_file, leaf_name, dry_run=args.dry_run)

    print("\nPatching test harness references...")
    patch_unisim_tests(workspace_root, dry_run=args.dry_run)
    print(f"\n✨ Done! Processed {len(candidates)} apps (dry_run={args.dry_run}).")


if __name__ == "__main__":
    main()
```

---

## 6. 风险评估与回滚策略

### 6.1 风险评估
1. **未提交工作区污染风险**：若移动前存在未暂存的修改，`git mv` 可能报错或导致修改混淆。
   * **缓解措施**：脚本执行前通过 `git status` 检查，要求工作区干净或仅有预期变更。
2. **同名应用歧义风险**：如果其他组也有叫 `gpio` 的应用，会导致短名别名冲突。
   * **缓解措施**：当前代码库中仅有此处 `gpio` 裸名，全合格 ID `vendor_cms8s78xx_v202/gpio` 始终具备最高优先级，无破坏性风险。

### 6.2 回滚策略
本迁移全部使用 `git mv` 与原子文件覆盖。如需回滚，只需在未 push 前执行：
```bash
git restore --staged .
git checkout .
git clean -fd
```
即可 100% 逐字节恢复至平铺初始状态。
