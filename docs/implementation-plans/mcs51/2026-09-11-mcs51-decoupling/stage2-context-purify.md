# Stage2：`Mcu51Context` 数据结构纯净化

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S2-CONTEXT` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🔴 P0 |
| **关联 CPL** | CPL-11（soc_priv）、CPL-12（引脚掩码）、CPL-18（残留结构/isr/xdata）、CPL-19（复位播种） |
| **前置依赖** | stage0（caps_cache）、stage1（rail 接口已稳） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `adc0832/sysProt/buzzer/cms8sAdc` 移出通用体，经 `soc_priv` 按实例挂载。
- ✅ T3/T4/捕获比较、端口采样移入 `cms8s priv`；`classicBus` 移入 `at89 priv`；`isr_table`/XDATA 按描述符裁剪或文档化超配。
- ✅ 通用复位仅 Intel 标准种子；XSFR/ADCLDO/CKCON/Fosc 下放芯片包；厂商命名宏下沉。
- ✅ `{8,8,6,4}` 改读 `port_pin_masks`；`sizeof` 不净增；双 context 不串扰。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `include/mcs51_context.h` | ✏️ | 删厂商/板级状态与厂商宏，加 `instance_index` + 留 `soc_priv(void*)` + 标准状态；禁 include 任何厂商 priv 头 |
| `src/mcs51_context.cpp` | ✏️ | 标准种子 only + caps 快照 + `instance_index` 分配/重绑；XSFR/rail 播种删除 |
| `chips/cms8s78xx/include/cms8s_priv.h` | 🆕 | priv 结构（sys/buzzer/adc/T3T4/端口采样） |
| `chips/at89c52/include/at89_priv.h` | 🆕 | `classicBus` 归位 |
| `src/mcs51_timer.cpp` | ✏️ | 引脚掩码读描述符 |

架构红线：**soc_priv 锁定方案 A（按实例 BSS 池），否决 union 方案**——union 要求通用头 include 厂商 priv 头以确定大小，直接击穿 CPL-11/14；禁全局单例；禁堆。

## 3. 任务拆分

### Task S2-1：soc_priv 挂载（方案 A 锁定） `[状态: ⏳ 待开始]`

- [ ] **Step 0**：通用头加 `uint8_t instance_index`（纯通用字段）+ `MCS51_MAX_INSTANCES` 上限常量；`reset`/`set_family` 按 index 重绑 `soc_priv` + `memset` 对应池槽，family 切换时先解绑旧槽再绑定新槽。
- [ ] **Step 1**：建 `cms8s_priv.h` / `at89_priv.h`；芯片源内定义 `static Priv s_priv_pool[MCS51_MAX_INSTANCES]`，按 `ctx->instance_index` 分配，`ctx->soc_priv = &pool[idx]`。
- [ ] **Step 2**：通用体删 4 状态 + `adc_vref/vrail`（rail 默认改由芯片 reset 注入，通用 `mcs51_adc_reset` 仅清注入表）。
- [ ] **Step 3**：双 context 串扰单测（互写 XSFR/ADC 状态不互相污染；`instance_index` 越界钳位单测）。

### Task S2-2：残留结构与复位播种 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：T3/T4/比较/端口采样移入 cms8s priv；classicBus 移入 at89 priv。
- [ ] **Step 2**：`isr_table`/XDATA 按 `irq_count`/xram 裁剪或书面超配理由 + `static_assert` 预算。
- [ ] **Step 3**：通用 reset 删 `PS_*=0x7F` 与 `3000/3000` 播种；`MCS51_XRAM_SIZE_CMS8S78XX` 下沉。
- [ ] **Step 4**：timer 引脚合法性读 `port_pin_masks`，经典 P3.4/P3.5 可用。

### Task S2-0：`sizeof` 基线测量与分家族预算 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：基线测量并记录（迁移前唯一一次）：`static_assert` + 单测打印 `sizeof(Mcu51Context)` 及主要成员偏移（`timer/extint/uart/isr_table/xdata_shadow`），写入本计划 §4 预算表（当前约 68KB，实测为准）。
- [ ] **Step 2**：冻结预算：classic 实例与 CMS8S 实例拆分后各自 `sizeof` 不得超过基线；`xdata_shadow`/`isr_table` 若保留超配必须在 §4 写明理由 + 上限。

## 4. 验收

- L1：通用头零厂商结构/宏；预算表已填实测数（基线/拆分后 classic/拆分后 CMS8S 三行），拆分后两行均 ≤ 基线。
- L2：经典复位后 XDATA 无 XSFR 残留；经典 P3.4/P3.5 计数可用。
- L4：`grep -Ei 'cms8s|adc0832|0xF0' include/mcs51_context.h src/mcs51_context.cpp` 零命中；`grep -Ei '#include.*(cms8s|at89)_priv' include/mcs51_context.h` 零命中（union 穿透回归哨兵）。

## 5. 风险与回滚

- R-02（串扰/超限）：BSS 池 + 预算断言缓解；回滚 `git revert <S2-commit>`，priv 头独立提交可单独回退。
