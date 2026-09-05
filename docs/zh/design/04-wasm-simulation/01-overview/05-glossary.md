# 术语表（Glossary）

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / overview） |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | **术语权威释义唯一出处**：一行定义 + 链到机制/口径正文 |
| 不写 | 算法细节、状态矩阵、成熟度总表 |
| 上次核对 | 2026-08-02 |

> 增词规则：先出现在正文 → 必须回写本表一行；禁止仅在轴页给非正式定义。

| 术语 | 一行定义 | 详见 |
|---|---|---|
| UniSim | 浏览器/Node Wasm 仿真引擎（`@wink-ai/unisim`）；Worker 托管同源 C + JS 外设，经 `wasm_bridge` ABI 交互 | [`01-architecture.md`](./01-architecture.md)、[`10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) |
| 命名地图 | 同一引擎在不同语境的名字：npm 包 `@wink-ai/unisim`、C/CMake target `wink_simulator`、Emscripten 导出名 `WasmSandbox`（`EXPORT_NAME`）、文档术语「UniSim/Wasm 仿真」 | [`01-architecture.md`](./01-architecture.md) |
| Step-Lock Pipe | 联合仿真单步契约：插件读控制信号 → 按 Δt 更新物理 → 注入回底座，三拍锁步；破坏（读墙钟/不同 Δt）即不可复现 | [`01-architecture.md` §2.2](./01-architecture.md) |
| Δt（step delta） | 单个仿真步推进的虚拟时间量，绑定 `s_virtual_us`；plant 与 OS 必须共用同一 Δt 以保证锁步与可复现 | [`01-architecture.md` §2.2](./01-architecture.md)、[`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| plant（被控对象） | 控制论术语，指被固件控制的物理本体/环境（小车运动学、传感器等）；在本架构中对应 Plugin 物理域，与 App 控制域解耦 | [`01-architecture.md` §2](./01-architecture.md) |
| 联合仿真三域 | App 控制域（100% C）/ 平台仿真 OS / Plugin 物理域 — 注入**仅**发生在 PAL | [`01-architecture.md`](./01-architecture.md) |
| ProductWorld | Vue3 主线程 3D UI（规划中/进行中）；距离等物理量→引脚/ADC，**禁止**直连 DAL | [`01-architecture.md`](./01-architecture.md)、[`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| PinArbiter | TS 多驱动引脚仲裁（逻辑电平 + 驱动强度）；IRQ 边沿事件源 | [`07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md)、[`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| Asyncify | Emscripten 变换：阻塞 C 调用 yield Wasm 栈；JS 侧可覆写 import 返回 Promise | [`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| Fiber | 协作式协程 `sim_ctx_*`；Win32 Fibers / emscripten fiber 实现 | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| WCET | 仿真调度器时间片守卫（约 5ms → Fault 8002）；HEADLESS / debugger 模式可旁路 | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| Gate（时钟） | 虚拟时钟单一写入点 `wink_vclock_advance_internal`，维护 `s_virtual_us` | [`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| soft-stepping | 行为级定时器/PWM 时间近似（轴 C）；非 cycle-accurate；设计术语，成熟度 Partial/Planned | [`09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) |
| safe-off | Fault 阶段执行器安全关断 `wink_actuator_safe_off_all` | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| safeWrap | 高阶函数包装 `js_*` import：宿主 throw → `host_fault` 8003（实现于 `createUnisimImports.ts`） | [`10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md)、[`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| Fault | 锁存运行时故障域（OOM/WCET/host 等）；触发 safe-off 与隔离 | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md)、[`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |
| 软 WDT | 规划中的虚拟时间饿死看门狗（C5.2）；**尚未实现** | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md)、[`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |
| Accuracy Mode | 行为 \| 时序 \| 周期精度策略；与 Execution Mode 正交 | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| Execution Mode | `INTERACTIVE` vs `HEADLESS`（[ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md)） | [`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| Evidence-L1 / L2 | **证据级别**（Accuracy）：L1 状态机；L2 payload/边沿因果等——**仅**用于观测效力 | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| Fault-L1 / L2 / L3 | **故障注入层**：L1 引脚中间件；L2 总线；L3 器件错误语义——**勿**与证据级混淆 | [`06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) |
| Test-L0…L3 | **测试金字塔层**（编译→单测→集成→同 binary 确定性）——**勿**与 Fault/Evidence 混淆 | [`06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) §9 |
| 四通道 + 1b | Pin / Bus / Analog / Buffer（PWM 为通道 1b 定时调制） | [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| 旁路 / Bypass | 在 PAL 替换物理源；语义旁路须 JSON 门禁（ADR-0040） | [`04-methodology.md`](./04-methodology.md)、[ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| HIL | 硬件在环；承接仿真 🚫 场景 | [`03-production-contract.md`](./03-production-contract.md)、[`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) |
| selftest / Bringup | `runtime/selftest/`；阻塞辅助仅允许 STRICT 外；STRICT 下编译剔除 | [`04-methodology.md`](./04-methodology.md)、[`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| 观测平面 | PinTracer + VcdExporter / SessionRecorder / BusAnalyzer | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| A~F | 仿真保真正交轴；**唯一定义**见本目录轴表 | [`02-axes-af.md`](./02-axes-af.md) |
| C1~C25 | 一致性场景编号命名空间（问题/方案/预言契约） | [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |

