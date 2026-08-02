# DAL API 8位单片机 (8051) 适配规范与扩展指南

| 项 | 内容 |
|----|------|
| **规范版本** | v1.0.0 (Draft) |
| **状态** | 拟定中 / 补充扩展规范 |
| **适用范围** | `wink-micro-os` 器件抽象层 (`dal/`) 在 8 位微控制器（如 8051、STC8、AVR、PIC 等）上的驱动适配与代码生成器 (`codegen`) 导出 |
| **主规范引用** | [`dal-api-consistency-spec.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v2.1.0) |
| **变更历史** | v1.0.0 (2026-08-01) 初稿，建立 8 位单片机 Profile 适配体系与硬核物理约束解法 |

---

## 1. 背景与 8 位架构特有物理约束

基准规范 [`dal-api-consistency-spec.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) 针对 32 位/64 位目标平台（如 ESP32-S3、STM32、Wasm 仿真）制定了严密的 API 一致性契约。然而，在适配 **8051 / STC8 / AVR** 等 8 位超低端单片机时，受限于其特殊的硬件架构与编译器实现，直接复用主规范存在严重的物理阻碍。

### 1.1 物理硬件限制

1. **RAM 资源极度匮乏**：经典 8051 内部通用 RAM (idata/data) 仅 128~256 字节，扩展 RAM (xdata) 通常仅 1KB~4KB，调用栈深度（Stack）往往不足 64 字节。无法承受大结构体深拷贝与深层函数调用。
2. **无硬件浮点单元 (FPU)**：8 位 CPU 无浮点硬件支持，引入 `float` 运算会导致编译器嵌入数 KB 的软浮点模拟库（Soft-Float），且单次加减乘除消耗数百至上千个 CPU 周期。
3. **Harvard 哈佛架构与物理存储区隔离**：Program Memory (Flash/ROM `code`) 与 Data Memory (RAM `data`/`idata`/`xdata`) 地址空间物理隔离，访问不同存储区的指令与周期差异巨大。

### 1.2 编译器方言约束 (以 Keil C51 / SDCC 为例)

1. **局部变量覆盖分析 (Overlay Analysis)**：Keil C51 默认**不使用动态调用栈**传递局部变量，而是通过静态分析调用树（Call Tree）将互斥函数的局部变量重叠分配在静态 RAM 区。
2. **函数指针灾难**：一旦使用函数指针（Function Pointer）或 `void *` 泛型接口，编译器的 Overlay 分析将失效，迫使局部变量与参数强制压栈或分配固定 RAM，导致 RAM 瞬间耗尽或栈溢出。
3. **非重入性 (Non-Reentrant)**：C51 函数默认不可重入。若 ISR 中调用的函数在 Task/主循环中同时被调用，会导致局部变量覆盖破坏。

### 1.3 Profile 分级设计原则

为实现“**同源 YAML SSOT，两端精准生成**”，`wink-micro-os` 引入 Profile 区分机制：
* **Full Profile (32-bit / POSIX / WASM)**：使用 `float` 物理量、包含 `owner` 跟踪、支持 32/64 位高精度时间戳与通用句柄。
* **Micro Profile (8-bit / 8051)**：使用定点数/缩放整数、Flash Zero-Copy 句柄、静态硬编码分发、16 位低开销计数器。

---

## 2. 物理量与数据类型规范 (定点数与量纲缩放)

### 2.1 禁用 `float` / 引入整数量纲缩放

在 8 位 Profile 模式下，DAL 公开 API **MUST NOT** 使用 `float` 或 `double` 数据类型。所有物理量与归一化控制量 MUST 转换为固定量纲的整型（`int16_t` / `uint16_t` / `int8_t`）。

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-U-001 | MUST | 8 位 Profile 下公开 API 物理量参数与出参 MUST 使用整型与显式量纲后缀 |
| DAL-8B-U-002 | MUST | 归一化控制量（如速度、占空比）MUST 使用 **千分比 (promille, ‰)** 整数表示，取值范围 `[-1000, 1000]` 或 `[0, 1000]` |
| DAL-8B-U-003 | SHOULD | 角度物理量 SHOULD 使用 **0.1 度 (deci-degree, ddeg)** 或 **1 度** 整数表示 |

### 2.2 标准 8 位量纲对照表

| 物理量 | Full Profile (32-bit) 类型 | Micro Profile (8-bit) 类型 | 单位说明与取值范围 |
|--------|---------------------------|----------------------------|-------------------|
| 速度归一化 | `float speed` (`[-1.0, 1.0]`) | `int16_t speed_promille` | 千分比，`[-1000, 1000]` |
| 占空比 | `float duty` (`[0.0, 1.0]`) | `uint16_t duty_promille` | 千分比，`[0, 1000]` |
| 舵机角度 | `float angle_deg` (`[0.0, 180.0]`) | `uint16_t angle_ddeg` | 0.1 度，`0 ~ 1800` (180.0°) |
| 距离 | `float distance_cm` | `uint16_t distance_mm` | 毫米，`0 ~ 65535 mm` |
| 毫秒延迟/超时 | `uint32_t timeout_ms` | `uint16_t timeout_ms` | 毫秒，`0 ~ 65535 ms` (最大 65.5s) |
| 微秒脉冲 | `uint32_t pulse_us` | `uint16_t pulse_us` | 微秒，`0 ~ 65535 us` |

---

## 3. 内存与句柄设计 (Zero-Copy Flash 配置)

### 3.1 内存修饰符宏抽象

为适配 8051 哈佛架构，DAL 头文件与生成代码中与存储区相关的修饰符 MUST 使用 PAL 统一抽象宏：

```c
#if defined(WINK_TARGET_MCU_8051)
  #define WINK_CODE     code      /* 存储于 Flash / ROM */
  #define WINK_XDATA    xdata     /* 存储于 外部扩展 RAM */
  #define WINK_IDATA    idata     /* 存储于 内部高 128B RAM */
  #define WINK_DATA     data      /* 存储于 内部低 128B RAM */
#else
  #define WINK_CODE
  #define WINK_XDATA
  #define WINK_IDATA
  #define WINK_DATA
#endif
#define WINK_CODE_PTR   const WINK_CODE   /* Flash 字符串/只读结构体指针修饰符 */
```

### 3.2 Flash 常量配置引用模式 (Zero-Copy)

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-S-001 | MUST | 在 8 位 Profile 下，`dal_<type>_t` 句柄 **MUST NOT** 深拷贝整个 `config_t` 结构体到 RAM |
| DAL-8B-S-002 | MUST | 句柄结构体包含指向 Flash 的只读配置指针：`const WINK_CODE dal_<type>_config_t *cfg;` |
| DAL-8B-S-003 | MUST | 配置结构体 `dal_<type>_config_t` 变量 MUST 加上 `WINK_CODE` 声明在 ROM 中 |

**句柄定义对比**：

```c
/* 32 位 Full Profile: 深拷贝配置，占用 24+ 字节 RAM */
typedef struct {
    dal_led_config_t config; /* 深拷贝 */
    bool initialized;
} dal_led_t;

/* 8 位 Micro Profile: Zero-Copy Flash 引用，仅占用 3~4 字节 RAM */
typedef struct {
    const WINK_CODE dal_led_config_t *cfg; /* 指向 ROM 的指针 (2 字节) */
    uint8_t initialized;                  /* C51 下 uint8_t 比 bool 更高效 (1 字节) */
} dal_led_8b_t;
```

### 3.3 `owner` 字段裁减规范

在 8 位环境，资源占用归因 `owner` 字符串无运行期仲裁价值。

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-S-010 | SHOULD | 当编译宏 `#ifdef WINK_DISABLE_OWNER_TRACKING` 启用时，`config_t` 中可以裁剪掉 `const char *owner` 成员，以在 8 位 MCU 上再省去 2~3 字节指针 |

---

## 4. 调度、调用契约与零开销分发 (Zero-Cost Dispatch)

### 4.1 彻底禁用函数指针与 `void *`

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-F-001 | MUST | 8 位 Profile 下 **MUST NOT** 使用 `void *dev` 虚分发与函数指针表 (Function Pointer Tables) |
| DAL-8B-F-002 | MUST | 驱动方法必须为具名静态函数或内联函数（如 `dal_led_on(dal_led_8b_t *dev)`），允许编译器进行完整的 Overlay 覆盖分析 |

### 4.2 Codegen 编译期硬编码与静态内联

Codegen 在为 8051 生成 `app_codegen.c` 时：
1. 替代运行时遍历驱动 Registry，直接根据 YAML 绑定生成硬编码直接调用（Direct Static Calls）；
2. 对于极简驱动（如 GPIO LED、Relay），Codegen 应直接生成宏（Macro）或 `static inline` 函数，避免函数调用进栈开销。

```c
/* 8051 Codegen 导出的静态内联示例 */
static inline void app_led_on(void) {
    /* 直接操作 51 端口 sbit 引脚，0 字节栈消耗，1 周期指令 */
    P1_0 = 1; 
}
```

---

## 5. 并发、中断与编译器重入契约

### 5.1 单核极简关中断临界区

8051 为单核处理器，无乱序执行与内存屏障需求，并发临界区通过控制总中断使能位 `EA` 实现。

```c
/* 8051 临界区宏定义范式 */
#define WINK_8B_CRITICAL_ENTER()  do { uint8_t _ea_save = EA; EA = 0;
#define WINK_8B_CRITICAL_EXIT()        EA = _ea_save; } while(0)
```

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-C-001 | MUST | 8 位 Profile 下不使用原子指令，临界区保护统一使用 `EA` 保存/恢复宏 |
| DAL-8B-C-002 | MUST | 临界区内部 **MUST NOT** 调用含有耗时 busy-wait 或复杂状态机推进的代码 |

### 5.2 Keil C51 重入契约 (Reentrancy)

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-C-010 | MUST | 若某个 DAL API 既可能在 ISR 中被调用，又可能在主循环 (Task) 中被调用，在 Keil C51 环境下该 API **MUST** 加上 `reentrant` 关键字声明，或设计为完全无局部变量的内联宏 |

---

## 6. 整型位宽与时间戳裁剪

### 6.1 16 位整型/时间戳替代 32 位

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-T-001 | MUST | 8 位 Profile 下时间戳与计数值默认使用 `uint16_t` |
| DAL-8B-T-002 | MUST | 状态标志与布尔值 **MUST** 使用 `uint8_t` 代替 `bool`（在 C51 中 `uint8_t` 直接对应 R0-R7 寄存器，运算比标准 C `bool` 更高效） |

---

## 7. Codegen YAML 与 Profile 导出机制

`wink-micro-os/codegen/drivers/*.yaml` 作为驱动定义的 SSOT，通过增加 `profiles` 识别与 8 位变体表达式：

```yaml
codegen_schema: "1.1"
type: rc_servo
category: actuator
is_actuator: true

# 声明支持的 Profile
profiles:
  - full
  - micro_8bit

# 8 位 Profile 专用类型重映射与量纲
profile_overrides:
  micro_8bit:
    config_type: dal_rc_servo_8b_config_t
    handle_type: dal_rc_servo_8b_t
    value_types:
      angle: { type: uint16_t, unit: deci_deg, range: [0, 1800] }

role_bindings:
  position_actuator:
    verbs:
      set_angle:
        template_full: "dal_rc_servo_set_angle(&{{dev}}, {{angle_deg}}f);"
        template_micro_8bit: "dal_rc_servo_8b_set_angle(&{{dev}}, {{angle_ddeg}});"
```

---

## 8. 示例：舵机驱动 (RC Servo) 两端 API 对比

### 8.1 32 位 Full Profile (ESP32 / WASM)
```c
/* 配置结构体 (深拷贝到 RAM) */
typedef struct {
    const char *owner;
    uint16_t pin;
    float max_angle;
} dal_rc_servo_config_t;

typedef struct {
    dal_rc_servo_config_t config;
    bool initialized;
    float current_angle;
} dal_rc_servo_t;

/* API 接口：使用 float 角度 */
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle_deg);
```

### 8.2 8 位 Micro Profile (8051 / Keil C51)
```c
/* 配置结构体：存储在 Flash (code) 区 */
typedef struct {
    uint8_t pin;             /* 51 引脚编号 (1 字节) */
    uint16_t max_angle_ddeg; /* 0.1 度单位 (2 字节) */
} dal_rc_servo_8b_config_t;

/* 句柄：极其轻量，占用 < 5 字节 RAM */
typedef struct {
    const WINK_CODE dal_rc_servo_8b_config_t *cfg; /* ROM 指针 */
    uint8_t initialized;
    uint16_t current_angle_ddeg; /* 0.1 度单位 */
} dal_rc_servo_8b_t;

/* API 接口：使用 0.1 度整数角度，适合 8 位 CPU 快速计算脉宽 */
wink_status_t dal_rc_servo_8b_set_angle(dal_rc_servo_8b_t *dev, uint16_t angle_ddeg);
```

---

## 9. 总结与合规矩阵

通过引入本扩展规范，`wink-micro-os` 实现了从 ESP32-S3 双核高性能 MCU 到 8051 超低端 8 位单片机的完整覆盖：

1. **统一抽象**：在 YAML 逻辑层保持统一的 Role Verb (如 `set_angle`, `set_speed`) 与生命周期逻辑；
2. **零开销导出**：在 8 位端放弃 `float`、放弃动态指针与结构体深拷贝，利用 C51 物理特性获得极致的内存与 CPU 效率；
3. **架构安全**：彻底规避 C51 函数指针导致的 Overlay 失效风险与栈溢出隐患。
