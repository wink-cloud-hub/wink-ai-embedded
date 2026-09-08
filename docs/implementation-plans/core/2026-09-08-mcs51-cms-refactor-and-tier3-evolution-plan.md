# 【实施计划】MCS-51 与 CMS 架构重构及 Tier 3 现代 1T ISS 长期演进计划

> 📋 **计划编号**：`PLAN-20260908-MCS51-TIER3-EVOLUTION`  
> 🎯 **计划版本**：v3.5（2026-09-08，架构纯洁度定稿：剔除过渡宏与多 MCU 虚幻切换、真值源收敛至 PCON/Depth、外设状态与 Core 解耦、确立日落策略与零旧符号门禁）  
> 🏛️ **所属层级**：Layer-③ 实施计划（兼具 Layer-② 技术架构设计规格）  
> 📚 **关联规范与基线**：
> - 活设计规范：[`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md)
> - 体系总纲：[`docs/zh/design/01-system-overall/01-system-overview.md`](../../zh/design/01-system-overall/01-system-overview.md)（四层仿真架构）
> - 物理与一致性限制：[`docs/zh/tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md`](../../zh/tech-designs/mcs51/2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md)
> - 运行时与阻塞纪律：[ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md)（阻塞 API 硬隔离）, [ADR-0025](../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md)（应用阻塞 API 诚信声明规范）, [ADR-0030](../../decisions/core/0030-esp-idf-never-auto-installed.md)（Wink 工具永不自动安装重型工具链 / 外部工具链探测不自动安装）
> - 关键 ADR：[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发红线）, [ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（拦截总纲 / AD-2 功能级时序基线）, [ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 数据面）, [ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域 / D5 同步派发意图）, [ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)（CMS8S ADC 真实图与 0 周期即时）, [ADR-0074](../../decisions/core/0074-mcs51-channel1-external-read-pin.md)（Read-Pin 外部缝）, [ADR-0075](../../decisions/core/0075-mcs51-production-wasm-target-headless.md)（生产 wasm 链接）, [ADR-0076](../../decisions/core/0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（双后端路线与 A/B 类缺口 / ISS 选型前置条件）, [ADR-0077](../../decisions/core/0077-gpio-write-drive-strength-axis.md)（准双向口强度模型）  
> - **拟提议新 ADR 与交付门禁**：
>   - `ADR-0078`：8051 中断两阶段挂起、语义中断源映射与在服务优先级屏蔽（正式 supersede AD-2 同步派发与嵌套限制条款；**Task R3 代码合入前必须先落 ADR-0078 并同步回写活设计规范 07**）；
>   - `ADR-0079`：Tier 3 现代 1T ISS 选型与外设时序策略架构（许可证红线、自研 vs 宽松许可核、SDCC 工具链探测、外设周期排程；**Task T1 启动前必须先落 ADR-0079**）。

---

## 1. 元数据表与配置面 Schema

### 1.1 元数据表

| 字段 | 内容 |
|------|------|
| **目标平台/SoC** | `wasm` (UniSim 浏览器沙箱 & Node Headless) / `host` (MSVC & GCC 单元测试) |
| **工具链/语言标准**| C++17 (`-std=c++17`, MSVC `/std:c++17`), Emscripten (Asyncify + JSPI), SDCC 4.2+ (Target=SDCC), Python 3.10+ |
| **计划状态** | 📋 架构纯洁度定稿已就绪（v3.5 Production Ready Architecture） |
| **优先级** | 🔴 P1（重构关键数据面下沉与中断收口，直接决定 Tier 3 资产复用率） |
| **计划负责人** | 嵌入式系统架构团队 |
| **所需技能** | `embedded-best-practice`, `c-runtime-polymorphism-reading` |

### 1.2 配置面契约（`wink-app.json`）

系统配置面显式声明 8051 仿真后端形态与时钟拓扑（具备完全缺省兼容能力，未声明时默认 `backend="native"`, `clock_hz=24000000`，`wink-micro-app` 源码与配置一行不改）：

```json
{
  "name": "vendor_cms8s78xx_v202_adc_ldo",
  "target": "mcs51",
  "board": "cms8s78xx",
  "simulation": {
    "backend": "native",
    "clock_hz": 24000000,
    "microstep_us": 5,
    "timing_tolerance": false
  }
}
```
* `backend`：可选 `"native"`（默认，Tier 2 C++ 代理）或 `"iss"`（Tier 3 1T 解释器）；
* `clock_hz`：MCU 主频，Task F3 用于动态换算机器周期与配额步长；
* `timing_tolerance`：是否容忍未加桩纯软件空循环的时序漂移（见 Task R5 与 §8 验收矩阵）。

---

## 2. 总体架构定位与关键认知澄清

### 2.1 仿真层级与指令集范围精确界定

1. **四层仿真分工（UniSim SSOT）**：
   * **Tier 1**：AI-Native 统一 OS 架构（ESP32 / Wasm 共享 PAL/DAL/BAL 接口，高保真行为级沙箱）。
   * **Tier 2（现行主线）**：**生态源码级 C++ 代理层**。通过 `mcs51_cleanup.py`（负责 ISR 签名重写、死循环注入与具名延时加桩）配合 `REGX52.H` / `REG_CMS8S78XX.H` 编译期方言擦除，将未修改的 Keil C51 源码以 C++17 编译，经由 `WinkSfr` / `WinkSbit` 代理 + Fiber 协程运行。
   * **Tier 3（未来按需触发）**：**1:1 指令级解释器架构（Micro-ISA Emulation / ISS）**。UniSim 内置专用指令集解释器，直接运行厂商原生 Hex/Bin 或经 SDCC 编译生成的机器码。
2. **ISA 边界与工具链现实澄清**：
   * **8051 ISS 仅覆盖 8051 架构 MCU**：包括中微 CMS8S78xx、宏晶 STC8/STC8H、新唐 N76E003、AT89C51/52 等。
   * **应广（Padauk / PDK）与辉芒微（FMD FT6x）属于专有 RISC 架构**，拥有完全不同的 Opcode 集合与寻址空间，属于独立的解释器目标：
     - **PDK**：存在开源 SDCC 后端（`pdk13`/`pdk14`/`pdk15`），工具链成熟度高，是非 8051 目标的现实首选；
     - **FT6x**：无开源编译器（仅厂商专有 IDE），ISS 路径只能直接加载二进制 Hex，工具链缺口极大，规划优先级排在 PDK 之后。

### 2.2 现代 8051 微架构真相：1T 为主，绝非经典 12-T！

* **时钟与机器周期现实**：
   * 经典 AT89C51/52 为 12-T（12 个振荡周期构成 1 机器周期）；
   * **现代工业级 8051（CMS8S78xx、STC8H、N76E003 等）绝大多数是 1T (单周期流水线) 核心**！中微数据手册原文：CMS8S78xx 为“增强型闪存 8 位 1T 8051 微控制器”，$F_{sys} \le 24\text{MHz}$ 时为 1Tsys。
* **WS2812 亚微秒时序的敏感性**：
   * 在 24MHz 主频下，1T 机器周期 $T_{mach} \approx \mathbf{41.67\text{ns}}$；
   * WS2812 亚微秒单脚时序（$T0H \approx 400\text{ns}$ 对应 9~10 个 1T 机器周期）。若硬套经典 12-T 解释器，单周期放大至 500ns，时序将被彻底粉碎！
* **架构约束**：
   * 解释器核心的**指令周期表必须按厂商可插拔（Pluggable Vendor Cycle Profile）**；
   * 256 个基本 Opcode 解码器通用共享，但每条指令的机器周期计数、SFR 映射图及 XSFR 窗口由厂商 Profile 动态配置。

---

## 3. 现阶段代码深度核实与客观缺口

### 3.1 核对属实的核心事实

1. **全局静态单例内存状态裸露（属实）**：
   * `wink_mcs51_sfr_shadow[256]` 与 `wink_mcs51_pin_traps[4][8]`（`mcs51_sfr.cpp`）；
   * SFR 读写钩子表 `wink_mcs51_sfr_read/write_hooks[256]`（`mcs51_trap.h:29-53`）；
   * 中断向量表 `s_isr_table[28]` 与嵌套标志 `s_in_isr`（`mcs51_isr.cpp:17-21`）；
   * 外设私有状态：`s_timers`（`mcs51_timer.cpp`）、`s_extint`（`mcs51_extint.cpp`）、`s_uart`（`mcs51_uart.cpp`）、`s_adc`/`s_adet`（`cms8s_adc.cpp:69-71`）。
2. **写寄存器调用栈内同步嵌套派发（属实，且源自 AD-2 显式决策）**：
   * `cms8s_adc.cpp:106` 在 `on_adcon0_write` 钩子内同步执行 `wink_mcs51_dispatch_vector(VECTOR_ADC)`；
   * `mcs51_uart.cpp:104` 在 SBUF 写路径内同步派发向量 4；
   * `wink_mcs51_isr.h` 头注释明确声明：`"Nested virtual interrupts are not modeled (functional level, AD-2)"`。改动需配套 ADR-0078 正式更新该决策。
3. **准双向口强度仲裁与 0 周期外设已被 headless 生产闭环证实（属实）**。

### 3.2 纠偏与修正后的客观工程事实

1. **`mcs51_bridge.cpp` 结构精炼，非“缺乏 SoC 抽象”**：
   * 全长仅 222 行，SFR 钩子已经是成熟的表驱动（256 项数组），板级差异经 `mcs51_board_config.h`（`__has_include`）+ `post_init_hook` 解耦，全文件仅 2 处 `#if`。
   * 真实缺口仅是：生命周期函数调用（`init` / `reset` / `poll`）以分散裸函数写在框架主循环中，需收敛为**编译期静态 const 描述符表**。
2. **`mcs51_cleanup.py` 边界克制，非“缺乏词法分析器”**：
   * 全长仅 316 行，注释/字符串感知遮罩完备。方言擦除刻意完全委托给 `REGX52.H`。
   * 真实缺口是：① 不感知预处理条件编译（如 `#if 0` 块内的 `interrupt` 会被误重写）；② 缺少 Keil $\to$ SDCC 的方言输出映射；③ 缺少对具名延时函数的调用点加桩。
3. **GPIO 数据面下沉必须同时覆盖读路径双入口（Read-Pin vs Read-Latch）**：
   * 准双向口三路仲裁、diff 边沿提取、`WEAK`/`SUPPLY` 强度上报（ADR-0077）内联在 `mcs51_proxy.hpp`（348 行）中。
   * **Tier 3 机器码绝不走 C++ 运算符重载**！下沉必须同时提供：`read_pin`（普通整字节/位读走外部电平仲裁）与 `read_latch`（RMW 指令读锁存器，如 `CPL P1.0` / `ANL P1, A`）。若只下沉 pin 读，Tier 3 将无法复用正确的硬件锁存器翻转语义。
4. **`ucsim` 的致命缺陷：GPLv2 许可证传染与 1T 缺失**：
   * `ucsim` 为 GPLv2，与仓库 Apache-2.0 冲突，严禁剪裁进入**分发交付物**；且其主要针对经典 12T。
   * **许可证红线约束的是“随产品分发”**：`ucsim` 允许作为 CI 期间独立运行、不对外链接的**差分对拍参考机（Oracle）**。
5. **多实例部署形态的物理隔离边界澄清（坚决摒弃单进程虚拟切换）**：
   * **浏览器端**：多 MCU 采用**多 Wasm 实例**作为天然的线性内存与全局符号物理隔离基线；
   * **Host 单元测试端**：在 Tier 2 Native 编译下，固件 C 全局变量（`data`/`idata`）属于宿主原生静态存储，且 `WINK_ISR` 生成全局 `extern "C" wink_isr_vector_N` 符号，同进程多固件必然产生链接碰撞。因此 **Host 端多 MCU 测试必须采用多独立进程（或独立动态库域）**；
   * 上下文容器（`Mcu51Context`）的真实职责：提供单元测试间整块状态快照与硅片复位种子装载、封装 Tier 3 虚拟机执行上下文，**坚决不搞无物理意义的单进程 `pin_base_offset` 与 RAII guard 切换机制**。
6. **中断向量号厂商发散，外设模型与物理向量号硬编码耦合（客观事实）**：
   * 经典 8051 仅 5 个中断；扩展外设向量号跨厂商毫无规律（CMS8S ADC=19、STC8H ADC=5、N76E ADC=11）。外设层必须只发**语义中断请求**（`mcs51_raise_irq(IRQ_SOURCE_ADC)`），由厂商 Profile 映射表翻译为物理向量。
7. **软件延时循环在两代 Tier 间的时序撕裂（客观事实）**：
   * Tier 2 下忙延时存在两个极端：含 `_nop_()` 的被高估 100 倍（微步记账 5µs vs 42ns），纯计算空循环虚拟时间推进为 0µs。Tier 3 下时间自然流逝。必须在 Tier 2 显式加桩（Task R5），并在验收中断言两类场景。
8. **外设模型时序策略单一（四红线是 Native 专属契约，客观事实）**：
   * `mcs51_trap.h` 规定的“四红线（0 周期即时完成）”是为 Native 编译防止纯内存轮询死锁而设立的。在 Tier 3 下，CPU 具备逐指令推进能力，真实 ADC 需要 $150\mu\text{s}$。若 ISS 复用 0 周期外设，固件轮询 `ADGO` 将首轮假退出，破坏保真度。共享外设模型必须具备**时序策略位**（Native 即时 vs ISS 排程）。

---

## 4. Phase 1 重排后的任务架构（关键路径优先）

按照**“Tier 3 结构前置、中断闭环、链接安全与低功耗治理”**重新编排实施序列：

```
Phase 1 重排后实施序列：
┌────────────────────────────────────────────────────────────────────────┐
│ R3: 中断两阶段挂起 + 语义中断源映射 + 优先级/在服务屏蔽 (配套 ADR-0078)   │
│  ├─ 前置门禁: 合入前落 ADR-0078 并回写活规范 07; 保留 WINK_MCS51_TWO_PHASE 开关│
│  ├─ 特征化测试: ADC ISR 写 SBUF、同优先级防抢占、EA 临界区全绿           │
│  └─ 可观测性: 注入 WINK_TRACE_MCS51_IRQ 事件 (对接 04-runtime-and-trace)│
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R0: [核心资产下沉] GPIO 仲裁与强度逻辑下沉为后端中立服务                 │
│  ├─ mcs51_gpio_sfr_write/bit_write + read_pin/read_latch 双读路径       │
│  └─ C++ WinkSfr proxy 与未来 ISS SFR MMIO 陷阱共享同一套底层 C-ABI     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R2: 补全并封装标准 MCU 核心上下文 (Mcu51Context)                       │
│  ├─ 封装核心内存、向量表、标准外设态、edge_queue 与 in-service 嵌套栈   │
│  ├─ 物理隔离边界: 明确多 MCU 走多 Wasm 实例 / 多进程，砍去单进程切换虚饰│
│  └─ 零兼容债硬切换: 同 PR 全量原子改名，禁止遗留过渡宏与旧入口 wrapper │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R1: 将 init/reset/poll 收敛为编译期 const 描述符表 (静态分发红线)       │
│  ├─ 严禁弱符号 weak！遵循 mcs51_board_config.h 强符号链接 (防 PE 解析 NULL)│
│  ├─ 增加 next_event_us 槽: 外设自主上报下一次事件时刻供调度器查询        │
│  └─ 严格保持 phase 执行序 (charge -> RX drain -> extint -> adc)       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R4: mcs51_cleanup.py 预处理条件分支感知 (#if 0 忽略) & SDCC 方言输出前置│
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R5: 软件延时加桩与双后端时序差异隔离                                    │
│  ├─ cleanup.py 门控: 仅 native 改写, sdcc/iss 严禁加桩; 本地定义防劫持 │
│  ├─ 运行时 wink_delay_us(): 满量子 microstep + 尾款 charge_us (防超调)  │
│  └─ 边界声明: #define 宏延时无法改写 (打 timing-tolerance 标签); 声明平移合法│
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ R6: 低功耗待机与停泊原语 (PCON.IDL / PCON.PD) 纳入 OSAL 纪律治理       │
│  ├─ 双模机制: 活跃排程态走微步泵快进(零墙钟损耗), 外部事件置 wake_flag 退出 │
│  ├─ 真静默态: 仅无内部事件时 pend, horizon 超时直接终止 scenario 运行   │
│  └─ PD 掉电: 停振冻结定时器, 仅外部边沿/复位唤醒(受硬件 EA/EXx 使能约束)│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Phase 1 核心任务详细规格

### Task R3 — 中断两阶段挂起、语义中断映射与在服务屏蔽（配套 ADR-0078）

#### 1. 决策演进背景与交付门禁
* **前置交付门禁**：Task R3 代码合入前，必须先正式产出并接受 `ADR-0078`，并**同步回写活设计规范 [`07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md)**，推翻原 AD-2 的同步派发条款。
* **二分回退开关**：保留编译期宏 `#ifndef WINK_MCS51_TWO_PHASE_IRQ`（默认 1 开启）。若遇到未知时序敏感固件异常，可通过置 0 瞬间回退为老版同步调用栈，为生产排障保留安全阀。

#### 2. 语义中断源与厂商映射表（核心解耦）
外设模型**严禁硬编码物理向量号**，统一发语义请求：

```c
// 架构中立的逻辑中断源（8051 家族公共语义）
typedef enum {
    IRQ_SOURCE_INT0 = 0, IRQ_SOURCE_TIMER0, IRQ_SOURCE_INT1,
    IRQ_SOURCE_TIMER1, IRQ_SOURCE_UART0,
    IRQ_SOURCE_ADC,               // 厂商发散重灾区：CMS=19 / STC8H=5 / N76E=11
    IRQ_SOURCE_UART1, IRQ_SOURCE_PWM, IRQ_SOURCE_I2C, IRQ_SOURCE_SPI,
    IRQ_SOURCE__COUNT
} mcs51_irq_source_t;

// 外设侧唯一入口：挂起语义中断（不感知向量号、不感知优先级寄存器）
void mcs51_raise_irq(mcs51_irq_source_t src);

// 厂商 Profile 映射表条目（SoC/板级描述头强符号提供）
typedef struct {
    uint8_t vector;        // 物理向量号（Tier 2 查 isr_table，Tier 3 跳 0x0003+8*vector）
    uint8_t ie_sfr;        // 使能寄存器地址（IE/EIE1/EIE2...）
    uint8_t ie_bit;        // 使能位
    uint8_t flag_sfr;      // 中断请求标志寄存器（TCON/SCON/EIF2...）
    uint8_t flag_bit;      // 标志位
    uint8_t prio_sfr;      // 优先级寄存器（IP/EIP1/EIP2...）
    uint8_t prio_bit;
    uint8_t clear_mode;    // MCS51_IRQ_HW_AUTO_CLEAR / MCS51_IRQ_SW_CLEAR
} mcs51_irq_map_entry_t;
```

* **标志清零契约**：
  * `MCS51_IRQ_HW_AUTO_CLEAR`：边沿触发型标志在响应进入 ISR 时硬件自清（如 T0/T1 TF 位）；
  * `MCS51_IRQ_SW_CLEAR`：电平/锁存型标志必须由固件软件清零（如 UART RI/TI、ADCIF）。**若 ISR 返回后标志仍为 1，中断保持挂起，在下一派发点再次触发**——还原真实硬件“忘清标志反复进中断”物理规律。

#### 3. 可观测性 Trace 集成
在 `mcs51_raise_irq` 与汇合点 `dispatch_vector` 处埋入结构化事件：
```c
WINK_TRACE_MCS51_IRQ(src, vector, prio, virtual_us, event_type); // event_type: RAISE / DISPATCH / RETI
```
对接 `04-runtime-and-trace` 体系，解决“ISR 为什么晚了 N 个微步”的排障诉求。

#### 4. 硬件中断仲裁、服务屏蔽与单指令抑制（Hardware Arbitrament & Masking Specification）

还原真实 8051 微架构中断控制器的物理规律，支撑 `Mcu51Context` 中 `in_service_prio_stack` 与 `reti_suppress_one` 状态机的严密闭环：

1. **在服务屏蔽（In-Service Masking）与嵌套抢占规则**：
   * 8051 硬件支持 2 级（标准 IP）或扩展 4 级优先级（IP/IPH）。
   * **同优先级不抢占**：若 CPU 正在执行某优先级 $P$ 的 ISR，任何优先级 $\le P$ 的新中断请求只能在 pending 位图中排队，**绝对不允许打断当前 ISR**；
   * **高优先级才允许嵌套**：仅当请求优先级 $> P$ 时，硬件才允许中断当前 ISR 实现嵌套抢占；
   * **状态维护**：`Mcu51Context::in_service_prio_stack` 数组维护嵌套优先级栈（最大深度 4 级），`in_service_depth` 指示当前 ISR 栈顶。ISR 入口 push 当前优先级，`RETI` 时 pop。
2. **同优先级自然扫描仲裁（Natural Vector Scan Order）**：
   * 当多个相同优先级的中断同时挂起（pending）且满足使能条件时，硬件扫描器在每个微步/指令汇合点按照**固定自然轮询次序（即向量号升序）**仲裁派发；
   * 派发优先级次序：INT0 (0) > Timer0 (1) > INT1 (2) > Timer1 (3) > UART0 (4) > ... > ADC (19)；每次汇合点仅响应向量号最小的待处理中断，其余保持 pending。
3. **RETI 与写 IE/IP 后的单指令执行抑制（Single-Instruction Suppression）**：
   * **防饥饿硬件纪律**：在执行 `RETI` 指令离开 ISR，或对 `IE` / `IP` 寄存器进行写操作后，8051 硬件强制**至少执行一条主程序指令**，然后才允许响应下一个 pending 中断，防止主循环被高频连续中断彻底饥饿（Starvation）；
   * **建模实现**：由 `Mcu51Context::reti_suppress_one` 状态机建模。当执行 `RETI` 或写 `IE/IP` 时置为 `true`；中断扫描器检测到该标志为真时跳过当次派发，并在主线程/微步推进完成一个操作周期后自动清除。
4. **长跨度挂起塌缩防护（Pending Collapse Prevention & Step-Pumped Discipline）**：
   * **全局不可破红线**：**任何跨越虚拟时间的跃迁或快进（包括 Task R5 延时加桩、Task R6 低功耗停泊快进），严禁直接瞬移大跨度时间**！
   * **物理原因**：若时间瞬间跳过数十毫秒，周期性硬件定时器在数学上溢出了几十次，如果直接瞬移时间，几十次定时器中断将塌缩为单次 pending，造成灾难性丢步与事件丢失；
   * **统一规则**：长跨度时间推进必须统一委托给微步调度泵（Step-Pumped Catch-Up），逐个微步或事件切片有序演进，驱动定时器溢出、边沿采样与中断服务按物理时序次序派发。

---

### Task R0 — [关键前置] GPIO 数据面仲裁与强度下沉为后端中立 C 服务

#### 1. 目标与双读路径
彻底剥离 `mcs51_proxy.hpp` 中的 348 行电气逻辑。必须**显式切分 Read-Pin 与 Read-Latch**：

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── 写路径 ──────────────────────────────────────────────────────────────
void mcs51_gpio_sfr_write(uint8_t port, uint8_t new_val);
void mcs51_gpio_bit_write(uint8_t port, uint8_t bit, uint8_t level);

// ── 读路径（调用方必须按指令语义选路！）───────────────────────────────────
// Read-Pin：外部引脚三路仲裁（MOV A, Pn / MOV C, bit）：
//   优先级 1: internal on_read trap（如 ADC0832 DO 线）
//   优先级 2: UniSim channel-1 外部电平（js_pal_gpio_read_state）
//   优先级 3: 外部 HiZ/Conflict 回退读锁存器影子
uint8_t mcs51_gpio_read_pin(uint8_t port);
uint8_t mcs51_gpio_bit_read_pin(uint8_t port, uint8_t bit);

// Read-Latch：RMW 指令专用（ANL/ORL/XRL Pn、CPL/SETB/CLR/JBC Pn.bit、MOV Pn.bit, C）
//   硬件直接读端口锁存器，严禁走外部引脚三路仲裁！
uint8_t mcs51_gpio_read_latch(uint8_t port);
uint8_t mcs51_gpio_bit_read_latch(uint8_t port, uint8_t bit);

#ifdef __cplusplus
}
#endif
```

#### 2. Tier 3 收益
ISS 解码器遇到 `ANL P1, A` 时调 `read_latch`；遇到 `MOV A, P1` 时调 `read_pin`。准双向口与强度仲裁资产 100% 顺延至 Tier 3。

---

### Task R2 — 补全 MCU 运行期标准核心上下文（`Mcu51Context`）

#### 1. 上下文结构体设计（`frameworks/mcs51/include/mcs51_context.h`）

```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "mcs51_trap.h"
#include "wink_event.h"

// 定时边沿注入事件
struct McuEdgeEvent {
    uint64_t fire_us;
    uint16_t pin;
    uint8_t  level;
};

// 外设与数据面标准核心上下文容器
// 【内存约束】：sizeof(Mcu51Context) 约 66KB（含 64KB XDATA 阴影），严禁在 Fiber 栈或测试函数栈上局部声明，
// 必须且仅允许分配于全局静态存储区（BSS）或堆上，杜绝协程栈溢出（Stack Overflow）。
struct Mcu51Context {
    // 1. 内存与 SFR 镜像
    uint8_t sfr_shadow[256];
    uint8_t xdata_shadow[65536];       // XSFR 走 xdata 高端窗口（如 PS_ADET=0xF0CC）

    // 2. 陷阱与钩子表（签名显式携带 struct Mcu51Context* ctx 参数，解耦全局单例）
    mcs51_pin_trap_t        pin_traps[4][8];
    mcs51_sfr_read_hook_t   sfr_read_hooks[256];  // (*hook)(struct Mcu51Context* ctx, uint8_t addr)
    mcs51_sfr_write_hook_t  sfr_write_hooks[256]; // (*hook)(struct Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val)

    // 3. 中断子系统完整状态（SSOT：in_service_depth > 0 完全等价于 in_isr，废除冗余 bool）
    void (*isr_table[28])(void);
    uint32_t pending_interrupts;
    uint8_t  in_service_prio_stack[4];
    uint8_t  in_service_depth;
    bool     interrupts_enabled;
    bool     reti_suppress_one;
    uint32_t isr_dispatch_count[28];   // 彻底收归 Context 管理，reset 时一并清零，严禁悬空全局静态区

    // 4. 时钟与虚拟调度态
    uint64_t virtual_us;
    uint32_t step_count;

    // 5. 8051 标准核心外设私有状态聚合与 SoC 扩展解耦
    Mcu51TimerState    timer;
    Mcu51ExtIntState   extint;
    Mcu51UartState     uart;
    void*              soc_priv;       // 厂商扩展外设私有状态（如 CMS8S ADC、STC8 PCA）；通用 Core 严禁内嵌特定 SoC 状态

    // 6. 定时边沿注入队列（F1）
    McuEdgeEvent edge_queue[64];
    uint8_t      edge_head;
    uint8_t      edge_tail;

    // 7. 低功耗停泊 OSAL 原语（SSOT：IDLE/PD 状态 100% 派生自 sfr_shadow[0x87]，坚决不设重复 bool）
    wink_event_t wake_event;           // R6 映射 OSAL 事件对象（真静默态停泊，reset 时初始化）
};
```

#### 2. 多实例物理隔离边界裁决（彻底剔除单进程切换虚饰）
针对多 MCU 协同仿真（如“面板机 + 主控机”），确立基于真实物理边界的隔离架构：
1. **Tier 2 Native 编译的物理局限**：
   * 8051 业务固件的 C 全局变量（`data`/`idata`）在宿主本地编译后直接落入宿主进程的原生 `.data`/`.bss` 静态段，**无法被 `Mcu51Context` 封装隔离**；
   * `WINK_ISR(n)` 宏展开为全局 `extern "C" void wink_isr_vector_N(void)`，同一进程链接两个固件 TU 必然引发链接器符号碰撞（`multiple definition`）；
2. **物理隔离真实形态**：
   * **Web 浏览器端（生产环境）**：以**多 Wasm 实例**为天然物理边界，线性内存与全局变量原生彻底隔离；
   * **Host 单元测试端**：多 MCU 对拍采用**多独立进程**（或独立动态加载域），严禁在框架单进程内空想“上下文切换”；
   * **剔除虚饰设计**：Phase 1 彻底砍掉 `pin_base_offset`（0/32 命名空间）与 `McuContextGuard` RAII 类。
3. **上下文访问器**：
   上下文在当前进程内保持清晰单例指针访问，零抽象成本：
   ```cpp
   extern Mcu51Context* g_active_mcu_context;
   inline Mcu51Context* mcs51_get_context(void) { return g_active_mcu_context; }
   inline void mcs51_set_active_context(Mcu51Context* ctx) { g_active_mcu_context = ctx; }
   ```

#### 3. 零过渡宏硬切换纪律与旧入口物理拔除（Zero-Macro Cutover）
项目初期坚决杜绝“带病合入”的伪平滑过渡层：
1. **废止过渡宏**：
   * 严禁引入 `#define wink_mcs51_sfr_shadow (mcs51_get_context()->sfr_shadow)` 等 5 个过渡宏！
   * 框架内部总共仅约 13 个 TU，所有旧全局符号名在同一个原子重构 PR 中全量改写为 `ctx->...` 或 `mcs51_get_context()->...`，杜绝旧符号在新代码中继续蔓延。
2. **物理拔除孤立旧入口与 Wrapper**：
   * 彻底删除各外设散落的 `cms8s_adc_reset()`、`wink_mcs51_timers_reset()`、`wink_mcs51_uart_reset()` 等 C 符号，统一收敛为 R1 描述符表 `reset(ctx)` 驱动；
   * 外设模型中硬编码的 `VECTOR_ADC = 19` 常量彻底清除，统一使用 `IRQ_SOURCE_ADC` 逻辑枚举；
   * `wink_mcs51_dispatch_vector` 降级为中断控制器内部 static 函数，外设模型严禁跨模块直调；
   * 彻底物理删除 `s_in_isr` 标志位，`wink_mcs51_in_isr()` 统一收敛为单行表达式：`return mcs51_get_context()->in_service_depth > 0;`。

#### 4. 硬件重置生命周期与种子规范：BSS 清零（`memset 0`）$\ne$ 硬件复位（Reset）
* **硬件种子红线**：
  * **端口锁存器 P0–P3 上电复位种子必须为 `0xFF`**：呈现高电平弱上拉输入态。若容器化时误用 `memset 0` 清零，会导致所有引脚被硬件强拉至低电平，破坏 PinArbiter 外部输入仲裁；
  * **特殊功能寄存器复位种子**：CMS8S `PS_ADET`（XSFR 0xF0CC）硬件种子为 `0x7F`；堆栈指针 `SP` 硬件种子为 `0x07`；`PCON` 为 `0x00`；`SBUF` 保持未定义；
* **生命周期规范**：上下文初始化统一由 `mcs51_context_reset(Mcu51Context* ctx)` 负责，按真实硅片 Reset 种子表逐项装载，并在内部显式调用 `wink_event_init(&ctx->wake_event)` 初始化停泊事件对象。**严禁在单元测试中直接 `memset` 覆盖上下文！**

---

### Task R1 — 将外设收敛至编译期 `const` 描述符表（坚守 ADR-0004）

#### 1. 严禁 weak 弱符号，强制使用强符号链接
* **历史教训**：[`mcs51_bridge.cpp:205-209`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/mcs51/src/mcs51_bridge.cpp#L205-L209) 已明确证明：在 Windows MinGW/PE 格式下，无强符号后备的弱符号（weak）会直接被链接器解析为 `NULL`，直接导致 Host 测试空指针崩溃！
* **架构约束**：**严禁使用 weak 默认表！** 描述符表由 SoC Profile TU（如 `cms8s78xx_profile.c`）强符号提供；单元测试由测试桩 TU 显式提供强定义。

#### 2. 静态描述符表设计（`frameworks/mcs51/include/mcs51_peripheral.h`）
```c
#pragma once
#include <stdint.h>
struct Mcu51Context;

typedef enum {
    MCS51_PHASE_CLOCK    = 0,
    MCS51_PHASE_RX_DRAIN = 1,
    MCS51_PHASE_EXTINT   = 2,
    MCS51_PHASE_ADC      = 3,
    MCS51_PHASE_EDGE     = 4,
    MCS51_PHASE_MAX
} mcs51_poll_phase_t;

typedef struct {
    const char*        name;
    void             (*init)(struct Mcu51Context* ctx);
    void             (*reset)(struct Mcu51Context* ctx);
    void             (*poll)(struct Mcu51Context* ctx);
    uint64_t         (*next_event_us)(struct Mcu51Context* ctx); // 返回下一次内部事件的最早虚拟时间戳（无则返回 UINT64_MAX）
    mcs51_poll_phase_t phase;
} mcs51_peripheral_desc_t;

// 强符号常量表，严禁 weak
extern const mcs51_peripheral_desc_t g_mcs51_peripherals[];
extern const uint8_t g_mcs51_num_peripherals;
```
*注：`next_event_us` 由各外设自主上报下一次内部事件的最早时刻（例如 Timer 溢出时刻、ADC 转换完成时刻），直接服务于 Task R6 的低功耗停泊睡眠时间计算，彻底避免无事件轮询。*

#### 3. 外设生命周期与 `post_init_hook` 职责清晰切分
* **生命周期完全收敛**：所有片上与板载外设的 `init`/`reset`/`poll` 生命周期**100% 归描述符表静态驱动**，彻底终结在主循环或 bridge 中散落裸调用的历史；
* **`post_init_hook` 降级为纯测试缝隙（Test Seam）**：`post_init_hook` 更名并严格限定为 `mcs51_test_bind_pin_traps()`，仅用于单元测试运行时动态绑定引脚 Trap（因为 `const` 描述符表无法在单测中逐用例动态修改），严禁将其作为芯片/板级外设初始化的后门通道。

---

### Task R4 — `mcs51_cleanup.py` 预处理条件分支跟踪 & SDCC 转译前置

* **预处理分支跟踪**：在 `build_code_mask` 扫描器中加入状态机，跟踪 `#if / #ifdef / #ifndef / #else / #elif / #endif`，对未激活分支（如 `#if 0`）整段空白遮罩，消除废弃代码段内的 ISR 误重写。
* **SDCC 转译分支**：`interrupt N` $\to$ `__interrupt(N)`；`code` $\to$ `__code`；`_at_ 0xNN` $\to$ `__at(0xNN)`；提供 SDCC 专用的厂商 Shim 设备头。
* **加桩编译目标门控（Target Gating）**：加桩改写 Pass **仅在 `--target=native`（Tier 2 C++ 代理）下启用**；当 `--target=sdcc`（Tier 3 现代 1T ISS 编译）时，**严禁改写加桩**！ISS 依靠 CPU 逐指令执行真实消耗周期，加桩会导致指令执行 + shim 推进的双重时间膨胀与符号缺失；`_nop_()` 在 native 下映射为 `wink_mcs51_microstep()`，在 SDCC 下严格保留为原生 8051 `NOP` 汇编指令。
* **用户本地同名函数守卫（Local Definition Guard）**：改写扫描器优先检索工程/TU 内函数定义。若固件源码本身已包含 `delay_ms` / `delay_us` 的函数实体实现（Function Definition），判定为用户私有延时实现，**强制跳过改写**并在构建日志中输出 `[mcs51_cleanup] Skipped locally-defined delay function: delay_ms`，严防用户自定义函数被劫持为 SDK shim 导致失联或死循环。

---

### Task R5 — 软件延时加桩与双后端时序差异隔离

#### 1. 运行时服务 `wink_delay_us()`：微步步进调度泵（Step-Pumped Loop）
延时期间**严禁一次性粗暴跳跃时间**（否则会冻结延时期间并发的定时器与数码管扫描中断），且**严禁直接裸写 `ctx->virtual_us`**（必须通过时钟模块计费以驱动配额 yield 和 catch-up hook）：
```c
void wink_delay_us(uint32_t total_us) {
    uint32_t elapsed = 0;
    while (elapsed + WINK_MCS51_MICROSTEP_US <= total_us) {
        // 满量子轮：微步泵进（内部 charge 5µs 并驱动外设微步钩子与 catch-up 派发中断）
        wink_mcs51_microstep();
        elapsed += WINK_MCS51_MICROSTEP_US;
    }
    uint32_t remainder = total_us - elapsed;
    if (remainder > 0) {
        // 尾款轮：直接计费剩余时间，杜绝微步满量子超调与双重记账
        wink_mcs51_charge_us(remainder);
    }
}
```
* **计费纪律**：满量子轮调用 `wink_mcs51_microstep()`（内部包含 `charge_us` + 外设微步钩子），尾款轮调用 `wink_mcs51_charge_us(remainder)`。全程严禁直接写 `ctx->virtual_us`，彻底杜绝“时间双倍膨胀”与“尾款强制进位为 5µs 超调”。*
* **延时入口统一与废除粗粒度瞬移**：历史遗留的 `wink_mcs51_delay_ms(ms)`（`mcs51_clock.cpp:142`）原本采用 10ms 粗暴瞬移与单次 catch-up，严重违背防中断塌缩红线；重构后 `wink_mcs51_delay_ms(ms)` **彻底重写为对 `wink_delay_us((uint32_t)ms * 1000)` 的单行内联转发**，全系统统一通过微步调度泵平滑推进，物理注销粗粒度瞬移旧代码。*

#### 2. 时序平移合法性与宏延时边界声明
1. **时序平移预期收益**：具名延时加桩后，固件时间从 $0\mu\text{s}$ 变为真实耗时，依赖延时的场景波形时间戳必然后移。**该平移是消除“仿真时间冻结 Bug”的预期收益，验收时不视为回归错误**。
2. **宏延时边界与本地同名函数**：
   * 形如 `#define DELAY_MS(x)` 的宏延时无法通过调用点 AST 重写，属于未加桩已知差异，场景打上 `timing-tolerance` 标签，断言走宽容区间；
   * 用户本地自定义的 `delay_ms()` 因本地守卫跳过改写，虚拟时间自然流逝（Native 下为 0µs 差异区间，ISS 下为真实指令周期），同样走 `timing-tolerance` 验收规则。

---

### Task R6 — [新增] 低功耗待机与停泊原语（PCON.IDL / PCON.PD）

#### 1. 机制缺陷与时钟域错位根因剖析（Wall-Clock vs Virtual Clock）
* **时钟域错位地雷**：
  `wink_event_pend(&wake_event, timeout_ms)` 是 OSAL 线程级阻塞原语，挂在宿主操作系统的**墙钟（Wall-Clock）**上：
  * 若在 IDLE 停泊时直接调用 `pend(timeout_ms)`，内部事件（如 100ms 后的定时器 tick）将变成**“宿主墙钟硬等 100ms”**——在 Headless / CI 自动化测试中，仿真虚拟时间本应在数十微秒内纯算完成，硬等会导致用例耗时剧烈膨胀，甚至触发 CI 超时！
  * 停泊期间虚拟时钟冻结，仿真器**“零成本快进虚拟时间”**的核心价值完全丧失。
* **双模解法（活跃微步泵进 vs 静默态事件停泊）**：
  1. **活跃排程态（有内部计划事件，`next_us != UINT64_MAX`）**：**绝不调用 OSAL `pend`**！直接复用 Task R5 的微步调度泵循环推进虚拟时间。微步内部每达到配额 `WINK_MCS51_QUOTA_US`（$10000\mu\text{s}$ / 10ms，对齐 100Hz 主 tick）天然执行 `pal_os_sleep_ms(0)` 出让 CPU 供宿主注入外部事件；外部事件通过置位 `ctx->wake_flag` 提前打断微步泵。**纯 CPU 运算瞬间快进，零墙钟损耗**！
  2. **真静默态（无内部计划事件，`next_us == UINT64_MAX`）**：仅在 MCU 内部无自发事件（如全部定时器关闭或处于 PD 掉电）时，才调用 `wink_event_pend` 等待外部宿主事件唤醒；
  3. **Horizon 超时语义**：静默态下若超时达到 `scenario_horizon`，其语义是**本次 Scenario 场景生命周期结束（运行超时终止）**，直接退出测试运行，严禁当作普通唤醒继续盲跑。

#### 2. 双模调度原语与 IDLE / PD 模式切分
写 PCON 钩子由 `mcs51_on_pcon_write` 统一拦截：

```c
// 计算整个 MCU 系统下一次内部计划事件的最早虚拟时间戳
uint64_t mcs51_calc_next_event_us(struct Mcu51Context* ctx, bool is_pd) {
    uint64_t earliest = UINT64_MAX;

    // 1. 若处于 IDLE 模式（非 PD 掉电），遍历 R1 描述符表查询各外设计划事件
    if (!is_pd) {
        for (uint8_t i = 0; i < g_mcs51_num_peripherals; i++) {
            if (g_mcs51_peripherals[i].next_event_us) {
                uint64_t t = g_mcs51_peripherals[i].next_event_us(ctx);
                if (t < earliest) earliest = t;
            }
        }
    }

    // 2. 查询定时边沿注入队列（F1 edge_queue 队头）
    if (ctx->edge_head != ctx->edge_tail) {
        uint64_t edge_t = ctx->edge_queue[ctx->edge_tail].fire_us;
        if (edge_t < earliest) earliest = edge_t;
    }

    return earliest;
}

void mcs51_on_pcon_write(struct Mcu51Context* ctx, uint8_t old_val, uint8_t new_val) {
    // ── 1. IDLE 待机模式 (PCON.0 = 1) ──────────────────────────────────────
    if ((new_val & 0x01) && !(old_val & 0x01)) {
        WINK_TRACE_MCS51_LOWPOWER(ENTER_IDLE, ctx->virtual_us);

        uint64_t next_us = mcs51_calc_next_event_us(ctx, false);
        if (next_us != UINT64_MAX && next_us > ctx->virtual_us) {
            // 【模式 A：活跃排程态】通过微步泵快速快进虚拟时钟，杜绝墙钟硬等！
            // 每次 microstep() 自动推进 5µs 并进行外设 catch-up，每消耗配额自动协作式 yield 给宿主
            // 中断挂起检测：若在此期间有中断请求挂起（pending_interrupts != 0），自然打断微步泵退出
            while (ctx->virtual_us < next_us && ctx->pending_interrupts == 0) {
                wink_mcs51_microstep();
            }
        } else {
            // 【模式 B：真静默态】无任何内部未来事件，挂起等待外部宿主事件注入
            wink_status_t st = wink_event_pend(&ctx->wake_event, WINK_MCS51_SCENARIO_HORIZON_MS);
            if (st == WINK_ERR_TIMEOUT) {
                // Horizon 超时：生命周期已尽，场景终止退出，防无事件僵死
                wink_mcs51_scenario_terminate_timeout(ctx);
                return;
            }
        }

        ctx->sfr_shadow[0x87] &= ~0x01; // 退出 IDLE，PCON 影子位为唯一真值源
        WINK_TRACE_MCS51_LOWPOWER(EXIT_IDLE, ctx->virtual_us);
    }

    // ── 2. Power Down 掉电模式 (PCON.1 = 1) ──────────────────────────────────
    if ((new_val & 0x02) && !(old_val & 0x02)) {
        WINK_TRACE_MCS51_LOWPOWER(ENTER_PD, ctx->virtual_us);

        // 真实硬件规律：停振、片内定时器全部冻结，仅外部使能引脚中断（INT0/INT1 且 EA=1）或复位可唤醒
        // 进入真静默态挂起，等待宿主注入；若超时到达则触发场景超时终止
        wink_status_t st = wink_event_pend(&ctx->wake_event, WINK_MCS51_SCENARIO_HORIZON_MS);
        if (st == WINK_ERR_TIMEOUT) {
            wink_mcs51_scenario_terminate_timeout(ctx);
            return;
        }

        ctx->sfr_shadow[0x87] &= ~0x02; // 退出 PD，PCON 影子位为唯一真值源
        WINK_TRACE_MCS51_LOWPOWER(EXIT_PD, ctx->virtual_us);
    }
}
```

#### 3. 异步外部中断唤醒机制（SSOT 与纯事件驱动）
当宿主或 UniSim 通道产生外部中断并调用 `mcs51_raise_irq(src)` 时：
* **请求挂起（自然打断微步泵）**：中断控制器置位 `ctx->pending_interrupts` 对应位。处于 IDLE 活跃排程泵循环中的 CPU 在下一微步检测到 `pending_interrupts != 0` 即刻自然退出微步泵循环，无需额外维护 `wake_flag` 布尔标志；
* **真静默态唤醒**：若当前芯片正处于 `wink_event_pend`（真静默态）：
  - **IDLE 模式**：直接调用 `wink_event_post(&ctx->wake_event)` 唤醒挂起的 Fiber；
  - **PD 掉电模式**：严格遵循硬件物理纪律，仅当芯片处于中断全局使能（`EA=1`）且对应外部中断使能位有效时，才调用 `wink_event_post(&ctx->wake_event)` 唤醒 CPU；
* 主线程退出低功耗态后清除 PCON 对应位，在下一微步汇合点按 Task R3 仲裁规则依序进入中断服务程序（ISR）。

---

## 6. Phase 2 补充任务优化（闭环恢复）

### Task F3 — 微步量子动态标定
微步量子（`WINK_MCS51_MICROSTEP_US`）与 `wink-app.json` 中配置的 `clock_hz` 成反比动态标定，严禁全局硬编码写死 1µs。

### Task F1 — 定时边沿注入队列时间戳对齐
边沿事件挂载虚拟时钟 `virtual_us`，队列置于 `Mcu51Context` 中，解锁 DHT11 / 红外 NEC / 超声波 ECHO 场景。

### Task F2 — [闭环恢复] TMOD C/T=1 外部脉冲计数建模
* **消除 STRICT 阻断**：在 `mcs51_timer.cpp` 中将 `MCS51_FEAT_TIMER_EXT_CLK` 从 `wink_mcs51_unsupported` STRICT 报错中正式转正；
* **机制**：监听 T0 (P3.4) 与 T1 (P3.5) 的引脚下降沿事件（复用 F1 时间线），驱动计数器递增，支撑编码器与外部脉冲测频。

### Task F4 — [闭环恢复] 通道 1b 软 PWM 占空量测与引脚角色声明
在 `unisim-scenarios` 与 bridge 侧引入基于引脚翻转时间戳的高精度占空比聚合计算器，量测精度误差 $< 2\%$，支撑电机调速与呼吸灯断言。

---

## 7. Phase 3 详细架构设计：Tier 3（现代 1T ISS）演进与选型

### 7.1 Task T1 — 许可证前置门、选型 Spike 与工具链策略（产出 ADR-0079）

* **前置交付门禁**：Task T1 启动前必须先产出并评审通过 `ADR-0079`。
* **工具链探测策略（遵循 ADR-0030）**：SDCC 作为外部可选工具链，**框架绝不静默下载安装**。通过 `wink toolchain check` 探测，若未安装则友好提示安装指引。
* **开发期 Oracle 对拍**：`ucsim` 不进入交付代码库，仅在 CI 中作为独立二进制参考机进行对拍。

### 7.2 Task T2 — CPU 执行态（`Cpu51State`）与通用外设时序策略位

Tier 3 虚拟机状态 = `Mcu51Context`（外设态，R2）+ `Cpu51State`（CPU态，T2）：

```cpp
struct Cpu51State {
    uint8_t  iram[256];    // 0x00-0x7F 直接/间接共享; 0x80-0xFF 仅 @R0/@R1 间接寻址
    uint16_t pc;
    uint8_t  sp;           // 复位默认 0x07
    uint8_t  acc, b, psw;  // 含真实 CY/AC/OV/P
    uint16_t dptr[2];      // 双 DPTR 支持
    uint8_t  dps;          // DPTR 选择位
    uint8_t  sfr_page;     // SFR 翻页选择
    uint8_t  code_bank;    // CODE banking
    uint64_t cycle_count;  // 1T 周期计数
};
```

#### 外设时序策略位（Timing Policy）与完成事件排程
外设模型支持策略切换，彻底兼顾 Native 与 ISS：
* `MCS51_TIMING_NATIVE_0CYCLE`（Native 模式）：写触发后 0 周期穿透完成；
* `MCS51_TIMING_ISS_SCHEDULED`（ISS 模式）：写 `ADCON0` 后保持 `ADGO=1`，向上下文排定未来 $+150\mu\text{s}$ 的完成事件，恢复硬件级真实轮询。
  * **完成事件排程落地（复用 Context 调度时间线）**：
    完成事件明确挂载至 `Mcu51Context` 调度时间线（复用 Task F1 `edge_queue` 同款时间戳队列机制，或由外设私有状态记录 `busy_until_us = ctx->virtual_us + 150` 并直接通过 Task R1 描述符的 `next_event_us(ctx)` 槽暴露给调度器）。
    当虚拟时间演进到达完成时刻，微步调度器触发完成回调：影子装载转换结果、清零 `ADGO`、置位 `ADCIF`，并调用 `mcs51_raise_irq(IRQ_SOURCE_ADC)`，确保外设排程有明确的落脚点与生命周期管理。

### 7.3 Task T3 — ISA 深水区与 Golden 验证硬门禁
* **Golden ISA Suite 硬门禁**：必须 100% 跑通公开 8051 全指令汇编单测集（覆盖 256 Opcode、奇偶位 P、`DA A` 十进制调整、`JBC` 原子测试清零、堆栈溢出），**测试全绿才允许进入外设挂接**；
* **差分对拍**：自研核与 CI 参考机（ucsim）随机指令流对拍，寄存器/内存差异为 0。

### 7.4 Tier 3 运行拓扑

```
┌────────────────────────────────────────────────────────────────────────┐
│ 8051 ISA Core Engine (宽松协议开源核 / 自研 256 Opcode 解码器)           │
│  ├─ Cpu51State: iram[256] / SP / PC / ACC / PSW / 双 DPTR / banking    │
│  ├─ 现代 1T 可插拔周期表 (Pluggable Vendor Cycle Profile)               │
│  └─ 指令边界中断扫描器 (对接 Context pending 位图与 in-service 栈)      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Universal MMIO & Data Bus (带外设时序策略位)                           │
│  ├─ MOV Px, A ──> R0 下沉的 mcs51_gpio_sfr_write()（100% 复用强度仲裁） │
│  ├─ ANL Px, A ─> read_latch 路径；MOV A, Px ─> read_pin 三路仲裁       │
│  ├─ MOV 0xDF, A ──> 经时序策略调度: ISS 排程 150µs 转换 / Native 0 周期 │
│  ├─ 外设事件 ──> mcs51_raise_irq(IRQ_SOURCE_x) ──> 厂商映射表 ──> pend  │
│  └─ PCON.IDL/PD ──> R6 双模停泊（活跃泵进 / 静默 pend；ISS 侧解码器停推进） │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ UniSim PinArbiter / js_pal_* / 前端 Vue 插件 (100% 保持不变)            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 8. 验收标准与测试保障矩阵

彻底拆分**功能逻辑等价性**与**时序重基线**，杜绝自相矛盾的假门禁：

| 检验阶段 | 验收类别 | 验收项 | 验收指标与门禁命令 |
| :--- | :--- | :--- | :--- |
| **Phase 1 前置** | 逻辑断言 | 中断特征化测试 | 新增 ADC ISR 写 SBUF、同优先级不抢占、关 EA 临界区 3 条用例全绿 |
| **Phase 1 R0** | 逻辑断言 | GPIO 下沉零回归 | 既有 gpio / RMW 隔离全绿；read_pin 与 read_latch 选路单测全绿 |
| **Phase 1 重构** | **逻辑断言** | **功能逻辑零回归** | **MSVC/MinGW CTest 23/23 PASS**；Headless 5 个场景中**引脚最终电平、总线 Payload、中断计数 100% 吻合** |
| **Phase 1 重构** | **交叉编译门禁** | **双 Target 同源闭环** | **Wasm/Node CTest 10/10 PASS**（ADR-0002 / ADR-0075 生产 wasm 链接无未决符号，Node 环境下 10 项外设与端到端测试全绿） |
| **Phase 1 重构** | **纯洁度门禁** | **旧符号与宏零残留** | `git grep -E "wink_mcs51_sfr_shadow|wink_mcs51_xdata_shadow|wink_mcs51_pin_traps|cms8s_adc_reset|s_in_isr" frameworks/mcs51/` **0 命中** |
| **Phase 1 重构** | **应用零侵入** | **wink-micro-app 一行不改** | 现有 12 个 MCS51 示例工程源码与配置 100% 保持原样，构建与仿真行为零破坏 |
| **Phase 1 R5** | **时序断言** | **延时加桩审计重基线** | 具名延时改写构建日志 100% 可审计；**允许波形时间戳按加桩换算值平移，CI 输出平移 Diff 审计日志**；加桩场景误差 $< 5\%$ |
| **Phase 1 R6** | 逻辑断言 | IDLE/PD 停泊与唤醒 | 验证 PCON.IDL/PD 触发挂起、定时器到期快进与外部中断立即唤醒，0 假死死锁 |
| **Phase 1 重构** | 架构门禁 | 架构分层门禁 | `python wink-tools/wink.py lint arch --pack layering --pack api` **0 findings** |
| **Phase 2 扩展** | 逻辑/时序 | 定时计数与软 PWM | **Task F2 外部脉冲计数测试 PASS**；**Task F4 软 PWM 占空量测精度误差 $< 2\%$** |
| **Phase 3 Tier 3**| 核心门禁 | **Golden ISA Suite** | 全 256 Opcode 寻址方式 / Flag 边界 / BCD / 堆栈 **100% PASS**（T3 准入条件） |
| **Phase 3 Tier 3**| 核心门禁 | 差分对拍 | 随机指令流自研核 vs CI 参考机（ucsim oracle）寄存器/内存差异 **0** |
| **Phase 3 Tier 3**| 场景一致性 | 双后端同场景一致性 | 加桩场景双后端运行波形 100% 吻合；`timing-tolerance` 场景走宽容区间 |
| **Phase 3 Tier 3**| 性能吞吐 | 亚微秒时序闭环 | 1T @ 24MHz 吞吐 $\ge 20\text{M}$ inst/s；WS2812 亚微秒时序波形闭环 |

---

## 9. 架构纯洁度纪律、日落策略（Sundown Policy）与实施决议指引

### 9.1 架构纯洁度与日落策略（Sundown Policy）四大铁律

为防止重构过程中因“兼容旧代码”而引入伪平滑冗余或留下永久技术债，项目初期全员必须严格执行以下四条工程红线：

1. **【零过渡宏与旧入口物理拔除铁律】**：
   * 严禁在主干保留任何过渡宏（如 `wink_mcs51_sfr_shadow -> ctx->...`）或旧入口 wrapper（如独立的 `cms8s_adc_reset()`）；
   * 所有调用点重构必须在对应任务的原子 Commit 内一次性改写完毕；
   * PR 合入前必须执行静态门禁扫描：`git grep -E "wink_mcs51_sfr_shadow|wink_mcs51_xdata_shadow|wink_mcs51_pin_traps|cms8s_adc_reset|s_in_isr" frameworks/mcs51/`，确保 **0 命中**。
2. **【`WINK_MCS51_TWO_PHASE_IRQ` 逃生开关的生命周期窗口】**：
   * 该宏仅作为 Task R3 调试期的临时排障安全阀；
   * **严格日落窗口**：在 Node Headless 5 个基线场景与厂商示例全绿后的**紧接着下一个 Commit**，必须将该宏分支彻底物理拔除，严禁双分支代码带入生产分支维护。
3. **【多实例物理隔离边界与防过度设计底线】**：
   * 坚决不在单进程内虚拟化多 MCU 上下文切换；
   * 多 MCU 协同仅承认两类物理边界：**Web 端多 Wasm 实例**与 **Host 端多独立进程**；
   * 严禁在框架内引入无物理基础的 `pin_base_offset`、`McuContextGuard` 等过度设计。
4. **【`wink-micro-app` 零侵入承诺底线】**：
   * 本计划所有底层重构恪守 ADR-0070 / ADR-0075 的核心承诺：**`wink-micro-app` 源码与配置 100% 一行不改**；
   * 配置面自动防御：`wink-app.json` 缺省 `simulation` 字段时自动 fallback 至 Native 24MHz / 5µs 微步；
   * 宏延时与本地延时函数实现守卫：严防 AST 加桩对用户私有延时函数的错误劫持。

### 9.2 总结与架构决议指引

本实施计划（v3.5 架构纯洁度定稿版）确立了兼具理论高度与工程实战性的演进闭环：
1. **战略定力与门禁先行**：坚持以 Tier 2 为主线；**R3 合入前必须先落地 ADR-0078 并回写规范 07，T1 启动前必须先落地 ADR-0079**；
2. **战术关键序列**：**R3**（中断两阶段收口、映射、在服务屏蔽与防塌缩）$\to$ **R0**（GPIO 双读路径下沉）$\to$ **R2**（标准核心 Context、复位种子、Hook 签名携带 ctx、剥离伪多 MCU 虚饰与厂商状态解耦）$\to$ **R1**（强符号描述符表与事件槽、澄清 test seam）$\to$ **R4/R5**（cleanup 加固、延时加桩 Target 门控、同名函数守卫、步进泵防超调与延时入口统一）$\to$ **R6**（IDLE 微步泵进与静默态双模时钟域治理、PCON 影子位 SSOT、Horizon 超时终止闭环）；
3. **微架构与链接底线**：严守 1T 单周期机器模型；**坚决不使用 weak 弱符号以防 MinGW/PE 链接事故**；外设模型注入**时序策略位与 Context 调度落地**以支撑 Tier 3 真实轮询；
4. **诚实与科学验收**：坚决摒弃“改了延时却假装零回归”的掩耳盗铃，验收矩阵分轨为**“功能逻辑 100% 零回归（MSVC 23/23 + Wasm 10/10）”**、**“时序断言审计式重基线”**与**“旧符号宏零残留静态门禁”**；ISS 准入以 Golden ISA Suite 为硬门禁。
