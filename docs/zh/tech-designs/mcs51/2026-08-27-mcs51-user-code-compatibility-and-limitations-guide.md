# 8051 用户层业务代码兼容性红线与避坑手册 (User-Facing Coding & Migration Guide)

| 属性 | 内容 |
| :--- | :--- |
| **文档状态** | Formal Standard - 现行技术规范 |
| **基线版本与 ADR** | ADR-0012 (契约诚实), ADR-0070 (C++拦截), ADR-0071 (数据面代理), ADR-0072 (时钟与中断), ADR-0073 (CMS8S ADC), ADR-0076 (UART RX), ADR-0077 (准双向口) |
| **创建日期** | 2026-08-27 |
| **修订日期** | 2026-09-02 (全面对齐现行框架源码与 ADR-0077 架构) |
| **适用对象** | 8051 业务固件工程师、小家电应用迁移者、AI 嵌入式代码生成 Agent |
| **所属模块** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` |
| **底层设计规格书** | [总纲：8051 零侵入仿真拦截与 C++ 代理架构](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)<br>[数据面：SFR 影子代理、整端口 RMW 与边沿感知](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)<br>[时序面：时钟域与时序一致性规格书](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)<br>[主规划书：MCU 兼容方案规划](mcu-compat-plan.md) |

---

## 1. 手册定位与核心原则 (Executive Summary)

本手册专为使用 **WinkMicroOS 8051 仿真层（`frameworks/mcs51/`）** 的业务代码开发者与 AI 代码生成系统编写。

在 Host (Linux/macOS/Windows) 与 Wasm (WebAssembly 浏览器端) 仿真环境下，Wink 8051 兼容层通过 **现代 C++17 操作符重载、虚拟寄存器影子内存与 Level 2 即时陷阱（Instant Trap）** 技术，实现了对标准 Keil C51 业务代码的**源码级零侵入直接编译运行**。

然而，由于宿主采用现代编译器（GCC/Clang/MSVC/Emscripten）与单一平坦线性内存（Flat Memory）模型，原生 8051 单片机的部分**历史非标编译器方言、底层物理哈佛架构特异性以及机器周期级纳秒时序**无法也无需在功能仿真层完全复刻。遵循 [ADR-0012 契约诚实优于静默降级](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)，本手册全景式列出所有受限语法与业务避坑指南。

> [!TIP]
> **STRICT 双模式机制**：
> 框架支持通过 CMake 开关 `-DWINK_MCS51_STRICT=ON` 开启严苛模式。在 STRICT 模式下，任何调用未建模特性（如试图使用外部计数脉冲、Timer0 Mode 3 等）的行为均会直接触发断言（Assert Fail），便于在 CI 门禁与单元测试中精准捕获隐患；在 Release 模式下，框架采用 Rate-Limited 单次告警（Warn Once）并安全降级。

---

## 2. 语法与编译器方言规范 (Syntax & Dialect Restrictions)

### 2.1 位变量 `sbit` 语法规范（绝对与相对形式均支持）

* **实现事实**：
  框架在 [REGX52.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REGX52.H) 中定义了 `#define sbit inline WinkSbit`。
  `WinkSbit` 拥有 `constexpr WinkSbit(int abs_bit_addr)` 构造函数（[mcs51_proxy.hpp](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp)），能够自动从绝对位地址（如 `0x80~0xFF`）反推 SFR 基地址（`abs & 0xF8`）与 GPIO 端口号（`0x80/0x90/0xA0/0xB0 -> P0..P3`）。
  因此，**绝对位地址形式与相对位寻址形式（`REG^bit`）均获 100% 完整功能支持**，引脚陷阱与边沿通知均正常派发。
* **规则建议**：
  官方头文件自带的全部 64 个预定义 sbit（`P0_0` ~ `P3_7`, `CY`, `TF0` 等）均使用绝对位地址声明。业务代码中推荐使用相对位寻址（如 `sbit LED = P1^0;`），可读性更佳且不受芯片位地址映射差异影响。

```c
/* ✅ 完全支持：相对位寻址（推荐，语义清晰） */
sbit LED = P1^0;
sbit RELAY = P2^3;
sbit CY_FLAG = PSW^7;

/* ✅ 完全支持：绝对位地址形式（Keil 经典写法，框架官方头文件同款） */
sbit P1_0 = 0x90;
sbit TF0  = 0x8D;
sbit CY   = 0xD7;
```

---

### 2.2 严禁 1980 年代 K&R 风格函数定义 (Fatal)

* **受限原因**：用户 8051 源文件在编译进入宿主沙箱时，强制开启了 `-std=c++17` 模式以支撑位代理重载。C++ 语言标准自诞生起就**彻底废弃了无形参类型列表的 K&R C 语法**。
* **规则约束**：所有函数定义必须遵循 ANSI C (C89/C99) 标准原型声明。

```c
/* ❌ 严禁使用（K&R 风格函数定义，C++ 编译器直接报语法错误） */
void DelayMs(ms)
int ms;
{
    /* ... */
}

/* ✅ 正确写法（标准 ANSI C 函数原型） */
void DelayMs(unsigned int ms) {
    /* ... */
}
```

---

### 2.3 严禁未声明函数隐式调用 (Fatal)

* **受限原因**：经典 C89 允许在函数未做前向声明时直接调用，编译器默认推导其返回值为 `int`。在现代 C++17 规则下，未声明调用属于严重语法错误（`error: 'foo' was not declared in this scope`）。
* **规则约束**：任何函数调用前，必须在文件头部显式提供函数原型声明，或显式包含对应 `.h` 头文件。

```c
/* ❌ 严禁使用（未做前向声明直接在 main 中调用） */
void main(void) {
    InitTimer0(); // 编译报错：'InitTimer0' was not declared in this scope
}
void InitTimer0(void) { /* ... */ }

/* ✅ 正确写法（显式提供函数前向声明） */
void InitTimer0(void);

void main(void) {
    InitTimer0(); // 编译正常
}
void InitTimer0(void) { /* ... */ }
```

---

### 2.4 Keil 原生内联汇编隔离规范 (Fatal)

* **受限原因**：Keil 的 `#pragma asm ... #pragma endasm` 内嵌的是针对 Intel MCS-51 8 位核心的专有汇编助记符（`MOV`, `SJMP`, `CJNE` 等）。宿主 x86/x64/Wasm 编译器无法解析该机器码指令。预处理清理脚本（`mcs51_cleanup.py`）不剥除汇编块，直接送入宿主编译器将报错中断。
* **规则约束**：业务逻辑必须使用纯 C 实现。若特定驱动必须保留汇编供 Keil 真机编译，必须使用 Keil 预定义宏 `__C51__` 进行物理隔离。

```c
/* ❌ 严禁使用（原生 51 汇编直接裸写） */
void ResetWatchdog(void) {
    #pragma asm
    MOV 0xA6, #0x01  // 某些芯片的看门狗喂狗指令
    #pragma endasm
}

/* ✅ 正确写法（使用 Keil 预定义宏 __C51__ 隔离） */
void ResetWatchdog(void) {
#ifdef __C51__
    #pragma asm
    MOV 0xA6, #0x01
    #pragma endasm
#else
    // 仿真环境下由 WinkMicroOS 守护，空操作或调用 C 逻辑即可
#endif
}
```

---

### 2.5 字符串字面量转 `char*` 的类型规范 (Warning / Diagnostic)

* **受限原因**：在 8051 经典串口打印函数中，开发者习惯声明为 `void UART_SendString(char *s);`，并在调用时传入字符串字面量 `UART_SendString("OK\r\n");`。在 C++17 中，`"OK\r\n"` 严格属于 `const char[5]`，隐式截断为 `char*` 属于非法类型丢失。
* **规则约束**：虽然仿真层 CMake 已对用户源文件注入了 `-Wno-write-strings` 降级开关，但依然**强烈建议在业务函数签名中显式使用 `const char *`**，保证跨工具链严谨性。

```c
/* ⚠️ 不推荐（虽可编译但属于陈旧 C 弱类型写法） */
void UART_SendString(char *str);

/* ✅ 强烈推荐（标准只读字符串指针） */
void UART_SendString(const char *str);
```

---

### 2.6 源码多字节字符编码（UTF-8 与 GBK 回退机制）

* **受限原因**：国内 8051 教学与工程源码历史上有大量文件采用 Windows ANSI (GBK / GB2312) 编码。GBK 的部分汉字（如“续”、“功”、“筹”等）次字节编码恰好是 `0x5C`（ASCII 中的反斜杠 `\`）。若单行注释以该字结尾：`// 执行延时状态持续\`，现代编译器按字符拼接预处理时会将下一行代码吃掉。
* **框架自愈与工具支持**：
  1. `mcs51_cleanup.py` 内部实现了 **UTF-8 优先、GBK 回退** 的自动解码转码管道（`read_source()`），生成给沙箱编译的 `.cpp` 副本始终规范化为 UTF-8；
  2. 针对供应商 GBK 头文件，工具提供 `--transcode` 模式进行无损编码转换：`python mcs51_cleanup.py --transcode <input.h> <output.h>`。
* **编写建议**：建议用户源文件统一保存为 UTF-8 编码，或在中文注释末尾追加空格/句号。

---

### 2.7 `bdata` 与 `sfr16` 方言限制

* **受限原因**：
  Keil C51 的 `bdata`（位寻址 RAM 区）与 `sfr16`（16 位 SFR 组合）是专用编译器扩展。框架的通用头文件未对 `bdata` 和 `sfr16` 进行宏抹除。
* **规则约束**：
  1. 避免使用 `bdata` 声明位寻址 RAM 变量，改用标准 C 位域（`struct { unsigned int f0:1; ... }`）或通过宏/位掩码操作普通 `uint8_t` 变量；
  2. 避免使用 `sfr16`，改用标准的双 8 位字节寄存器分别赋值（如分别操作 `TH0` 与 `TL0`）。

---

## 3. 存储器模型与寻址限制 (Memory Model & Pointers)

### 3.1 哈佛架构独立物理地址空间并存假设不予模拟 (Core Concept)

* **受限原因**：真实 8051 硬件属于物理哈佛架构，`DATA` (0x00~0x7F)、`IDATA` (0x00~0xFF)、`XDATA` (0x0000~0xFFFF) 和 `CODE` (0x0000~0xFFFF) 拥有物理隔离的地址总线与访问指令（`MOV`, `MOVX`, `MOVC`）。
  但在 Host 与 Wasm 仿真沙箱中，进程属于单一平坦线性内存（Flat Memory）。仿真层头文件通过预处理宏：
  ```c
  #define data
  #define idata
  #define xdata
  #define pdata
  #define code const
  ```
  将限定符抹除为空，变量统一在宿主线性 RAM 中分配。
* **规则约束**：
  1. 业务代码写 `xdata unsigned char buf[1024]` 仅用于突破 8051 片内 128B RAM 的硬件限制，仿真器透明支持；
  2. **严禁依赖哈佛架构地址数值并存的黑客代码**：不能假设 `data 0x20` 与 `xdata 0x20` 是两个物理独立不同的变量。

---

### 3.2 禁止通过计算地址裸指针访问具名 SFR (Fatal)

* **受限原因**：
  1. **8051 硬件体系原理**：8051 的 SFR 寄存器仅能通过**直接寻址**（`Direct Addressing`）访问；使用间接寻址指针（如 `idata *` 访问 0x80~0xFF）在真实硬件上访问的是 8052 片内高 128 字节 RAM，而不是 SFR 寄存器。
  2. **仿真拦截原理**：8051 的具名寄存器（`P0`~`P3`, `TCON`, `SCON` 等）在仿真层中被封装为带有 `operator=` 属性的 C++ 代理类（`WinkSfr`），利用赋值语句捕获跳变并触发 Level 2 硬件即时陷阱。
  若使用裸指针计算绝对地址访问（例如 `*(unsigned char *)0x90 = 0x55;`），CPU 仅向宿主线性内存写了 1 字节，**完全绕过了 C++ 代理的操作符重载，导致仿真器无法感知引脚跳变，外设陷阱彻底瘫痪**。
* **规则约束**：访问特殊功能寄存器必须直接使用具名标识符。

```c
/* ❌ 严禁使用（裸指针计算地址写入 SFR，绕过代理层） */
*(unsigned char *)0x90 = 0x01; // 试图点亮 P1.0，代理失效！

/* ✅ 正确写法（直接使用具名变量触发操作符重载） */
P1 = 0x01;
P1_0 = 1;
```

---

### 3.3 不支持 `_at_` 绝对地址变量绑定 (Fatal)

* **受限原因**：Keil C51 支持 `unsigned char xdata LCD_PORT _at_ 0x8000;` 将变量强制物理绑定到总线地址。宿主 GCC/Clang 编译器无此语法扩展，仿真头文件中 `#define _at_(addr)` 将其抹除为空。
* **规则约束**：宏抹除后变量将由链接器自由分配于 BSS/Data 段。依赖固定总线地址显存映射的代码不支持，需改用外设驱动函数封装。

```c
/* ❌ 不支持（_at_ 绝对地址段定位） */
unsigned char xdata DispBuffer[128] _at_ 0x2000;

/* ✅ 正确写法（普通全局数组） */
unsigned char DispBuffer[128];
```

---

### 3.4 不支持假设 Generic 3 字节胖指针内部结构

* **受限原因**：Keil C51 的通用指针为 3 字节结构（第 0 字节为存储区类型 Tag：0x01 为 IDATA，0x02 为 XDATA，0xFF 为 CODE；第 1-2 字节为偏移量）。在 Host (64位) 与 Wasm (32位) 环境下，所有指针均为原生平坦裸指针。
* **规则约束**：严禁通过联合体（Union）或类型强转去读取指针内部的 Tag 字节。

---

### 3.5 资源容量与内存预算声明（防范“仿真通、真机爆”）

> [!WARNING]
> **反向失真防范：内存预算不设防**
> 在仿真沙箱中，变量分配在宿主虚拟内存中。代码中声明过大的全局数组（如 300 字节的 `data` 数组，或 128KB 的 `xdata` 数组）在仿真中编译与运行均正常，但烧录到只有 128B/256B RAM 或 64KB XDATA 的物理芯片上时会直接编译失败或堆栈溢出。

* **受控受检窗口**：仅通过 `absacc.h` 访问的 `XBYTE[]` / `XWORD[]` 总线空间受 `WINK_MCS51_XDATA_SIZE`（默认 8KB，可配置 4~16KB）边界检查约束，越界访问在 STRICT 模式下断言，在 Release 模式下告警并返回 0xFF。
* **规则约束**：业务工程师必须以 Keil 生成的 `.map` 文件或物理 MCU 数据手册为准把关 RAM/ROM 预算，不可将仿真通过作为容量合规依据。

---

## 4. 外设行为、时钟与时序模型限制 (Peripherals & Timing)

### 4.1 仿真时钟分辨率与微步推进（`5µs` 粒度）

* **模型事实**：
  仿真器属于**功能级与时钟步进级**仿真。[wink_mcs51_clock.h](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_clock.h) 定义了 `#define WINK_MCS51_MICROSTEP_US 5u`。每个拦截点（SFR 读写或 `_nop_()`）按 5µs 推进虚拟时钟。
* **规则约束**：
  依赖在 C 代码中数 `_nop_` 产生极高频精确纳秒脉冲的非标协议（如单线驱动 WS2812 幻彩灯带、无外设时钟的纯软件严格 1µs 1-Wire 跳动）在仿真器中无法纳秒级保真。小家电测温建议统一采用标准 ADC 芯片或中微片内 ADC。

---

### 4.2 不支持连续模拟 RC 充放电测温 (Analog Limit)

* **受限原因**：小家电极低成本方案中有使用单片机 GPIO 配合电容充放电时间反推 NTC 阻值。UniSim 物理总线目前对 8051 GPIO 呈现为离散的数字逻辑电平（高 1 / 低 0），不建立纳秒级模拟 RC 电压积分与反相门逻辑电平阈值穿越模型。
* **规则约束**：小家电温控（如电水壶、养生壶）测量温度，**必须挂接 ADC**：
  1. 经典 89C52 方案：外挂 **ADC0832** 芯片（框架自带驱动模型 [ADC0832.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/ADC0832.H)）；
  2. 增强型 51 方案：使用 **中微 CMS8S** 等集成片内 12-bit ADC 的芯片。

---

### 4.3 不支持算术运算后依赖 `PSW` 硬件标志位 (ALU Flag)

* **受限原因**：8051 汇编在 `ADD A, R0` 后会自动更新硬件 `PSW` 寄存器中的 `CY` (进位)、`OV` (溢出)、`AC` (辅助进位)、`P` (奇偶校验)。用户业务 C 代码在经过宿主编译器优化后，数学运算被直接映射为 x86/x64 或 Wasm 本地算术指令，**不会同步将进位反映到虚拟 `PSW` 寄存器影子中**。
* **规则约束**：纯 C 语言编写的业务算法，判断溢出请直接使用 C 语言原生逻辑表达式，严禁在 C 语言加减法后去读取 `CY` 标志。

```c
/* ❌ 严禁使用（C 加法后试图依赖硬件 PSW.CY 标志） */
uint8_t a = 200, b = 100, c;
c = a + b;
if (CY) { // 错误：CY 标志未被宿主 ALU 算术驱动更新
    HandleOverflow();
}

/* ✅ 正确写法（纯 C 逻辑判断溢出） */
uint8_t a = 200, b = 100, c;
if ((uint16_t)a + b > 255) {
    HandleOverflow();
}
c = a + b;
```

---

### 4.4 CMS8S78xx 片内 ADC：按原厂寄存器图即时转换（ADR-0073）

* **模型事实**：中微 **CMS8S78xx** 片内 12-bit ADC 按原厂真实寄存器图建模：`ADCON0@0xDF`（bit1 **ADGO** 写 1 启动、硬件完成后自清零；bit6 **ADFM** 0=左对齐/1=右对齐）、`ADCON1@0xDE`（bit7 **ADEN** 使能）、`ADCCHS@0xD9`（通道号 0~25）、结果寄存器 `ADRESH@0xDD`/`ADRESL@0xDC`；转换完成中断为 `EIE2/EIF2` bit4 + Keil `interrupt 19`。写 ADGO 的同一条语句内转换即同步完成（0 周期穿透），忙等循环首次判断即退出。
* **XSFR 访问规范**：
  中微芯片的 XSFR 寄存器（如 `ADCLDO@0xF692`、引脚复用 `PxxCFG@0xF000`）在物理芯片上位于 MOVX/xdata 空间。原厂头文件宏定义 `*(unsigned char volatile xdata *)0xF692` 在宿主环境下会因 `xdata` 抹空退化为绝对裸指针解引用而崩溃。
  **框架解决方案**：框架提供了专用头文件 [REG_CMS8S.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REG_CMS8S.H)，内部使用 `xsfr ADCLDO(0xF692);`（`WinkXsfr` 代理）安全映射到受检 XSFR 窗口。

```c
/* ✅ 标准 CMS8S78xx 片内 ADC 采集范式（右对齐，包含 REG_CMS8S.H） */
#include <REG_CMS8S.H>

uint16_t Read_ADC_Channel(uint8_t ch) {
    ADCCHS = ch;             // 选择通道
    ADCON1 |= 0x80;          // ADEN = 1，使能 ADC
    ADCON0 = 0x40;           // ADFM = 1，右对齐
    ADCON0 |= 0x02;          // ADGO = 1，启动转换
    while (ADCON0 & 0x02) { ; } // 忙等：ADGO 硬件自清，首轮即退出
    return (uint16_t)(0x0FFF & ((ADRESH << 8) | ADRESL)); // 标准右对齐读取 (0~4095)
}
```

---

### 4.5 GPIO 准双向口模型与整端口 RMW 语义（ADR-0077）

* **模型机制**：
  8051 的 P0~P3 端口为经典的准双向口（Quasi-Bidirectional）。框架在 [mcs51_proxy.hpp](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp) 中严格复刻了真实硅片行为：
  1. **初始化播种**：系统初始化时自动为 P0~P3 锁存器赋 `0xFF`，并向 PinArbiter 播种 `WEAK-HIGH` 弱上拉状态；
  2. **读引脚 vs 读锁存器**：直接读端口（`val = P1;`）读取外部引脚真实电平（Read-Pin）；
  3. **读-改-写 (RMW) 语义**：所有复合赋值操作（`P1 |= 0x01;`, `P1 &= ~0x02;`, `P1 ^= 0x04;`, `P1++` 等）**严格读取锁存器影子（Read-Latch）**，与真机 `ORL`/`ANL` 指令行为完全一致，彻底杜绝了外部被下拉为 0 的引脚被误写回锁存器导致的锁死问题；
  4. **驱动强度映射**：锁存器写 1 输出弱上拉（`MCS51_DRIVE_WEAK`），写 0 输出强下拉（`MCS51_DRIVE_SUPPLY`）。
* **业务习惯**：
  标准 Keil 输入习语（输入前向端口写 1：`P1_0 = 1;` 或 `P1 = 0xFF;` 释放引脚）在仿真中完全保真，无需任何修改。

---

### 4.6 UART 串行通信模型与时序语义（ADR-0065 / ADR-0076）

* **发送端模型**：
  对 `SBUF` 寄存器的写操作（`SBUF = c;`）会立即将字节同步派发至控制台输出、内存捕获缓冲区以及 live UARTBus 路由，并在**同一调用返回前同步置位 `TI` 发送完成标志（SCON.1）**。
  经典发送等待循环 `while(!TI); TI = 0;` 在仿真中首轮判断即通过，零延迟无阻塞。
* **接收端模型**：
  外部串口数据（如通过 `wink_mcs51_uart_rx_push()` 注入）进入接收等待队列（Pending FIFO），在 Fiber 协程的微步拦截点同步排空。
  接收模型严格遵循 `SCON.REN` 使能位；接收到字节后自动置位 `RI`（SCON.0）并在使能时触发中断向量 4；若前序 `RI` 尚未被软件清零，新到字节将被丢弃，如实模拟 8051 无硬件 FIFO 的溢出特性。
* **时序与波特率边界**：仿真层不建立纳秒级波特率波形，字节传输为逻辑事件流，空闲帧间隔按真实虚拟时钟推进。

---

### 4.7 未建模外设与模式清单

根据契约诚实原则，以下硬件特性在功能仿真层未予建模，代码中涉及应注意避坑：
1. **Timer2**：`REGX52.H` 中未声明 `T2CON`、`RCAP2L`、`RCAP2H`、`TL2`、`TH2`，全框架未提供 Timer2 计数模型。请统一使用 Timer0 或 Timer1；
2. **Timer0 Mode 3（双 8 位独立分拆模式）**：Timer0 在 Mode 3 下保持空闲，STRICT 模式下触发 `MCS51_FEAT_TIMER_MODE3` 断言；
3. **Timer 外部 C/T 引脚脉冲计数**：`TMOD` 中配置 `C/T = 1` 时无外部脉冲源，定时器保持空闲，STRICT 模式下触发 `MCS51_FEAT_TIMER_EXT_CLK` 断言。

---

## 5. 中断体系与保真度陷阱 (Interrupt System & Fidelity Pitfalls)

### 5.1 ISR 语法转换与自动注册

* **Keil 原生 ISR 语法**：`void Timer0_ISR(void) interrupt 1 [using 1]`
* **转换机制**：构建工具 `mcs51_cleanup.py` 在编译前自动将用户函数头重写为 `WINK_ISR(N)`，`using M` 寄存器 Bank 切换语法被正则安全剥除；
* **C++ 自动注册宏**：宏 `WINK_ISR(N)` 在全局静态初始化期自动向向量表注册函数指针，POD 表在 BSS 段清零，不受静态初始化顺序（Static Initialization Order Fiasco）影响；
* **向量容量**：框架向量表支持 **28 项中断向量**（[wink_mcs51_isr.h](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_isr.h)），完全覆盖标准 8051 向量（0=外部中断0, 1=Timer0, 2=外部中断1, 3=Timer1, 4=UART）以及中微 CMS8S 等增强向量（如 interrupt 19 = ADC 转换完成中断）。

---

### 5.2 调度模型与执行门控

1. **同步派发**：中断在 Fiber 协程上由外设事件（如定时器溢出、ADC 完成）同步调用，**不建立抢占式多线程与嵌套中断模型**，`IP` 优先级寄存器不建模；
2. **运行相位门控**：在框架初始化完成前，中断派发处于全局关闭状态（`s_interrupts_enabled = false`），确保静态注册期与加载期绝不触发 ISR；
3. **ISR 期间禁止 Yield**：在 ISR 执行期间，`wink_mcs51_in_isr()` 为 true，虚拟时钟计费但不进行协程 Yield，确保中断执行的原子性。

---

### 5.3 🚨 核心陷阱：`volatile` 反向失真警告 (Simulation False-Pass)

> [!CAUTION]
> **极其致命的“仿真过、真机死”反向失真！**
> 在真实 8051 单片机上，主循环与 ISR 共享的全局标志位**必须显式声明为 `volatile`**。若漏写 `volatile`，Keil 编译器在优化时会将变量读操作提升（Hoist）出 `while(!flag)` 循环，导致真机死循环卡死。
> 
> 然而在仿真环境中，ISR 是通过外部函数指针 `wink_mcs51_dispatch_vector()` 进行同步调用的。由于宿主 C++ 编译器（GCC/Clang/MSVC）在主循环遇到未知外部函数调用时，不敢跨调用假设全局变量不变，因此即使**漏写了 `volatile`，在仿真器中也能正常读取并跳出循环**！
> 
> **规则红线**：所有在 ISR 中修改并在主循环中轮询读取的变量，**必须无条件声明为 `volatile`**！

```c
/* ❌ 极度危险（漏写 volatile：仿真正常运行，真机编译优化后死循环！） */
uint8_t timer_flag = 0; // 错误：缺少 volatile

void Timer0_ISR(void) interrupt 1 {
    timer_flag = 1;
}

void main(void) {
    InitTimer0();
    while(!timer_flag); // 真机上编译为死循环！
}

/* ✅ 正确写法（显式修饰 volatile，双端绝对安全） */
volatile uint8_t timer_flag = 0; // 正确

void Timer0_ISR(void) interrupt 1 {
    timer_flag = 1;
}

void main(void) {
    InitTimer0();
    while(!timer_flag); // 仿真与真机均 100% 正常
}
```

---

### 5.4 🚨 核心陷阱：局部变量静态覆盖与不可重入踩踏 (Static Overlay Corruption / Keil L15)

> [!CAUTION]
> **极其隐蔽的“仿真栈独立、真机踩内存”反向失真！**
> 8051 硬件堆栈深度极浅（通常仅数十字节），Keil C51 编译器默认**不使用动态调用栈分配局部变量与传递参数**，而是通过链接器进行静态覆盖分析（Static Data Overlaying），将调用关系互斥的函数局部变量固定复用在片内 `DATA/IDATA` 绝对地址上。
> 
> 在 Host 与 Wasm 仿真环境下，所有函数调用均运行在宿主现代操作系统的原生动态栈（Native Stack Frame）上，即使同一个函数被 `main` 主线程与中断 ISR 同时调用，各自拥有独立的栈帧空间，**仿真测试 100% 运行正常**。
> 
> 然而烧录到物理单片机后，如果 `main` 主循环正在执行某子函数时突发中断，而该中断服务函数（或其子调用树）也调用了该同名子函数，中断将**直接强行覆写破坏 `main` 正在使用的静态局部变量内存**！中断返回后，主循环局部变量已被污染，导致偶发性严重死机或计算错误。Keil 编译时仅报出容易被忽视的 `WARNING L15: MULTIPLE CALL TO SEGMENT`。
> 
> **规则红线**：
> 1. **严禁在 ISR 调用树与主循环调用树中同时调用同一个非 `reentrant` 普通子函数**；
> 2. 中断与主循环公用的逻辑应拆分为两个独立命名的函数，或在主循环调用处使用关中断（`EA = 0; ... EA = 1;`）保护，或显式声明为 Keil `reentrant`（需注意仿真层宏擦除与真机模拟栈开销）。

```c
/* ❌ 极度危险（公共函数被 main 与 ISR 同时调用：仿真完美通过，真机局部变量被踩踏！） */
uint16_t CalcCRC(const uint8_t *buf, uint8_t len) {
    uint16_t crc = 0xFFFF;
    uint8_t i; // Keil 静态分配在固定 DATA 地址
    for (i = 0; i < len; i++) {
        crc ^= buf[i];
    }
    return crc;
}

void Timer0_ISR(void) interrupt 1 {
    uint16_t isr_crc = CalcCRC(isr_buf, 4); // 中断触发，覆写了 main 正在使用的 crc 与 i！
}

void main(void) {
    while(1) {
        uint16_t main_crc = CalcCRC(main_buf, 16); // 执行途中被打断，局部变量直接损坏！
    }
}

/* ✅ 正确写法 1（拆分为独立命名的函数，静态地址空间物理隔离） */
uint16_t CalcCRC_Main(const uint8_t *buf, uint8_t len) { /* ... */ }
uint16_t CalcCRC_ISR(const uint8_t *buf, uint8_t len)  { /* ... */ }

/* ✅ 正确写法 2（在主循环调用处包裹临界区关中断保护） */
EA = 0;
main_crc = CalcCRC(main_buf, 16); // 保证执行期间绝不被 ISR 抢占
EA = 1;
```

---

### 5.5 跨中断多字节共享变量的 8 位读写撕裂 (Data Tearing)

* **受限与失真机理**：
  8051 是纯 8 位总线 CPU，读取或写入多字节变量（如 16 位 `uint16_t`/`int` 或 32 位 `uint32_t`/`long`）需要执行多条单字节汇编指令（如连续两条 `MOV`）。
  若主循环正在读取一个 16 位共享计时变量 `g_sys_ticks`，刚读完低 8 位后突发定时器中断，ISR 将 `g_sys_ticks` 从 `0x00FF` 递增为 `0x0100` 并退出；主循环接着读取高 8 位（读到 `0x01`），最终拼凑出错误的 `0x01FF`（产生了整整 256ms 的严重跳变脏数据）。
  而在 32 位/64 位 Host 与 Wasm 仿真沙箱中，多字节读写属于原生单指令原子操作，仿真无法暴露这种硬件读写撕裂。
* **规则约束**：
  主循环在读取由 ISR 更新的多字节共享全局变量时，**必须显式使用关中断临界区保护（`EA = 0; ... EA = 1;`）**，或采用连续双重读取一致性校验算法。

```c
/* ❌ 存在撕裂隐患（8 位单片机读取 16 位变量非原子：仿真正常，真机偶发离谱脏数据） */
volatile uint16_t g_ms_ticks = 0;

void Timer0_ISR(void) interrupt 1 {
    g_ms_ticks++;
}

void main(void) {
    uint16_t now;
    while(1) {
        now = g_ms_ticks; // 8 位总线需分两次读取，在 0x00FF->0x0100 进位临界点会读出 0x01FF！
        ProcessTask(now);
    }
}

/* ✅ 正确写法（加关中断临界区保护原子性读取） */
void main(void) {
    uint16_t now;
    while(1) {
        EA = 0;
        now = g_ms_ticks; // 关中断期间安全原子读取 16 位变量
        EA = 1;
        ProcessTask(now);
    }
}
```

---

## 6. 工程结构与协程调度纪律 (Engineering & Coroutine Health)

### 6.1 头文件包含与符号重映射

* **框架全自动自愈机制**：
  为了将用户的 `void main(void)` 裸机函数透明重映射为协程任务入口，[REGX52.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REGX52.H) 内部声明了 `#define main wink_mcs51_user_main`。
  框架在 `REGX52.H` 最顶层**前置安全拉入了常用的基础系统头文件**（`<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`, `<stdio.h>`），全部位于 `#define main` 展开之前。
* **用户体验（包含顺序自由）**：
  用户可以完全按习惯任意顺序书写头文件包含指令：

```c
/* ✅ 完全支持：先包含 REGX52.H，再包含 stdio.h */
#include <REGX52.H>
#include <stdio.h>

void main(void) {
    printf("Ready!\r\n");
}
```

---

### 6.2 裸机死循环与微步协程配额 (Catch-Up 机制)

* **调度模型事实**：
  系统底层使用轻量协程（Fiber）执行 8051 用户代码。
  框架配置了 **10ms 虚拟时间切片配额**（`WINK_MCS51_QUOTA_US = 10000u`，对齐 100Hz Master Tick）。
  当用户代码在主循环中消耗满 10ms 虚拟时间后，Fiber 会自动执行 0 延时主动让出（`pal_os_sleep_ms(0)`）交还主调度器，并在恢复执行时执行 **Catch-Up 补账机制**（推进虚拟定时器并派发溢出中断），保证虚拟时间与物理 Tick 严格 1:1 守恒。
* **业务编码规范**：
  业务主循环内部必须包含正常的业务逻辑（按键轮询、状态机推进、延时等待等），避免毫无物理意义的纯空转。

```c
/* ✅ 正确写法（包含正常的微步推进或延时交互） */
void main(void) {
    InitHardware();
    while(1) {
        ScanKeys();
        UpdateState();
        delay_ms(10); // 主动让出 Fiber 协程，物理世界平滑积分
    }
}
```

---

## 7. 常见编译排错与运行时诊断速查表 (Quick Troubleshooting Matrix)

| 典型编译器报错 / 运行时告警 | 触发代码原因 | 对应本手册章节 | 1 分钟快速修复方法 |
| :--- | :--- | :--- | :--- |
| `error: 'InitTimer0' was not declared in this scope` | 函数未声明直接在上方调用 | **§2.3** | 在文件开头补充 `void InitTimer0(void);` 原型声明 |
| `error: expected ';' before 'asm'` | 代码中直接包含 `#pragma asm` 汇编块 | **§2.4** | 用 `#ifdef __C51__` 宏包裹隔离该段汇编 |
| `error: invalid conversion from 'const char*' to 'char*'` | 字符串字面量传给普通 `char*` | **§2.5** | 将函数形参修改为 `const char *str` |
| `warning: multi-line comment [-Wcomment]` | GBK 汉字末尾包含反斜杠 `\` 吞掉下一行代码 | **§2.6** | 将源文件保存为 UTF-8 编码，或在注释后加空格 |
| `error: unknown type name 'bdata' / 'sfr16'` | 使用了 Keil 专有未擦除方言关键字 | **§2.7** | `bdata` 改为普通变量+位掩码；`sfr16` 改为分别操作高低字节 |
| `error: 'T2CON' was not declared in this scope` | 使用了 8052 Timer2 相关寄存器 | **§4.7** | 框架未建模 Timer2，改用 Timer0 或 Timer1 |
| `WINK_WARN_WCET_EXCEEDED` (运行时告警 8002) | 裸机代码中出现单次执行超过 5,000µs (5ms) 的死等 | **§6.2** | 在密集轮询体中补充 `_nop_()` 或 `delay_ms()` 让出 |
| **真机死循环、仿真却正常运行** | ISR 与主循环共享变量漏写 `volatile` | **§5.3 (🚨红线)** | 为共享全局变量补充 `volatile` 修饰符 |
| **真机偶发死机/计算错乱，仿真测试全过** | ISR 与主循环并发调用了同一普通函数，发生局部变量覆盖 | **§5.4 (🚨红线)** | 拆分为独立命名的函数，或在主循环调用处加 `EA=0/EA=1` 保护 |
| **真机多字节变量偶发读出离谱大数/脏数据** | 主循环读取 16/32 位跨中断共享变量时未关中断（数据撕裂） | **§5.5** | 主循环读取前加 `EA=0`，读取后加 `EA=1` 保护 |

---

## 8. 仿真保真度边界总表 (Simulation Fidelity Boundary Matrix)

| 领域分类 | 仿真层行为级高保真（可完全信赖 ✅） | 简化 / 不建模 / 物理特异性（不可依赖 ⚠️） |
| :--- | :--- | :--- |
| **GPIO / 端口** | • 准双向口初始态（0xFF + WEAK-HIGH）<br>• 引脚电平读取（Read-Pin）<br>• 整端口复合赋值 RMW 严格读锁存器（Read-Latch）<br>• 边沿感知与 UniSim PinArbiter 即时事件派发 | • 模拟 RC 连续电压充放电积分<br>• 纳秒级引脚瞬态过冲与门电路穿越时延 |
| **时钟与指令** | • 5µs 步进微步计时<br>• 10ms 虚拟时间配额切片与 Catch-Up 补账<br>• 标准 `delay_ms()` 毫秒级时间守恒 | • 12-T 单机器周期纳秒级时序（WS2812 数 nop 协议）<br>• 指令级执行时钟差异 |
| **ADC 采样** | • CMS8S78xx 片内 ADC 原厂寄存器图（ADCON0/ADFM/ADGO）<br>• ADC0832 外部 SPI 芯片时序模型<br>• 0 周期即时转换与结果装载 | • 内部参考源 AN63（BGR/内部温度）未建模<br>• 连续采样转换建立时间（Sample/Hold time） |
| **UART 串口** | • SBUF 写入同步触发 TI<br>• RX Pending FIFO 微步拦截点排空<br>• SCON.REN 门控与 Vector 4 派发<br>• 无 FIFO 溢出丢包模拟 | • 物理波特率发生器纳秒时序波形<br>• 奇偶校验错误（Parity Error）硬件触发 |
| **中断系统** | • 28 项中断向量注册与同步派发<br>• 运行相位门控（初始化期安全闭锁）<br>• Timer0/Timer1 溢出自动触发 | • 中断抢占与嵌套（Nested ISR）<br>• `IP` 中断优先级<br>• `using M` 寄存器 Bank 物理切换<br>• Keil 静态覆盖（Static Overlay）局部变量重叠踩踏（仿真原生栈无法暴露） |
| **内存与算术** | • 平坦线性 RAM 访问透明映射<br>• `XBYTE[]` 8KB 受检窗口越界防护<br>• 标准 C 算术逻辑完全自洽 | • 原生哈佛物理地址独立并存假设<br>• C 算术运算后更新硬件 `PSW`（CY/OV/AC/P）标志<br>• 编译期 RAM/ROM 容量超限拦截（需看 Keil map）<br>• 漏写 `volatile` 的变量优化行为（仿真无法暴露）<br>• 8-bit CPU 访问 16/32 位变量的非原子数据撕裂（仿真 32/64 位原子操作无法暴露） |

---

## 9. 结语与合规判定

遵循上述规范的 8051 业务代码，能够做到：
1. **在 Keil C51 中**：直接编译烧录进真实芯片（如 AT89C52、中微 CMS8S 等），物理功能完全正常；
2. **在 Wink 仿真环境中**：零侵入直接通过 Host 与 Wasm 编译，无缝在浏览器与 CI 自动化门禁中高保真运行，与虚拟物理模型完美闭环。
