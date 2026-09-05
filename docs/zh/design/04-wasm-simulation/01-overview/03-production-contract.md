# 生产口径与保真边界

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / overview） |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | 「A~F 完备 ≠ 虚实恒等」**正式口径唯一措辞**；可验 / 不可验 / 旁路纪律摘要 |
| 管辖 ADR | [0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) |
| 迁自 | `04-wasm-simulation-2.0/00-README.md` §2–§3；`11-consistency-spec.md` §0.5（去重合并；2.0 已删除，纯文本溯源） |
| 上次核对 | 2026-08-02 |

> **引用纪律**：其它文件**禁止**另写一套产品承诺；须链回本文件。场景可测状态见 [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)；机制与预言见 [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。

---

## 1. 生产口径（A~F 完备 ≠ 虚实恒等）

落实轴 A~F **不等于**「仿真 ≡ 真机」或「可免真机发版」。这是产品承诺的正式口径（ADR 依据 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)）。

| 主张 | A~F 全落地后 |
|---|---|
| 仿真作为 CI / 低代码主路径的**高置信行为级预检**（漏检可分类、假绿可解释、🚫 强制真机） | **可以接近** |
| 仿真结果可**替代**真机 / HIL 作为放行依据 | **否** |
| 仿真与真机 **bit / μs 级恒等** | **永不承诺** |

```text
A～F 完备
  → 生产级「行为/协议/资源预检流水线」 ✅
  → 生产级「虚实结果恒等 / 免真机放行」 ❌

残余不一致 → 用 checklist + HIL/真机门禁 + ADR-0003 口径管理，而不是消灭
```

---

## 2. 仍必然存在的不一致（模型上限，非「没做完」）

| 来源 | 说明 |
|---|---|
| 电气 / 模拟 | ADC 量化、阻抗、电源完整性、晶振漂移（C11，多属 🚫 非目标） |
| 中断模型上限 | 协作式 IRQ ≠ NVIC 抢占嵌套（C4）；默认 tick≈10ms 延迟；仿真不能证明无逃逸 / 硬实时 IRQ |
| 定时器 / 快环上限 | 软步进 / 虚拟时钟 ≠ 芯片硬 ISR、PWM–ADC 硬件触发（C10） |
| 同刻事件总序 | [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md) 已钉契约；跨源 bit-exact 反测仍 Planned |
| UART 异步 RX | [ADR-0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)：事务级 Partial；字节流+RX IRQ **Planned** |
| 浮点 / golden | [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)：host↔wasm 默认 tolerance；禁 fast-math 构建核查 Planned |
| 多核 / 微架构 | 单虚拟核；cache / DMA 争用 / silicon errata → 真机（C9/C24） |
| 宿主环境 | Worker、Asyncify 挂起点改变墙钟观感；**逻辑时序靠虚拟时钟，不靠墙钟对齐** |

> 上表若干项的闭环任务见 [`implementation-plans/2026-08-02-unisim3-mechanisms-review-closure-plan.md`](../../../implementation-plans/unisim/2026-08-02-unisim3-mechanisms-review-closure-plan.md)。

---

## 3. 会显著收敛的不一致

- 轴 A + 同源铁律：换算 / 超时 / 协议打包 / 错误恢复在仿真中真实执行。
- 轴 B：去抖、超时、周期任务在 `s_virtual_us` 下可复现。
- 轴 C/F 门禁子集：引脚与定时器资源冲突、非法阻塞、配额 / Fault Fail-Loud。
- 轴 E 协作子集：饿死、软 WDT、部分共享状态问题更早暴露。
- 轴 F + [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)：不可验场景显式 🚫，避免「仿真绿灯 = 真机安全」。

---

## 4. 行为级边界

Wink-AI 仿真为**行为级（causal）高保真**：保证业务逻辑的因果顺序与逻辑正确性，**不保证** cycle/tick 级时序保真与电气保真（[ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)）。

### 4.1 可验证

对应轴落地且场景为 ✅/🟡 时：

- 业务状态机
- 经 PAL 同源路径的传感器/执行器语义
- I2C/UART **payload** 级协议
- 超时/断线等异常处理
- 虚拟时钟下的逻辑时序子集

### 4.2 不可验证（或仅弱近似）

- 硬实时微秒响应
- **中断抢占与优先级嵌套**
- 芯片级 HW timer / FOC 硬 ISR
- 多核 SMP 真并发
- 模拟电路（ADC 量化/阻抗/电源）
- 指令/微架构级行为

### 4.3 旁路纪律

- 旁路必须落在 PAL（物理量来源替换）；**禁止** DAL 业务直通 `#ifdef`。
- 语义旁路须过 JSON 门禁（[ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)）。
- 超声波等若仍走 C 侧 `distanceCm`→μs 捷径（deprecated），不得宣称通道 1 沿捕获已对齐——见 [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)。

时序 / 并发 / 电气级交付以真机或 HIL 为准。

