# 8051 零代码侵入仿真拦截与 C++ 代理架构技术设计规格书

| 属性 | 内容 |
| :--- | :--- |
| **文档状态** | Draft - 方案深化与落实现行标准 (In-Progress) |
| **创建日期** | 2026-08-27 |
| **所属模块** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` |
| **关联规范** | [ADR-0004 编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md), [ADR-0035 Arduino 兼容多态沙箱](../../decisions/core/0035-arduino-compat-polymorphism-sandbox.md), [ADR-0036 C++ 子集裁剪策略](../../decisions/core/0036-cpp-subset-compilation-policy.md), [STM32 零代码侵入设计](../unisim/2026-08-25-stm32-zero-code-api-interception-simulation-design.md), [MCU 兼容方案规划](mcu-compat-plan.md), [SFR 影子代理与整端口 RMW 规格书](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md), [时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md), [用户业务代码兼容性红线与避坑手册](2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md) |

---

## 1. 方案背景与设计愿景 (Executive Summary)

### 1.1 核心目标：源码级“零侵入”
传统 8051 教学或工业代码（以 Keil C51 为代表）充斥着非标准 C 方言扩展：
- 文件作用域异或位寻址：`sbit LED = P2^0;`、`sbit ADC_CS = P1^2;`
- 特殊功能寄存器定义：`sfr P1 = 0x90;`
- 存储器修饰符与扩展关键字：`code`、`data`、`xdata`、`idata`、`using`、`interrupt`
- 裸机阻塞死循环：`void main(void) { while(1) { ... } }`

**本项目目标**：在 **Host (Linux/macOS/Windows) 与 Wasm (WebAssembly 浏览器环境)** 上构建高保真仿真拦截层（`frameworks/mcs51/`）。用户上传未修改的 Keil 8051 C 源码时，**无需改动业务逻辑代码**，即可在 WinkMicroOS 协程调度体系与 UniSim 虚拟物理引擎之上全速运行。

> [!TIP]
> **业务开发者查阅指引**：
> 本设计规格书聚焦于底层 C++ 拦截机制与编译器内部实现。如果您是进行 8051 业务代码编写、移植或进行 AI 固件代码生成，请直接查阅独立技术手册：
> 🔗 **[8051 用户层业务代码兼容性红线与避坑手册](2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)**，获取完整的 15 类受限语法、正确替代代码与 1 分钟排错表。

### 1.2 架构定性：叶子 C++ 沙箱与纯 C 内核解耦
严格遵循 [ADR-0035](../../decisions/core/0035-arduino-compat-polymorphism-sandbox.md)：
- **Wink 内核保持纯 C 静态分发**：`pal`、`dal`、`wink_runtime` 不引入 C++ 虚表，保持零开销。
- **仿真拦截层作为 C++ 叶子沙箱**：`frameworks/mcs51/` 仅在 Host/Wasm 构建下编译（真机构建物理隔绝），利用现代 C++17 的操作符重载与内联变量机制将 8051 非标语法向上提升，在类型系统层面彻底吃掉方言差异。

---

## 2. 目录结构与构建体系 (Directory & Build System)

### 2.1 目录结构（全量 `.cpp` 归一化）

为了彻底杜绝 C/C++ 混编引发的符号重整（Name Mangling）断裂与头文件可见性混乱，`frameworks/mcs51/` 内部源码**全量统一采用 `.cpp` 编译**：

```
frameworks/mcs51/
├── CMakeLists.txt                 # 仅 host/wasm 构建编入 wink_mcs51_compat 静态库
├── README.md                      # 兼容层原理与不支持清单
├── include/
│   ├── REGX52.H                   # 影子 SFR 声明 + 方言宏消除 + main 重映射（主头文件）
│   ├── REG52.H / reg52.h          # 兼容大小写别名
│   ├── mcs51_proxy.hpp            # C++ WinkSfr / WinkSfrBitProxy 代理类与操作符重载
│   ├── mcs51_trap.h               # Level 2 即时陷阱 C-ABI 结构体与注册 API
│   ├── mcs51_adc.h                # UniSim 外部物理量注入 C API 接口
│   ├── wink_mcs51_isr.h           # WINK_ISR(n) 静态自动注册宏
│   ├── intrins.h                  # _nop_/_crol_/_cror_/_testbit_ 固有函数与微步推进
│   └── absacc.h                   # XBYTE/XWORD 线性影子访问钩子
└── src/                           # 【核心规则】：内部实现全量为 .cpp 沙箱
    ├── mcs51_runtime.cpp          # Wink App Callbacks 桥接 + Fiber 任务上下文驱动
    ├── mcs51_sfr.cpp              # SFR 影子实体内存 (256B) + 陷阱表全局实体分配
    ├── mcs51_timer.cpp            # Timer0/1 行为模型、溢出计算与微步推进
    ├── mcs51_uart.cpp             # SBUF/SCON 状态机 → 仿真串口桥 (stdout / JS Console)
    ├── mcs51_isr_table.cpp        # 8051 中断向量分发表与派发管理
    ├── mcs51_adc0832.cpp          # 外接 ADC0832 即时时序状态机 (跨端口引脚绑定)
    └── periph/
        └── cms8s_adc.cpp          # 中微 CMS8S 片内 ADC 影子模型 (即时转换完成)
```

### 2.2 CMake 构建与预处理流水线

#### 1. 语言标志与靶向兼容参数链
- **用户代码靶向参数**：通过 CMake 对用户 51 源码追加 `-x c++ -std=c++17` 强行激活 C++ 解析模式以支撑 `sbit` 操作符重载与内联变量，并注入靶向降级开关：
  ```cmake
  target_compile_options(wink_mcs51_user_app PRIVATE
      -x c++ -std=c++17
      -Wno-write-strings   # 【关键兼容】：允许字符串字面量传给非 const char*，GCC/Clang 双端原生支持
      -Wno-pointer-sign    # 消除 51 常见 unsigned char* 与 char* 符号差异告警
  )
  ```
  - ⚠️ **严禁使用全盘宽松 `-fpermissive`**：因为 Wasm 底座依赖的 Emscripten/Clang 对 C++ 类型系统极其严苛，对 `-fpermissive` 支持极弱甚至直接忽略，盲目开启会导致 Host 通过但 Wasm 崩溃的“假通过”现象；
  - ⚠️ **源码字符编码防护**：禁止在 CMake 硬编码 `-finput-charset=GBK`（会误杀现代 UTF-8 源码），由 `wink-tools` 在 CLI 构建前置阶段做轻量文件编码探测，统一规范为 UTF-8。
- **兼容层自身沙箱**：`frameworks/mcs51/src/*.cpp` 按原生 C++17 构建，严格遵循 [ADR-0036](../../decisions/core/0036-cpp-subset-compilation-policy.md) 裁剪标志：
  ```cmake
  target_compile_options(wink_mcs51_compat PRIVATE
      -fno-exceptions
      -fno-rtti
      -fno-threadsafe-statics
      -nostdlib++
  )
  ```

#### 2. 中断语法无感转换 Pass (CMake Regex Pass)
Keil 中断函数声明形如 `void Timer0_ISR(void) interrupt 1 using 1`。在 C++ 语法体系下，`interrupt 1` 无括号无法被宏直接参数化匹配，若强行抹掉会导致 `1` 成为游离字面量引发语法错误。

为此，在 CMake 源码预处理阶段增加无感正则清洗过滤（或通过轻量构建脚本），在进入编译器前完成规范化：
```regex
# 目标：匹配 void <name>(void) interrupt <vector_num> [using <bank>]
Match:    void\s+(\w+)\s*\(\s*(?:void)?\s*\)\s*interrupt\s+(\d+)(?:\s+using\s+\d+)?
Replace:  WINK_ISR($2)
```
- **效果**：用户源码库 100% 保持原生 Keil 格式不变，编译期被安全替换为合法的 `WINK_ISR(n)` 展开，达成真正意义上的零侵入。

#### 3. 板级配置与 `wink-tools` 构建期代码生成 (Board Codegen Pipeline)

项目整体构建由 CLI 工具 `wink-tools`（`packages/wink-tools`）主导调度。由于兼容层沙箱严格遵从 [ADR-0036](../../decisions/core/0036-cpp-subset-compilation-policy.md) 启用了 `-nostdlib++`，沙箱内部不存在 C++ 标准库及运行时 JSON 解析器。

因此，位于应用目录 `wink-micro-app/<app>/wink-app.json` 的硬件拓扑声明**严禁由固件在运行时动态解析**，而是由 `wink-tools` 在执行 `wink build sim` 时执行双向派发：

```
                   [ wink-micro-app/<app>/wink-app.json (SSOT) ]
                                         │
                                         ▼ (wink build sim)
                      wink-tools (tools/codegen/app_codegen.py)
                       ┌─────────────────┴─────────────────┐
                       ▼                                   ▼
            【固件编译期静态头文件】                 【UniSim 仿真前端资产】
         include/mcs51_board_config.h             unisim-assets/device-tree.json
                       │                                   │
                       ▼                                   ▼
              frameworks/mcs51 编译                    UniSim / JS 运行时
             (-nostdlib++ 静态常数)                 (Wokwi 连线与外设物理引擎)
```

1. **固件编译期生成**：`app_codegen.py` 解析 `wink-app.json` 并渲染生成 `include/mcs51_board_config.h`：
   ```c
   // 由 wink-tools 从 wink-app.json 自动生成的引脚绑定宏 (mcs51_board_config.h)
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
   `src/mcs51_runtime.cpp` 在 `app_init` 启动时直接读取常量完成 `mcs51_adc0832_init(...)` 静态初始化，零运行时开销、零 JSON 库依赖。
2. **仿真运行期导出**：同时导出 `unisim-assets/device-tree.json`，供浏览器 Wokwi / UniSim 物理总线在沙箱外加载和渲染虚拟元件。

---

## 3. 四大 `extern "C"` 语言边界与链接规范 (Linkage Boundaries)

为保证整个系统符号清晰、杜绝 C++ Name Mangling 冲突，固化以下 4 个物理链接边界：

```
+-----------------------------------------------------------------------------------+
|                        用户 8051 源码 (*.c, 强制 -x c++)                          |
|                        - void main(void) { ... }                                  |
|                        - void Timer0_ISR(void) interrupt 1 { ... }                |
+-----------------------------------------+-----------------------------------------+
                                          |
                      [边界 ①] 宏重映射 + | [边界 ②] 正则清洗 + WINK_ISR(n)
                      extern "C" 前向声明 | 静态注册器 + extern "C" 函数原型
                                          v
+-----------------------------------------------------------------------------------+
|                  frameworks/mcs51/ 内部实现 (统一全量 .cpp 编译)                   |
|                  - mcs51_proxy.hpp (WinkSfr 字节/位级操作符重载)                   |
|                  - mcs51_sfr.cpp (s_sfr_shadow[256] 实体内存)                     |
|                  - mcs51_runtime.cpp (Fiber 协程让出与调度推进)                   |
+--------------------+----------------------------------------+---------------------+
                     |                                        |
     [边界 ③] C-ABI 函数指针表                 [边界 ④] Wink 内核桥接
     mcs51_trap_register_*                    wink_app_get_callbacks
     extern "C" 注册与查询                    extern "C" 物理量注入 API
                     v                                        v
+------------------------------------+   +------------------------------------------+
| Level 2 即时陷阱状态机             |   | WinkMicroOS 核心 / UniSim 物理仿真总线   |
| (ADC0832, CMS8S ADC, I2C/SPI)      |   | (pal, dal, wink_runtime, WebAssembly JS) |
+------------------------------------+   +------------------------------------------+
```

### 3.1 边界 ①：用户 `main` 入口符号绑定与包含时序纪律

#### 1. `REGX52.H` 标准实现
```c
// include/REGX52.H
#pragma once

// 步骤 1：在任何宏替换前，前置拉入基础与常用系统头文件，防止后续 main 宏污染系统原型
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __C51__
#ifdef __cplusplus
extern "C" {
#endif
// 步骤 2：提前以 extern "C" 前向声明重命名后的入口
void wink_mcs51_user_main(void);
#ifdef __cplusplus
}
#endif

// 步骤 3：符号重映射
#define main wink_mcs51_user_main

// 步骤 4：Keil C51 扩展方言宏消除
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

#endif

#include "mcs51_proxy.hpp"
```

> **语言标准依据（C++ Standard [dcl.link]）**：
> 一个函数只要在当前翻译单元（TU）内第一次声明时具有 `extern "C"` 语言链接属性，后续用户写出的 `void wink_mcs51_user_main(void) { ... }` 实现定义将自动继承 C 链接，导出未修饰符号。
> 严禁使用 `#define main extern "C" void ...` 模式，因为用户写 `void main(void)` 会导致展开为 `void extern "C" void ...`，构成不可恢复的 C++ 语法错误。

#### 2. 方言消除、指针语法等价性与哈佛架构平坦化规范

##### 1. `#define code const` 指针修饰语法等价性证明
Keil C51 中 `code` 用于声明数据或指针存放于程序 ROM 空间。将其映射为 `const` 后，在标准 C/C++ 类型系统中达成严格等价的访问控制与存储分配：
- **只读数组**：`unsigned char code table[]` $\rightarrow$ `unsigned char const table[]`（分配于只读段 `.rodata`，禁止写入，语义 100% 等价）；
- **指向 ROM 的指针**：`unsigned char code *p` $\rightarrow$ `unsigned char const *p`（指向只读数据，`*p = 1` 报编译错误，语义等价）；
- **存放在 ROM 的指针**：`unsigned char * code p = buf` $\rightarrow$ `unsigned char * const p = buf`（顶层指针为常量不可改指，指向普通 RAM，语义等价）；
- **双重 ROM 约束**：`unsigned char code * code p = table` $\rightarrow$ `unsigned char const * const p = table`（常量指针指向只读数据，语义等价）。

##### 2. 存储限定符抹除与哈佛架构降维平坦化（Harvard-to-Flat Architecture）
8051 硬件属于物理哈佛架构，`DATA` (0x00~0x7F)、`IDATA` (0x00~0xFF)、`XDATA` (0x0000~0xFFFF) 和 `CODE` (0x0000~0xFFFF) 拥有完全独立的物理地址总线与操作指令（`MOV`, `MOVX`, `MOVC`）。
- **仿真平坦化决策**：在宿主 PC（x86/x64）和浏览器（Wasm32）环境下，进程空间统一为平坦线性内存（Flat Memory）。宏直接将 `data/idata/xdata/pdata` 抹除为空：
  - 用户写 `xdata unsigned char buf[1024]` 99.9% 的业务本质仅是为了**突破 8051 片内 128B RAM 的硬件容量瓶颈**。平坦化后直接在宿主大内存中透明分配，计算结果与业务逻辑完全不受影响；
  - 彻底摒弃了维护开销巨大、严重拖慢解引用性能的带 Tag 胖指针（Fat Pointer）方案，保留了纯原生 C/C++ 裸指针的极限执行性能与 ABI 兼容性；
- **契约透明声明**：明确写入《不支持特性清单》（见主规划书 §3.8），不支持依赖物理哈佛分体地址特性的底层代码（如强行假设 `xdata 0x20` 与 `data 0x20` 分属两块物理介质独立并存、或解析 generic 3 字节指针内部 Tag 字节等）。

#### 3. 常见系统头文件无感预引入与任意顺序包含机制 (Header Pre-Inclusion)
在真实 8051 工程中，极多开发者习惯在源文件第 1 行直接书写 `#include <REG52.H>`，随后再引入 `<stdio.h>` 等。
为彻底避免强加“包含顺序限制”打扰用户，`REGX52.H` 在顶部前置安全引入了 `<stdio.h>` 与 `<stdlib.h>` 等常用头文件。借助标准库自带的宏防护守卫（Header Guard），后续用户源文件无论以何种顺序包含系统库，标准库原型均已在安全环境下展开完毕，**绝不会受到后续 `#define main` 的符号污染，达成 100% 任意顺序书写兼容**。

---

### 3.2 边界 ②：中断自动注册宏 `WINK_ISR(n)` 跨平台实现

摒弃 GCC 专有的 `__attribute__((constructor))`（避免在 MSVC 下失效），采用标准 C++ 静态结构体构造器完成 ISR 向量表自注册，保证跨 MSVC / Clang / GCC / Emscripten 100% 行为一致：

```cpp
// include/wink_mcs51_isr.h
#pragma once

#ifdef __C51__
  #define WINK_ISR(n) interrupt n
#else
  #ifdef __cplusplus
  extern "C" {
  #endif
  void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void));
  #ifdef __cplusplus
  }
  #endif

  // 展开后成为完整且合法的 extern "C" 函数原型声明与静态注册结构体
  #define WINK_ISR(n) \
    extern "C" void wink_isr_vector_##n(void); \
    namespace { \
      struct WinkIsrAutoReg_##n { \
        WinkIsrAutoReg_##n() { wink_mcs51_set_isr(n, wink_isr_vector_##n); } \
      } s_auto_reg_##n; \
    } \
    extern "C" void wink_isr_vector_##n(void)
#endif
```

> 🛡️ **静态初始化安全保障 (B3 闭环)**：
> 核心表结构（`s_isr_table`、`s_sfr_shadow`、`s_pin_traps` 等）100% 采用纯 C POD 零初始化（在 BSS 段完成，早于任何 C++ 构造器运行），`WinkSfr` 实例强制 `constexpr` 常量初始化，彻底杜绝跨编译单元静态初始化顺序死锁（Static Initialization Order Fiasco）。详细推导详见 🔗 [时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §4。

---

### 3.3 边界 ③：Level 2 陷阱 C-ABI 接口与注册模型

完全遵从 [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md) 静态分发原则，**彻底摒弃 C++ 虚函数表**，使用轻量 POD 结构体与函数指针：

```c
// include/mcs51_trap.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mcs51_pin_write_fn_t)(void *ctx, uint8_t val);
typedef uint8_t (*mcs51_pin_read_fn_t)(void *ctx);

typedef struct {
    mcs51_pin_write_fn_t on_write;
    void *write_ctx;
    mcs51_pin_read_fn_t  on_read;
    void *read_ctx;
} mcs51_pin_trap_t;

// 注册接口：支持 C 和 C++ 外设状态机无缝注册
void mcs51_trap_register_write(uint8_t port, uint8_t bit_idx, mcs51_pin_write_fn_t fn, void *ctx);
void mcs51_trap_register_read(uint8_t port, uint8_t bit_idx, mcs51_pin_read_fn_t fn, void *ctx);
void mcs51_trap_clear(uint8_t port, uint8_t bit_idx);

#ifdef __cplusplus
}
#endif
```

---

### 3.4 边界 ④：Wink 内核桥接与 UniSim 外部物理量注入

#### 1. Wink App Callbacks 导出（标准兼容规范）
针对 C++ 标准中对聚合初始化的约束，采用无名聚合初始化或运行时逐字段赋值：

```cpp
// src/mcs51_runtime.cpp
#include "wink_app.h"

extern "C" const wink_app_callbacks_t* wink_app_get_callbacks(void) {
    static wink_app_callbacks_t s_callbacks = {};
    static bool s_inited = false;
    if (!s_inited) {
        s_callbacks.app_init = mcs51_app_init;
        s_callbacks.app_loop = mcs51_app_loop;
        s_callbacks.app_cleanup = mcs51_app_cleanup;
        s_inited = true;
    }
    return &s_callbacks;
}
```

#### 2. UniSim 3.0 通道 3 模拟量拉取 (Pull) 与测试注入契约 (AD-8 / ADR-0057)

根据 [UniSim 3.0 模拟信号通道规范](../../design/04-wasm-simulation/02-mechanisms/08-channel-routing.md#24-通道-3模拟量analog-signal)，所有模拟传感器（如 NTC、电位器、LDR）的物理源统一挂载于 `PinArbiter`，并通过 C 导入接口 `js_pal_adc_read_norm(pin)` 提供 $[0.0, 1.0]$ 的归一化浮点采样值。

```c
// include/mcs51_adc.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1. 模拟量获取入口：由虚拟外设状态机内部自动调用（用户代码完全零感知） */
uint16_t mcs51_adc_get_value(uint8_t ch);

/* 2. 单测/CI 注入覆盖接口：仅用于 Host 单元测试或自动化故障注入 */
void     mcs51_adc_set_value(uint8_t ch, uint16_t raw);

/* 3. 向前兼容 shim：外接 8 位 ADC0832 直接映射至统一接口 */
static inline void mcs51_adc0832_set_value(uint8_t ch, uint8_t val) {
    mcs51_adc_set_value(ch, (uint16_t)val);
}
static inline uint8_t mcs51_adc0832_get_value(uint8_t ch) {
    return (uint8_t)(mcs51_adc_get_value(ch) & 0xFF);
}

#ifdef __cplusplus
}
#endif
```

* **双轨数据流设计（Dual-Track Data Path）**：
  1. **生产运行轨（UniSim 3.0 Wasm 环境，标准 Pull 模型）**：
     - `mcs51_adc_get_value(ch)` 底层直接调用 `js_pal_adc_read_norm(global_pin)`，向 JS `PinArbiter` 即时拉取最新物理分压比；
     - 驱动模型自动按目标芯片分辨率换算码值：
       - **ADC0832（8 位）**：$\text{raw} = (\text{uint8\_t})(\text{norm} \times 255.0\text{f} + 0.5\text{f})$；
       - **CMS8S 片内 ADC（12 位）**：$\text{raw} = (\text{uint16\_t})(\text{norm} \times 4095.0\text{f} + 0.5\text{f})$；
     - **零 JS 胶水**：前端无需专有 51 导出函数，完美复用标准通道 3 与电热 NTC 物理插件！
  2. **单测/CI 校验轨（Host 原生环境）**：
     - 若未链接 Wasm 运行时或调用了 `mcs51_adc_set_value(ch, raw)`，优先返回手动注入值，保证 C 语言单元测试与 CI 门禁的高速确定性执行。
* **用户业务代码零侵入保证**：
  - 用户业务代码只书写原生 8051 GPIO 脉冲时序（ADC0832）或操作 `ADCON` 寄存器（CMS8S）；
  - 采样与拉取完全在虚拟外设 C++ 沙箱内部静默完成，对上层业务代码 **0 污染、0 改动、0 依赖**。

---

## 4. SFR 影子内存与 WinkSfr / WinkSfrBitProxy 模型

### 4.1 线性影子空间与 ODR 防护

8051 的特殊功能寄存器固定分布在 `0x80 ~ 0xFF` 区域，其中 P0~P3 的经典地址分别为：
- `P0 = 0x80` (`port_idx = 0`)
- `P1 = 0x90` (`port_idx = 1`)
- `P2 = 0xA0` (`port_idx = 2`)
- `P3 = 0xB0` (`port_idx = 3`)

使用 256 字节的线性数组 `s_sfr_shadow[256]` 实现 O(1) 访问。
- **声明与定义分离（规避 ODR 多重定义）**：
  - `include/mcs51_proxy.hpp` 仅包含 `extern "C" { extern uint8_t s_sfr_shadow[256]; extern mcs51_pin_trap_t s_pin_traps[4][8]; }`；
  - `src/mcs51_sfr.cpp` 分配唯一实体内存：`uint8_t s_sfr_shadow[256] = {0}; mcs51_pin_trap_t s_pin_traps[4][8] = {};`。

---

### 4.2 `WinkSfrBitProxy` 实现与 `sbit` 宏定义（位级读写陷阱）

```cpp
// include/mcs51_proxy.hpp (节选)

struct WinkSfrBitProxy {
    uint8_t sfr_addr; // 物理 SFR 地址 (0x80, 0x88, 0x90, 0x98, 0xA8, 0xD0 等)
    uint8_t port_idx; // 0..3 对应 P0..P3；0xFF 对应非 GPIO 位可寻址控制 SFR (TCON/SCON/IE/IP/PSW 等)
    uint8_t bit_idx;  // 0..7
    uint8_t bit_mask; // 1 << bit_idx

    // 位写赋值：更新真实物理影子，严禁越界查表，按类型分流引脚 Trap 与 SFR 写钩子
    inline WinkSfrBitProxy& operator=(uint8_t val) {
        const uint8_t old_val = s_sfr_shadow[sfr_addr];
        const uint8_t old_bit = (old_val >> bit_idx) & 1;
        const uint8_t new_bit = val ? 1 : 0;
        const uint8_t new_val = new_bit ? (old_val | bit_mask) : (old_val & ~bit_mask);

        s_sfr_shadow[sfr_addr] = new_val;

        if (port_idx < 4) {
            // GPIO 引脚：电平跳变时触发引脚写陷阱
            if (old_bit != new_bit) {
                auto& trap = s_pin_traps[port_idx][bit_idx];
                if (trap.on_write) trap.on_write(trap.write_ctx, new_bit);
            }
        } else {
            // 控制 SFR (TCON, SCON, ADCON 等)：触发 SFR 专用写钩子
            if (old_bit != new_bit && s_sfr_write_hooks[sfr_addr]) {
                s_sfr_write_hooks[sfr_addr](sfr_addr, old_val, new_val);
            }
        }
        return *this;
    }

    // 位读求值：GPIO 拉取外部动态电平；控制 SFR 触发读前钩子 (如读取 TF0 时推进定时器)
    inline operator uint8_t() const {
        if (port_idx < 4) {
            auto& trap = s_pin_traps[port_idx][bit_idx];
            if (trap.on_read) return trap.on_read(trap.read_ctx) ? 1 : 0;
        } else {
            if (s_sfr_read_hooks[sfr_addr]) {
                s_sfr_read_hooks[sfr_addr](sfr_addr);
            }
        }
        return (s_sfr_shadow[sfr_addr] & bit_mask) ? 1 : 0;
    }

    // 位级 RMW 复合运算：严格读物理锁存器影子
    inline WinkSfrBitProxy& operator^=(uint8_t rhs) {
        return *this = (((s_sfr_shadow[sfr_addr] >> bit_idx) & 1) ^ (rhs & 1));
    }
    inline WinkSfrBitProxy& operator|=(uint8_t rhs) {
        return *this = (((s_sfr_shadow[sfr_addr] >> bit_idx) & 1) | (rhs & 1));
    }
    inline WinkSfrBitProxy& operator&=(uint8_t rhs) {
        return *this = (((s_sfr_shadow[sfr_addr] >> bit_idx) & 1) & (rhs & 1));
    }
};
```

#### `sbit` 宏定义与语法规范
在 `include/REGX52.H` 中定义：
```cpp
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define sbit inline auto   // C++17 内联变量自动推导，常量初始化，ODR 安全
#else
  #define sbit static auto   // C++14 内部链接常量退路
#endif
```
- ⚠️ **语法限制**：仅支持 `sbit X = REG^bit;` 相对位格式；严禁 `sbit X = 0xXX;` 绝对位地址语法（后者会被推导为 `int` 常量引起静默数据损坏，详见 🔗 [SFR 代理与边沿感知规格书](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md) §5.3）。

---

### 4.3 `WinkSfr` 整端口模型与边沿跳变检测 (A2 闭环)

> 📖 **专属技术设计规格书**：
> 有关 8051 准双向 I/O 硬件底层（Read-Latch vs Read-Pin 隔离）、`WinkSfr` 完整 C++ 操作符代数体系（含 `+=`, `-=`, `++`, `--`, `<<=`, `>>=`）、ODR 内联消除与 ADC0832 边沿防伪推演，详见独立子系统规格书：
> 🔗 [8051 SFR 影子内存代理、整端口 RMW 语义与边沿跳变感知技术设计规格书](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)。

用户既可能使用 `sbit CLK = P1^2; CLK = 1;`，也可能使用整端口写入 `P1 |= 0x04;` 或 `P1 = 0x55;`。`WinkSfr` 必须对整字节读写提供严格等价的边沿触发支持：

```cpp
// include/mcs51_proxy.hpp (核心实现节选)

struct WinkSfr {
    uint8_t sfr_addr; // 0x80, 0x90, 0xA0, 0xB0 或其它 SFR
    uint8_t port_idx; // 0..3 代表 P0..P3，0xFF 代表非 GPIO 端口 SFR

    constexpr WinkSfr(uint8_t addr)
        : sfr_addr(addr),
          port_idx((addr == 0x80) ? 0 :
                   (addr == 0x90) ? 1 :
                   (addr == 0xA0) ? 2 :
                   (addr == 0xB0) ? 3 : 0xFF) {}

    constexpr WinkSfr(uint8_t addr, uint8_t port)
        : sfr_addr(addr), port_idx(port) {}

    // 【透传物理地址】：保证 TCON(0x88)^5 能正确将 0x88 写入代理对象
    constexpr WinkSfrBitProxy operator^(uint8_t bit_idx) const {
        return WinkSfrBitProxy{sfr_addr, port_idx, bit_idx, (uint8_t)(1 << bit_idx)};
    }

    // 整字节写入：对比新旧值 diff，仅对发生电平跳变的 bit 触发 on_write
    inline WinkSfr& operator=(uint8_t val) {
        uint8_t old_val = s_sfr_shadow[sfr_addr];
        s_sfr_shadow[sfr_addr] = val;

        if (port_idx < 4) {
            uint8_t diff = old_val ^ val; // 提取跳变掩码
            if (diff == 0) return *this;   // 快速路径：零开销返回

            for (uint8_t bit = 0; bit < 8; ++bit) {
                if ((diff & (1 << bit)) && s_pin_traps[port_idx][bit].on_write) {
                    uint8_t new_level = (val >> bit) & 1;
                    s_pin_traps[port_idx][bit].on_write(
                        s_pin_traps[port_idx][bit].write_ctx, new_level
                    );
                }
            }
        }
        return *this;
    }

    // 整字节读取 (Read-Pin)：动态拉取注册了 on_read 的引脚当前电平，动态重构返回字节
    inline operator uint8_t() const {
        uint8_t val = s_sfr_shadow[sfr_addr];
        if (port_idx < 4) {
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (s_pin_traps[port_idx][bit].on_read) {
                    uint8_t pin_level = s_pin_traps[port_idx][bit].on_read(
                        s_pin_traps[port_idx][bit].read_ctx
                    );
                    if (pin_level) val |= (1 << bit);
                    else          val &= ~(1 << bit);
                }
            }
        }
        return val;
    }

    // 复合赋值 (RMW)：基准值必须严格读取锁存器影子，严禁读引脚导致输入引脚误锁地！
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

---

## 5. Level 2 即时同步陷阱执行引擎与运行纪律 (Execution Discipline)

### 5.1 同步模型对比

| 级别 | 机制 | 触发时机 | 适用场景 |
| :--- | :--- | :--- | :--- |
| **Level 1: 批量同步** | `sync_in()` / `sync_out()` | 调度器每 tick / Fiber 上下文切换前后 | 普通 LED 状态指示、按键状态批量读取 |
| **Level 2: 即时陷阱** | `s_pin_traps` 函数指针表 | 引脚写/读语句发生的**当前 CPU 周期内立即同步执行** | ADC0832、软 I2C、软 SPI 等高速 Bit-Bang 时序 |

### 5.2 Trap 回调四大执行红线（硬实时纪律）

由于 `on_write` / `on_read` 是在用户 Fiber 正在执行引脚赋值语句的调用栈深处直接触发，为防止重入破坏与上下文损坏，确立以下红线：

> [!CAUTION]
> 1. **零延时（Zero Wait）**：Trap 回调内部**严禁调用任何物理延时或阻塞函数**（如 `pal_os_busy_wait_us`、`pal_os_delay_ms`）。
> 2. **禁止协程让出（No Yield）**：Trap 回调内部**严禁调用 `sim_ctx_yield()`**，严禁触发 Fiber 切换。
> 3. **纯状态机驱动**：回调职责仅限于外设自身内部状态转移、时钟计数累加，或直接修改/读取 SFR 影子寄存器。
> 4. **时钟解耦**：需要推进虚拟时间的场景（定时器中断、UART 传输速率）必须交由外部调度器的 Fiber tick 驱动，绝不借道 Trap 回调隐式推进。

---

## 6. 经典与增强外设行为模型实现规格 (Peripheral Models)

### 6.1 外接 ADC0832 即时状态机 (`src/mcs51_adc0832.cpp`)

#### 1. 跨端口引脚绑定接口
支持板级连线跨越不同端口（如 CS 接 P1.2，CLK 接 P3.4）：
```c
void mcs51_adc0832_init(
    uint8_t cs_port,  uint8_t cs_bit,
    uint8_t clk_port, uint8_t clk_bit,
    uint8_t di_port,  uint8_t di_bit,
    uint8_t do_port,  uint8_t do_bit
);
```
- **3 线 DIO 复用自适应 (B1 闭环)**：若初始化参数传入 `di_port == do_port && di_bit == do_bit`，状态机自动进入 3 线 DIO 并联模式，通过阶段划分（`PHASE_INPUT` 采样配置位 vs `PHASE_OUTPUT` 移位输出并屏蔽 MCU 释放总线写操作），完美兼容经典 51 教学板单引脚双向电路。完整状态机代码与时序推导详见 🔗 [时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §3。

#### 2. 状态机迁移逻辑
ADC0832 是经典 8 位分辨率 A/D 转换芯片，其典型时序与状态机在纯同步 Trap 中瞬间完成推进：

```
                +-------------------------------------------------------+
                |                    IDLE (空闲态)                      |
                +-------------------------------------------------------+
                                        | CS 下降沿 (CS: 1 -> 0)
                                        v
                +-------------------------------------------------------+
                |               START_BIT (等待起始位 DI=1)              |
                +-------------------------------------------------------+
                                        | CLK 上升沿
                                        v
                +-------------------------------------------------------+
                |              CONFIG (采样 SGL/DIF 与 ODD/SIGN)         |
                +-------------------------------------------------------+
                                        | CLK 上升沿 (锁定转换通道 CH0/CH1)
                                        v
                +-------------------------------------------------------+
                |              CONVERTING (转换与数据输出)               |
                | - 每个 CLK 下降沿：移出 1 位到 on_read 动态输出缓存    |
                | - 同时同步镜像写入 DO 对应引脚的 s_sfr_shadow 影子位  |
                | - 连续 8 个 CLK 下降沿依次推移 MSB-first 数据        |
                +-------------------------------------------------------+
                                        | CS 上升沿 (CS: 0 -> 1)
                                        v
                                回到 IDLE，DO 释放为高阻态
```

- **读取机制定性 (on_read 主 + 影子镜像辅)**：
  DO 数据读取**定死以 Level 2 `on_read` 动态回调为主路径**，确保 3 线 DIO 复合引脚在输出阶段准确返回移位数据；同时在移位时**同步更新 `s_sfr_shadow` 对应位**，确保 `if(ADC_DIO)`（位读）与 `if(P1 & 0x01)`（整端口字节读）取得完全一致的数据，彻底杜绝歧义。
- **验证要求**：CI 测试样例中必须同时包含“`sbit` 引脚位翻转”与“`P1 |= / &=` 整端口跳变”两套 ADC0832 驱动代码，验证两者均能闭环读取相同的 8 位转换值。

---

### 6.2 中微 CMS8S78xx 片内 12-bit ADC 即时模型 (`frameworks/mcs51/src/cms8s_adc.cpp`)

> **M5 夹具核对回写（ADR-0073，2026-08-29）**：本节早期版本在无原厂资料阶段给出的寄存器图（`ADCON@0xE1` / START bit6 / EOC bit5 / `ADCFG@0xE2` / `ADCH@0xE3` / `ADCL@0xE4`）系惯例臆测，**与真实硅片不符，已废弃**。以下内容以原厂夹具三处交叉验证为准：CMS8S78xx 参考手册 V1.1.1 第 22 章、设备头 `cms8s78xx.h` SFR 表、StdDriver `adc.c` 实际读写。夹具为参考资料（`docs/vendors/Cmsemicon/`，不入库为交付物）。

增强型 51（中微 CMS8S78xx，小家电主力芯片）集成片内 12-bit SAR ADC：26 个外部通道 AN0~AN25（AN0~7=P00~P07、AN8~15=P10~P17、AN16~21=P20~P25、AN22~25=P30~P33）加内部通道 AN63（BGR/温度/VDD 复用），通过 SFR 控制、XSFR（MOVX 空间）配置参考源 LDO。

#### 6.2.1 真实寄存器映射与物理语义

| 寄存器名 | 物理地址 | 关键位定义 | 仿真映射与拦截行为 |
| :--- | :--- | :--- | :--- |
| **`ADCON0`** | `0xDF` | **bit1 `ADGO`**：写 1 启动转换；**硬件完成后自清零为 0——该位即忙标志**（轮询 `while(ADCON0 & 0x02)`）。<br>**bit6 `ADFM`**：结果对齐（0=左对齐，1=右对齐）。<br>bit5:2 `ANACH`：AN63 内部子通道选择。 | SFR 写钩子 `mcs51_trap_register_sfr_write(0xDF, …)`：捕获 ADGO 写脉冲，即时拉取码值、装载结果、影子自清 ADGO、锁存 ADCIF/派发向量 19。 |
| **`ADCON1`** | `0xDE` | **bit7 `ADEN`**：ADC 模块使能（转换门控）。<br>bit6:4 `ADCKS`：转换时钟分频（Fsys/2…/256）。 | 门控判断：`ADEN=0` 时 ADGO 写被忽略。 |
| **`ADCCHS`** | `0xD9` | bit5:0：通道选择（0~25 = AN0~AN25；`0x3F` = AN63 内部）。 | 钩子读取影子取通道号。 |
| **`ADRESH`** | `0xDD` | 结果高字节（只读）。 | 模型按 ADFM 装载影子；不挂读钩子。 |
| **`ADRESL`** | `0xDC` | 结果低字节（只读）。 | 同上。 |
| **`EIE2`** | `0xAA` | bit4 `ADCIE`：转换完成中断使能。 | 完成时若置位则锁存标志。 |
| **`EIF2`** | `0xB2` | bit4 `ADCIF`：转换完成标志（**软件清零、硬件不自动清**，同 UART TI 先例）。 | 完成时置位；ISR/轮询代码自行清零。 |
| **`IE`** | `0xA8` | bit7 `EA`：全局中断使能。 | EA+ADCIE 均置位时派发 **Keil 中断向量 19**（向量地址 0x9B）。 |
| **`ADCLDO`** | XSFR **`0xF692`**（MOVX/xdata 空间） | bit7 `LDOEN`、bit6:5 `VSEL`（参考电压 1.2/2.0/2.4/3.0V）、bit4 `LDOOUTEN`。 | `WinkXsfr` 代理 → xdata XSFR 窗口 `[0xF000,0x10000)` 受检读写；v1 VSEL 不影响满量程。 |
| **`PxxCFG`** | XSFR `0xF000..0xF033` | 引脚功能复用（如 P00CFG=0xF000、P32CFG=0xF032）。 | 同上，WinkXsfr 代理。 |

掩码/枚举宏在 `frameworks/mcs51/include/REG_CMS8S.H` 中按原厂**逐字命名**提供（`ADC_ADCON0_ADFM_Msk=0x40`、`ADC_ADCON0_ADGO_Msk=0x02`、`ADC_ADCON1_ADEN_Msk=0x80`、`ADC_ADCLDO_LDOEN_Msk=0x80`、`IRQ_EIE2_ADCIE_Msk=0x10`、`IRQ_EIF2_ADCIF_Msk=0x10`、`ADC_CH_0..25`、`ADC_CH_63`、`ADC_RESULT_LEFT/RIGHT`、`ADC_VREF_*`、`ADC_IS_BUSY`、`ADC_GO()` 等）。与原厂 `adc.h` 重名的枚举宏**采用原厂逐字 token 间距**（如 `(0x03<<ADC_ADCLDO_VSEL_Pos)`）：GCC 无 `-Wmacro-redefined`（Clang flag 被忽略），仅当两定义 token 流含空白完全一致才静默接受重定义。

> **tier-b 收割（2026-08-29，ADR-0073 D6）**：原厂 StdDriver `adc.c` 已**未修改**编译并运行（`test_mcs51_cms8s_vendor`）。committed shim `frameworks/mcs51/include/cms8s78xx.h` 置于 include 路径首位遮蔽原厂 Keil 设备头（重定义 stdint/sfr、野指针 `ADCLDO`），仅 `#include "REG_CMS8S.H"`；原厂 GBK `adc.c/adc.h` 经 `mcs51_cleanup.py`（UTF-8 优先/GBK 回退 + `--transcode`）在构建树规范化为 UTF-8（源只读、不入库）；多 TU 经 C++17 `inline WinkSfr/WinkXsfr` ODR 安全共享；vendor 头目录标 SYSTEM include（`-isystem` 抑制第三方告警，自家 TU 仍 `-Werror`），MSVC `/wd4005`；夹具缺失 CMake 优雅跳过。

---

#### 6.2.2 即时转换完成机制与 0 周期穿透 (Instant Conversion Model)

##### 1. 用户典型轮询习语（原厂 StdDriver 风格）
```c
ADCCHS = ADC_CH_0;        // 选通道
ADCON1 |= 0x80;           // ADEN = 1，模块使能
ADCON0 = 0x40;            // ADFM = 1（右对齐）
ADCON0 |= 0x02;           // ADGO = 1，启动转换（ADC_GO()）
while (ADCON0 & 0x02) { ; }  // 等 ADGO 硬件自清（= 忙标志）
code = 0x0FFF & ((ADRESH << 8) | ADRESL);  // 右对齐读取
```
- **死锁成因**：与 ADC0832 相同——单线程协作式仿真下，若等下一 tick 才清忙标志，`while` 裸机紧凑轮询将霸占 fiber，调度器无法步进。
- **即时模型闭环解法**：状态机挂接在 **ADCON0（0xDF）的 SFR 写钩子**上。整字节写（`ADCON0 = 0x42`）与 RMW（`ADCON0 |= 0x02`）都经 `WinkSfr::operator=`（影子先存、钩子后发），钩子在**当条写入语句内同步完成**：
  1. 门控：`new_val & 0x02`（ADGO）且影子 `ADCON1 & 0x80`（ADEN），否则忽略；
  2. 取通道 `ch = ADCCHS & 0x3F`；ch≤25 从 12-bit 通道-3 注入轨 `mcs51_adc_get_value(ch) & 0x0FFF` 拉取；ch==0x3F（AN63 内部）v1 返回 0；
  3. 按 ADFM（`new_val & 0x40`）拆分装载 ADRESH/ADRESL（见 6.2.3）；
  4. **影子自清 ADGO**：`shadow[0xDF] = new_val & ~0x02`；
  5. 中断：若 `EIE2 & 0x10`（ADCIE）则 `EIF2 |= 0x10`（ADCIF 锁存）；若再 `IE & 0x80`（EA）则 `wink_mcs51_dispatch_vector(19)`。
- **收益**：`while (ADCON0 & 0x02)` 首轮判断即为假，**0 周期无缝穿透**（ADR-0072 D1：即时外设 0µs），忙等死锁与协程切换开销双双消除。

---

#### 6.2.3 12-bit 结果对齐规则 (ADFM Modes)

12-bit 码值（`0 ~ 4095`）装入 ADRESH/ADRESL 的格式严格受 `ADCON0.ADFM`（bit6）控制，拆分公式与原厂 `ADC_GetADCResult()` 的读取公式互逆：

| 对齐模式 | `ADFM` | `ADRESH` 内容 | `ADRESL` 内容 | 用户读取公式（原厂逐字） |
| :--- | :--- | :--- | :--- | :--- |
| **右对齐** | `ADFM = 1` | `0000_D11..D8`（低 4 位有效） | `D7..D0`（全 8 位） | `0x0FFF & ((ADRESH << 8) \| ADRESL)` |
| **左对齐** | `ADFM = 0` | `D11..D4`（全 8 位） | `D3..D0_0000`（高 4 位在 [7:4]） | `0x0FFF & ((ADRESH << 4) \| (ADRESL >> 4))` |

模拟注入轨统一为 **12-bit**（`mcs51_adc.h`：`MCS51_ADC_RAW_MAX=4095`，32 通道表覆盖 AN0~AN25 + AN63 余量；Pull 路径 `raw = norm * 4095`）。8-bit ADC0832 的两个消费点均 `& 0xFF` 取低字节，轨加宽对其零影响（M4 测试全回归）。

##### XSFR 代理与向量表扩表（两处 M5 基础设施）

```cpp
// 1) XSFR：原厂宏 #define ADCLDO *(volatile unsigned char xdata *)0xF692
//    清洗擦除 xdata 后会退化为宿主野指针——REG_CMS8S.H 改以常量初始化代理取代：
inline WinkXsfr ADCLDO(0xF692);   // mcs51_xsfr.hpp：operator=/uint8_t/|=&=^=
                                  // 全部走 wink_mcs51_xdata_read/write(addr, v, kind=XSFR)
//    xdata 合法孔径 = [0, WINK_MCS51_XDATA_SIZE) ∪ [0xF000, 0x10000)（XSFR 窗口）；
//    窗口内落 64KB 影子，窗口外保持 STRICT(assert+abort)/release(告警+丢弃/0xFF) 双态。

// 2) 向量 19：WINK_MCS51_NUM_VECTORS 8 → 28（核心 0~7；CMS8S 扩展 8~27；ADC=19）。
//    旧值下 set_isr 对 n≥8 静默丢弃，ADC ISR 永不派发——扩表为向量 19 的硬前提。
```

##### 生产级状态机实现 (`frameworks/mcs51/src/cms8s_adc.cpp`，核心钩子)

```cpp
void on_adcon0_write(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if ((new_val & 0x02u) == 0u) return;                                  // ADGO?
    if ((wink_mcs51_sfr_shadow[0xDE] & 0x80u) == 0u) return;              // ADEN?
    uint8_t ch = (uint8_t)(wink_mcs51_sfr_shadow[0xD9] & 0x3Fu);          // ADCCHS
    uint16_t raw = (ch <= 25u) ? (uint16_t)(mcs51_adc_get_value(ch) & 0x0FFFu)
                               : (uint16_t)0u;                            // AN63 → 0 (v1)
    if (new_val & 0x40u) {                                                // ADFM=1 右对齐
        wink_mcs51_sfr_shadow[0xDD] = (uint8_t)((raw >> 8) & 0x0Fu);
        wink_mcs51_sfr_shadow[0xDC] = (uint8_t)(raw & 0xFFu);
    } else {                                                              // 左对齐
        wink_mcs51_sfr_shadow[0xDD] = (uint8_t)((raw >> 4) & 0xFFu);
        wink_mcs51_sfr_shadow[0xDC] = (uint8_t)((raw & 0x0Fu) << 4);
    }
    wink_mcs51_sfr_shadow[0xDF] = (uint8_t)(new_val & ~0x02u);            // ADGO 自清
    if (wink_mcs51_sfr_shadow[0xAA] & 0x10u) {                            // EIE2.ADCIE
        wink_mcs51_sfr_shadow[0xB2] |= 0x10u;                             // EIF2.ADCIF 锁存
        if (wink_mcs51_sfr_shadow[0xA8] & 0x80u)                          // IE.EA
            (void)wink_mcs51_dispatch_vector(19u);
    }
}
```

- **验收证据（M5）**：样例 `test/mcs51/samples/cms8s_adc_test.c`（未修改 Keil 风格，三次转换 AN0 右/AN1 左/AN25 右 → XDATA 0x10~0x15）经 host e2e（`test_mcs51_cms8s_adc_e2e.c`）与 wasm/Node（`wasm_mcs51_cms8s_adc_test`）双跑断言 0xABC/0x801/0xFFF；单元测试 `test_cms8s_adc_instant.cpp` 覆盖 0 周期自清、左右装载、ADCIE/EA 向量 19 门控（恰派发 1 次、标志保持）、ADEN 门控、AN25 透传、AN63→0、XSFR 窗口合法 + 0xE000 OOB 计数。
- **tier-b 验收证据（2026-08-29）**：未修改原厂 StdDriver `adc.c` 编译运行 —— `test/mcs51/unit/test_cms8s_vendor_stdriver.cpp` 以原厂 `ADC_*` API 驱动模型（`ADC_ConfigRunMode` 右/左对齐 + `ADC_GetADCResult` 原厂公式、`ADC_Stop` 门控、`ADC_EnableInt`+EA → 向量 19 派发 + ISR `ADC_ClearIntFlag`、XSFR LDO ADCLDO 0x80→0xF0→清位无 OOB、compare/trig/`ADC_ConfigAN63` smoke）；vendor exe 直跑 PASS，MSVC host mcs51 ctest 23/23（17 host + 6 wasm）、MinGW host 17/17。host-only（wasm 已由 cms8s_adc e2e 覆盖同一模型）。
- **v1 收窄（计划内偏差）**：AN63 内部通道返回 0；ADCLDO VSEL 不影响满量程；ADRESH/ADRESL 不挂读钩子；~~原厂 StdDriver `adc.c`（tier-b）不挂构建~~（**已收割，见上**）；完整 ADC_Ldo 例程（tier-c，需 system.h/gpio.h shim + 19 个 ISR 桩）延后 M6。

---

## 7. 运行时协程（Fiber）与入口调度

### 7.1 裸机阻塞循环与 Fiber 协程让出
由于 8051 用户代码天然在 `void main(void)` 中执行 `while(1)`，系统通过轻量协程将其包装为独立 Fiber：
- 在 `_nop_()`、延时函数以及带超时的轮询点调用 `sim_ctx_yield()`；
- 调度器配额监控：每次让出前记录微秒时间戳，若单次连续占用超过阈值（如 500µs），调度器强制切出，驱动 UniSim 物理模型和 Wasm 浏览器 UI。

> ⏱️ **UniSim 1:1 硬实时时间映射契约 (B2 闭环)**：
> 严格确立以宿主 `app_loop`（UniSim 物理微积分）为主时钟、51 Fiber 内部虚拟时钟为从时钟的主从分工体系。定义 **$1\text{ms 物理时间} = 1\text{ms 51 虚拟时间}$ 的严格 1:1 映射基准**；ADC0832/CMS8S ADC 等即时外设在 Trap 内瞬间完成（耗时 $0\mu\text{s}$），严防电热闭环物理时间尺度失真。详细数学推导详见 🔗 [时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md) §2。

---

## 8. 架构决议与验收门禁矩阵

### 8.1 架构决议 (ADR Alignment)

| 决议项 | 结论 | 理由与落地规则 |
| :--- | :--- | :--- |
| **AD-01** | **源码级零侵入** | 用户原生代码不修改，`interrupt N` 通过 CMake 正则清洗为 `WINK_ISR(N)`。 |
| **AD-04** | **强制 C++17 模式** | 用户代码强制 `-x c++ -std=c++17`，以支撑 `P1^0` 操作符重载与 C++17 内联变量（P0386R2）消除 ODR 重复定义。 |
| **AD-11** | **Framework 全量 .cpp 与 4 大 C 边界** | 消除内部混排，4 边界严格加 `extern "C"` 规避 Name Mangling 污染与虚表泄漏。 |
| **AD-12** | **整端口与边沿感知** | `WinkSfr` 必须实现 `diff` 边沿检测与读重构，覆盖整端口 Bit-Bang 路径。 |
| **AD-13** | **Trap 四大执行红线** | 严禁在 Trap 内调用延时与 `yield`，保证确定性同步状态迁移。 |

### 8.2 验收样例门禁

1. `test/mcs51/blinky.c`：Timer0 ISR 翻转 P1.0，波形周期符合定时器初值计算；
2. `test/mcs51/uart_printf.c`：`SBUF` 写入无缝重定向到 Host stdout 与 Wasm 控制台；
3. `test/mcs51/iron_ntc.c`：
   - 分别以 `sbit` 与 `整端口 RMW` 方式驱动 ADC0832 采样 NTC 码值；
   - 联动 UniSim `thermal_heater_plate` 插件，验证毫秒级控温切断与继电器吸合。
