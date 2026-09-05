# 仿真多轴总览（正交维度 A~F）

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / overview） |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | **A~F 字母与含义的唯一定义处**；跨轴对照表（上限为**缩略**） |
| 不写 | 机制算法细节；上限展开长文（→ [`03-axes/`](../03-axes/)）；场景状态 |
| 迁自 | `04-wasm-simulation-2.0/00-README.md` §1（2.0 已删除，纯文本溯源） |
| 上次核对 | 2026-08-02 |

仿真能力按下列正交轴理解与验收。各轴独立演进；宣称「高一致」时**必须指明覆盖了哪些轴**。A~F 的定义在本目录**仅此一处**。

| 轴 | 回答的问题 | 主要机制 | 主文档（3.0） | 典型上限（缩略） |
|---|---|---|---|---|
| **A. 外设物理源** | 传感器/执行器/总线数据从哪来 | 四通道 + PWM 子类（Pin / Bus / Analog / Buffer）；PinArbiter；Plugin | [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)、[`07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md)（secondary） | 不仿真电气前端；通道 3/4 多为 Planned |
| **B. 时间基** | delay / 超时 / 脉宽以谁为钟 | `s_virtual_us` SSOT；单一 Gate；禁止 `pal_delay` 双步进 | [`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md)；C2/C14 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | 非墙钟实时；快进改变可观测墙钟时长 |
| **C. 定时器硬件语义** | HW timer / PWM 周期 / capture | PAL timer；软步进近似；资源独占门禁 | [`09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md)；C10/C17 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)、[ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) | 无 10kHz+ 硬 ISR；FOC 快环行为级 |
| **D. 中断模型** | ISR 何时跑、能否抢占嵌套 | Asyncify 协作插入；IRQ 队列 Poll（**非真抢占**） | [`04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md)；C4/C15/C20 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | **不可**验优先级嵌套 / 硬实时抢占 |
| **E. 调度与并发** | 多任务、阻塞、临界区、多核 | 协作式单虚拟核调度器 | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md)；C3/C5/C9/C16 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | SMP / 真抢占 → 真机 |
| **F. 故障与观测** | OOM、WDT、竞态、Trace | Fault 策略、lint/门禁、场景清单、观测平面 | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md)、[`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)（secondary accuracy）、[`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)、[`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) | 清单中 🚫 项必须 HIL |

```text
固件 C (App/BAL/DAL 同源编译)
        │
        ├─ A 外设物理源   ← 08 四通道 / 07 UniSim Plugin / PinArbiter
        ├─ B/C 时间与定时器 ← 02 VirtualClock / 09 定时器语义
        ├─ D 中断/回调序   ← 04 Asyncify + IRQ Poll
        ├─ E 调度与共享状态 ← 03 协作调度 / 单虚拟核
        └─ F 故障与门禁    ← 05/06 + assurance/01/02 + lint + 11 观测
                ↓
         真机 / HIL（电气、硬实时、多核…）
```

**交叉示例**：轴自称正交，但保真承诺往往落在交叉点；典型组合：

| 场景 | 涉及轴 | 关键约束 |
|---|---|---|
| 超声波 ECHO 沿捕获 | **A + B**（+ Accuracy Mode） | ECHO 沿走通道 1 + `s_virtual_us` 量脉宽；必须用 `timing` Accuracy Mode。仅靠 Hub 注入 `distanceCm` 捷径**不得**宣称沿捕获一致 |
| 舵机 / PWM 输出 | **A + C**（+ Accuracy Mode） | 通道 1b 定时调制路由到 PWM 语义（轴 C）；行为级角度对齐，非 cycle-accurate |
| I2C 从机故障 / 丢包 | **A + D + F** | 总线字节经通道 2 + IRQ/回调序（轴 D）+ Fault/丢包注入与观测（轴 F） |
| 按键去抖 | **A + B + F** | 边沿经通道 1 + 虚拟时钟去抖窗（轴 B）+ 事件观测（轴 F） |

详见 [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)、[`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)。

---

## 与轴页分工

- 本文件：**定义** + 缩略上限；[`03-axes/`](../03-axes/) 各轴页可回声「回答的问题」句、**展开**典型上限。
- **禁止**轴页改写本文件中的定义措辞；轴页须链回本表对应行。

