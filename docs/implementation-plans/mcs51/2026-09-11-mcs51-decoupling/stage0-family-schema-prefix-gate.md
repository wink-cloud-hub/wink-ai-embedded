# Stage0：描述符 v2 schema 与命名门禁先行（无生产行为变更）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S0-SCHEMA` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🔴 P0（后续全部阶段的前置依赖） |
| **关联 CPL** | CPL-20（schema）、CPL-24（前缀/枚举/knob 门禁） |
| **前置依赖** | 无 |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ 冻结 `McuFamilyDescriptor v2`：`port_pin_masks`、`irq_vector_table + irq_count`、`wdt_present`、`iap_present`、`uart_count`、`timer_caps`、`capabilities`；`Mcu51Context` 增 `caps_cache`。
- ✅ 前缀门禁落地：`wink_mcs51_*` 仅通用，厂商 `cms8s_*`、板级 `board_*`，STRICT 枚举按家族拆分，lint 可执行。
- ✅ 零行为变更：机制文件语义不变，40/40 全绿。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `frameworks/mcs51/include/mcs51_family.h` | ✏️ | v2 字段 + `MCS51_CAP_*` 位定义 |
| `frameworks/mcs51/include/mcs51_context.h` | ✏️ | 增 `caps_cache`（只读缓存，不搬状态） |
| `frameworks/mcs51/src/mcs51_family.cpp` | ✏️ | 补两家族新字段值，只加行 |
| `frameworks/mcs51/src/mcs51_context.cpp` | ✏️ | reset/set_family 快照 `caps_cache` |
| `frameworks/mcs51/tools/lint/` | ✏️ | 前缀 + 通用头 vendor 残留扫描规则 |
| `frameworks/mcs51/include/wink_mcs51_strict.h` | ✏️ | 枚举按家族作用域拆分（仅重命名/分组，不改触发语义） |

架构红线：本阶段禁动任何外设行为分支；禁改 `CMakeLists.txt` 目标结构。

## 3. 任务拆分

### Task S0-1：冻结 v2 schema + 快照 `[状态: ⏳ 待开始]`

| 字段 | 内容 |
|------|------|
| **优先级** | 🔴 P0 |
| **修改文件** | `mcs51_family.h`、`mcs51_context.h`、`mcs51_family.cpp`、`mcs51_context.cpp` |

- [ ] **Step 1**：descriptor 加 `capabilities`、`port_pin_masks[4]`、`irq_vector_table` 指针 + `irq_count`、`wdt_present`、`iap_present`、`uart_count`、`timer_caps`；context 加 `caps_cache`。
- [ ] **Step 2**：`mcs51_family.cpp` 补 classic/cms8s78xx 两行值（CMS8S：`{8,8,6,4}` 掩码、28 向量、WDT/IAP present；classic：全 8 脚、6 向量、WDT/IAP absent）。
- [ ] **Step 3**：reset/set_family 快照 `caps_cache = desc->capabilities`；加单测断言快照一致。

验证：新增字段单测通过；`grep -n "caps_cache"` 仅 3 处（定义/快照/读取点位）。

### Task S0-2：前缀 + STRICT 门禁 `[状态: ⏳ 待开始]`

| 字段 | 内容 |
|------|------|
| **优先级** | 🔴 P0 |
| **前置依赖** | Task S0-1 |

- [ ] **Step 1**：lint 新增规则：通用 `include/mcs51_*`、`wink_mcs51_*` 命中 `cms8s\|CMS8S\|0xF0\|ADCLDO\|FUNCCR\|PS_` 即 fail（白名单仅 schema 字段名）。
- [ ] **Step 2**：STRICT 枚举分组注释（通用 vs `CMS8S_FEAT_*` vs `BOARD_FEAT_*`），保持数值稳定不 renumber。
- [ ] **Step 3**：`WINK_MCS51_XDATA_SIZE` 标记为待下沉（本阶段只加 `TODO(stage6)` 注释，不搬）。

验证：`python wink-tools/wink.py lint arch --pack layering --pack api`（或等效 lint 入口）通过；故意放一个越权前缀能 fail。

## 4. 验收（L0-L4 精简）

- L0：双轨测试全绿；lint 全绿。
- L1：schema 单测覆盖率 100% 字段。
- L3：总纲 §2 状态更新为执行中。
- L4：确认无行为变更 diff（除快照字段外 `git diff` 无语义改动）。

## 5. 风险与回滚

- R：STRICT 重命名误改数值 → 缓解：数值 `static_assert` 锁死。
- 回滚：`git revert <S0-commit>`；lint 规则文件独立提交，可单独 revert。
