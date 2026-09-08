# ADR-0078：8051 中断两阶段挂起、语义中断源映射与在服务优先级屏蔽

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-08 拍板）** |
| 日期 | 2026-09-08 |
| 触发 | 现行 MCS-51 拦截层中外设模型（如 ADC、UART）在写寄存器钩子内同步调用 `wink_mcs51_dispatch_vector`，在同一调用栈内递归调用用户 ISR，导致深层调用栈隐患；且原 AD-2 条款声明“不建模嵌套虚拟中断”，导致同优先级中断相互穿透或被错误抢占，且外设直接硬编码物理向量号（CMS8S ADC=19），在多厂商（STC8H ADC=5、N76E ADC=11）扩展与向 Tier 3 指令级解释器演进时无法复用外设模型。 |
| 影响范围 | `wink-micro-os/frameworks/mcs51/`（`wink_mcs51_isr.h`, `mcs51_isr.cpp`, `cms8s_adc.cpp`, `mcs51_uart.cpp`, `mcs51_timer.cpp`, `mcs51_extint.cpp`）；活规范 `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`；单元测试及 e2e 驱动。 |
| 决策者 | 嵌入式系统架构团队 |
| 关联 ADR | [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（拦截总纲 / supersede AD-2 局部条款）、[ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步调度）、[ADR-0073](0073-cms8s-adc-real-register-map-supersedes-ssot.md)（CMS8S ADC）、[ADR-0076](0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（双后端路线图） |
| 关联计划 | [`docs/implementation-plans/core/2026-09-08-mcs51-cms-refactor-and-tier3-evolution-plan.md`](../../implementation-plans/core/2026-09-08-mcs51-cms-refactor-and-tier3-evolution-plan.md) |

---

## 1. 背景（Context）

在原有的 MCS-51 仿真拦截层实现中，中断派发机制存在以下关键架构缺陷：

1. **同步写寄存器调用栈递归（AD-2 历史遗留）**：
   外设（如 `cms8s_adc.cpp` 在 `on_adcon0_write` 中、`mcs51_uart.cpp` 在 SBUF 写路径中）在执行写钩子期间直接同步调用 `wink_mcs51_dispatch_vector(vector)`。若用户固件在 ADC ISR 内部执行 `printf` 发送串口，而串口发送又同步触发 UART ISR，调用栈将在宿主栈上层层嵌套，严重违背真实单片机在指令边界打断主程序的硬件执行规律。
2. **缺乏在服务屏蔽（In-Service Masking）与嵌套优先级规则**：
   原有代码中 `s_in_isr` 仅为简单布尔量，不支持嵌套优先级；当一个中断正在执行时，同优先级或低优先级的其他中断如果被同步调用，将破坏当前 ISR 的执行上下文。真实 8051 架构支持 2 级（IP）或 4 级优先级（IP/IPH），且规定**同优先级绝不抢占**、**仅高优先级可抢占**。
3. **外设模型与物理向量号强耦合**：
   经典 8051 仅定义 5 个中断向量；工业级扩展 8051（中微 CMS8S、宏晶 STC8、新唐 N76E 等）的外设向量号完全不一致（如 ADC 中断向量号 CMS8S 为 19，STC8H 为 5，N76E 为 11）。外设模型若硬编码 `VECTOR_ADC = 19`，将无法做到多芯片通用，亦阻碍 Tier 3 现代 1T ISS 虚拟指令引擎复用外设。
4. **中断请求标志清除语义失真**：
   真实 8051 硬件区分自动硬件清零标志（如定时器溢出 TF0/TF1、外部中断边沿 IE0/IE1）与软件强制清零标志（如 UART RI/TI、ADCIF）。若软件未清零软件标志，硬件退出 ISR 后必须能够再次触发中断。

---

## 2. 方案比选（Options）

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 维持原 AD-2 同步派发 | 写钩子直接同步调 ISR，外设各自判断使能与向量 | 零重构成本 | 调用栈递归不可控、同优先级抢占破坏时序、无法支持跨厂商 Profile、无法复用于 Tier 3 | ❌ 否决 |
| B. 引入异步事件循环调度线程 | 引入独立 RTOS 线程与消息队列派发中断 | 解耦彻底 | 破坏 UniSim 确定性单线程沙箱、引入并发竞争与墙钟开销、违背静态分发原则 | ❌ 否决 |
| C. **两阶段挂起 + 语义中断源映射 + 汇合点仲裁派发** | 外设仅提语义 IRQ 挂起位图；微步汇合点按硬件优先级与在服务栈扫描派发；物理映射由厂商表完成 | 100% 还原真实硅片行为；消除深调用栈；外设模型跨厂商中立；完全对接 Tier 3 指令边界 | 需轻量重构中断控制器与外设触发点；保留二分回退宏防时序回归 | ✅ **采纳** |

---

## 3. 决策结论（Decision）

### D1. 架构中立的逻辑中断源与厂商映射表

外设模型严禁硬编码物理向量号，统一使用公共语义枚举发出中断请求：

```c
typedef enum {
    IRQ_SOURCE_INT0 = 0,
    IRQ_SOURCE_TIMER0,
    IRQ_SOURCE_INT1,
    IRQ_SOURCE_TIMER1,
    IRQ_SOURCE_UART0,
    IRQ_SOURCE_ADC,
    IRQ_SOURCE_UART1,
    IRQ_SOURCE_PWM,
    IRQ_SOURCE_I2C,
    IRQ_SOURCE_SPI,
    IRQ_SOURCE__COUNT
} mcs51_irq_source_t;

// 外设发起语义中断的唯一入口
void mcs51_raise_irq(mcs51_irq_source_t src);
```

SoC 描述头/Profile 提供强符号映射条目，解耦芯片差异：

```c
#define MCS51_IRQ_HW_AUTO_CLEAR 0
#define MCS51_IRQ_SW_CLEAR      1

typedef struct {
    uint8_t vector;        // 物理向量号（Tier 2 查 isr_table，Tier 3 跳 0x0003+8*vector）
    uint8_t ie_sfr;        // 使能寄存器地址（IE/EIE1/EIE2...，0xFF 表示常开或无 SFR）
    uint8_t ie_bit;        // 使能位（0..7）
    uint8_t flag_sfr;      // 中断请求标志寄存器（TCON/SCON/EIF2...，0xFF 表示内部维护）
    uint8_t flag_bit;      // 标志位（0..7）
    uint8_t prio_sfr;      // 优先级寄存器（IP/EIP1/EIP2...，0xFF 表示默认低优先级 0）
    uint8_t prio_bit;      // 优先级位（0..7）
    uint8_t clear_mode;    // MCS51_IRQ_HW_AUTO_CLEAR / MCS51_IRQ_SW_CLEAR
} mcs51_irq_map_entry_t;
```

### D2. 两阶段挂起（Two-Phase IRQ Dispatch）与微步汇合点仲裁

- **第一阶段（挂起）**：外设发生事件时调用 `mcs51_raise_irq(src)`，中断控制器校验全局 EA 与外设独立 IE 使能位，置位 `pending_interrupts` 位图中对应的位；
- **第二阶段（派发）**：在每个微步推进（`wink_mcs51_microstep()`）、配额切出让出（`wink_mcs51_yield()`）等汇合点（Rendezvous points），中断控制器执行仲裁扫描；
- **逃生安全阀**：保留编译期宏 `#ifndef WINK_MCS51_TWO_PHASE_IRQ`（默认 1 启用）。调试期置 0 可瞬时回退至单阶段同步调用，防生产未知敏感固件阻断。

### D3. 硬件在服务屏蔽（In-Service Masking）与嵌套仲裁规则

1. **优先级分级与在服务栈**：
   - 支持标准 2 级优先级（0=低，1=高）；
   - 使用 `in_service_prio_stack[4]` 记录当前正在执行的 ISR 优先级，`in_service_depth`（0..4）代表调用深度；
   - 深度 `in_service_depth > 0` 完全等价于原 `wink_mcs51_in_isr()`。
2. **嵌套抢占规则**：
   - 若 `in_service_depth > 0`，当前执行的 ISR 优先级为 $P_{curr}$；
   - 仅当待处理中断的优先级 $P_{req} > P_{curr}$ 时，才允许嵌套抢占当前 ISR；
   - **同优先级不抢占**（$P_{req} \le P_{curr}$）：新中断保持在 `pending_interrupts` 中排队，直至当前 ISR 执行完毕（RETI）。
3. **同优先级自然扫描序（Natural Vector Scan Order）**：
   - 同一优先级多个挂起中断，按照物理向量号升序（INT0 > T0 > INT1 > T1 > UART > ... > ADC）仲裁派发；每次汇合点响应向量号最小的待处理中断。

### D4. 单指令执行抑制（Single-Instruction Suppression）

- 当执行 `RETI` 或写入 `IE`/`IP` 寄存器后，置位 `reti_suppress_one = true`；
- 仲裁扫描器在当次汇合点跳过派发，保证主循环代码至少获得一个执行微步推进周期，杜绝高频中断引发主程序饥饿（Starvation）。

### D5. 标志清零契约（Clear Mode Contract）

- `MCS51_IRQ_HW_AUTO_CLEAR`：进入 ISR 时由硬件自动清零 `flag_sfr` 对应位（如 TF0、TF1、边沿 IE0/IE1）；
- `MCS51_IRQ_SW_CLEAR`：由固件软件负责清零（如 RI/TI、ADCIF）。若 ISR 返回后该位仍为 1，中断在后续扫描点将再次触发。

### D6. 可观测性 Trace 集成

在 `mcs51_raise_irq` 与汇合点 `dispatch_vector` 处埋入统一事件：
```c
WINK_TRACE_MCS51_IRQ(src, vector, prio, virtual_us, event_type); // event_type: RAISE / DISPATCH / RETI
```

### D7. 条款更新宣告（Supersede Statement）

本 ADR 正式推翻并取代 [ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md) 中关于 AD-2 规定的“同步派发且不建模嵌套中断”条款。

---

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 彻底消除了写寄存器钩子内的深递归嵌套风险；ADC ISR 写 SBUF 不再产生崩溃 | 具名延时与微步执行中增加了中断扫描汇合点逻辑；需确保扫描算法在 O(1) 内完成（按位图查找） |
| 真正实现了 8051 硬件同优先级防抢占与高优先级嵌套的物理规律 | 单元测试中若测试用例忘记开 EA 或未正确清零 SW_CLEAR 标志，将真实反映硬件行为 |
| 外设模型与芯片物理向量彻底解耦，为 STC8H / N76E 扩展与 Tier 3 共享外设铺平道路 | 各外设触发点需全面迁移至 `mcs51_raise_irq(src)` |

---

## 5. 遵循与后续（Compliance & Follow-up）

1. **代码落地**：
   - `wink_mcs51_isr.h` / `mcs51_isr.cpp` 引入 `mcs51_raise_irq`、映射表与在服务栈；
   - `cms8s_adc.cpp`、`mcs51_uart.cpp`、`mcs51_timer.cpp`、`mcs51_extint.cpp` 迁移至 `mcs51_raise_irq`；
2. **测试验证**：
   - 新增特征化单测：ADC ISR 写 SBUF、同优先级防抢占、关 EA 临界区保护；
   - 既有 26 项 host 测试与 10 项 wasm 测试保持 100% PASS；
3. **规范回写**：
   - 同步更新活设计规范 `docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`。
