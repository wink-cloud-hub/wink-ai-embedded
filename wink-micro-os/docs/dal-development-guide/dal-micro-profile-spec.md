# WinkOS DAL 8位 Micro Profile 扩展规范 (Micro Profile Extension Specification)

| 项 | 内容 |
|----|------|
| **规范版本** | v1.1.0 (Proposed) |
| **状态** | 拟定中 / 待评审 (Proposed — 待 8051 PAL Port 与 SDCC CI 编译集成落地后升 MUST) |
| **适用范围** | `wink-micro-os` 面向 8 位超低端 MCU (8051 / STC8 / AVR / PIC) 的 `dal/` 扩展驱动 |
| **关联主规范** | [`dal-api-consistency-spec.md`](dal-api-consistency-spec.md)（§9 单位、量纲与值域） |
| **关联 ADR** | [ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md) (静态分发与零开销)、[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) (跨 Profile 量纲 A/B 两分类与定标整数) |
| **变更历史** | v1.0.0 (2026-08-03) 从主规范 `dal-api-consistency-spec.md` 独立解耦解包，统一 SDCC 工具链与 C89 退化矩阵; v1.1.0 (2026-08-03) 对齐 [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)：DAL-8B-F-002 函数名去 `_8b_`（类型名保留 `_8b_t`）；量纲表述对齐 A/B 两分类——A 类执行器命令全 Profile 同定标整数，B 类传感器测量定点分化 |

---

## 1. 背景与适用范围

在 8 位 MCU (如 8051/STC8) 硬件环境下，由于 RAM 极其匮乏 (128B ~ 256B)、缺少硬件 FPU、具有 Harvard 哈佛架构存储区隔离限制，无法直接运行 Full Profile 的 32 位 C 驱动。

本规范作为主规范 `dal-api-consistency-spec.md` 的可选扩展，在保持 YAML SSOT 定义一致的前提下，定义 8 位 target 的专门内存模型、类型映射与语言子集退化规则。

---

## 2. 工具链与存储区抽象

### 2.1 推荐工具链与语法抽象层

本规范选定 **SDCC (Small Device C Compiler)** 为标准 CI 编译工具链，同时通过 PAL 宏定义提供与 Keil C51 的兼容互通。

```c
#if defined(WINK_TARGET_MCU_8051)
  #if defined(__SDCC)
    #define WINK_CODE     __code     /* Flash / ROM 静态存储区 */
    #define WINK_XDATA    __xdata    /* 外部扩展 RAM */
    #define WINK_IDATA    __idata    /* 内部高 128B RAM */
    #define WINK_DATA     __data     /* 内部低 128B RAM */
    #define WINK_REENTRANT __reentrant
  #elif defined(__C51__) || defined(__CX51__)
    #define WINK_CODE     code
    #define WINK_XDATA    xdata
    #define WINK_IDATA    idata
    #define WINK_DATA     data
    #define WINK_REENTRANT reentrant
  #endif
#else
  #define WINK_CODE
  #define WINK_XDATA
  #define WINK_IDATA
  #define WINK_DATA
  #define WINK_REENTRANT
#endif

#define WINK_CODE_PTR   const WINK_CODE   /* Flash 只读指针修饰符 */
```

### 2.2 Flash 常量配置引用模式 (Zero-Copy ROM Handle)

为防止将 24+ 字节的 `config_t` 深拷贝到 RAM 造成内存挤压，8 位 Profile 采用 Zero-Copy ROM 引用模式：

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-S-001 | MUST | 在 8 位 Profile 下，`dal_<type>_8b_t` 句柄 **MUST NOT** 深拷贝整个 `config_t` 结构体到 RAM |
| DAL-8B-S-002 | MUST | 句柄结构体包含指向 Flash 的只读配置指针：`const WINK_CODE dal_<type>_8b_config_t *cfg;` |
| DAL-8B-S-003 | MUST | 配置结构体 `dal_<type>_8b_config_t` 变量 MUST 加上 `WINK_CODE` 声明在 ROM 中 |
| DAL-8B-S-010 | SHOULD | 当编译宏 `#ifdef WINK_DISABLE_OWNER_TRACKING` 启用时，`config_t` 中可以裁剪掉 `const char *owner` 成员。**注意**：裁剪 owner 时线格式（Wire format）必须同步解耦或在 YAML 中定义 `profile_overrides` |
| DAL-8B-S-020 | MUST | 在 Micro Profile 模式下 **MUST NOT** 调用任何动态内存分配函数（`malloc` / `free` / `calloc` / `realloc`）。所有临时 Buffer、驱动句柄与配置数据必须为编译期确定的静态存储期 |

**句柄结构示意**：

```c
/* 8 位 Micro Profile: Zero-Copy Flash 引用，仅占用 3 字节 RAM */
typedef struct {
    const WINK_CODE dal_led_8b_config_t *cfg; /* 指向 Flash ROM 的配置指针 (2 字节) */
    uint8_t initialized;                      /* 初始化标志 (1 字节) */
} dal_led_8b_t;
```

---

## 3. 函数分发、并发与 ANSI C89 退化矩阵

### 3.1 函数分发与重入契约

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-F-001 | MUST | 8 位 Profile 下 **MUST NOT** 使用 `void *dev` 虚分发与函数指针表 (Function Pointer Tables)，防止破坏编译器 Overlay 覆盖分析 |
| DAL-8B-F-002 | MUST | 驱动方法 MUST 为具名静态函数或内联函数，且**函数符号名 MUST 去 `_8b_` 后缀**，与 Full Profile 完全一致（如 `dal_led_on(dal_led_8b_t *dev)`）。**句柄/配置类型名保留 `_8b_t`/`_8b_config_t`**（其内存模型与 Full 不同构，需类型区分）。一个固件只链接一个 Profile，同名不冲突（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) §4） |
| DAL-8B-C-001 | MUST | 8 位 Profile 下不使用原子指令，临界区保护统一使用 `EA` 保存/恢复宏：<br/>`#define WINK_8B_CRITICAL_ENTER() do { uint8_t _ea_save = EA; EA = 0;`<br/>`#define WINK_8B_CRITICAL_EXIT() EA = _ea_save; } while(0)` |
| DAL-8B-C-002 | MUST | 临界区内部 **MUST NOT** 调用含有耗时 busy-wait 或复杂状态机推进的代码 |
| DAL-8B-C-010 | MUST | 若某个 DAL API 既可能在 ISR 中被调用，又可能在 Task/Main 循环中被调用，在 Keil/SDCC 环境下该 API **MUST** 加上 `WINK_REENTRANT` 声明，或设计为完全无局部变量的内联宏 |

### 3.2 ANSI C89 退化矩阵

为兼容早期 C89/C90 严格模式编译器，8 位 Micro Profile 下的 C99 特性退化如下：

| C99 特性 | Full Profile (32-bit) | Micro Profile (8-bit C89 退化) | 说明 |
|----------|-----------------------|--------------------------------|------|
| 布尔类型 | `bool` (`stdbool.h`) | `uint8_t` (`1` / `0`) | 8 位 MCU 处理 `uint8_t` 比 `bool` 更加原生高效 |
| 编译期断言 | `_Static_assert` | `WINK_STATIC_ASSERT` (负数组大小退化) | 防止 C89 编译器报语法错误 |
| 属性宏 | `WINK_WARN_UNUSED_RESULT` | 空宏定义 | C89 / SDCC 不支持 GCC 特性属性时安全退化 |
| 注释风格 | `//` 注释 | `/* ... */` 注释 | 确保 strict C89 编译器编译通过 |

---

## 4. 整型量纲缩放与时间戳

### 4.1 浮点数禁用与整型量纲

8 位 Micro Profile 下 DAL 公开 API **MUST NOT** 使用 `float` / `double`（无 FPU，软浮点库膨胀且缓慢）。量纲遵循主规范 §9 的 **A/B 两分类**（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）：

- **A 类·执行器命令**（速度、占空比、舵机角度、位置、PWM 频率、超时）：**与 Full Profile 完全相同的定标整数类型与刻度**（见主规范 §9.4），两端字面量一字不差，零软浮点、零转换宏。
- **B 类·传感器测量**（温度、距离、电压、IMU）：退化为定点整型，与 Full 的 float 类型/单位不同，差异由 codegen binding 吸收（见主规范 §9.5）。

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-U-001 | MUST | 8 位 Profile 下公开 API 物理量参数与出参 **MUST NOT** 使用 `float` / `double`，MUST 使用整型与显式量纲后缀 |
| DAL-8B-U-002 | MUST | A 类归一化控制量（如速度、占空比）MUST 使用 **千分比 (promille, ‰)** 整数表示，取值范围 `[-1000, 1000]` 或 `[0, 1000]`，且**与 Full Profile 同类型同刻度** |
| DAL-8B-U-003 | SHOULD | A 类角度物理量 SHOULD 使用 **0.1 度 (deci-degree, ddeg)** 整数表示，取值 `0 ~ 1800`，与 Full 同刻度 |
| DAL-8B-U-004 | MUST | B 类传感器测量量在 8 位 MUST 使用带单位后缀的定点整型（如 `uint16_t distance_mm`、`int16_t temp_ddegc`）；物理不变量（折算到标准单位）MUST 与 Full 一致，超 Micro 位宽量程由 codegen 生成期报错，MUST NOT 静默截断 |
| DAL-8B-T-001 | MUST | 8 位 Profile 下时间戳与计数值默认使用 `uint16_t` (最大 65535 ms / us)。**时间计算 MUST 采用无符号差值溢出回绕语义（`(uint16_t)(now_ms - start_ms) >= timeout_ms`），禁止直接比较绝对时间**。更长超时可显式用 `uint32_t`（8051 运算更贵，须在头注释声明成本，见主规范 DAL-U-032） |

**标准 Micro Profile 量纲对照表**：

| 物理量 | 分类 | Full Profile (32-bit) | Micro Profile (8-bit) | 单位说明与取值范围 |
|--------|------|----------------------|-----------------------|-------------------|
| 速度归一化 | A | `int16_t speed_promille` | `int16_t speed_promille` | 千分比，`[-1000, 1000]`（全 Profile 同刻度） |
| 占空比 | A | `uint16_t duty_promille` | `uint16_t duty_promille` | 千分比，`[0, 1000]`（全 Profile 同刻度） |
| 舵机角度 | A | `uint16_t angle_ddeg` | `uint16_t angle_ddeg` | 0.1 度，`0 ~ 1800`（全 Profile 同刻度） |
| 距离 | B | `float distance_cm` | `uint16_t distance_mm` | 毫米，`0 ~ 65535 mm`（由 binding 换算） |
| 温度 | B | `float temp_degc` | `int16_t temp_ddegc` | 0.1°C，`-400 ~ 1250`（由 binding 换算） |
| 毫秒延迟/超时 | A | `uint32_t timeout_ms` | `uint16_t timeout_ms` | 毫秒，`0 ~ 65535 ms`（宽度差异，刻度相同） |

> **B 类字面量与运行时转换**：B 类量的编译期字面量换算宏（`_LITERAL` 后缀，零开销，仅对常量实参成立）与运行时变量转换函数（Micro 上软浮点成本由头注释显式声明）细节见主规范 §9.5。MUST 严格区分字面量与运行时变量，不得用同一宏掩盖软浮点成本。

---

## 5. 合规与提级条件

当前 Micro Profile 处于 **Proposed** 状态。提级至正式 **MUST** 执行之前必须满足以下前置条件：

1. 完成 8051 PAL 移植包（`pal_8051`）；
2. 建立 `dal_led_8b.h/c` 黄金参考实现并全量合规；
3. CI 中接入 SDCC 自动化编译拦截。
