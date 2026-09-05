# wink-micro-os 多系列 MCU 兼容方案：真机 port + 8051 仿真拦截

> 目标读者：wink-micro-os 维护者
> 前身：《wink-micro-os 分层模型 8051 可移植性审计》。原稿把 8051 当真机 port 审，方向有误，本稿重写。
> 版本：**v2.1**（架构收敛：8051 仿真拦截层底层细节解耦至独立规格书 `tech-designs/`，明确全量 `.cpp` 沙箱、四大 `extern "C"` 边界、`WinkSfr` 整端口边沿检测与 Level 2 陷阱执行红线纪律）。
> 结论先行：**wink 本体不跑在 8051 真机上**。8051 支持 = 仿真拦截层（`frameworks/mcs51/`），与 Arduino 兼容层同构。真机 port（ESP32/STM32）走另一条轴，通过 PAL 子集门控加固。
> 独立规格书：
> 1. 8051 零侵入拦截与 C++ 代理总架构详见 [tech-designs/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)；
> 2. SFR 影子代理、整端口 RMW 语义与边沿检测子系统详见 [tech-designs/2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)；
> 3. 混合仿真时钟域、外设时序推进与生命周期一致性子系统详见 [tech-designs/2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)。

---

## 0. 核心模型：两条正交轴

兼容各系列 MCU 不是"把 wink 移植到每种芯片"，而是分清两条轴：

| 轴 | 目录 | wink 本体由谁编译 | 跑什么 | 现有实现 |
|---|---|---|---|---|
| **A. 真机 port** | `targets/<mcu>/` + `osal/<port>/` | MCU 原厂工具链（C99/C11） | wink 真跑在芯片上 | `targets/esp32/`、`targets/host/`、`targets/wasm/`、`osal/baremetal/` |
| **B. 仿真拦截层** | `frameworks/<eco>/` | **宿主 / emscripten（C99/C11/C++17）** | 外国生态的用户代码跑在 wink 之上 | `frameworks/arduino/` |

两条轴的关键区别：**轴 B 永远不碰原厂 8 位工具链**。

### 支持矩阵

| 系列 | 轴 A 真机跑 wink | 轴 B 仿真拦截用户码 | 备注 |
|---|---|---|---|
| ESP32 | ✅ 已有 | ✅ wasm | 主力目标 |
| STM32 (Cortex-M3/M4/M7) | 🔙 待做 | 可经 stm32duino | 有 LDREX/STREX、可选 FPU |
| STM32 (Cortex-M0/M0+) | 🔙 待做，需裁剪 | 同上 | 无原子指令、无 FPU，走临界区+定点 |
| 8051 / STC89C52 | ❌ 明确排除 | ✅ `frameworks/mcs51/` 新建 | 本方案主体 |
| 增强 8051 (CMS8S / STC8H) | ❌ 暂不做 | ✅ 片内 SFR 模型拦截 | 中微 CMS8S 用于电熨斗控温（§3.12） |
| AVR | ❌ | ✅ `frameworks/arduino/` 已覆盖 | — |

---

## 1. 为什么 8051 不需要 C90 改造

### 1.1 Keil C51 的 C 标准事实

Keil C51（含 CX51/PK51）语言基线是 **ANSI C89/C90** + 8051 方言扩展，C99 支持零碎非完整：

| 特性 | Keil C51 |
|---|---|
| `<stdint.h>` / `<stdbool.h>` / `_Bool` | ❌ 无 |
| `inline` 关键字 | ❌ 无 |
| `long long` / 64 位整数 | ❌ 无（最大 `unsigned long` = 32 bit） |
| VLA / compound literal / `restrict` | ❌ 无 |
| `//` 注释 | ✅ 扩展接受 |
| 方言：`sfr/sbit/data/xdata/idata/pdata/code/interrupt/using/_at_/reentrant/bit` | ✅ 专属 |
| generic pointer | 3 字节；memory-specific pointer 2 字节 |

> SDCC 对 C95/C99 支持更好（自带 stdint/stdbool、支持 long long 和 inline），但方言与 Keil 不完全兼容。

**原审计在"用 Keil 编 wink 本体"前提下，关于 C90 的 P0 结论成立。** 但该前提不成立（见下）。

### 1.2 wink 本体不进 Keil

8051 开发分两套构建，工具链完全隔离：

| 代码 | 8051 真机（Keil C51） | 仿真（host/wasm） |
|---|---|---|
| **wink 本体**（pal/osal/dal/bal/runtime） | ❌ 不参与编译 | emcc/clang/msvc，C99/C11 原生 |
| **用户 8051 代码** | Keil 编译（用户自己负责） | **emcc/clang 开启 C++ 模式（`-x c++`）编译**，方言由宏与代理吃掉 |
| **framework shim** | ❌ 不参与 | C99/C++ 编写，提供假 `<REGX52.H>` 与 SFR 代理 |

关键：**C90 是现代 C/C++ 的严格子集**。用户的 Keil C 代码（C90 + 方言），方言被宏与代理对象映射后在宿主编译器下天然合法。方向是 shim 把用户方言**向上提升**，而不是把 wink **降级**到 C90。

因此原审计 Phase 0 的 C90 兼容层（手写 stdint/stdbool、`#define inline` 空、`pal_time_t` 替换 uint64_t）**整段不需要**：
- 仿真侧宿主编译器全自带；
- 真机侧（ESP32/STM32）工具链也是 C99；
- 没有任何一个 wink 目标需要 C90。

---

## 2. 现状审计结论（重新定性）

原审计扫描了全仓 263 处 `uint64_t`、float 用法、malloc 位置，数据准确，保留作为参考。按新模型重新定性：

| 原稿问题 | 新定性 | 处置 |
|---|---|---|
| `uint64_t pal_os_get_ms/us()` 穿透 5层 | 仿真/32位真机均原生支持，非问题 | 不改；可选加 wraparound-safe elapsed helper |
| `<stdint.h>/<stdbool.h>/<stddef.h>` | 所有 wink 工具链都自带 | 不改 |
| `static inline` / `_Static_assert` / `__attribute__` | 所有 wink 工具链都支持 | 不改 |
| `wink_init_ctor.h` L28 `#error` | emcc 走 `__GNUC__` 分支，碰不到 | 不改 |
| `pal_atomic.h` `#else` 落 `<stdatomic.h>` | 真坑，但服务的是无原子指令的 32 位真机（Cortex-M0），非 8051 | 轴 A 加固时修，见 §4 |
| `pal_lockfree_pipeline.h` FOC | 算力问题属 Cortex-M0 等无 FPU 真机 | 轴 A 用 `PAL_HAS_*` 门控 |
| baremetal ringbuf `malloc` | 小堆真机问题 | 轴 A 改静态分配 |
| `WINK_WEAK` 空宏 | 8051 不编 wink，无影响；真机 GCC 全支持 | 不改 |
| float in PID/运动学 | 无 FPU 真机问题 | 轴 A 提供定点双路 |

**原稿的"问题分布地图"有参考价值，但"解法"（C90 兼容层 + targets/mcs51 真机端口）整体作废。**

---

## 3. 轴 B：新建 `frameworks/mcs51/` 仿真拦截层（本方案主体）

对标 `frameworks/arduino/` 的生态拦截思想，针对 8051 的非标方言（`sfr/sbit/interrupt`）、阻塞式执行范式与即时外设访问特性进行专门架构设计。

> **核心技术设计规格书（单一事实来源 SSOT）**：
> 本节涉及的底层编译预处理流水线（`-x c++` + CMake 正则清洗）、全量 `.cpp` 沙箱与四大 `extern "C"` 语言边界、`WinkSfr` 整端口边沿跳变检测（`diff = old ^ new`）、Level 2 即时陷阱 POD 表与四大执行红线纪律、以及 ADC0832 / CMS8S 状态机实现细节已完整收敛至独立技术规格书：
> 📖 [tech-designs/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)。

### 3.0 运行时执行模型与入口桥接（Fiber 协程）

#### 3.0.1 核心问题：`main()` 冲突与阻塞 `while(1)`
- **Arduino**：天然拆分为 `setup()` 与 `loop()`，可无缝注册到 `wink_app_callbacks_t`。
- **8051**：标准代码格式是 `void main(void)` 且内部包含裸机阻塞循环 `while(1) { ... }`。
  - 若直接编译，会与 WinkMicroOS 宿主 `main()` 符号冲突（`multiple definition of 'main'`）；
  - 若在 Wink 初始化时直接同步调用，`while(1)` 将**永远霸占主线程**，导致 Wink 协作式调度器、UniSim 物理引擎、Wasm 浏览器事件循环全部冻结。

#### 3.0.2 架构解法：入口重映射 + 仿真 Fiber
1. **符号隔离与 C 链接规范（边界 ①）**：
   在 `REGX52.H` 中，前置标准头并前向声明 `extern "C" void wink_mcs51_user_main(void);`，再行 `#define main wink_mcs51_user_main`。根据 C++ 标准 [dcl.link]，用户写出的 `void main(void) { ... }` 展开后自动继承 C 链接，导出未修饰符号，彻底消除符号重整（Mangle）冲突。
2. **Fiber 协程运行**：
   复用 Wink 已有的轻量级仿真协程基础设施 `targets/common/src/wink_sim_scheduler.c`（基于 ucontext 或 emscripten fiber）：
   - 在 `frameworks/mcs51/src/mcs51_runtime.cpp` 中实现 `wink_app_get_callbacks()`（兼容 C++17 聚合初始化）：
     - `app_init`：调用 `sim_scheduler_register(mcs51_fiber_entry, ...)` 将用户的 `wink_mcs51_user_main` 注册为一个独立协程任务。
     - `app_loop`：每次驱动调度器运行，协调各协程、虚拟定时器与 UniSim 外部物理量交互。
3. **协作式让出（Yield）**：
   用户的死循环内部没有 OS 让出调用。为此：
   - 隐式让出：在 `_nop_()`、延时函数、读取带超时的状态位时调用 `sim_ctx_yield()`；
   - 时钟步进：若用户写了紧凑空轮询 `while(1) {}`，Wink 调度器通过时间片配额机制限制单次最大运行微秒数，超时后强制上下文切换，向主系统报告事件。

### 3.1 目录结构（全量 `.cpp` 归一化沙箱）

遵循 [ADR-0035](../../decisions/core/0035-arduino-compat-polymorphism-sandbox.md)，兼容层内部源码**全量统一为 `.cpp` 编译**，彻底杜绝 `.c`/`.cpp` 混排导致的类型阻断与符号断裂：

```
frameworks/mcs51/
├── CMakeLists.txt                 # 仅 host/wasm target 编入 wink_mcs51_compat 静态库
├── README.md                      # 拦截原理 + 不支持清单
├── include/
│   ├── REGX52.H                   # 影子 SFR 声明 + 方言宏 + main 重映射（主头文件）
│   ├── REG52.H / reg52.h          # 兼容别名
│   ├── mcs51_proxy.hpp            # C++ WinkSfr / WinkSfrBitProxy 代理类与操作符重载
│   ├── mcs51_trap.h               # Level 2 即时陷阱 C-ABI 结构体与注册 API（边界 ③）
│   ├── mcs51_adc.h                # UniSim 外部物理量注入 C API 接口（边界 ④）
│   ├── wink_mcs51_isr.h           # WINK_ISR(n) 静态自动注册宏（边界 ②）
│   ├── intrins.h                  # _nop_/_crol_/_cror_/_testbit_ shim + 虚拟步进
│   └── absacc.h                   # XBYTE/XWORD 线性影子访问钩子
└── src/                           # 【核心规则】：内部实现全量统一为 .cpp 沙箱
    ├── mcs51_runtime.cpp          # Wink App Callbacks 桥接 + Fiber 任务驱动
    ├── mcs51_sfr.cpp              # SFR 影子实体内存 (256B) + 陷阱表全局实体分配
    ├── mcs51_timer.cpp            # Timer0/1 行为模型，计算溢出时刻，驱动 ISR
    ├── mcs51_uart.cpp             # SBUF/SCON 模型 → 仿真串口（stdout / JS Console）
    ├── mcs51_isr_table.cpp        # 向量分发表
    ├── mcs51_adc0832.cpp          # 外接 ADC 即时时序状态机（§3.11，跨端口引脚绑定）
    └── cms8s_adc.cpp              # CMS8S78xx 片内 12-bit ADC SFR 模型（§3.12，真实寄存器图/即时转换，ADR-0073）
```

### 3.2 方言宏映射与包含时序纪律（`REGX52.H` 核心）

```c
// include/REGX52.H (仿真构建生效；Keil 真机构建时此头不在 include 路径)
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define interrupt(n)
#define using(n)
#define reentrant
#define _at_(addr)
#define data
#define idata
#define xdata
#define pdata
#define code            const
#define bit             uint8_t
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define sfr           inline WinkSfr
  #define sbit          inline auto
#else
  #define sfr           static WinkSfr
  #define sbit          static auto
#endif

#ifndef __C51__
#ifdef __cplusplus
extern "C" {
#endif
void wink_mcs51_user_main(void); // 前向声明赋予 C 链接属性
#ifdef __cplusplus
}
#endif
#define main            wink_mcs51_user_main
#endif

#include "mcs51_proxy.hpp"
```

**系统头文件预引入机制**：`REGX52.H` 在内部顶部前置拉入了常用的 C 标准库（`<stdio.h>`, `<stdlib.h>` 等），利用标准库自带的宏守卫彻底阻断了宏污染风险，用户可完全以任意顺序书写头文件包含指令。

### 3.3 双级外设与引脚同步模型（普通 GPIO 批量 vs 外设即时陷阱）

| 级别 | 适用对象 | 同步机制 | 触发时机 |
|---|---|---|---|
| **Level 1: 基础 GPIO 同步** | 普通 GPIO 状态观测（如 P1 接 LED、P3 接按键） | 输出跳变通过 `js_pal_gpio_write` 即时派发至 `PinArbiter` (零延迟)；输入在 `sync_in()` 批量拉取 | 写输出在电平发生跳变瞬间即刻触发；读输入在调度器每 tick 刷新 |
| **Level 2: 即时陷阱 (Write-Trap)** | 注册了虚拟外设的专用引脚（如 ADC0832 的 CS/CLK/DI） | `s_pin_traps[port][bit]` POD 函数指针跳转 | 引脚写入（`sbit = 1/0` 或 `WinkSfr` 字节 RMW）的**当个 CPU 周期内立即触发** |

#### 1. 位级代理与防越界寻址
```cpp
// include/mcs51_proxy.hpp
struct WinkSfrBitProxy {
    uint8_t sfr_addr; // 物理地址 (0x80, 0x88, 0x90, 0x98...)
    uint8_t port_idx; // 0..3 对应 P0..P3；0xFF 对应控制 SFR (TCON/SCON/IE)
    uint8_t bit_idx;  // 0..7
    uint8_t bit_mask; // 1 << bit_idx

    // 位写赋值：统一写物理影子，按 port_idx 门禁分流引脚 Trap 与 SFR 写钩子
    inline WinkSfrBitProxy& operator=(uint8_t val) {
        const uint8_t old_val = s_sfr_shadow[sfr_addr];
        const uint8_t old_bit = (old_val >> bit_idx) & 1;
        const uint8_t new_bit = val ? 1 : 0;
        s_sfr_shadow[sfr_addr] = new_bit ? (old_val | bit_mask) : (old_val & ~bit_mask);

        if (port_idx < 4) {
            if (old_bit != new_bit) {
                // 1. UniSim 3.0 通道 1：即时同步全局 PinArbiter (零延迟前端动画)
                const uint16_t global_pin = (uint16_t)((port_idx << 3) | bit_idx);
                js_pal_gpio_write(global_pin, new_bit);

                // 2. 硬件即时陷阱 (Level 2 Instant Trap)
                if (s_pin_traps[port_idx][bit_idx].on_write) {
                    s_pin_traps[port_idx][bit_idx].on_write(s_pin_traps[port_idx][bit_idx].write_ctx, new_bit);
                }
            }
        } else {
            if (old_bit != new_bit && s_sfr_write_hooks[sfr_addr]) {
                s_sfr_write_hooks[sfr_addr](sfr_addr, old_val, s_sfr_shadow[sfr_addr]);
            }
        }
        return *this;
    }

    // 位读求值：GPIO 拉取引脚 trap，控制 SFR 先触发 s_sfr_read_hooks 推进时钟
    inline operator uint8_t() const {
        if (port_idx < 4) {
            auto& trap = s_pin_traps[port_idx][bit_idx];
            if (trap.on_read) return trap.on_read(trap.read_ctx) ? 1 : 0;
        } else {
            if (s_sfr_read_hooks[sfr_addr]) s_sfr_read_hooks[sfr_addr](sfr_addr);
        }
        return (s_sfr_shadow[sfr_addr] & bit_mask) ? 1 : 0;
    }
};
```

#### 2. 整端口字节 RMW 与边沿感知（A2 闭环）
用户若使用 `P1 |= 0x04;` 或 `P1 = 0x55;`，`WinkSfr` 通过比较 `diff = old_val ^ val` 仅对发生跳变的引脚触发 `on_write`；读端口时动态拉取 `on_read` 引脚电平重构字节（Read-Pin）；而 RMW 复合赋值则严格读取锁存器影子（Read-Latch），彻底避免破坏输入引脚状态：

> 🔗 **实现细节详见子系统规格书**：[tech-designs/2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)。

```cpp
// include/mcs51_proxy.hpp (核心模型)
struct WinkSfr {
    uint8_t sfr_addr; // 0x80, 0x90, 0xA0, 0xB0...
    uint8_t port_idx; // 0..3 代表 P0..P3，0xFF 代表普通 SFR

    constexpr WinkSfr(uint8_t addr)
        : sfr_addr(addr),
          port_idx((addr == 0x80) ? 0 :
                   (addr == 0x90) ? 1 :
                   (addr == 0xA0) ? 2 :
                   (addr == 0xB0) ? 3 : 0xFF) {}

    constexpr WinkSfr(uint8_t addr, uint8_t port)
        : sfr_addr(addr), port_idx(port) {}

    // 【核心透传】：将真实物理地址透传给位代理，杜绝 TCON/SCON 算错地址
    constexpr WinkSfrBitProxy operator^(uint8_t bit_idx) const {
        return WinkSfrBitProxy{sfr_addr, port_idx, bit_idx, (uint8_t)(1 << bit_idx)};
    }

    // 整字节写入：diff 边沿感知分发
    inline WinkSfr& operator=(uint8_t val) {
        uint8_t old_val = s_sfr_shadow[sfr_addr];
        s_sfr_shadow[sfr_addr] = val;
        if (port_idx < 4) {
            uint8_t diff = old_val ^ val; // 仅对跳变位触发
            if (diff == 0) return *this;   // 快路径

            for (uint8_t b = 0; b < 8; ++b) {
                if ((diff & (1 << b)) && s_pin_traps[port_idx][b].on_write) {
                    s_pin_traps[port_idx][b].on_write(s_pin_traps[port_idx][b].write_ctx, (val >> b) & 1);
                }
            }
        }
        return *this;
    }

    // 整字节读取 (Read-Pin)：动态重构引脚电平
    inline operator uint8_t() const {
        uint8_t val = s_sfr_shadow[sfr_addr];
        if (port_idx < 4) {
            for (uint8_t b = 0; b < 8; ++b) {
                if (s_pin_traps[port_idx][b].on_read) {
                    uint8_t lvl = s_pin_traps[port_idx][b].on_read(s_pin_traps[port_idx][b].read_ctx);
                    if (lvl) val |= (1 << b); else val &= ~(1 << b);
                }
            }
        }
        return val;
    }

    // RMW 复合赋值：严格读取锁存器影子，严禁读物理引脚误锁死下拉 FET
    inline WinkSfr& operator|=(uint8_t rhs) { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] | rhs); }
    inline WinkSfr& operator&=(uint8_t rhs) { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] & rhs); }
    inline WinkSfr& operator^=(uint8_t rhs) { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] ^ rhs); }
    inline WinkSfr& operator+=(uint8_t rhs) { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] + rhs); }
    inline WinkSfr& operator-=(uint8_t rhs) { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] - rhs); }
    inline WinkSfr& operator++()            { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] + 1); }
    inline uint8_t  operator++(int)         { uint8_t o = s_sfr_shadow[sfr_addr]; *this = (uint8_t)(o + 1); return o; }
    inline WinkSfr& operator--()            { return *this = (uint8_t)(s_sfr_shadow[sfr_addr] - 1); }
    inline uint8_t  operator--(int)         { uint8_t o = s_sfr_shadow[sfr_addr]; *this = (uint8_t)(o - 1); return o; }
};
```

> **Trap 四大执行红线（硬实时纪律）**：
> 1. **零延时（Zero Wait）**：严禁在 Trap 内调用延时或阻塞 API；
> 2. **禁止让出（No Yield）**：严禁调用 `sim_ctx_yield()`；
> 3. **纯状态机驱动**：仅允许修改外设内部状态或读写 SFR 影子；
> 4. **时钟解耦**：定时推进交由调度器 Fiber tick 完成。

### 3.4 `sbit` 语法零侵入处理（C++ 模式构建）

Keil C51 的 `sbit` 语法为：
```c
sbit P1_0 = P1^0;
sbit ADC_CS = P1^2;
```
- **为什么必须用 C++ 模式**：
  在纯 C99 中，若 `P1` 是变量，全局文件作用域下 `sbit P1_0 = P1^0;` 会报 `initializer element is not a compile-time constant` 致命编译错误。
- **实施方案**：
  CMake 在编译用户 51 代码时加上 `-x c++ -std=c++17`：
  1. `P1`、`P2` 声明为 `WinkSfr` 实例；
  2. `WinkSfr::operator^(uint8_t bit)` 重载异或运算符，返回对应位代理 `WinkSfrBitProxy`；
  3. 全局定义 `#define sbit auto` 或 `#define sbit WinkSfrBitProxy`。
- **效果**：用户的 `sbit LED = P2^0;`、`sbit ADC_CS = P1^2;` 源码**100% 不改动**即可通过编译，且写入天然走即时陷阱。

### 3.5 中断注册与 CMake 正则清洗（真正零侵入）

Keil 中断声明如 `void Timer0_ISR(void) interrupt 1 using 1`。为达成真正的源码级零侵入：
1. **CMake 正则清洗 Pass**：在构建前自动将 `void <name>(void) interrupt <num>` 正则替换为 `WINK_ISR(<num>)`；
2. **`WINK_ISR` 跨平台展开**：
   ```cpp
   // include/wink_mcs51_isr.h
   #define WINK_ISR(n) \
     extern "C" void wink_isr_vector_##n(void); \
     namespace { \
       struct WinkIsrAutoReg_##n { \
         WinkIsrAutoReg_##n() { wink_mcs51_set_isr(n, wink_isr_vector_##n); } \
       } s_auto_reg_##n; \
     } \
     extern "C" void wink_isr_vector_##n(void)
   ```
3. 展开后直接对接用户的函数体 `{ ... }`，跨 GCC / MSVC / Clang 均在静态初始化期安全完成注册。

#### 忙等死锁防御与时钟自推进
经典 51 代码中普遍存在等待标志位的忙等：
- 模式 A：`while(!g_timer_flag);`（等待 Timer0 中断置位）
- 模式 B：`while(!TF0); TF0 = 0;`（直接轮询定时器溢出标志）

为防止单线程仿真挂死：
1. **模式 B 拦截（读时懒求值）**：在位代理读路径挂接 `s_sfr_read_hooks[0x88]`。当代码读取 `TF0` 时，自动调用 `mcs51_timer_on_tcon_read()`，依据虚拟微秒差推进时钟并即时刷新溢出位，达成 0 延时懒求值闭环。
2. **模式 A 防御**：用户代码调用的 `_nop_()` 或延时循环中，自动驱动虚拟时钟步进并周期性交出 Fiber 执行权，确保后台定时器 ISR 获得触发时机。

### 3.6 UART

`SBUF` 写/`SCON` 状态机参考 `frameworks/arduino/src/WinkHardwareSerial.cpp`：
- 写 `SBUF` 时，将字节压入仿真串口环形缓冲区，并自动在极短延时后置位 `TI`（发送完成标志）；
- 宿主（Host）下桥接到 stdout/stderr；
- Wasm 下通过 emscripten JS 桥接到浏览器 Console 或虚拟串口终端。

### 3.7 `intrins.h` / `absacc.h` shim

```c
// intrins.h
static inline void _nop_(void) {
    pal_os_busy_wait_us(1);
    wink_mcs51_fiber_maybe_yield(); // 推进微步并防单线程饥饿
}
static inline uint8_t _crol_(uint8_t v, uint8_t n){ return (v<<n)|(v>>(8-n)); }
static inline uint8_t _cror_(uint8_t v, uint8_t n){ return (v>>n)|(v<<(8-n)); }
static inline uint8_t _testbit_(uint8_t* p){ uint8_t r=*p; *p=0; return r; }

// absacc.h
extern volatile uint8_t XBYTE[65536];  // 影子 XDATA，写可挂回调钩子
```

### 3.8 不支持清单（写进 README，`WINK_SIM_STRICT` 下 assert）

- **精确指令周期时序**：bit-bang WS2812/1-Wire 靠 `_nop_()` 数周期的，仿真时序不准。`_nop_` 给粗略 1µs，过不了亚微秒协议。
- **PSW 标志（CY/AC/OV/P）**：C 代码算术后读标志的极少，但多精度库有；影子变量无法模拟 ALU 标志。
- **计算地址访问 SFR**：`*(unsigned char idata *)0x80 = x;` 影子方案只拦具名 SFR。
- **Keil 内联汇编**（`#pragma asm`）：宿主 GCC/Clang 不认，需用户用宏隔离。
- **`_at_` 绝对定位**：宏抹掉后链接器自由分配，依赖固定地址的代码（显存映射等）不支持。
- **RC 充放电 / 比较器测温**：依赖连续模拟电平、GPIO 阈值与电容充放电时序，仿真为功能级，无法复现真实温度-时间曲线。无 ADC 的 51 读 NTC 请用外接 ADC（ADC0832，见 §3.11），不保证 RC 路线的仿真精度。
- **generic 3 字节指针宽度假设**：宿主是 4/8 字节平指针；应用层极少依赖。
- **`sbit` 绝对位地址语法**：仅支持 `sbit name = REG^n` 相对位声明；不支持 `sbit name = 0xXX` 绝对位地址语法（后者展开后退化为 `int` 导致静默赋值失效；官方头文件已全量采用 `REG^n`，第三方工程若有由 CMake 预处理 Pass 抛出错误并提示修改）。
- **哈佛架构独立地址空间与存储限定符平坦化（`data`/`idata`/`xdata`/`pdata`/`code`）**：通过预处理宏将 `data`/`idata`/`xdata`/`pdata` 抹除为空，将 `code` 映射为 `const`。变量全量在宿主/Wasm 单一平坦线性内存（Flat Memory）中分配。不支持依赖 8051 独立地址空间物理特性的代码（例如：假设 `xdata 0x20` 与 `data 0x20` 为两个独立物理单元、强行解析 generic 3 字节指针内部 Tag 字节等）。
- **极端历史方言与未声明隐式调用（K&R / 隐式原型）**：仿真拦截层要求用户 8051 源码遵循标准 **ANSI C (C89/C99)** 语法规范（行业 99% 的 51 库均基于此）。不支持 1980 年代无形参类型的 K&R 函数声明，不支持省略前向声明的隐式函数调用，不支持严重依赖 C 专属弱类型特性的代码（如隐式将 `void*` 赋给类型指针未做强转）。

> [!IMPORTANT]
> **完整用户层开发避坑与迁移指南**：
> 面向业务开发工程师与 AI 代码生成的 15 类详尽限制说明、常见受限写法与正确替代代码对比（含常见编译报错 1 分钟快速排错表），请直接查阅独立技术手册：
> 🔗 **[8051 用户层业务代码兼容性红线与避坑手册](2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)**。

### 3.9 CMake 集成与编译器定向兼容策略

照 `frameworks/arduino/CMakeLists.txt` 模式：
- `ESP_PLATFORM` 下直接 `return()`（真机固件零增量）；
- 仿真构建建立 `wink_mcs51_compat` 静态库；
- 对用户源文件追加跨编译器靶向兼容编译标志：
  ```cmake
  # 注入用户 51 源码的精准兼容参数（GCC / Clang / Emscripten 双端 100% 支持）
  target_compile_options(wink_mcs51_user_app PRIVATE
      -x c++ -std=c++17
      -Wno-write-strings   # 允许字符串字面量 ("OK") 传给历史非 const char*，杜绝 C++17 硬拦截
      -Wno-pointer-sign    # 消除 unsigned char* 与 char* 符号差异告警
  )
  ```
- ⚠️ **拒绝全盘宽松参数**：严禁在 CMake 中盲目追加 `-fpermissive`（Wasm 底层的 Emscripten/Clang 根本不吃此开关，且会掩盖内存隐患）；严禁硬编码 `-finput-charset=GBK`（会误杀 UTF-8 现代工程），源码字符编码由 `wink-tools` CLI 前置打包时统一无损检测并规范化为 UTF-8。

### 3.10 验收 sample

`test/mcs51/` 下放经典 89C52 与增强型 51 程序：
1. `blinky.c`：Timer0 中断翻转 P1.0，仿真验证波形周期。
2. `uart_printf.c`：串口 printf，验证 SBUF→仿真串口。
3. `gpio_in_out.c`：P3 按键输入 → P1 LED 输出，验证 sync。
4. `cms8s_adc_test.c`：CMS8S78xx 片内 12-bit ADC 专用测试（真实寄存器图，ADR-0073），验证写 `ADCON0|=0x02`（ADGO）启动后 0 周期穿透 `while(ADCON0&0x02)`，并精确断言 12-bit 码值右对齐/左对齐两种装载与读取（AN0/AN1/AN25）。
5. `sfr_rmw_isolation_test.c`：整端口 RMW 与 bit-bang 强隔离性测试。验证对 P1 某一位执行 `|=`、`&=` 或整端口直接赋值时，严格只有发生跳变的引脚触发 Trap，其余未变动引脚 Trap 触发计数严格为 0；重复写入相同值走快路径（0 触发）。

host + wasm 两个 target 都要过。

---

### 3.11 外接 ADC 拦截与电热样例（ADC0832 + NTC 温度采样）

经典 89C52/STC89C52 **片内无 ADC**。小家电（如经典电熨斗套件）通过 ADC0832 芯片进行 8 位数字转换。

**板级描述 SSOT 与 `wink-tools` 构建期 Codegen (AD-10 落地)**：
应用项目统一由 `wink-tools`（`packages/wink-tools`）进行编译构建。硬件拓扑单一事实来源（SSOT）为应用目录下的 `wink-app.json`：
```json
{
  "app_name": "iron_ntc_demo",
  "board": "mcs51_custom",
  "devices": {
    "adc0832": {
      "type": "adc0832",
      "cs_pin":  { "port": 1, "bit": 2 },
      "clk_pin": { "port": 1, "bit": 1 },
      "di_pin":  { "port": 1, "bit": 0 },
      "do_pin":  { "port": 1, "bit": 0 }
    }
  }
}
```
`wink-tools` 在执行 `wink build sim` 时双向派发：
1. **固件编译期**：`app_codegen.py` 解析 `wink-app.json` 并渲染生成 `include/mcs51_board_config.h`，供 `frameworks/mcs51` 静态包含（零运行时开销、零 JSON 库依赖）：
   ```c
   /* 由 wink-tools app_codegen 从 wink-app.json 自动生成 */
   #define MCS51_HAS_ADC0832          1
   #define MCS51_PIN_ADC0832_CS_PORT  1
   #define MCS51_PIN_ADC0832_CS_BIT   2
   #define MCS51_PIN_ADC0832_CLK_PORT 1
   #define MCS51_PIN_ADC0832_CLK_BIT  1
   #define MCS51_PIN_ADC0832_DI_PORT  1
   #define MCS51_PIN_ADC0832_DI_BIT   0
   #define MCS51_PIN_ADC0832_DO_PORT  1
   #define MCS51_PIN_ADC0832_DO_BIT   0
   ```
2. **仿真运行期**：同时导出 `unisim-assets/device-tree.json`，供浏览器 Wokwi / UniSim JS 物理总线加载渲染。

**即时状态机拦截原理**：
1. `ADC0832.H` 将 CS/CLK/DI/DO 映射到位代理；支持 4 线独立引脚与 3 线 DIO 并联复用（当 DI 与 DO 物理共线时自动激活阶段隔离状态机）；
2. CS、CLK、DI 引脚挂接 Level 2 即时写陷阱，DO 引脚挂接即时读陷阱；
3. 用户调用 `ADC0832_Read()` 时，在同一个函数调用内完成的 CS 下降沿、CLK 脉冲与 DI 通道配置被 `mcs51_adc0832.cpp` 的状态机**即刻捕获**；
4. 状态机自动区分输入配置阶段（`PHASE_INPUT`）与数据输出阶段（`PHASE_OUTPUT`），在输出阶段屏蔽 MCU 释放总线的 `DIO = 1` 写入动作，后续每个 CLK 下降沿将注入值的对应位输出到 `on_read` 缓存，并**同步镜像写入锁存器影子位**；
5. 用户代码无论通过位读 `if(ADC_DIO)` 还是整端口字节读 `if(P1 & 0x01)`，均能取得完全一致的 8 位转换值。
- 📖 **3 线 DIO 复合引脚状态机源码详见**：[tech-designs/2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §3。

**UniSim 3.0 通道 3 模拟量拉取与注入契约 (AD-8 落地)**：
```c
// include/mcs51_adc.h
uint16_t mcs51_adc_get_value(uint8_t ch);             // 状态机内部拉取入口 (Wasm 下直通 js_pal_adc_read_norm)
void     mcs51_adc_set_value(uint8_t ch, uint16_t raw); // 单测/CI 注入覆盖接口 (8-bit: 0~255, 12-bit: 0~4095)

// 向前兼容 inline shim
static inline void mcs51_adc0832_set_value(uint8_t ch, uint8_t val) {
    mcs51_adc_set_value(ch, (uint16_t)val);
}
```
- **双轨数据流设计**：在 UniSim Wasm 仿真中，`mcs51_adc_get_value` 自动调用 `js_pal_adc_read_norm`（ADR-0057 标准 Pull 模型）向 `PinArbiter` 拉取当前最新物理量并折算为 raw 码值，前端零专有胶水；在 Host 单测中则通过 `mcs51_adc_set_value` 注入模拟数值。用户业务代码完全零感知。

**验收样例 `test/mcs51/iron_ntc.c`**：
- 纯 51 风格电熨斗业务逻辑：ADC0832 读 NTC → 查表控温 → 控制 P1.0 继电器与 P1.1 指示灯；
- Host 单元测试注入电压断言逻辑切换；
- Wasm 环境连接 UniSim `thermal_heater_plate` 插件，验证毫秒级物理热平衡闭环。

---

### 3.12 增强型 51 片内外设拦截（CMS8S78xx 片内 12-bit ADC）

> **M5 夹具核对回写（ADR-0073，2026-08-29）**：早期本节按无夹具期理想化图写作（`ADCON@0xE1` / START bit6 / EOC bit5 / `ADCFG/ADCH/ADCL`）。原厂夹具（CMS8S78xx 参考手册 Ch.22 + 设备头 `cms8s78xx.h` + StdDriver `adc.c`）核对后，真实型号为 **CMS8S78xx**、真实寄存器图见下；0xE1 图废弃。

中微 CMS8S78xx 集成片内 12-bit SAR ADC（AN0~AN25 外部通道 + AN63 内部），通过片内 SFR 控制、XSFR（MOVX 空间）配置参考源 LDO。真实寄存器：`ADCON0@0xDF`（bit1 **ADGO** 启动/忙、bit6 **ADFM** 对齐）、`ADCON1@0xDE`（bit7 **ADEN** 使能）、`ADCCHS@0xD9`（通道）、`ADRESH@0xDD`/`ADRESL@0xDC`（结果）、`EIE2@0xAA` bit4 ADCIE / `EIF2@0xB2` bit4 ADCIF、Keil **中断向量 19**；XSFR `ADCLDO@0xF692`、`PxxCFG@0xF000..`。

**即时转换完成模型（Instant Conversion Model）与死锁消除**：
- 原厂 StdDriver 风格轮询习语：
  ```c
  ADCCHS = 0;               // 选通道 AN0
  ADCON1 |= 0x80;           // ADEN = 1
  ADCON0 = 0x40;            // ADFM = 1（右对齐）
  ADCON0 |= 0x02;           // ADGO = 1，启动（ADC_GO()）
  while (ADCON0 & 0x02) { ; }                 // 等 ADGO 硬件自清（= 忙标志）
  code = 0x0FFF & ((ADRESH << 8) | ADRESL);   // 右对齐读取 (0~4095)
  ```
- **ADCON0 SFR 写钩子拦截**（`mcs51_trap_register_sfr_write(0xDF, …)`）：整字节写与 RMW 都经 `WinkSfr::operator=`（影子先存、钩子后发），钩子在当条写入语句内**同步立刻**完成：
  1. 门控 `new_val & 0x02`（ADGO）且影子 `ADCON1 & 0x80`（ADEN）；取通道 `ch = ADCCHS & 0x3F`，从 12-bit 注入轨 `mcs51_adc_get_value(ch) & 0x0FFF` 拉取 `raw`（AN63 v1 返回 0）；
  2. 按 `ADFM`（`new_val & 0x40`）反向拆分（与原厂 `ADC_GetADCResult` 读取公式互逆）：
     ```cpp
     // 右对齐 ADFM=1：0xFFF & ((ADRESH<<8)|ADRESL)
     shadow[0xDD] = (uint8_t)((raw >> 8) & 0x0F);  // ADRESH 低 4 位 = D11..D8
     shadow[0xDC] = (uint8_t)(raw & 0xFF);          // ADRESL = D7..D0
     // 左对齐 ADFM=0：0xFFF & ((ADRESH<<4)|(ADRESL>>4))
     shadow[0xDD] = (uint8_t)((raw >> 4) & 0xFF);   // ADRESH = D11..D4
     shadow[0xDC] = (uint8_t)((raw & 0x0F) << 4);   // ADRESL 高 4 位 = D3..D0
     ```
  3. **影子自清 ADGO**（`shadow[0xDF] = new_val & ~0x02`）；若 `EIE2 & 0x10`（ADCIE）则锁存 `EIF2 |= 0x10`（ADCIF，软件清零同 UART TI），若再 `IE & 0x80`（EA）则 `wink_mcs51_dispatch_vector(19)`。
- **优势**：`while (ADCON0 & 0x02)` 首次判断即为假，**0 周期即时穿透**，杜绝单线程忙等死锁与协程切换开销。
- **M5 两处基础设施**：① ISR 向量表 `WINK_MCS51_NUM_VECTORS` 8→**28**（核心 0~7、CMS8S 扩展 8~27、ADC=19；旧值下 n≥8 被静默丢弃）；② xdata 开 **XSFR 窗口** `[0xF000,0x10000)`，原厂 `*(unsigned char xdata *)0xF692` 宏由常量初始化 `WinkXsfr` 代理取代（清洗擦除 `xdata` 后否则为宿主野指针），全部走受检 `wink_mcs51_xdata_read/write` 路径。
- 📖 **状态机详细源码、对齐推导与 v1 收窄（AN63→0、VSEL 忽略、StdDriver tier-b 不挂构建）详见**：[tech-designs/2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md) §6.2 与 [ADR-0073](../../decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)。

**统一板级描述与注入接口**：
板级能力在 `device-tree.json` 的 `adc` 节声明：
```json
"adc": {
  "kind": "internal",
  "family": "cms8s",
  "default_full_scale_mv": 5000,
  "default_resolution_bits": 12
}
```
UniSim 统一调用与芯片族解耦的通用注入函数：
```c
void mcs51_adc_set_value(uint8_t ch, uint16_t raw);
uint16_t mcs51_adc_get_value(uint8_t ch);
```

---

## 4. 轴 A：真机 port 通用加固（惠及 ESP32/STM32，与 8051 无关）

这些不是 8051 需求，是让 PAL 契约对"资源受限的 32 位 MCU"更友好，为 STM32 port 铺路。一次做，全平台受益。

### 4.1 target caps 自注入

现状 `pal/include/hal/pal_target_caps.h:12-31` 是 `#if ESP_PLATFORM / #elif __wasm__ / #else` 硬编码链，加一个 target 改一次。改为分发到各 target 自带头：

```c
#if defined(ESP_PLATFORM)
  #include "targets/esp32/pal_target_caps_esp32.h"  // 内含 SOC_* 映射
#elif defined(STM32)
  #include "targets/stm32/pal_target_caps_stm32.h"  // 由 STM32Cube HAL 宏填
#else
  #include "pal_target_caps_sim.h"                  // host/wasm
#endif
```

### 4.2 编译期 HAL 子集门控 `PAL_HAS_*`

现状子系统缺失只在运行时返 `WINK_ERR_UNSUPPORTED`。小 MCU 需要链接期就不编入。新增能力宏：

```c
#define PAL_HAS_DMA      0/1
#define PAL_HAS_MCPWM    0/1   // ESP32 特有，stm32/8051=0
#define PAL_HAS_PCNT     0/1
#define PAL_HAS_RMT      0/1
#define PAL_HAS_HW_FPU   0/1
#define PAL_HAS_ATOMICS  0/1
```

- `pal_dma.h / pal_mcpwm.h / pal_pcnt.h / pal_rmt.h` 整组用 `#if PAL_HAS_*` 包。
- DAL 驱动裁剪：`cmake/wink_dal_drivers.cmake` 已有按驱动裁剪机制，改为读这些 caps。
- FOC pipeline（`pal/include/osal/pal_lockfree_pipeline.h`）用 `PAL_HAS_ATOMICS` 或 `PAL_HAS_FOC` 门控。
- PID/servo 浮点：有 FPU 用 float，无 FPU 复用已有的 `q15_t/q31_t` 定点（`pal_lockfree_pipeline.h:21-22` 已定义，提取为公共 `pal_fixedpoint.h`）。

### 4.3 `pal_atomic.h` 三档实现

现状 `pal/include/osal/pal_atomic.h:69-79` 的 `#else → #include <stdatomic.h>` 是真坑（任何非 GCC/Clang/MSVC 编译器直接炸）。按真机能力分三档：

| 档 | 条件 | 实现 |
|---|---|---|
| 原生原子 | Cortex-M3+/ESP32/RISC-V A | GCC `__atomic_*`（现有 GCC/Clang 分支） |
| 临界区退化 | Cortex-M0/裸机无原子指令 | `PAL_ATOMIC_*` 展开为 `save_irq / op / restore_irq`，只保证 8/16 位；32 位操作在临界区内完成 |
| 仿真 | host/wasm | 现有 win32 Interlocked / pthread |

这是给 STM32F0/G0 等 Cortex-M0 准备的，**不是给 8051**。

### 4.4 baremetal ringbuf 静态分配

`osal/baremetal/pal_osal_baremetal.c` 的 ringbuf 用 `malloc`（原审计 P1-4）。改为外部传入 buffer 或静态池：新增 `pal_os_ringbuf_create_static(buf, size)`，现有 `_create` 在有堆的 target 保留。任何小堆真机受益。

### 4.5 时间 wraparound 安全 helper（可选，低优）

`pal_os_get_ms/us()` 返回 uint64_t 全平台无虞。但 `frameworks/arduino/src/Common.cpp:134-140` 把它截断成 `unsigned long`（32 位），71 分钟溢出。提供差值宏并在 framework 层使用：

```c
#define PAL_TIME_AFTER(a, b)    ((int32_t)((b) - (a)) < 0)
#define PAL_TIME_ELMSD(now, ts) ((uint32_t)((now) - (ts)))
```

---

## 5. STM32 真机 port（独立后续，不混入本次）

单独立项，依赖 §4 加固完成：
- 新建 `targets/stm32/`（HAL 实现：GPIO/PWM/UART/I2C/SPI/ADC/HWTIMER，基于 STM32Cube HAL 或 LL）。
- `osal/` 新增 `freertos_stm32/` 或复用 `baremetal/`。
- caps 从 STM32 HAL 宏（`STM32F4xx`/`STM32G0xx` 等）派生。
- Cortex-M0 型号（F0/G0/L0）走 `PAL_HAS_ATOMICS=0` + 定点路径。
- 不实现 mcpwm/pcnt/rmt（ESP32 特有）。

---

## 6. 路线图与工时（双轨解耦）

> **关键调整**：8051 仿真跑在 Host/Wasm 上，不依赖轴 A 的 Cortex-M0 原子降级。因此将**轴 B 仿真业务线**与**轴 A 真机加固线**解耦为两条并行轨道，优先跑通 8051 + 电熨斗仿真闭环。

### 轨道 1：轴 B 8051 仿真主线（业务优先）

| 阶段 | 内容 | 工时 | 依赖 | 交付物与验证要求 |
|---|---|---|---|---|
| **P1 骨架 + Fiber + 语法兼容基线** | CMakeLists + REGX52.H + Fiber 运行模型 + sbit C++ 代理 + 基础 blinky；**真实 51 样本在 GCC/MSVC/Emscripten 三端编译兼容性打磨** | **4-5 天** | 无 | `test/mcs51/blinky.c` 在三端通过编译运行 |
| **P2 定时器 + ISR** | Timer0/1 行为模型 + 虚拟时钟微步自推进 + WINK_ISR 注册表 | 3 天 | P1 | 中断精确翻转周期验证 |
| **P3 串口 + 外设基础设施** | SBUF/SCON 模型 + 仿真串口桥 + intrins/absacc | 2 天 | P1 | 串口字符输出断言 |
| **P3.5 ADC0832 即时陷阱** | 外接 ADC0832 CS/CLK/DI 即时状态机 + DO 动态求值 + 3 线 DIO 复合复用 | 2-3 天 | P1, P2 | 8 位数据通信时序闭环 |
| **P3.6 CMS8S78xx 片内 ADC（M5，ADR-0073）** | `frameworks/mcs51/src/cms8s_adc.cpp` 真实寄存器图即时转换模型（ADCON0@0xDF ADGO 自清/ADFM、ADCON1@0xDE ADEN、ADCCHS@0xD9、ADRESH/L@0xDD/0xDC、向量 19）+ XSFR 窗口/WinkXsfr 代理 + 12-bit 注入轨 | 2-3 天 | P1 | `cms8s_adc_test.c` + `test_cms8s_adc_instant.cpp` 0 周期穿透/双对齐装载/IRQ-19 断言（host 16/16、wasm 6/6） |
| **P4 电热样例与 UniSim 闭环** | `iron_ntc.c` 经典电熨斗样例（双 ADC 路线可选）+ Host/Wasm 测试闭环 | 2-3 天 | P3.5/P3.6 | Wasm 界面与 Host 双通过 |
| **轨道 1 小计** | **跑通 8051 完整仿真与电热控温样例（含真实开源工程打磨）** | **~3.5 周 (16~19 天)** | | 消除排期虚标，工时扎实可信 |

#### 工时评估与排期风险缓冲说明
1. **P1 工时缓冲理由**：P1 从原先过于乐观的 3-4 天调整为 4-5 天，专门预留了 1~2 天作为**“真实开源样本兼容性测试缓冲”**。需采集 3 套真实世界的 51 工程（普中温控流水灯、串口交互 Demo、中微 CMS8S 原厂工程），在 Host GCC、MSVC 以及 Wasm Clang (Emscripten) 三端全部跑通，确保靶向参数链与宏映射经受住真实工程考验。
2. **数学求和纠偏**：原计划细分项累加达 16~19 天，表格小计写“~2.5 周”存在数学失真。现客观修正为 **`~3.5 周`**，杜绝延期风险。

### 轨道 2：轴 A 真机通用加固（底层演进，可并行）

| 阶段 | 内容 | 工时 | 依赖 |
|---|---|---|---|
| **P0.1 caps 自注入** | 分发各 target caps 头 + `PAL_HAS_*` 编译期门控宏 | 2 天 | 无 |
| **P0.2 原子操作与静态池** | `pal_atomic.h` 临界区降级（修 `#else` 坑）+ 静态 ringbuf | 2-3 天 | 无 |
| **轨道 2 小计** | **消除 M0 障碍，惠及所有真机 target** | **~1 周** | |

---

## 7. 架构决议表（Architectural Decisions）

原 10 个开放问题已完成架构评审，正式收敛为以下架构决议：

| # | 决策项 | 架构决议结论 | 实施落地要求 |
|---|---|---|---|
| **AD-1** | **源码侵入度** | **真·源码级零侵入** | 保持 `sbit P1_0 = P1^0` 原样，`void Timer0_ISR(void) interrupt 1` 由 CMake 构建期正则清洗为 `WINK_ISR(1)`，用户源码 100% 不改。 |
| **AD-2** | **仿真时序精度** | **功能级（时钟与事件步进）** | 仅模拟 GPIO 状态、定时器毫秒/微秒中断周期，不模拟 12-T 指令机器周期。 |
| **AD-3** | **8051 芯片范围** | **分阶段收敛** | 首版仅支持经典 AT89C52（通用基础）与中微 CMS8S（小家电片内 ADC 专项）。STC8H 等后续另立。 |
| **AD-4** | **用户代码编译器** | **C++17 模式（`-x c++`）** | Host/Wasm 仿真构建下，用户 51 C 代码强制开启 `-std=c++17` 编译，以支撑操作符重载与 C++17 内联变量（P0386R2）消除 ODR 重复定义（老旧受限环境若降级至 C++14 可无缝平替为 `constexpr` 内部链接，详见规格书 §5.2.2）。 |
| **AD-5** | **仿真串口落点** | **Host 与 Wasm 双落点** | Wasm 桥接 Emscripten JS 终端；Host 直接重定向到 stdout。 |
| **AD-6** | **CI 门禁要求** | **必须覆盖** | CI 矩阵必须包含 `test_mcs51_host` 与 `test_mcs51_wasm`，每次 MR 自动验证 blinky 与 iron_ntc。 |
| **AD-7** | **外接 ADC 范围** | **首版仅锁定 ADC0832** | 满足经典电熨斗/电水壶教学套件需求；PCF8591 等 I2C 件遵循 YAGNI 后续按需支持。 |
| **AD-8** | **UniSim 注入与拉取** | **通道 3 标准 Pull 双轨模型** | 生产运行时（Wasm）通过 `mcs51_adc_get_value(ch)` 底层直接调用 `js_pal_adc_read_norm(pin)` 向 `PinArbiter` 拉取 $[0.0, 1.0]$ 归一化值并换算为目标码值（8 位外接 ADC0832 为 `0~255`，12 位片内 CMS8S 为 `0~4095`），零专有 JS 胶水；单测/CI 校验轨保留 `mcs51_adc_set_value(ch, raw)` 注入覆盖；提供 `mcs51_adc0832_set_value` 作为 inline 兼容 shim。用户业务代码完全零感知。 |
| **AD-9** | **增强 51 首发家族** | **锁定中微 CMS8S** | 配合电熨斗温控项目落地，只实现 CMS8S 的片内 ADC 寄存器映射。 |
| **AD-10**| **板级描述与编译期 Codegen** | **`wink-app.json` (SSOT) + `wink-tools` 构建期 Codegen** | 应用硬件拓扑单一事实来源为应用的 `wink-app.json`。由 `wink-tools`（`tools/codegen/app_codegen.py`）在构建配置阶段双向派发：1. 固件编译期生成 `include/mcs51_board_config.h` 供 `frameworks/mcs51` 静态包含（零运行时开销、零 JSON 依赖）；2. 仿真运行期导出 `unisim-assets/device-tree.json` 供 UniSim 前端/JS 物理引擎加载渲染。 |
| **AD-11**| **Framework 语言与边界** | **全量 .cpp 沙箱 + 4 大 C 边界** | `frameworks/mcs51/src/` 全量统一为 `.cpp`，消除混排；在 main、ISR、内核回调和物理注入 4 处严格规范 `extern "C"`。 |
| **AD-12**| **整端口与边沿感知** | **WinkSfr diff 边沿感知 + Read-Latch 隔离** | `WinkSfr` 字节级赋值实现新旧值异或，仅对翻转的引脚触发 `on_write`；读端口动态拉取 `on_read` 重构字节；RMW 严格读锁存器。落地基准详见 [tech-designs/2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)。 |
| **AD-13**| **Trap 执行红线** | **硬实时同步状态机纪律** | 严禁在 Trap 内调用延时阻塞与 `sim_ctx_yield()`，杜绝重入与时钟撕裂。 |
| **AD-14**| **UniSim 物理时钟映射** | **1:1 硬实时时钟映射** | 宿主 app_loop 为主时钟 (100Hz, dt=10ms)，51 虚拟时钟 1:1 映射 (1ms 物理 = 1ms 虚拟)；即时外设耗时 0µs，配额超额强制切出保证物理热平衡连续性。落地基准详见 [tech-designs/2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §2。 |
| **AD-15**| **ADC0832 DIO 复合引脚** | **3 线 DIO 复用自适应状态机** | 支持 DI 与 DO 物理并联在同一引脚，通过输入/输出阶段隔离屏蔽 MCU 释放总线写操作，保证配置与采集双向时序正确。落地基准详见 [tech-designs/2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §3。 |
| **AD-16**| **可变 SFR 常量初始化** | **`inline WinkSfr` + 花括号初始化** | 严禁将 SFR 实体声明为 `constexpr`（会赋予顶层只读属性导致 `P1 = 0x55` 报只读编译错误）；由 `WinkSfr` 的 `constexpr` 构造函数保障其在装载阶段完成静态 Constant Initialization（早于任何动态 ctor、零运行时开销、零 Fiasco），且对象保持可写；使用花括号 `{addr, port}` 杜绝小括号歧义。 |
| **AD-17**| **空转配额切出时钟补账** | **强制切出补账推进 (Catch-Up)** | 紧凑空轮询（如 `while(!TF0);`）耗尽配额被强制切出时，必须补齐当前 tick 未消费的微秒差额并驱动定时器步进，确保虚拟时钟与宿主物理世界保持 1:1 绝对守恒。 |
| **AD-18**| **UniSim 通道 1 线性引脚与即时通知** | **线性引脚映射 `(port << 3) \| bit` + GPIO 即时通知** | 8051 P0~P3 的 32 个引脚单调映射至全局 Pin 0~31；`WinkSfrBitProxy` 与 `WinkSfr` 发生电平跳变时，立即调用 `js_pal_gpio_write(global_pin, new_level)` 同步至 `PinArbiter`，实现前端 UI 动画零延迟；用户业务代码继续使用纯原生 `sbit` / `P1`，完全零感知。 |

---

## 8. 验收标准

1. 一份经典 89C52 Keil 工程（blinky + Timer0 ISR + UART printf）**不改动任何业务逻辑代码**（除 `interrupt N`→`WINK_ISR(N)` 声明外），在 host 和 wasm 两个仿真 target 下直接编译成功。
2. **死循环与协程调度安全**：用户代码中包含 `void main()` 与 `while(1)` 裸机循环时，系统稳定运行，Wasm 界面不冻结，无 `WINK_WARN_WCET_EXCEEDED` 致命告警。
3. **Bit-Bang 与整端口 RMW 强隔离性与时序闭环**：`ADC0832_Read()` 在单一函数调用内的 CLK/DI/DO 快速时序（无论通过 `sbit` 位操作、`P1 |= ...` 整端口操作，还是 3 线 DIO 单引脚并联复用模式）均能准确触发即时陷阱且无虚假边沿，成功读取注入的 8 位数据；必须通过 `sfr_rmw_isolation_test.c` 单元测试，断言整端口 RMW 严格杜绝任何无关引脚虚假跳变（Zero False-Trigger），且 RMW 复合赋值不破坏输入引脚状态。
4. **电热温控闭环与外设专属验证**：
   - `iron_ntc.c` 电熨斗样例（ADC0832 路线）在注入模拟 NTC 电压后，能正确执行控温判断并驱动继电器翻转，开路/短路安全态生效；
   - `cms8s_adc_test.c` 专属单元测试验证 CMS8S78xx 片内 ADC 轮询忙标志（ADGO 自清）0 周期即时穿透无死锁，右对齐/左对齐两种拆分填充精确码值，ADCIE+EA 下中断向量 19 恰好派发一次。
5. **真机固件零增量**：`frameworks/mcs51/` 在 `ESP_PLATFORM` 构建下完全不编入，固件大小零变化。
6. **轴 A 加固验收**：`PAL_HAS_*` 门控使 Cortex-M0 最小构建能够通过静态链接。
7. 不支持清单内的特性在 `WINK_SIM_STRICT` 下触发 assert，在 release 模式下降级为 `pal_log_w` 告警。

