# 8051 混合仿真时钟域、外设时序推进与生命周期一致性技术设计规格书

| 属性 | 内容 |
| :--- | :--- |
| **文档状态** | Draft - 方案深化与落实现行标准 (In-Progress) |
| **创建日期** | 2026-08-27 |
| **所属模块** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` |
| **上级总规格书** | [8051 零侵入仿真拦截与 C++ 代理架构](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md) |
| **数据面规格书** | [SFR 影子代理、整端口 RMW 与边沿感知规格书](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md) |
| **主规划书** | [wink-micro-os 多系列 MCU 兼容方案](mcu-compat-plan.md) |
| **关联架构决议** | AD-2 (功能级精度), AD-12 (边沿感知), AD-13 (Trap 执行红线), AD-14 (UniSim 1:1 时钟映射), AD-15 (DIO 双向状态机) |

---

## 1. 方案背景与问题陈述 (Problem Statement & B 组审查复盘)

在 8051 仿真拦截层构建过程中，除数据面（寄存器读写与操作符代理）外，系统在**时钟推进、多线外设协议以及跨单元生命周期**三个维度面临深层次的时序一致性挑战（B 组审查项）：

### 1.1 审查问题复盘
1. **B1：ADC0832 DIO 单线复用与双向陷阱时序撕裂**：
   - 经典单片机硬件为节约 I/O，通常将 ADC0832 的 `DI` 与 `DO` 物理并联接入单片机同一个引脚（合称 `DIO`，如 `P1.0`）；
   - 原设计仅有 4 线独立模型（独立 DI、DO），缺乏单引脚同时挂接读写陷阱（`on_write` 与 `on_read`）的处理；若在读取阶段 MCU 按准双向口惯例执行 `DIO = 1` 释放总线，状态机会将该释放电平**误当成又一个通道配置位**，导致协议状态机彻底错乱。
2. **B2：忙等自推进时钟与 UniSim 物理时间尺度失真**：
   - 51 内部定时器依赖虚拟微秒时钟，而即时外设（ADC0832 / CMS8S ADC）在 Trap 内完成（耗时 0µs）；
   - 若未定义 51 虚拟时钟与宿主 UniSim 物理积分步进的映射契约，当业务代码执行 `delay_ms(100)` 时，物理世界热平衡方程的时间步长可能被压缩或膨胀，导致电熨斗温度曲线失真。
3. **B3：WINK_ISR 注册与 C++ 静态初始化顺序死锁 (Static Initialization Order Fiasco)**：
   - `-x c++` 模式下，若 `WINK_ISR(n)` 展开为静态构造函数注册函数指针，如果 framework 层的全局分发表依赖运行时的 C++ 构造器，跨编译单元的构造顺序未定义会导致解引用野指针崩溃。

本规格书作为 **B1、B2、B3** 问题的单一事实来源（SSOT），系统性建立混合时钟域契约、3 线 DIO 阶段隔离状态机与全 POD 常量初始化安全规范。

---

## 2. 双时钟域架构与 UniSim 1:1 硬实时物理时间映射 (解决 B2)

### 2.1 架构核心：时间主从分工体系

仿真系统内部存在两套正交的时间轴，必须明确**主从时钟关系（Master-Slave Contract）**：

```
+-----------------------------------------------------------------------------------+
|                        UniSim 物理引擎与宿主事件循环 (Master)                       |
|   - 运行频率：固定 100 Hz (dt = 10 ms = 10,000 us)                                |
|   - 职责：求解发热盘、NTC 阻值与环境对流微分方程: T[n+1] = T[n] + (P - Q)/C * dt   |
+-----------------------------------------+-----------------------------------------+
                                          | 每 10ms 派发一个宿主 Tick，分配 10,000us 配额
                                          v
+-----------------------------------------------------------------------------------+
|                         51 协程运行时 Fiber 上下文 (Slave)                        |
|   - 虚拟时钟：s_virtual_us (单调递增微秒计数器)                                    |
|   - 职责：驱动 Timer0/Timer1 溢出、执行用户 main() 裸机循环与延时让出              |
+-----------------------------------------------------------------------------------+
```

1. **主时钟（Master Clock）**：**宿主调度器 `app_loop`（UniSim 物理引擎）**。
   宿主环境（Wasm 浏览器主线程或 Host 测试环境）以固定频率（推荐 100Hz，即单个宿主 tick 步进 $\Delta t = 10\text{ms} = 10,000\mu\text{s}$）进行物理微积分计算。
2. **从时钟（Slave Clock）**：**51 协程内部虚拟时间 `s_virtual_us`**。
   Fiber 内部虚拟时钟仅用于推进 8051 视角的定时器计数、串口波特率发生与延时循环。

### 2.2 1:1 硬实时步进契约与配额调度算法

为确保电熨斗控温代码的真实时间尺度与物理热模型严格闭环，制定如下调度契约：

1. **1:1 硬实时映射**：
   $$1\text{ ms 宿主物理时间} \equiv 1\text{ ms 51 内部虚拟时间}$$
2. **时间片配额守恒（Quota Invariant）**：
   在宿主派发的每一个物理 Tick（$10\text{ms}$）周期内，分配给 51 Fiber 的最大可消费虚拟时间严格限制为 $10,000\mu\text{s}$。
3. **即时外设零耗时原则（Zero Virtual-Time Cost）**：
   - ADC0832 的 Bit-Bang 脉冲序列与 CMS8S 的写 ADCON 均在 Level 2 纯同步 Trap 内瞬间完成；
   - 其消耗的虚拟时间定义为 **$0\mu\text{s}$**；
   - 物理语义：代表 CPU 在当前微秒时刻对物理世界进行了一次瞬时“理想脉冲采样”，不消耗宿主时间片。
4. **延时与忙等推进规范**：
   - 用户代码调用 `_nop_()`：虚拟时钟步进 $1\mu\text{s}$；
   - 用户代码调用 `delay_ms(N)`：内部调用 `sim_ctx_yield(N * 1000)`，通知调度器已消费虚拟微秒；
   - 若 Fiber 消费完当前 $10\text{ms}$ 配额，即使内部处于裸机忙等，调度器也将**强制切出 Fiber**，让控制权回到宿主以执行 UniSim 物理发热积分并刷新浏览器渲染，下一个宿主 tick 再行唤醒。
5. **收益**：彻底消除了“单片机死循环导致物理引擎冻结”以及“51 延时 10 秒但物理世界只加热 10 毫秒”的时间膨胀与收缩失真。

### 2.3 强制切出的虚拟时间记账补账规则 (Virtual Clock Quota Catch-Up Rule)

当用户代码执行类似 `while(!TF0);`、`while(BUSY);` 这类零 `_nop_` 极紧凑空轮询循环时，单次 tick 可能会迅速耗尽虚拟时间片配额并触发强制切出。

> **【M2 落地校准 2026-08-28，ADR-0072 Accepted】配额片 = 10,000µs（一个 100Hz 主 tick）。** 本节早期文本的「500µs 或 10ms」二选一，经接入生产 runtime 实测裁决为对齐 tick 的 10,000µs：`pal_sim_scheduler_run` 按 fiber **派发次数**计 tick（非时间边界），配额片必须与 tick 等长才能保持 `delay_ms(100) = 10 ticks` 与 1:1 计费守恒；让出动作为 duration-0 协作让出（`pal_os_sleep_ms(0)`），片时间在让出前经 `pal_os_busy_wait_us()` 1:1 计费给主时钟。实现与契约见 Layer-① `04-wasm-simulation/02-mechanisms/02-virtual-clock.md` §6。

为了保持虚拟时钟与宿主物理世界的严格 1:1 步进，调度器在执行强制切出时，必须执行**差额记账补足（Clock Catch-up）**：
```cpp
// 伪代码：在 Fiber 超额强制切出前的记账处理
const uint32_t remaining_quota_us = current_tick_quota_us - fiber_consumed_us;
if (remaining_quota_us > 0) {
    // 将本 tick 未显式通过 delay/nop 消费的剩余配额强制计入虚拟时钟
    s_virtual_us += remaining_quota_us;
    // 同步驱动 Timer0/1 计数器补足步进，确保定时器溢出与外设物理热平衡绝不丢步
    mcs51_timers_step_us(remaining_quota_us);
}
```
- **核心价值**：确保即使 8051 处于完全无阻塞的裸机死等，宿主物理积分、后台定时器计数和外设虚拟时间依然严格向前流动，不会发生由于空转而导致的时钟冻结或热模型失真。

---

## 3. 外设 Bit-Bang 时序推进与 3 线 DIO 复合引脚复用状态机 (解决 B1)

### 3.1 4 线独立 vs 3 线并联双模自适应

在 `include/mcs51_adc.h` 中，提供支持单引脚复用的跨端口初始化接口：

```c
void mcs51_adc0832_init(
    uint8_t cs_port,  uint8_t cs_bit,
    uint8_t clk_port, uint8_t clk_bit,
    uint8_t di_port,  uint8_t di_bit,
    uint8_t do_port,  uint8_t do_bit
);
```
- **自动模式识别**：
  若传入参数满足 `di_port == do_port && di_bit == do_bit`，状态机内部自动激活 **3 线 DIO 单线复用模式**（`is_dio_shared = true`）；
- **复合陷阱挂接**：
  在 `s_pin_traps[dio_port][dio_bit]` 上**同时注册** `on_write` 与 `on_read` 回调函数。

### 3.2 阶段划分 (Phased) 状态机机制

为彻底杜绝 MCU 在读取阶段执行 `DIO = 1` 释放总线时污染输入通道选择位，状态机划分为两个强隔离阶段：

```
                +-------------------------------------------------------+
                |                    IDLE (空闲态)                      |
                | - CS 为高电平 (CS = 1)                                |
                | - 移位寄存器复位，输出位释放为高阻态                  |
                +-------------------------------------------------------+
                                        | CS 下降沿 (CS: 1 -> 0)
                                        v
                +-------------------------------------------------------+
                |       PHASE_INPUT (配置输入阶段：DI/DIO 为输入)       |
                | - 仅响应 CLK 上升沿与 on_write(level)                 |
                | - 第 1 个 CLK 上升沿：采样 Start Bit (必须为 1)       |
                | - 第 2 个 CLK 上升沿：采样 SGL/DIF 位                 |
                | - 第 3 个 CLK 上升沿：采样 ODD/SIGN 位，锁定模拟通道  |
                | - on_read() 调用返回 1 (上拉态)                       |
                +-------------------------------------------------------+
                                        | 第 3 个 CLK 上升沿锁定完成
                                        v
                +-------------------------------------------------------+
                |       PHASE_OUTPUT (数据输出阶段：DO/DIO 为输出)      |
                | - 从 UniSim 3.0 通道 3 (js_pal_adc_read_norm) 拉取 8 位码值|
                | - 【关键屏蔽】：on_write() 忽略所有写入（吸收 MCU 释放）|
                | - 第 3 个 CLK 下降沿：先输出 1 个前导 Null 位 (电平 0)|
                | - 随后 8 个 CLK 下降沿：依次左移输出 MSB-first 8 位数据|
                | - on_read() 调用：返回当前 output 位的实际电平 (0/1)  |
                | - 同步镜像更新 s_sfr_shadow 影子（整端口字节读一致性）|
                | - 用户在 CLK 上升沿采样，时序无 off-by-one 偏差      |
                +-------------------------------------------------------+
                                        | CS 上升沿 (CS: 0 -> 1)
                                        v
                                强制复位，返回 IDLE
```

> **ADC0832 前导 Null 位与采样边沿对齐规范**：
> 真实 ADC0832 硬件在第 3 个 CLK 上升沿锁存 MUX 通道后，在随后的第 3 个 CLK 下降沿会首先输出 1 个周期的**前导空位（Leading Null Bit，高阻态/0）**，从第 4 个 CLK 下降沿才开始真正送出 MSB (Bit 7)。
> 状态机在进入 `PHASE_OUTPUT` 后的首个下降沿输出 0，随后 8 个下降沿送出 Data[7..0]，从而与用户端标准 bit-bang 接收时序（下降沿芯片准备数据、上升沿 MCU 采样）实现微秒级零偏差对接，杜绝错位 1 位（off-by-one）的硬故障。

### 3.3 生产级代码实现 (`src/mcs51_adc0832.cpp`)

```cpp
#include <stdint.h>
#include <stdbool.h>
#include "mcs51_trap.h"
#include "mcs51_adc.h"

enum Adc0832Phase {
    ADC_PHASE_IDLE = 0,
    ADC_PHASE_INPUT,
    ADC_PHASE_OUTPUT
};

struct Adc0832State {
    uint8_t cs_port,  cs_bit;
    uint8_t clk_port, clk_bit;
    uint8_t dio_port, dio_bit;
    bool    is_dio_shared;

    enum Adc0832Phase phase;
    uint8_t bit_counter;
    uint8_t channel_cfg; // [1]: SGL/DIF, [0]: ODD/SIGN
    uint8_t shift_data;  // 8位转换结果移位寄存器
    uint8_t current_out_bit;
} s_adc0832;

// 1. CS 引脚写入陷阱
static void on_cs_write(void* ctx, uint8_t level) {
    if (level == 0) {
        // CS 下降沿：复位并进入配置阶段
        s_adc0832.phase = ADC_PHASE_INPUT;
        s_adc0832.bit_counter = 0;
        s_adc0832.channel_cfg = 0;
        s_adc0832.current_out_bit = 1;
    } else {
        // CS 上升沿：终止转换回到空闲态
        s_adc0832.phase = ADC_PHASE_IDLE;
        s_adc0832.current_out_bit = 1;
    }
}

// 2. CLK 引脚写入陷阱（状态机核心驱动力：指令边沿推进而非时间）
static void on_clk_write(void* ctx, uint8_t level) {
    if (s_adc0832.phase == ADC_PHASE_IDLE) return;

    if (level == 1) {
        // CLK 上升沿：输入配置采样阶段
        if (s_adc0832.phase == ADC_PHASE_INPUT) {
            uint8_t di_val = (s_sfr_shadow[0x80 + (s_adc0832.dio_port << 4)] >> s_adc0832.dio_bit) & 1;
            s_adc0832.bit_counter++;
            
            if (s_adc0832.bit_counter == 1) {
                // 起始位 (Start Bit)，必须为 1
                if (di_val != 1) s_adc0832.phase = ADC_PHASE_IDLE; // 协议错误复位
            } else if (s_adc0832.bit_counter == 2) {
                s_adc0832.channel_cfg |= (di_val << 1); // SGL/DIF
            } else if (s_adc0832.bit_counter == 3) {
                s_adc0832.channel_cfg |= di_val;        // ODD/SIGN
                
                // 3 个配置位收集完毕，锁定通道并换向进入输出阶段
                uint8_t ch = (s_adc0832.channel_cfg & 0x01); // 简化映射：0: CH0, 1: CH1
                s_adc0832.shift_data = (uint8_t)(mcs51_adc_get_value(ch) & 0xFF); // 统一调用通用接口 (底层直通 js_pal_adc_read_norm)
                s_adc0832.phase = ADC_PHASE_OUTPUT;
                s_adc0832.bit_counter = 0;
            }
        }
    } else {
        // CLK 下降沿：数据输出移位阶段 (定死以 on_read 为主、影子镜像为辅)
        if (s_adc0832.phase == ADC_PHASE_OUTPUT) {
            if (s_adc0832.bit_counter < 8) {
                // 移出当前最高位到输出缓存
                s_adc0832.current_out_bit = (s_adc0832.shift_data >> (7 - s_adc0832.bit_counter)) & 1;
                s_adc0832.bit_counter++;
            } else {
                s_adc0832.current_out_bit = 1; // 8 位移完后释放
            }

            // 同步镜像写入真实锁存器影子，保证整端口读 (如 P1 & 0x01) 也能读出一致电平
            uint8_t dio_sfr = 0x80 + (s_adc0832.dio_port << 4);
            if (s_adc0832.current_out_bit) s_sfr_shadow[dio_sfr] |=  (1 << s_adc0832.dio_bit);
            else                           s_sfr_shadow[dio_sfr] &= ~(1 << s_adc0832.dio_bit);
        }
    }
}

// 3. DI/DIO 写入陷阱
static void on_di_write(void* ctx, uint8_t level) {
    // 若处于 PHASE_OUTPUT 阶段，MCU 执行 DIO = 1 纯属准双向口输入使能操作，坚决忽略！
    if (s_adc0832.phase == ADC_PHASE_OUTPUT) {
        return;
    }
    // 配置阶段仅记录锁存器，由 CLK 上升沿统一采样
}

// 4. DO/DIO 读取陷阱
static uint8_t on_do_read(void* ctx) {
    if (s_adc0832.phase == ADC_PHASE_OUTPUT) {
        return s_adc0832.current_out_bit;
    }
    return 1; // 默认空闲上拉为高
}

// 初始化注册
extern "C" void mcs51_adc0832_init(
    uint8_t cs_port,  uint8_t cs_bit,
    uint8_t clk_port, uint8_t clk_bit,
    uint8_t di_port,  uint8_t di_bit,
    uint8_t do_port,  uint8_t do_bit
) {
    s_adc0832.cs_port = cs_port;   s_adc0832.cs_bit = cs_bit;
    s_adc0832.clk_port = clk_port; s_adc0832.clk_bit = clk_bit;
    s_adc0832.dio_port = di_port;  s_adc0832.dio_bit = di_bit;
    s_adc0832.is_dio_shared = (di_port == do_port && di_bit == do_bit);
    s_adc0832.phase = ADC_PHASE_IDLE;

    // 挂接 CS / CLK 陷阱
    s_pin_traps[cs_port][cs_bit].on_write = on_cs_write;
    s_pin_traps[clk_port][clk_bit].on_write = on_clk_write;

    // 挂接 DI (写) 与 DO (读) 陷阱
    s_pin_traps[di_port][di_bit].on_write = on_di_write;
    s_pin_traps[do_port][do_bit].on_read  = on_do_read;
}
```

---

## 4. 跨编译单元生命周期与静态初始化安全规范 (解决 B3)

### 4.1 静态初始化顺序死锁 (Static Initialization Order Fiasco) 风险
在 C++ 标准（[basic.start.dynamic]）中，**不同编译单元（Translation Units）之间全局变量的动态初始化顺序是不确定的**。
若用户工程中：
1. `user_main.c` 经 `-x c++` 编译，内部声明了 `void Timer0_ISR() interrupt 1`，展开为全局注册类构造函数；
2. 若该构造函数运行时，framework 侧的 `s_isr_table` 或 `s_sfr_shadow` 尚未完成构造，将产生未定义行为甚至段错误。

### 4.2 消除死锁三大铁律

为在物理层面彻底杜绝该问题，建立如下架构硬性约束：

#### 铁律 1：核心运行时表结构 100% 采用“零初始化 POD（Plain Old Data）”
所有全局状态表**严禁使用任何带有自定义构造函数（Non-trivial Constructor）的 C++ 类**，统一采用纯 C 原生数组：
```cpp
// 必须且全部直接在 BSS 段静态分配
extern "C" uint8_t                s_sfr_shadow[256]    = {0};
extern "C" mcs51_pin_trap_t       s_pin_traps[4][8]    = {};
extern "C" mcs51_sfr_write_hook_t s_sfr_write_hooks[256] = {nullptr};
extern "C" void                 (*s_isr_table[32])(void) = {nullptr};
```
- **原理保证**：根据 C++ 标准规范，静态存储期的 POD 变量在程序装载阶段即由操作系统/加载器完成 **Zero-Initialization（零初始化）**，其执行时机在任何全局 C++ 对象的构造函数运行**之前**。因此无论哪个编译单元先运行，查表与写入永远指向合法安全的物理内存。

#### 铁律 2：`WinkSfr` 代理实例保持非 const 可变性，通过 `constexpr` 构造函数达成常量初始化
在 `include/mcs51_proxy.hpp` 与 `REGX52.H` 中，所有的端口与 SFR 定义统一声明为：
```cpp
inline WinkSfr P0{0x80, 0};
inline WinkSfr P1{0x90, 1};
inline WinkSfr P2{0xA0, 2};
inline WinkSfr P3{0xB0, 3};
```
- **严禁加 `constexpr` 修饰对象**：在 C++ 中，`constexpr` 修饰变量时**天然隐含 `const` 顶层只读属性**。若声明为 `inline constexpr WinkSfr P0...`，会导致后续用户代码在执行 `P1 = 0x55;` 时调用非 const 的 `operator=` 直接触发编译错误（`no matching operator= for const object`）；
- **常量初始化保证（Constant Initialization）**：因为 `WinkSfr::WinkSfr(...)` **构造函数被声明为 `constexpr`**，且传入参数为编译期常量，根据 C++17 标准（[basic.start.static]），该对象被强制在编译/加载阶段完成静态常量初始化，**完全早于任何运行时动态初始化**，依然 100% 免疫静态初始化顺序死锁（Fiasco）；
- **花括号列表初始化 `{0x80, 0}`**：统一采用花括号语法，彻底消除了传统小括号 `(0x80, 0)` 带来的 Most-Vexing-Parse 语法歧义与宏展开逗号表达式风险。

#### 铁律 3：中断派发执行期隔离（Execution-Phase Gate）
中断分发表 `s_isr_table` 在静态初始化期间**仅允许注册**（写入函数指针）。系统在 `app_init()` 之前设置全局门控标志 `s_interrupts_enabled = false`：
- 在宿主完成全部初始化、正式启动 Fiber 协程之前，任何虚拟时钟与中断调度器严禁触发派发；
- 彻底斩断了“在 main 之前执行 ISR 代码进而解引用未准备就绪的外设对象”的潜在时序死锁。

---

## 5. 关联文档交叉引用矩阵 (Cross-Reference Matrix)

| 关联文档 | 引用章节 | 引用主题与联动要求 |
| :--- | :--- | :--- |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **文件头独立规格书** | 补充本规格书为第 3 份独立子系统标准，形成“1 规划 + 1 总纲 + 2 子系统”完整矩阵。 |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **§3.0.2 / §3.11** | 引用本规格书第 2 节（UniSim 1:1 时钟映射）与第 3 节（ADC0832 3 线 DIO 状态机）。 |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **§7 架构决议表** | 新增 **AD-14** (UniSim 1:1 时钟硬实时映射) 与 **AD-15** (ADC0832 DIO 复合引脚双向状态机)。 |
| [`2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md`](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md) | **§3.2 / §6.1 / §7** | 增补对本规格书三大章节的专门跳转，确保总纲与子系统规格书双向锚定。 |

---

## 6. 单元测试与时序验证计划 (Verification Plan)

在 `wink-micro-os/test/mcs51/` 下增设三组专用时序门禁测试：

### 6.1 `test_adc0832_dio_shared.cpp` (B1 验证)
- **目的**：验证 3 线 DIO 复用电路下的时序状态机健壮性。
- **动作**：
  1. 初始化 `mcs51_adc0832_init(P1, 2, P1, 1, P1, 0, P1, 0)`（DI 与 DO 均挂在 P1.0）；
  2. 模拟用户经典电熨斗读取函数（含 `DIO = 1` 释放动作）；
  3. 断言：状态机准确输出注入的 8 位码值，且总线释放写操作未引起任何位偏移。

### 6.2 `test_unisim_clock_mapping.cpp` (B2 验证)
- **目的**：验证 UniSim 物理时间积分与 51 虚拟时钟的 1:1 映射尺度。
- **动作**：
  1. 在 51 Fiber 中执行 `delay_ms(100)`；
  2. 驱动宿主步进 10 个物理 tick（每个 tick $10\text{ms}$）；
  3. 断言：Fiber 在第 10 个 tick 被准时唤醒，UniSim 发热盘物理温度积分步进严格等于 $100\text{ms}$，无时间膨胀/压缩。

### 6.3 `test_static_init_safety.cpp` (B3 验证)
- **目的**：验证多编译单元跨 TU 链接时无静态初始化崩溃。
- **动作**：
  - 构建包含 3 个独立 `.cpp` 文件的测试固件，分别在静态构造函数中注册 ISR 并读取 `P1`，断言初始化期 0 崩溃，正常进入 `main()`。
