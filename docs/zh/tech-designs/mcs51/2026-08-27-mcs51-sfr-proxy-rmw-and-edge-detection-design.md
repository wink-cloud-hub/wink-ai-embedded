# 8051 SFR 影子内存代理、整端口 RMW 语义与边沿跳变感知技术设计规格书

| 属性 | 内容 |
| :--- | :--- |
| **文档状态** | Draft - 方案深化与落实现行标准 (In-Progress) |
| **创建日期** | 2026-08-27 |
| **所属模块** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` |
| **上级总规格书** | [8051 零侵入仿真拦截与 C++ 代理架构](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md) |
| **主规划书** | [wink-micro-os 多系列 MCU 兼容方案](mcu-compat-plan.md) |
| **关联架构决议** | AD-1 (源码零侵入), AD-4 (C++ 模式构建), AD-12 (整端口与边沿感知), AD-13 (Trap 执行红线) |

---

## 1. 方案背景与问题陈述 (Problem Statement & A2 复盘)

### 1.1 经典 8051 端口访问范式
在 Keil C51 及各类经典 8051 嵌入式代码库中，开发者对 I/O 端口（`P0`、`P1`、`P2`、`P3`）和内部 SFR（如 `TCON`、`SCON`）的访问极度依赖以下四种代码模式：

1. **整字节直接写入**：
   ```c
   P1 = 0x0F;        // 低 4 位拉高，高 4 位拉低
   P0 = table[i];    // 驱动数码管/DAC 动态段码
   ```
2. **整字节状态读取（条件分支）**：
   ```c
   if (P3 & 0x04) { ... }  // 轮询 P3.2 按键状态
   uint8_t val = P1;       // 批量采集并行总线输入
   ```
3. **读-改-写 (Read-Modify-Write, RMW) 复合赋值**：
   ```c
   P1 |= 0x01;       // 仅置位 P1.0，保持其余引脚不变
   P1 &= ~0x02;      // 仅清零 P1.1
   P1 ^= 0x04;       // 翻转 P1.2
   P1++;             // 步进端口计数值 (INC P1)
   ```
4. **位级命名寻址与操作**：
   ```c
   sbit ADC_CS  = P1^2;
   sbit ADC_CLK = P1^1;
   sbit ADC_DI  = P1^0;
   ADC_CS = 1;
   ```

### 1.2 原架构方案 A2 缺陷深度复盘
在初期架构设计中，仅在 `include/mcs51_proxy.hpp` 中定义了针对 `sbit` 的位代理结构体 `WinkSfrBitProxy`（仅包含位级 `operator=` 和 `operator uint8_t()`），而忽视了整端口寄存器宿主 `WinkSfr` 的全量语义，导致如下致命缺陷：

- **缺陷 1：编译语法断裂**：用户代码中海量的 `P1 |= 0x01`、`P1 = 0x55` 在 C++ 编译器（`-x c++`）下直接引发 `no match for 'operator|='`、`no match for 'operator='` 编译阻断；
- **缺陷 2：外设即时陷阱 (Write-Trap) 静默漏发**：若用户通过整端口赋值 `P1 |= 0x01` 或 `P1 = 0x0F` 改变了挂接在 P1.0 上的虚拟外设（如 ADC0832 DI），由于未经过带有边沿检测的端口代理，挂接在 Level 2 陷阱表中的状态机根本无法感知引脚翻转，导致 SPI/Bit-Bang 时序彻底死锁；
- **缺陷 3：读-改-写 (RMW) 硬件语义倒挂**：若简单将 `operator|=` 实现为 `*this = ((uint8_t)*this | rhs)`，其内部隐式调用 `operator uint8_t()`，会将“读外部物理引脚 (Read-Pin)”误作“读锁存器 (Read-Latch)”，造成外部拉低的输入引脚被错误写回锁存器并永久锁死。

本设计规格书作为 **A2 阻断项** 的单一事实来源（SSOT），系统性建立 8051 SFR 影子内存模型、完整的 C++ 操作符代数体系与差分跳变分发引擎。

---

## 2. 8051 准双向 I/O 硬件语义 vs 仿真代理映射

### 2.1 8051 准双向端口的电气微体系结构
8051 的 P0~P3 是经典的**准双向 I/O 端口（Quasi-Bidirectional I/O）**，每个引脚由一个**内部输出锁存器（Output Latch）**、一个**下拉场效应管（FET）**、**内部弱上拉电阻**和两个**输入缓冲器（读引脚缓冲器与读锁存器缓冲器）**组成：

```
                    +Vcc
                     |
                    [弱上拉]
                     |
内部数据总线 ----> [锁存器 Q] ---- Gate of FET (低电平时关断，高电平时导通接地)
                     |                |
                     |                +---- 物理外部引脚 (Pin)
                     |                |
                     v                v
                 [读锁存器]       [读引脚]
                 (Read-Latch)    (Read-Pin)
```

1. **输入模式要求**：
   当用户需要将某个引脚用作输入（读取按键或传感器）时，CPU 必须先向该引脚写入 `1`。此时锁存器 $Q=1$，下拉 FET 关断，外部引脚被弱上拉为高电平；外部器件只需将其拉至低电平即可被 CPU 感知。
2. **锁死风险**：
   若锁存器 $Q=0$，下拉 FET 强行导通接地，此时即使外部器件输出高电平，引脚也始终呈现低电平，输入功能被完全锁死。

### 2.2 核心硬件戒律：Read-Latch vs Read-Pin

8051 硬件指令集严格区分了两类端口读操作：

| 操作分类 | 代表汇编指令 / C 代码模式 | 硬件读取目标 | 仿真中的真实数据源 |
| :--- | :--- | :--- | :--- |
| **普通读取 (Read-Pin)** | `MOV A, P1`、`if (P3 & 0x04)`、`val = P1;` | **物理引脚电压**（反映外部真实电平） | `on_read` 动态外设电平，缺省取锁存器 |
| **读-改-写 (Read-Latch, RMW)** | `ORL P1, #01H`、`ANL`、`XRL`、`CPL P1.0`、`INC P1`、`P1 |= 0x01` | **内部输出锁存器**（严禁读取外部引脚） | **直接读取 `s_sfr_shadow[sfr_addr]`** |

#### 【仿真致命陷阱】为什么 RMW 绝不能调用 `operator uint8_t()`？
设想以下真实电热温控（如 ADC0832 / NTC）场景：
1. P1.3 为数据输出引脚（`ADC_DO`），系统初始化时向 P1 写入 `0xFF`，P1.3 锁存器为 `1`；
2. ADC0832 在转换期间将 P1.3 外部物理电平强行拉低到地（$0\text{V}$）；
3. 此时业务代码执行 `P1 |= 0x04;`（试图拉高 P1.2 的 `ADC_CS` 片选）；
4. **若发生错误实现**：`P1 |= 0x04` 若调用 `operator uint8_t()` 读取引脚，它读到了外部拉低的 P1.3 为 `0`；然后计算 `(value & ~P1.3) | 0x04`，再将包含 `P1.3 = 0` 的新字节写回 P1；
5. **灾难性后果**：P1.3 的锁存器被覆写为 `0`，内部 FET 永久导通至地！ADC0832 此后无论输出高电平还是低电平，CPU 读取 P1.3 永远都是 `0`，整个 ADC 驱动永久瘫痪。

> **架构黄金定律**：
> **`WinkSfr` 与 `WinkSfrBitProxy` 的所有复合赋值操作符（`|=`, `&=`, `^=`, `+=`, `-=`, `++`, `--`）的运算基准值，必须且只能来自于锁存器影子内存 `s_sfr_shadow[sfr_addr]`，严禁越俎代庖去读取外部引脚！**

---

## 3. `WinkSfr` 与 `WinkSfrBitProxy` 完整操作符代数系统

### 3.1 实体声明与内存布局

为实现 $O(1)$ 常数时间拦截，全局分配 256 字节连续锁存器影子数组及 $4 \times 8$ 的 POD 陷阱分发表：

```cpp
// 包含于 include/mcs51_trap.h (纯 C ABI 声明)
#ifdef __cplusplus
extern "C" {
#endif

// 全局 256 字节锁存器物理映射（0x80~0xFF 为 SFR，0x00~0x7F 保留）
extern uint8_t s_sfr_shadow[256];

typedef struct {
    void* write_ctx;
    void (*on_write)(void* ctx, uint8_t level);
    void* read_ctx;
    uint8_t (*on_read)(void* ctx);
} mcs51_pin_trap_t;

// P0 ~ P3，每端口 8 个引脚对应独立 Level 2 陷阱回调
extern mcs51_pin_trap_t s_pin_traps[4][8];

// 内部外设专用 SFR (port_idx >= 4，如 CMS8S ADCON、UART SBUF、Timer TCON) 读写钩子
typedef void (*mcs51_sfr_write_hook_t)(uint8_t addr, uint8_t old_val, uint8_t new_val);
typedef void (*mcs51_sfr_read_hook_t)(uint8_t addr);

extern mcs51_sfr_write_hook_t s_sfr_write_hooks[256];
extern mcs51_sfr_read_hook_t  s_sfr_read_hooks[256];

#ifdef __cplusplus
}
#endif
```

### 3.2 位代理类 `WinkSfrBitProxy` 规范实现

```cpp
// include/mcs51_proxy.hpp (节选)
struct WinkSfrBitProxy {
    uint8_t sfr_addr; // 真实物理 SFR 地址 (0x80, 0x88, 0x90, 0x98, 0xA8, 0xD0 等)
    uint8_t port_idx; // 0..3 (对应 P0..P3)；0xFF 代表非 GPIO 的位可寻址控制 SFR (TCON/SCON/IE/IP/PSW 等)
    uint8_t bit_idx;  // 0..7
    uint8_t bit_mask; // 1 << bit_idx

    // 1. 位写入 (Write Latch & Trigger Trap / SFR Hook)
    inline WinkSfrBitProxy& operator=(uint8_t val) {
        const uint8_t old_val = s_sfr_shadow[sfr_addr];
        const uint8_t old_bit = (old_val >> bit_idx) & 1;
        const uint8_t new_bit = val ? 1 : 0;
        const uint8_t new_val = new_bit ? (old_val | bit_mask) : (old_val & ~bit_mask);

        // 统一更新真实物理地址的锁存器影子 (无论 P0~P3 还是 TCON/SCON/IE)
        s_sfr_shadow[sfr_addr] = new_val;

        // 分支安全派发：严禁 port_idx >= 4 盲查 s_pin_traps 引发数组越界
        if (port_idx < 4) {
            // GPIO 引脚 (P0..P3)：仅在真实电平跳变时触发通知与陷阱
            if (old_bit != new_bit) {
                // 1. UniSim 3.0 通道 1：即时同步全局 PinArbiter (零延迟驱动前端 UI/Wokwi 动画)
                const uint16_t global_pin = (uint16_t)((port_idx << 3) | bit_idx);
                js_pal_gpio_write(global_pin, new_bit);

                // 2. 硬件即时陷阱 (Level 2 Instant Trap, 如 ADC0832 CS/CLK 等)
                auto& trap = s_pin_traps[port_idx][bit_idx];
                if (trap.on_write) {
                    trap.on_write(trap.write_ctx, new_bit);
                }
            }
        } else {
            // 非 GPIO SFR (TCON, SCON, ADCON 等)：触发外设专用 SFR 写入钩子
            if (old_bit != new_bit && s_sfr_write_hooks[sfr_addr]) {
                s_sfr_write_hooks[sfr_addr](sfr_addr, old_val, new_val);
            }
        }
        return *this;
    }

    // 2. 位自赋值
    inline WinkSfrBitProxy& operator=(const WinkSfrBitProxy& rhs) {
        return *this = (uint8_t)rhs;
    }

    // 3. 位读取 (Read-Pin / SFR 读钩子懒求值)
    inline operator uint8_t() const {
        if (port_idx < 4) {
            // GPIO 引脚：优先拉取外部动态引脚电平
            auto& trap = s_pin_traps[port_idx][bit_idx];
            if (trap.on_read) {
                return trap.on_read(trap.read_ctx) ? 1 : 0;
            }
        } else {
            // 非 GPIO 控制 SFR (如 TCON 上的 TF0/TR0)：先触发 SFR 读前钩子推进时序 (Lazy Evaluation)
            if (s_sfr_read_hooks[sfr_addr]) {
                s_sfr_read_hooks[sfr_addr](sfr_addr);
            }
        }
        // 读取真实物理地址对应影子寄存器中的当前位
        return (s_sfr_shadow[sfr_addr] & bit_mask) ? 1 : 0;
    }

    // 4. 位级 RMW 复合运算 (基准值严格读取真实物理锁存器)
    inline WinkSfrBitProxy& operator^=(uint8_t rhs) {
        uint8_t latch = (s_sfr_shadow[sfr_addr] >> bit_idx) & 1;
        return *this = (latch ^ (rhs & 1));
    }
    inline WinkSfrBitProxy& operator|=(uint8_t rhs) {
        uint8_t latch = (s_sfr_shadow[sfr_addr] >> bit_idx) & 1;
        return *this = (latch | (rhs & 1));
    }
    inline WinkSfrBitProxy& operator&=(uint8_t rhs) {
        uint8_t latch = (s_sfr_shadow[sfr_addr] >> bit_idx) & 1;
        return *this = (latch & (rhs & 1));
    }
};
```

---

### 3.3 整端口/SFR 代理类 `WinkSfr` 规范实现

```cpp
// include/mcs51_proxy.hpp (节选)
struct WinkSfr {
    uint8_t sfr_addr; // 真实物理地址 (0x80, 0x90, 0x88, 0x98, 0xA8, 0x8E 等)
    uint8_t port_idx; // 0..3 代表 P0..P3，0xFF 代表非 GPIO 的普通/位可寻址 SFR

    // 编译期静态构造函数：由 sfr 地址自动推断 port_idx
    constexpr WinkSfr(uint8_t addr)
        : sfr_addr(addr),
          port_idx((addr == 0x80) ? 0 :
                   (addr == 0x90) ? 1 :
                   (addr == 0xA0) ? 2 :
                   (addr == 0xB0) ? 3 : 0xFF) {}

    constexpr WinkSfr(uint8_t addr, uint8_t port)
        : sfr_addr(addr), port_idx(port) {}

    // sbit 异或初始化重载：支持 sbit LED = P2^0; 及 sbit TF0 = TCON^5;
    // 【核心修复】：必须完整透传真实物理地址 sfr_addr！
    constexpr WinkSfrBitProxy operator^(uint8_t bit_idx) const {
        return WinkSfrBitProxy{sfr_addr, port_idx, bit_idx, (uint8_t)(1 << bit_idx)};
    }

    // ========================================================================
    // 1. 整字节直接写入：diff 边沿感知 + 批量单发 / SFR 写入钩子
    // ========================================================================
    inline WinkSfr& operator=(uint8_t val) {
        const uint8_t old_val = s_sfr_shadow[sfr_addr];
        s_sfr_shadow[sfr_addr] = val;

        // 1. GPIO 端口 (P0~P3)：执行 diff 边沿感知与引脚即时分发
        if (port_idx < 4) {
            const uint8_t diff = old_val ^ val; // 提取跳变掩码
            if (diff == 0) {
                return *this; // 快路径：无任何电平翻转，零开销退出
            }

            // 对每个发生电平跳变的引脚，同步派发 UniSim 通道 1 通知与外设陷阱
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (diff & (1 << bit)) {
                    const uint8_t new_level = (val >> bit) & 1;
                    const uint16_t global_pin = (uint16_t)((port_idx << 3) | bit);

                    // 1. UniSim 3.0 通道 1：即时同步全局 PinArbiter
                    js_pal_gpio_write(global_pin, new_level);

                    // 2. 硬件即时陷阱 (Level 2 Instant Trap)
                    if (s_pin_traps[port_idx][bit].on_write) {
                        s_pin_traps[port_idx][bit].on_write(
                            s_pin_traps[port_idx][bit].write_ctx, new_level
                        );
                    }
                }
            }
        } else {
            // 2. 内部外设 SFR (port_idx >= 4，如 CMS8S ADCON、UART SBUF、Timer TCON)
            auto hook = s_sfr_write_hooks[sfr_addr];
            if (hook) {
                hook(sfr_addr, old_val, val);
            }
        }
        return *this;
    }

    inline WinkSfr& operator=(const WinkSfr& rhs) {
        return *this = (uint8_t)rhs;
    }

    // ========================================================================
    // 2. 整字节直接读取 (Read-Pin / SFR 读钩子)：动态重构引脚电平
    // ========================================================================
    inline operator uint8_t() const {
        if (port_idx < 4) {
            uint8_t val = s_sfr_shadow[sfr_addr];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (s_pin_traps[port_idx][bit].on_read) {
                    const uint8_t pin_lvl = s_pin_traps[port_idx][bit].on_read(
                        s_pin_traps[port_idx][bit].read_ctx
                    );
                    if (pin_lvl) val |= (1 << bit);
                    else         val &= ~(1 << bit);
                }
            }
            return val;
        } else {
            // 非 GPIO SFR：先触发读前钩子 (如读取 TCON 时刷新定时器)
            if (s_sfr_read_hooks[sfr_addr]) {
                s_sfr_read_hooks[sfr_addr](sfr_addr);
            }
            return s_sfr_shadow[sfr_addr];
        }
    }

    // ========================================================================
    // 3. RMW (读-改-写) 运算符全家族：严格读取锁存器影子！
    // ========================================================================
    inline WinkSfr& operator|=(uint8_t rhs) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] | rhs);
    }
    inline WinkSfr& operator&=(uint8_t rhs) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] & rhs);
    }
    inline WinkSfr& operator^=(uint8_t rhs) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] ^ rhs);
    }
    inline WinkSfr& operator+=(uint8_t rhs) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] + rhs);
    }
    inline WinkSfr& operator-=(uint8_t rhs) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] - rhs);
    }
    inline WinkSfr& operator<<=(uint8_t shift) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] << shift);
    }
    inline WinkSfr& operator>>=(uint8_t shift) {
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] >> shift);
    }

    // 自增自减
    inline WinkSfr& operator++() { // ++P1
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] + 1);
    }
    inline uint8_t operator++(int) { // P1++
        const uint8_t old = s_sfr_shadow[sfr_addr];
        *this = (uint8_t)(old + 1);
        return old;
    }
    inline WinkSfr& operator--() { // --P1
        return *this = (uint8_t)(s_sfr_shadow[sfr_addr] - 1);
    }
    inline uint8_t operator--(int) { // P1--
        const uint8_t old = s_sfr_shadow[sfr_addr];
        *this = (uint8_t)(old - 1);
        return old;
    }
};
```

---

## 4. `diff` 差分跳变检测与外设陷阱分发算法

### 4.1 算法流程图

```
                写入操作：P1 = val 或 P1 |= rhs
                               |
                               v
               old_val = s_sfr_shadow[sfr_addr]
               s_sfr_shadow[sfr_addr] = val
                               |
                               v
                     diff = old_val ^ val
                               |
                    +----------+----------+
                    |                     |
               (diff == 0)           (diff != 0)
                    |                     |
                    v                     v
                 直接返回         bit = 0 到 7 循环
                (零开销快路径)            |
                                          +--> (diff & (1 << bit)) 为 0 ? ----> 跳过 (保持静止)
                                          |
                                          +--> 为 1 且注册了 on_write ?
                                                   |
                                                   v
                                          调用 on_write(ctx, new_level)
                                          (仅触发跳变引脚的状态机)
```

### 4.2 典型外设时序推演（ADC0832 同端口共存场景）

假设外部 ADC0832 挂接在 `P1` 端口上：
- `ADC_CS  = P1^2`（片选，低有效）
- `ADC_CLK = P1^1`（时钟，上升沿采样）
- `ADC_DI  = P1^0`（数据输入）

| 执行语句 | `old_val` | `new_val` | `diff` | 跳变引脚 | 触发动作与防伪效果 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `P1 = 0xFF;` | `0x00` | `0xFF` | `0xFF` | P1.0, 1, 2 全部由 0→1 | CS/CLK/DI 均置高，ADC0832 进入初始待机态 |
| `P1 &= ~0x04;` | `0xFF` | `0xFB` | `0x04` | 仅 P1.2 (CS) 由 1→0 | **仅 CS 下降**，进入选中态。CLK 保持为 1，**绝对不产生虚假时钟跳变** |
| `P1 |= 0x01;` | `0xFB` | `0xFB` | `0x00` | 无任何引脚改变 | 命中 `diff == 0` 快路径，**零开销跳过** |
| `P1 |= 0x02;` | `0xFB` | `0xFD` | `0x02` | 仅 P1.1 (CLK) 由 0→1 | **触发 CLK 上升沿陷阱**，ADC0832 读取当前 DI 电平，移位寄存器步进 |

**结论**：`diff` 差分跳变算法完美保证了整端口批量写入与独立位操作在时序行为上的**严格全等性**。

### 4.3 整端口 RMW 与引脚级即时陷阱强隔离性测试 (Zero False-Trigger Test Vectors)

为了严密防御“整端口操作意外误触同端口其他外设引脚”的致命隐患（如改写 P1.0 状态指示灯时意外给 P1.1 继电器或 P1.2 ADC0832 CS 产生虚假跳变），在 CI 门禁用例 `test/mcs51/sfr_rmw_isolation_test.c` 中固化以下 4 套强隔离测试向量：

```
                           P1 端口 (8 个物理引脚)
   [Pin 7] [Pin 6] [Pin 5] [Pin 4] [Pin 3] [Pin 2] [Pin 1] [Pin 0]
      |       |       |       |       |       |       |       |
    Trap7   Trap6   Trap5   Trap4   Trap3   Trap2   Trap1   Trap0  (独立计数器)
```

#### 测试向量 1：单位置位隔离性 (Single Bit Set Isolation)
- **初始状态**：P1 = 0x00，8 个引脚 Trap 计数器归零；
- **操作语句**：`P1 |= 0x01;`（将 bit 0 置 1）；
- **断言要求**：
  - `trap_count[0] == 1`（仅目标引脚收到跳变通知）；
  - `trap_count[1..7] == 0`（其余 7 个无关引脚 Trap 触发计数严格为 0，零误触发）。

#### 测试向量 2：单位清零隔离性 (Single Bit Clear Isolation)
- **初始状态**：接上步，P1 当前为 0x01；
- **操作语句**：`P1 &= ~0x01;`（将 bit 0 清零）；
- **断言要求**：
  - `trap_count[0] == 2`（目标引脚再次正确翻转）；
  - `trap_count[1..7] == 0`（其余引脚全程保持静默）。

#### 测试向量 3：重复写入快路径静默断言 (Zero-Delta Fast Path)
- **初始状态**：接上步，P1 当前为 0x00；
- **操作语句**：再次执行 `P1 = 0x00;` 或 `P1 &= ~0x01;`；
- **断言要求**：
  - 触发 `diff == 0` 快速返回路径；
  - `trap_count[0..7]` 均未增加（全端口 8 个引脚增加量严格为 0）。

#### 测试向量 4：跨端口输入引脚锁存器保护 (Read-Latch vs Read-Pin 隔离)
- **初始状态**：P1 = 0xFF，其中 Pin 2 挂接外部按键并处于按下状态（外部动态 `on_read` 返回 0）；
- **操作语句**：用户执行 `P1 &= ~0x01;`（改写 Pin 0 为 0）；
- **断言要求**：
  - 底层 RMW 严格读取锁存器影子（0xFF），运算结果为 0xFE；
  - 写入后：Pin 0 变为 0，`trap_count[0] == 1`；
  - Pin 2 锁存器影子保持为 1，外部按键输入通道未被闭环破坏；
  - `trap_count[1..7] == 0`。

### 4.4 UniSim 3.0 通道 1 桥接与端口-引脚扁平化映射规范 (`global_pin = (port << 3) | bit`)

根据 [UniSim 3.0 数据面多通道路由规范](../../design/04-wasm-simulation/02-mechanisms/08-channel-routing.md)，所有数字 GPIO 交互由 **通道 1 (Pin-Level)** 统一汇总至 `PinArbiter`。

#### 1. 全局引脚线性映射规范
8051 架构具有 4 个 8 位双向 I/O 端口（P0~P3，共 32 个物理引脚）。框架与 UniSim 3.0 统一采用以下单调线性映射公式：
$$\text{global\_pin} = (\text{port\_idx} \ll 3) \mid \text{bit\_idx} \quad (\text{即 } \text{port\_idx} \times 8 + \text{bit\_idx})$$

| 8051 端口位 | 全局线性引脚号 (`uint16_t pin`) | UniSim / Wokwi 画布对应器件引脚 (典型挂载示例) |
| :--- | :--- | :--- |
| **P0.0 ~ P0.7** | **Pin 0 ~ 7** | 动态数码管段选 / 扩展外部总线 |
| **P1.0 ~ P1.7** | **Pin 8 ~ 15** | ADC0832 (DI: 8, CLK: 9, CS: 10) / NTC 热敏电桥 / 独立状态指示灯 |
| **P2.0 ~ P2.7** | **Pin 16 ~ 23** | 继电器驱动 (Relay: 16) / 发热管驱动 PWM / 蜂鸣器 |
| **P3.0 ~ P3.7** | **Pin 24 ~ 31** | 独立轻触按键 (Key1: 25, Key2: 26) / UART (RXD: 24, TXD: 25) |

#### 2. 即时通知 (Instant Notification) 机制
* **痛点防御**：若普通 GPIO 仅在每 tick (10ms) 的 `wink_mcs51_sync_out()` 批量同步，用户执行 `P2_0 = 0; delay(500); P2_0 = 1;` 时，LED 灯光动画将产生明显的帧率滞后与阶跃跳变。
* **即时派发**：在 `WinkSfrBitProxy::operator=` 与 `WinkSfr::operator=` 内部，一旦检测到引脚发生真实电平翻转（`diff` 对应位为 1），框架在更新 `s_sfr_shadow` 影子的同时，**立即同步调用 `js_pal_gpio_write(global_pin, new_level)`**。
* **用户业务代码零侵入保证**：
  - 用户在 C 源码中依然 100% 编写原汁原味的 8051 逻辑：`sbit LED = P2^0; LED = 0;` 或 `P1 = 0xFE;`；
  - 映射计算 `(port_idx << 3) | bit_idx` 与 `js_pal_gpio_write` 派发完全封闭在 C++ 运算符重载门禁内部；
  - 用户代码无需依赖任何全局引脚变量，达成 **0 污染、0 改动、0 依赖**。

---

## 5. 编译期与链接期工程化规范

### 5.1 单参数推导与用户自定义 SFR 兼容
在 Keil C51 中，扩展 SFR 定义格式为：
```c
sfr AUXR = 0x8E;
```
在引入 `#define sfr inline WinkSfr` 后：
```cpp
inline WinkSfr AUXR = 0x8E;
```
调用 `WinkSfr::WinkSfr(uint8_t addr)` 构造函数：
- `addr = 0x8E`；
- 内部判断：`addr != 0x80/0x90/0xA0/0xB0`，自动将 `port_idx` 初始化为 `0xFF`；
- 此后对 `AUXR |= 0x01` 的操作自动转化为锁存器影子的修改，且不触发任何无效的 GPIO 引脚循环扫描，零开销支持任意厂商专有 SFR。

### 5.2 多编译单元 ODR 防护与标准适配策略 (One Definition Rule)

若用户项目包含多个 `.c` 文件（如 `main.c` 与 `driver.c`），且都包含了 `<REGX52.H>` 或第三方厂商头文件（如 `STC15.H` 定义了上百个 `sfr AUXR = 0x8E;`），必须解决跨编译单元重复定义的问题。

#### 5.2.1 首选基线方案：C++17 `inline WinkSfr` 弱符号合并与常量初始化
- **语法实现**：在 `REGX52.H` 中声明为 `inline WinkSfr P1{0x90, 1};`；第三方扩展寄存器通过 `#define sfr inline WinkSfr` 展开；
- **标准保证（C++17 P0386R2 & [basic.start.static]）**：
  1. **避免 constexpr 误杀只读**：严禁将变量本身声明为 `constexpr WinkSfr`，因为在 C++ 中 `constexpr` 作用于变量实体时**天然隐含 `const` 顶层只读**，会导致后续用户代码 `P1 = 0x55;` 调用非 const 的 `operator=` 时报硬编译错误（`no matching operator= for const object`）；
  2. **常量初始化（Constant Initialization）**：因为 `WinkSfr::WinkSfr(...)` 构造函数本身声明为 `constexpr` 且实参是字面量，C++ 标准保证其在程序装载阶段完成静态常量初始化，早于任何运行时动态初始化，依然 100% 免疫静态初始化顺序死锁；
  3. **弱符号去重**：C++17 编译器为 `inline` 变量打上弱符号（Weak Symbol）标记，链接器在多 TU 间自动合并消重，全程序拥有唯一物理地址，100% 符合 ODR 规范；
  4. **花括号初始化**：统一采用花括号 `{0x90, 1}` 语法，杜绝小括号 `(0x90, 1)` 带来的 Most-Vexing-Parse 歧义与逗号表达式风险；
- **适用场景**：现行 Host (GCC 7+/Clang 5+/MSVC 2017+) 与 Wasm (Emscripten) 标准编译环境（轴 B 官方基线）。

#### 5.2.2 备选降级方案：C++14 `static WinkSfr` 内部链接机制 (老工具链平替)
- **语法实现**：若环境因历史包袱必须锁死在 `-std=c++14`，则将宏调整为：
  ```cpp
  #define sfr static WinkSfr
  ```
- **语言标准依据与安全性**：
  1. 每个编译单元独立拥有内部链接实例，链接器绝不报重定义错误；
  2. `WinkSfr` 内部只有两个只读的 `sfr_addr` 和 `port_idx`（共 2 字节），且构造函数为 `constexpr`；
  3. 在 `-O2` 优化下，编译器直接将地址与端口号折叠为汇编立即数（Immediate Constants），**在 RAM 和 Flash 中 0 字节开销**；
  4. 绝不加 `constexpr`，对象始终保持可变（Mutable），赋值畅通无阻。

#### 5.2.3 方案对比与自适应宏定义指南

| 维度 | C++17 `inline WinkSfr` (首选基线) | C++14 `static WinkSfr` (备选降级) |
| :--- | :--- | :--- |
| **标准门槛** | C++17 及以上 | C++11 / C++14 / C++17 通用 |
| **符号属性** | 外部链接 (External Linkage) + 弱符号合并 | 内部链接 (Internal Linkage，等同 static) |
| **跨 TU 地址** | 全程序单一全局地址 (`&P1 == &P1`) | 每个 TU 独立地址 (对 51 业务无影响) |
| **RAM / Flash 开销** | 0 字节 (单弱符号折叠) | 0 字节 (编译期常量折叠为立即数) |
| **对象可写性** | ⭐️⭐️⭐️⭐️⭐️ 完美可变（非 const） | ⭐️⭐️⭐️⭐️⭐️ 完美可变（非 const） |
| **厂商头文件支持** | ⭐️⭐️⭐️⭐️⭐️ 完美 | ⭐️⭐️⭐️⭐️⭐️ 完美 |

在 `include/REGX52.H` 中支持自适应预编译分支：
```cpp
#if defined(__cplusplus) && __cplusplus >= 201703L
  // C++17 标准基线：全局弱符号内联变量（非 const，走 Constant Initialization）
  #define sfr inline WinkSfr
#else
  // C++14 降级退路：命名空间内部链接静态变量
  #define sfr static WinkSfr
#endif
```

### 5.3 位寻址宏 `#define sbit` 规范与语法支持边界

#### 5.3.1 标准宏定义实现（零构造顺序开销）
在 Keil C51 语言规范中，`sbit` 为存储类型说明符，**规范限定仅能出现于文件/全局作用域**（函数内部声明为非法语法）。在 `REGX52.H` 中声明为：
```cpp
#if defined(__cplusplus) && __cplusplus >= 201703L
  // 【C++17 官方基线】：内联变量自动推导，多编译单元弱符号自动合并消重，100% 免疫 ODR
  #define sbit inline auto
#else
  // 【C++14 备选退路】：内部链接自动推导，各编译单元独立常量，0 运行时初始化
  #define sbit static auto
#endif
```

#### 5.3.2 常量初始化（Constant Initialization）证明
由于宿主寄存器（如 `TCON(0x88)`、`P1(0x90)`）均被声明为 `constexpr WinkSfr`，`TCON^5` 调用的是 `constexpr operator^`，整个表达式在 C++ 编译期即求值为编译期字面量。
因此 `sbit TF0 = TCON^5;` 在程序装载阶段即由操作系统/加载器完成 **Constant Initialization**，**早于任何 C++ 运行时动态构造函数的执行**，物理层面彻底根除静态初始化顺序死锁（Static Initialization Order Fiasco）。

#### 5.3.3 语法边界限制（严禁绝对位地址语法）
Keil C51 官方语法包含两种位定义格式：
1. **相对位声明**：`sbit TF0 = TCON^5;`、`sbit P1_0 = P1^0;` —— **完整支持**，通过 `operator^` 构造安全的 `WinkSfrBitProxy` 代理；
2. **绝对位地址声明**：`sbit CY = 0xD7;`、`sbit P1_0 = 0x90;` —— **严格不支持**！
   - **风险原理**：若用户源码出现绝对位地址，经 `#define sbit inline auto` 展开后成为 `inline auto CY = 0xD7;`，变量类型被静默推导为 `int`（值为 215）。后续 `CY = 1;` 仅修改该普通整型局部/全局变量，硬件 PSW 标志位完全未被更新，造成灾难性静默运行故障；
   - **治理措施**：
     - 官方提供的 `REGX52.H`、`REG52.H` 已全量规范为 `REG^bit` 形式；
     - 明确列入《不支持特性清单》（见主规划书 §3.8）；
     - CMake 源码预处理流水线增设 Linter 检测正则：匹配到 `sbit\s+\w+\s*=\s*0x[0-9a-fA-F]+;` 时直接抛出 `FATAL_ERROR` 阻断构建，强行提示用户调整为 `REG^bit` 相对位格式。

---

## 6. 关联文档交叉引用矩阵 (Cross-Reference Matrix)

本设计规格书作为底层细节的 SSOT，其决策在其它核心设计文档中的引用锚点如下：

| 关联文档 | 引用位置 | 引用主题与联动要求 |
| :--- | :--- | :--- |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **§3.3**（整端口字节 RMW 与边沿感知） | 引用本规格书第 2 节与第 3 节，作为 A2 闭环的技术标准。 |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **§7** 架构决议表 **AD-12** | 明确将 `diff` 边沿跳变感知与 Read-Latch 语义列入实施硬性门禁。 |
| [`mcu-compat-plan.md`](mcu-compat-plan.md) | **§8** 验收标准 **第 3 项** | 补充整端口 RMW 对 ADC0832 即时陷阱触发的自动化测试验收。 |
| [`2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md`](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md) | **§4.3**（整端口模型与边沿跳变检测） | 修正 `operator|=` 代码隐患，全面链接并跳转至本规格书。 |

---

## 7. 单元测试与验证计划 (Verification Plan)

为确保该方案在各类边界条件下坚如磐石，将在 `wink-micro-os/test/mcs51/` 下新增以下针对性测试用例：

### 7.1 测试用例 1：`test_sfr_rmw_latch_integrity.cpp`
- **目的**：验证 RMW 复合赋值是否严格遵循 Read-Latch，严禁污染外部拉低的输入引脚。
- **动作**：
  1. 向 `P1` 写入 `0xFF`，在 `P1.3` 挂接 `on_read` 陷阱固定返回 `0`（模拟外部下拉按键或传感器）；
  2. 读取 `(uint8_t)P1`，断言 bit 3 读出为 `0`；
  3. 执行 RMW 操作 `P1 |= 0x01;`；
  4. 移除 `on_read` 陷阱（释放外部下拉）；
  5. 再次读取 `(uint8_t)P1`，断言 bit 3 恢复为 `1`（证明锁存器中的 `1` 从未被 RMW 抹成 `0`）。

### 7.2 测试用例 2：`test_sfr_edge_dispatch_accuracy.cpp`
- **目的**：验证整端口写入与 RMW 时，多引脚边沿分发的精确性与防伪触发。
- **动作**：
  1. 在 `P1.0` (DI)、`P1.1` (CLK)、`P1.2` (CS) 分别挂接事件计数器；
  2. 初始写入 `P1 = 0x00;`；
  3. 执行 `P1 = 0x05;`（DI=1, CS=1, CLK=0）；
  4. 断言：DI 计数器 +1，CS 计数器 +1，**CLK 计数器保持为 0**（严防伪时钟）；
  5. 执行 `P1 |= 0x02;`（CLK=1）；
  6. 断言：仅 CLK 计数器 +1，DI 与 CS 计数器保持不变。

### 7.3 测试用例 3：`test_sfr_operators_coverage.cpp`
- **目的**：全面覆盖编译并验证 `P1++`、`--P1`、`P1 ^= 0x55`、`P1 >>= 2` 等全部 C++ 操作符。
