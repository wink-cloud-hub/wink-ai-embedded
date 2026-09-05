# 轴 A — 外设物理源

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

传感器/执行器/总线数据从哪来

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 A 行。

## 2. 主机制（primary）

- [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) — 四通道数据面与外设选型（物理量从哪条通道进入固件）

## 3. 次机制（secondary）

- 配置面（注册表 / PinArbiter / Schema）→ [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md)
- 物理退化与总线故障注入（信号域，非电气 SPICE）→ [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)

## 4. 典型上限（展开版）

在 overview 缩略「不仿真电气前端；通道 3/4 多为未接线能力」之上展开：

1. **模型上限**：不仿真 ADC 量化前端、阻抗、电源完整性等电气特性；电气类结论须真机/HIL（生产口径见 [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)）。
2. **通道覆盖**：通道 1（Pin 边沿）与通道 2（Bus payload）为主路径；通道 1b（定时调制）仅作 PWM **duty 路由**（硬件周期/capture 语义属轴 C，见 [`C-timer-semantics.md`](./C-timer-semantics.md)）。
3. **尚未接线**：通道 3（Analog）与通道 4（Buffer/高吞吐）目前多为架构预留，不得当作端到端保真证据。
4. **UART**：事务级 / 主机 TX 可用；异步 RX、RX IRQ、按虚拟时钟的字节流时序尚未闭合——不得对 `dal_uart` RX 中断路径宣称 timing 同源（与轴 D timing 禁区交叉）。
5. **旁路纪律**：只替换物理量来源，禁止 DAL 业务捷径；超声波若仍走 `distanceCm`→μs 捷径，不得宣称通道 1 沿捕获已对齐。
6. **交叉宣称**：ECHO 沿捕获属 **A+B**（+ `timing` Accuracy Mode）；舵机/PWM 输出属 **A+C**；I2C 丢包/从机故障属 **A+D+F**。交叉示例见 overview [`02-axes-af`](../01-overview/02-axes-af.md)。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)（本页不写状态符）。

- **C1** — 业务因果与状态机逻辑
- **C7** — 总线协议帧 / CRC / 错误恢复
- **C17** — 外设资源互斥 / 时基耦合
- （通道相关）**C8** — DMA/总线异步传输窗口；**C18** — 总线故障态机
