# Stage4：外设增强机制与私有外设剥离

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S4-PERIPH` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-03（GPIO）、CPL-04（UART）、CPL-05（Timer）、CPL-07（ExtInt）、CPL-10（自注册） |
| **前置依赖** | stage3（头文件已归位，钩子可用） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `cms8s_gpio/uart/timer/extint.cpp` 承接全部专有逻辑，通用文件经 `caps_cache` + 钩子外包。
- ✅ 通用 `mcs51_gpio/uart/timer` 全文零 `cms8s`/厂商地址；`mcs51_peripheral.cpp` 仅 core 三件套，芯片包自注册。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/src/cms8s_gpio.cpp` | 🆕 | PxxCFG/PxTRIS/PxUP/开漏 |
| `chips/cms8s78xx/src/cms8s_uart.cpp` | 🆕 | FUNCCR/PS_RXD |
| `chips/cms8s78xx/src/cms8s_timer.cpp` | 🆕 | Timer3/4 + W0C |
| `chips/cms8s78xx/src/cms8s_extint.cpp` | 🆕 | 端口中断 + 引脚选择 |
| `src/mcs51_gpio/uart/timer/extint.cpp` | ✏️ | 标准模型 + 快路径短路 |
| `src/mcs51_peripheral.cpp` | ✏️ | 自注册/按 target 编译 |

## 3. 任务拆分

### Task S4-1：GPIO + ExtInt `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`cms8s_gpio.cpp` 承接 TRIS/OD/UP/CFG；通用 GPIO 保留准双向，`caps` 短路后走钩子。
- [ ] **Step 2**：`cms8s_extint.cpp` 承接 `P0EXTIE`/`EICFG`/`PS_INT*`/vectors；通用仅 INT0/INT1。

### Task S4-2：UART + Timer + 注册 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：`cms8s_uart.cpp` 承接 FUNCCR/重映射；通用 UART 固定引脚 + 时钟回调。
- [ ] **Step 2**：`cms8s_timer.cpp` 承接 T3/T4/捕获/W0C；通用 Timer0/1/2。
- [ ] **Step 3**：peripheral 改自注册，芯片外设按 target 链入。

## 4. 验收

- L1：GPIO 热路径单测 assert 标准件零钩子调用。
- L4：`Select-String -Path src/mcs51_gpio.cpp,src/mcs51_uart.cpp,src/mcs51_timer.cpp -Pattern 'cms8s|0xF0'` 零命中。

## 5. 风险与回滚

- R：钩子时序回归 → 缓解：stage1 E-02 场景 + UART/Timer 回归全跑；回滚 `git revert <S4-commit>`（芯片新文件独立 commit）。
