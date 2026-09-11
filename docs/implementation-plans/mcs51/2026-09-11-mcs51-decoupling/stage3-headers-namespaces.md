# Stage3：头文件体系与命名空间归位

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S3-HEADERS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-09（桥裸包含/硬调用）、CPL-13（sfr_map）、CPL-14（公共头）、CPL-21（ADC0832）、CPL-24（前缀落地） |
| **前置依赖** | stage2（context 已纯净） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `REG_CMS8S78XX.H` 等下沉 `chips/cms8s78xx/include/`；`mcs51_sfr_map.h` 缩水为 Intel 标准，专有部分改 `CMS8S_` 前缀下沉。
- ✅ `wink_mcu.h` 上移出 51 框架，51 内仅留 51 路由；`wink_mcs51_wdt.h` 去 `cms8s_sys_notify` 硬导出。
- ✅ `mcs51_bridge.cpp` 去裸 include，TA 改钩子派发；ADC0832 下沉 `devices/`。
- ✅ 转发 shim 保留一版（告警），下阶段删除；前缀 lint 全绿。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/include/` | 🆕 | REG/cms8s 头 + `cms8s_sfr_map.h` + allowlist 下沉 |
| `include/mcs51_sfr_map.h` | ✏️ | 仅标准 SFR（`T2CON` 保留，`CKCON` 地址可提语义下沉） |
| `include/wink_mcu.h` | ✏️/搬迁 | 上移公共层 |
| `include/wink_mcs51_wdt.h` | ✏️ | 去厂商硬导出 |
| `src/mcs51_bridge.cpp` | ✏️ | 去裸 include，去 TA 硬调用 |
| `devices/adc0832/` | 🆕 | 状态机下沉，trap 接入 |

## 3. 任务拆分

### Task S3-1：寄存器表与厂商头下沉 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：建 `chips/cms8s78xx/include/` 并搬移 4 头 + allowlist；原路径留一版转发 shim（`#warning`）。
- [ ] **Step 2**：`mcs51_sfr_map.h` 删 `P0EXTIF/T34MOD/EIE2/EIF2/PS_*`，`T2CON` 保留。
- [ ] **Step 3**：`wink_mcu.h` 上移，51 内只剩 classic/cms8s 路由。

### Task S3-2：桥 + WDT + ADC0832 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：bridge 删 `ADC0832.H`/`cms8s_adc.h` 裸包含；`cms8s_sys_notify_sfr_write` 改为 `sfr_write_hooks` 派发。
- [ ] **Step 2**：`wink_mcs51_wdt.h` 去厂商导出；真通用语义保留，否则改名下沉。
- [ ] **Step 3**：ADC0832 移 `devices/adc0832/`，对外提供 `adc0832_device_attach(ctx, cs, clk, di, do)` 经 trap 挂载；需外挂的板型在自己的 post-init 钩子（`mcs51_framework_set_post_init_hook`）里调用，`MCS51_HAS_ADC0832` 作用域限器件/app。

## 4. 验收

- L0：全仓 include 链不断（shim 期零 error，告警可接受）。
- L4：公共 `include/` 无 `REG_CMS8S78XX.H`；bridge 编译不依赖芯片/器件头；`grep -rn 'MCS51_HAS_ADC0832' src/mcs51_bridge.cpp` 零命中（全仓仅 `devices/` 与 app 侧命中）；前缀 lint 全绿。

## 5. 风险与回滚

- R-03（include 断裂）：shim 缓解；回滚：先 revert 搬移 commit，shim 独立 commit 可单独回退。
