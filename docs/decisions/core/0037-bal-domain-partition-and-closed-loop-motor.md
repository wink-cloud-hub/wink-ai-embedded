# ADR-0037：BAL 域分类调整与闭环控制规范

> **命名硬切割与文件后缀**：见 [ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md)；活规范全文：[06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md)。本 ADR 的三域划分与闭环安全约束仍有效；`*_controller` 文件名等以 0038 为准。

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-17 |
| 影响范围 | BAL 目录分类、运动控制算法开发、实时闭环与 fail-safe 约束 |
| 关联 ADR | [ADR-0023](./0023-bal-business-abstraction-layer.md)、[ADR-0032](./0032-bal-role-operation-naming-classes.md)、[ADR-0038](./0038-bal-naming-hard-cut-and-layer-ssot.md) |
| 关联设计规范 | [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |

---

## 背景（Context）

在原有的 `wink-micro-os` 设计中，BAL（业务抽象层）的结构是按物理输入输出外设直接映射的（例如 `input/`、`output/`、`sensor/`、`actuator/`）。随着系统引入 PID 控制算法和闭环直流电机等新组件，面临以下两个架构设计挑战：
1. **纯数学算法无处安放**：如新引入的 `wink_pid`，作为纯计算函数，不持有任何底层物理器件（DAL）或操作系统调度资源（Runtime）。强行塞入物理分类中会有失偏颇，而直接放置在根目录又会导致结构泛滥。
2. **跨器件协同闭环逻辑划分歧义**：闭环电机需要聚合 Motor（执行器）和 Encoder（传感器）进行 PID 协同计算与周期性调度。它无法单独归类为物理的 `sensor` 或 `actuator`。
3. **安全与实时性问题**：闭环控制中，如果反馈源（Encoder）物理损坏导致读数停滞，积分控制器会自动持续加大控制量输出，引起失控飞转。系统缺乏相应的 fail-safe 制动隔离机制与统一的时间增量 $dt$ 测量标准。

---

## 决策结论（Decision）

### 1. BAL 目录域三向划分
将 BAL 重构为三个主要分类子域：
* **物理增强域（Physical Helpers）**：对齐 DAL 器件进行行为包装（包含 `input/`、`output/`、`sensor/`、`actuator/`、`display/`、`comm/`）。可以包含 `dal/`、`runtime/`，但禁止包含 `pal/`（除 `pal_log` 外）。
* **数学与纯算法域（`math/`）**：存放通用的、纯数学的控制与信号处理算法（如 PID、滤波、死区补偿、底盘正逆运动学解算）。
  * **硬性依赖红线**：`math/` 下的文件**严禁 `#include` 任何 DAL, Runtime 或 PAL 成员**。它必须保持纯计算函数的性质，支持无 Mock 直接运行测试。
* **领域控制域（`control/`）**：存放负责跨器件闭环控制、参数调优的逻辑单元（如闭环电机控制器、差速底盘控制器）。它可以引用 `math/` 算法、`dal/` 驱动和 `runtime/` 定时任务。

### 2. Opaque 隐藏设计与 Slot 池机制
* 闭环控制组件（Class A 类）必须严格贯彻 [ADR-0023](./0023-bal-business-abstraction-layer.md) 的 Slot 池机制。
* 公共头文件中禁止暴露任何 `wink_periodic_handle_t` 或用于周期性调度的内部运行状态。
* 外部公共 API 以目标 DAL 实例（如 `dal_motor_t*`）作为 Key。所有的动态运行状态应隐藏在 `.c` 内的 BSS 段 `static s_slots` 数组中。

### 3. API 动词对齐（对齐 ADR-0032）
运动闭环组件必须依照 Class A 类设计，公共 API 只应向外暴露出以下动词：
* 启动控制会话：`wink_closed_loop_dc_motor_start()` / `wink_closed_loop_dc_motor_start_ex()`（正名见 [ADR-0049](0049-bal-closed-loop-dc-motor-naming.md)；原 `wink_closed_loop_motor_*`）
* 停止控制会话（必须使能安全离合制动）：`wink_closed_loop_dc_motor_stop()`
* 运行时输入目标：`wink_closed_loop_dc_motor_set_speed()`

### 4. 实时与并发安全约束 (Fail-safe)
* **反馈失效超时诊断**：控制回路必须检测反馈更新频率。当目标速度不为 0 但在 `timeout_ms` 时间内编码器脉冲数完全未发生变化时，必须触发脱扣制动（调用 `dal_motor_safe_off`）并使用 `wink_trace_fault(WINK_FAULT_MOTOR_FEEDBACK_LOSS)` 上报故障。
* **实测 $dt$ 累加**：积分项与微分项计算必须调用高精度微秒定时器（如 `pal_os_get_us()`），通过当前计算周期与上一次计算周期的实际时间戳之差计算出高精度的实测 $dt$，禁止假设任务绝对准时。
* **写写与读写保护**：App 主线程更新设定值与后台周期控制任务读取应采用 `PAL_CRITICAL_SECTION` 临界区包紧，防止 32 位机器上的浮点数读写撕裂（Data Tear）。

### 5. 改进的 PID 算法规范
* **反馈微分设计（Derivative on Measurement）**：微分项必须基于 feedback 计算（$D = -K_d \cdot \frac{df}{dt}$），而不是基于误差计算（$e = setpoint - feedback$），以消除由于设定值阶跃造成的“微分冲击”。
* **前限制抗饱和（Anti-windup）**：积分项累加后，先限制其对 I-Term 的最大/最小贡献值，再将反馈限幅作用回积分器累加状态（Clamp 机制），防止积分饱和溢出。

---

## 后果与约束（Consequences & Constraints）

### 正面后果
* **架构解耦彻底**：纯数学算法可在任意环境测试，控制域组件实现了高层业务抽象与二进制防漏。
* **安全性提升**：硬件层面的卡死或滑丝可被软件 fail-safe 实时捕获，避免电机过载或机器失控。
* **代码可移植性高**：双 Target（Host & Wasm 与物理 ESP32）上共享完全一致的代码逻辑，仅 DAL/PAL 在两端进行独立特化。

### 约束与代价
* **静态 Slot 资源上限**：必须在编译期静态定义 Slot 上限（如 `WINK_CLOSED_LOOP_DC_MOTOR_MAX`），无法进行动态内存扩容。
* **调参认知门槛**：应用层需要明确知道运动控制的脉冲单位契约（如规定为 `counts/s`），调参在没有浮点协处理器时需要额外关注 float 计算的执行预算。

---

*状态变更记录：*
- 2026-07-17：Accepted
- 2026-07-18：命名硬切割见 [ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md)；活规范 [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md)
- 2026-07-28：API 前缀正名为 `wink_closed_loop_dc_motor_*`（[ADR-0049](0049-bal-closed-loop-dc-motor-naming.md)）；三域与 fail-safe 约束不变

