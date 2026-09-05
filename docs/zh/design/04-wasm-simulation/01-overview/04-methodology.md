# 方法论、阅读路径与静态门禁摘要

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / overview） |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | 按角色阅读路径；解法类型；旁路纪律；**STRICT_NONBLOCKING「为什么」** |
| 不写 | CMake/链接/selftest 落地细节（→ mechanisms，并双向链接） |
| 管辖 ADR | [0002](../../../decisions/unisim/0002-dual-target-compilation.md)、[0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)、[0025](../../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md)、[0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| 迁自 | `04-wasm-simulation-2.0/00-README.md` §0/§4；`11-consistency-spec.md` §0.1–§0.3 |

> 本文件回答：按角色怎么读 3.0 目录、一致性保障用哪类解法、旁路与阻塞 API 的纪律边界。引擎实现与构建落地见 [`02-mechanisms/`](../02-mechanisms/00-README.md)；场景契约与 oracle 词汇见 [`04-assurance/`](../04-assurance/00-README.md)。

---

## 1. 阅读入口

### 1.1 我想知道「某场景现在能不能验」

→ 直接看 **[`04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)** 的状态矩阵（✅/🟡/❌/🚫），再按链接跳到 [`04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) 读机制与预言。

### 1.2 我想知道「某引擎机制落地到哪了」

→ 看 **[`04-assurance/03-roadmap-and-governance.md` §1.1](../04-assurance/03-roadmap-and-governance.md)**（Landed/Partial/Stub/Planned/Deprecated 总表）；词表定义见 **[根 `00-README.md` §3.2](../00-README.md#32-实现成熟度词表机制落地状态)**。与 checklist 的场景可测性正交。

### 1.3 我想理解仿真引擎的某个机制

按下表按图索骥。A~F 正交轴（见 [`02-axes-af.md`](./02-axes-af.md)）用来定位"这份机制属于哪个保真维度"。

| 机制 | 文档 |
|---|---|
| Worker 隔离 / Asyncify 挂起恢复 / 执行模式（INTERACTIVE/HEADLESS）/ 构建链接参数 | [`02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| 虚拟时钟 `s_virtual_us` / 单一 Gate / 零 Yield 快进 / 回绕 | [`02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| 协作式调度器 / Fiber / 任务状态机 / WCET / 同步原语 / SMP 边界 | [`02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| 中断 Poll 模型 / IRQ FIFO / 临界区补发 / 嵌套边界 | [`02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) |
| 内存配额 / OOM / Fault 锁存 / safe-off / ASan | [`02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| 物理退化与故障注入（抖动/RC/预热/丢包） | [`02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) |
| 虚拟外设注册表 / PinArbiter 电气仲裁 / 三类 JSON 配置 | [`02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) |
| 四通道路由与外设选型 | [`02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| 轴 C 定时器 / PWM / `pal_hwtimer` 软步进 | [`02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) |
| Wasm↔JS ABI 全量符号与契约 | [`02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) |
| Accuracy Mode / 观测平面 / 生命周期与复位 | [`02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| 总体架构 / 联合仿真三域 / 数据面 | [`01-architecture.md`](./01-architecture.md) |

### 1.4 按角色的阅读路径

- **应用 / 低代码开发者**：[`00-README` §1–§3](../00-README.md) → [`02-axes-af.md`](./02-axes-af.md) → [`03-production-contract.md`](./03-production-contract.md) → [`08-channel-routing`](../02-mechanisms/08-channel-routing.md)（选型）→ [`02-consistency-checklist`](../04-assurance/02-consistency-checklist.md)（能不能验）。
- **驱动 / DAL 开发者**：[`08-channel-routing`](../02-mechanisms/08-channel-routing.md)（旁路只能落 PAL）→ [`10-wasm-js-bridge-abi`](../02-mechanisms/10-wasm-js-bridge-abi.md)（ABI）→ [`01-consistency-spec`](../04-assurance/01-consistency-spec.md) 中 C1/C7/C17 → [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)/[0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)。
- **仿真引擎 / UniSim 开发者**：[`01-architecture.md`](./01-architecture.md) → `01` → `02` → `03` → `04` → `10`（[`02-mechanisms/`](../02-mechanisms/00-README.md) 顺序），再读 `07`/`08`；轴 C（定时器/PWM/`pal_hwtimer`）见 [`09-timer-and-pwm-semantics`](../02-mechanisms/09-timer-and-pwm-semantics.md)；Accuracy/观测/复位读 [`11-accuracy-observation-lifecycle`](../02-mechanisms/11-accuracy-observation-lifecycle.md)。
- **CI / 质量负责人**：[`02-consistency-checklist`](../04-assurance/02-consistency-checklist.md)（状态）→ [`03-roadmap-and-governance`](../04-assurance/03-roadmap-and-governance.md)（CI）→ [`11-accuracy-observation-lifecycle`](../02-mechanisms/11-accuracy-observation-lifecycle.md)（Mode 与证据效力）→ [`01-consistency-spec`](../04-assurance/01-consistency-spec.md)（预言）。
- **评审 / 产品**：[`02-axes-af.md`](./02-axes-af.md) + [`03-production-contract.md`](./03-production-contract.md)（边界与口径）→ [`03-roadmap-and-governance`](../04-assurance/03-roadmap-and-governance.md)（路线图与 [`§1.1` 机制成熟度总表](../04-assurance/03-roadmap-and-governance.md)）。

---

## 2. 解法类型（保障方案用词）

场景契约五字段中的「保障方案」须标明下列解法类型之一或组合。完整 oracle 词汇与三道防线展开见 **[`04-assurance/01-consistency-spec.md` §0](../04-assurance/01-consistency-spec.md#0-总则结构冻结)**（§0.2 验收预言词汇、§0.3 解法类型与三道防线）；**本处不重复 oracle 长表**。

| 类型 | 职责 | 典型手段（摘要） |
|---|---|---|
| **A 约束写法** | 编译/IDE 毫秒级阻断危险写法 | lint 规则、`STRICT_NONBLOCKING` 隐藏符号、`wink-app.json` 引脚冲突门禁 |
| **B 引擎建模** | 在 Wasm/Host 内核复刻物理时间/总线/调度 | `s_virtual_us` SSOT、零 Yield 引脚事件队列、DAL 100% 同源、确定性混沌 |
| **C 观测门禁** | 运行时捕获并发/溢出/耗尽/超时并隔离 | 堆配额 Fault、影子内存 TSan、软 WDT、ASan/UBSan |
| **真机/HIL** | 承接刻意不建模的物理/硬实时/微架构 | HIL 板卡自动化、硬 ISR、SPICE、多核 cache 抽样 |

---

## 3. 旁路纪律

- **PAL 是唯一合法的旁路落点**：仅替换物理量来源（引脚电平、脉宽、总线响应字节、ADC 原值等），App/BAL/DAL 业务逻辑 100% 同源编译（[ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)）。
- **禁止 DAL `#ifdef SIMULATION` 业务分叉**：整驱动被 JS 替换会导致 CRC/超时/重试从未在仿真跑过（[ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) 决策 2）；旁路收窄至 PAL，DAL 目标零仿真宏。
- **JSON 门禁 Fail-Loud**：未在 `wink-app.json` 声明的语义 bypass 须在编译/运行期失败（[ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)）。
- 3D 碰撞、Hub 距离注入等插件输出，须经注入 API 转成 ECHO 沿或 ADC 原值写回底座，**不得**直接作为 DAL 返回值（C1.2；详见 [`08-channel-routing`](../02-mechanisms/08-channel-routing.md)）。

---

## 4. STRICT_NONBLOCKING 与 Bringup 隔离

### 4.1 为什么（ADR-0025）

为在仿真阶段尽早暴露非法阻塞并防范真机 WDT 饿死：

1. **仿真默认严格**：`wink-micro-os/CMakeLists.txt` 对 `wink_simulator` 的 app 源文件开启 `-DWINK_STRICT_NONBLOCKING=1`；`WINK_BLOCKING` API（如阻塞式 `dal_ultrasonic_read`）在头文件中隐藏，误用 → **链接期 fail-fast**。
2. **Bringup / Selftest 隔离**：阻塞辅助工具置于 `wink-micro-os/runtime/selftest/`，用 `#ifndef WINK_STRICT_NONBLOCKING` 包裹；严格模式仅留 stub 返回 `WINK_ERR_UNSUPPORTED`，防止阻塞代码进入仿真沙箱。
3. **业务回调与 `app_loop()` 禁止任何 blocking pragma**；真机构建不受此剔除，但仿真与 CI 主路径必须走非阻塞 API。

### 4.2 怎么做（构建与运行时落地）

CMake 定义、链接期符号剔除、调度器侧 WCET/饿死断言等**实现细节**不在本文件展开，见下列 mechanisms 正文（与本节双向链接）：

- [`02-mechanisms/01-sandbox-and-execution.md` §5](../02-mechanisms/01-sandbox-and-execution.md#5-strict_nonblocking-构建落地怎么做) — CMake 默认/`WINK_APP_SOURCES` 作用域、selftest 隔离、与 Asyncify/HEADLESS 边界
- [`02-mechanisms/03-scheduler-and-concurrency.md` §8](../02-mechanisms/03-scheduler-and-concurrency.md#8-strict_nonblocking-编译期门禁adr-0025) — 协作调度下阻塞语义、LIGHT 上下文断言、`app_loop` 纪律

规范依据：[ADR-0025](../../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md)。

---

## 5. 机制落地 vs 场景可测（正交）

两个维度**勿混用**：

| 维度 | 回答 | 标记体系 | 归属 |
|---|---|---|---|
| **机制落地** | 引擎能力做了没有？ | Landed / Partial / Stub / Planned / Deprecated | 根 [`00-README.md` §3.2](../00-README.md#32-实现成熟度词表机制落地状态)；mechanisms 文首；[`03-roadmap-and-governance` §1.1](../04-assurance/03-roadmap-and-governance.md) |
| **场景可测** | 某 C 场景现在能不能验？ | ✅ / 🟡 / ❌ / 🚫 | 仅 [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) |

例：IRQ Poll 机制可为 **Landed**，同时 C4.3「优先级嵌套」场景仍为 **🚫**——能力落地 ≠ 该场景可验。

查「某机制现在落地到哪」→ [`03-roadmap-and-governance` §1.1](../04-assurance/03-roadmap-and-governance.md)；查「某场景能不能验」→ [`02-consistency-checklist`](../04-assurance/02-consistency-checklist.md)。

