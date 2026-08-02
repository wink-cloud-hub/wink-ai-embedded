# DAL API 一致性规范 (DAL API Consistency Specification)

| 项 | 内容 |
|----|------|
| **规范版本** | v3.1.0 (Draft) |
| **状态** | 拟定中 / 待评审 |
| **适用范围** | `wink-micro-os` 器件抽象层 (`dal/`) 驱动开发与代码生成器 (`codegen`) |
| **关联活规范** | [`01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| **关联 ADR** | [ADR-0001](../../../docs/design/decisions/0001-error-code-sign-convention.md) (错误码符号约定), [ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md) (静态分发), [ADR-0017](../../../docs/design/decisions/0017-blocking-api-hard-isolation.md) (阻塞隔离), [ADR-0024](../../../docs/design/decisions/0024-fault-three-phase-model-and-dal-deinit-contract.md) (Deinit 清场), [ADR-0043](../../../docs/design/decisions/0043-yaml-driven-layer-lint.md) (Lint 规约), [ADR-0046](../../../docs/design/decisions/0046-dal-driver-registry-ssot.md) (驱动 Registry SSOT), [ADR-0048](../../../docs/design/decisions/0048-actuator-control-semantic-naming.md) (执行器语义命名) |
| **变更历史** | v1.0.0 (2026-08-01) 初稿; v2.0.0 基于评审重写; v2.1.0 整合 review notes; v3.0.0 整合 8 位 Profile 体系; v3.1.0 (2026-08-02) 补充 8 位动态内存禁令 (DAL-8B-S-020)、跨主频空循环忙等禁令 (DAL-B-012)、Golden Reference 样板驱动标定及 SDCC 8051 CI 编译拦截规约 (DAL-8B-T-010) |

---

## 术语与约定

本文使用 RFC 2119 关键字：

- **MUST / MUST NOT** — 强制规则，CI lint 以 error 报告违反（新增驱动指示平台立即适用）
- **SHOULD / SHOULD NOT** — 推荐规则，CI lint 以 warning 报告违反（存量可豁免至迁移期结束）
- **MAY** — 可选，不纳入 lint

**合规分级**：新增驱动须满足所有 MUST；存量驱动在基线冻结日前以 warning 模式运行，见 [§17 合规矩阵](#17-合规矩阵与迁移策略)。

---

## 1. 背景与架构目标

`wink-micro-os` 的 **DAL（Device Abstraction Layer）** 承载各类传感器、执行器、显示屏与通信模块的逻辑驱动。随着硬件种类增加，缺乏统一 API 规范会导致：

1. **命名碎片化** — 不同开发者对相同动作混用 `turn_on` / `enable` / `start` / `on`，AI 代码生成器正确率下降。
2. **工具链适配成本高** — `app_codegen.py` 和 `wink.py lint` 难以自动提取设备能力。
3. **应用层学习曲线陡峭** — 掌握 `dal_led` 后无法自然推导 `dal_dc_motor` 或 `dal_ultrasonic` 的使用范式。
4. **安全隐患** — 并发与失效安全缺乏统一契约，在双核 SMP (ESP32-S3) 上产生真实 bug。
5. **跨芯片级别兼容物理阻碍** — 8 位超低端 MCU (8051/STC8/AVR) 存在 RAM 极度匮乏 (<256B)、无硬件 FPU、Harvard 存储区隔离与 Keil C51 Overlay 覆盖分析机制，无法直接运行 32 位全量 C 代码。

### 核心设计目标

| 目标 | 含义 |
|------|------|
| **高度一致性** | 统一生命周期范式、领域动词库与注释契约 |
| **安全优先** | 并发/ISR 安全、失效安全是硬性条款 |
| **零破坏性演进** | 新特性不破坏现有 App/BAL 代码 |
| **两端同源** | Wasm 仿真、ESP32/STM32 硬件与 8051 极简 MCU 共享同一套 YAML SSOT |
| **静态化与零开销** | POD 句柄模式，无 vtable / 无堆分配（ADR-0004），8 位下实现零开销 Flash 引用 |
| **AI 可分析** | codegen 能从 DAL 静态提取能力并生成 role verb |

### 1.3 Profile 分级设计原则 (Profile Tiering)

为实现“**同源 YAML SSOT，两端精准生成**”，`wink-micro-os` 引入 Profile 区分机制：

* **Full Profile (32-bit / POSIX / WASM / STM32 / ESP32)**：使用 `float` 物理量、包含 `owner` 跟踪、支持 32/64 位高精度时间戳与 POD 配置深拷贝句柄。
* **Micro Profile (8-bit / 8051 / STC8 / AVR)**：使用定点数/整数量纲缩放、Flash Zero-Copy 句柄引用、静态硬编码/直接分发、16 位低开销计数器与 `uint8_t` 状态标志。

| 特性维度 | Full Profile (32-bit Target) | Micro Profile (8-bit Target) |
|---------|-----------------------------|------------------------------|
| **适用芯片** | ESP32-S3, STM32, WASM 仿真 | 8051, STC8, AVR, PIC |
| **物理量类型** | `float` / `double` | `int16_t` / `uint16_t` (整数量纲缩放) |
| **控制量刻度** | 归一化浮点 `[-1.0, 1.0]` | 千分比 promille ‰ `[-1000, 1000]` / 0.1度 ddeg |
| **句柄内存模式** | POD 深拷贝 `config_t` (24+ 字节 RAM) | Flash 指针引用 `const WINK_CODE *cfg` (2~4 字节 RAM) |
| **资源归因** | 包含 `const char *owner` 指针 | 可通过 `#ifdef WINK_DISABLE_OWNER_TRACKING` 裁减 |
| **调度与分发** | C 函数调用、C99 `bool` | 静态内联/硬编码宏、`uint8_t` 替代 `bool` |
| **临界区实现** | PAL 总线锁 / 互斥锁 / 原子操作 | `EA` 总中断使能保存/恢复宏 (`WINK_8B_CRITICAL_*`) |

---

## 2. 数据结构与句柄规范

### 2.1 配置结构体 `dal_<type>_config_t`

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-S-001 | MUST | 首个成员 MUST 为 `const char *owner;`（供 device_tree/codegen 静态资源冲突检测与运行时日志归因。DAL 层不直接做资源仲裁；底层资源冲突由 PAL resource claim 机制在 `pal_gpio_init` / `pal_pwm_init` 等调用中检测并返回 `WINK_ERR_BUSY` 或 `WINK_ERR_RESOURCE_EXHAUSTED`） |
| DAL-S-002 | MUST | `owner` MUST 指向**静态存储期**字符串（字符串字面量或 `static const char[]`），MUST NOT 指向栈/堆 |
| DAL-S-003 | SHOULD | 成员按数据类型尺寸降序排列以减少自然对齐填充（如 `uint32_t` → `uint16_t` → `bool`） |
| DAL-S-004 | MUST NOT | 当成员顺序兼做序列化线格式时（见 `apply_override`），MUST NOT 为了填充优化而重排已有成员。**兼容性 > 填充优化** |
| DAL-S-005 | MUST NOT | MUST NOT 使用位域（`uint8_t flags : 3;`）或 `#pragma pack` |

**范例**（`dal_led_config_t`）：

```c
typedef struct {
    const char *owner;     /* 资源占用 owner 静态字符串 */
    uint16_t pin;          /* 逻辑 GPIO 引脚 */
    bool active_high;      /* true: 高电平点亮 */
} dal_led_config_t;
```

### 2.2 实例句柄 `dal_<type>_t`

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-S-010 | MUST | MUST 为 POD (Plain Old Data) 结构体 |
| DAL-S-011 | MUST | **首个成员** MUST 内嵌 `dal_<type>_config_t config;`（`offsetof(dal_xxx_t, config) == 0`） |
| DAL-S-012 | MUST | MUST 包含 `bool initialized;` 状态标志 |
| DAL-S-013 | MUST | MUST 支持 `{0}` 零初始化（所有成员零值为安全默认态） |
| DAL-S-014 | SHOULD | 新增句柄类型 SHOULD 添加 `_Static_assert(offsetof(dal_xxx_t, config) == 0, ...)` |

### 2.3 ABI 稳定性断言

每个 config_t 和 dal_<type>_t SHOULD 添加编译期断言，使任何布局变化在编译时被捕获。

**首选 `offsetof` 断言**（与目标位宽无关，host 64 位测试与 32 位 target 均成立）：

```c
_Static_assert(offsetof(dal_dc_motor_t, config) == 0, "config must be the first member");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 24, "ABI break: initialized offset changed");
```

**整体 `sizeof` 断言须按 target 位宽分档**。`config_t` 含 `const char *owner` 指针，在 ILP32（ESP32 / wasm32）与 64 位 host 测试上尺寸不同，裸写 `sizeof(...) == N` 会在 64 位 host 编译失败：

```c
#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_dc_motor_config_t) == 24, "ABI break: config size changed on 32-bit target");
#else
_Static_assert(sizeof(dal_dc_motor_config_t) == 32, "ABI break: config size changed on 64-bit host");
#endif
```

> 纯数据字段（不含指针）的结构体可直接用无条件 `sizeof` 断言。含指针的句柄/config 优先用 `offsetof` 锁定关键成员偏移。

### 2.4 Micro Profile (8-bit) 句柄与内存规约 (Zero-Copy Flash)

在 8 位 MCU (8051/STC8/AVR) 环境下，由于 RAM 极其匮乏 (128~256B)，深拷贝整个 `config_t` 到句柄 RAM 中会导致严重的内存挤压。

#### 2.4.1 存储区修饰符抽象

为适配 Harvard 哈佛架构，DAL 头文件与生成代码中与存储区相关的修饰符 MUST 使用 PAL 统一抽象宏：

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

#### 2.4.2 Flash 常量配置引用模式 (Zero-Copy)

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-S-001 | MUST | 在 8 位 Profile 下，`dal_<type>_8b_t` 句柄 **MUST NOT** 深拷贝整个 `config_t` 结构体到 RAM |
| DAL-8B-S-002 | MUST | 句柄结构体包含指向 Flash 的只读配置指针：`const WINK_CODE dal_<type>_8b_config_t *cfg;` |
| DAL-8B-S-003 | MUST | 配置结构体 `dal_<type>_8b_config_t` 变量 MUST 加上 `WINK_CODE` 声明在 ROM 中 |
| DAL-8B-S-010 | SHOULD | 当编译宏 `#ifdef WINK_DISABLE_OWNER_TRACKING` 启用时，`config_t` 中可以裁剪掉 `const char *owner` 成员，以在 8 位 MCU 上再省去 2~3 字节指针 |

**句柄定义对比**：

```c
/* 32 位 Full Profile: 深拷贝配置，占用 24+ 字节 RAM */
typedef struct {
    dal_led_config_t config; /* 深拷贝 */
    bool initialized;
} dal_led_t;

/* 8 位 Micro Profile: Zero-Copy Flash 引用，仅占用 3~4 字节 RAM */
typedef struct {
    const WINK_CODE dal_led_8b_config_t *cfg; /* 指向 ROM 的指针 (2 字节) */
    uint8_t initialized;                     /* C51 下 uint8_t 比 bool 更高效 (1 字节) */
} dal_led_8b_t;
```

#### 2.4.3 Micro Profile 静态内存分配禁令 (No-Malloc Ban)

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-S-020 | MUST | 在 Micro Profile 模式下 **MUST NOT** 调用任何动态内存分配函数（`malloc` / `free` / `calloc` / `realloc`）。所有临时 Buffer、驱动句柄与配置数据必须为编译期可确定的静态存储期（`static` / 全局 / ROM）或栈常量，防止 8 位 MCU 堆内存崩溃 |

---

## 3. 生命周期 API

生命周期 API 按适用范围分为三档：**无条件必需**、**按 category 必需**、**推荐**。

### 3.1 无条件必需 (MUST — 所有驱动)

#### `dal_<type>_init`

```c
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_init(dal_<type>_t *dev, const dal_<type>_config_t *cfg);
```

| 规则 ID | 条款 |
|---------|------|
| DAL-L-001 | MUST 校验 `dev` 和 `cfg` 非 NULL |
| DAL-L-002 | Full Profile 下 MUST 将 `cfg` 深拷贝到 `dev->config`；Micro Profile 下 MUST 赋值 Flash 只读指针 `dev->cfg = cfg` (Zero-Copy) |
| DAL-L-003 | 成功时 MUST 置 `dev->initialized = true` (8 位下置 `dev->initialized = 1`) |
| DAL-L-004 | 对已 `initialized` 的设备重复调用 MUST 返回 `WINK_ERR_ALREADY_INITIALIZED`（fail-fast，不隐式 deinit） |
| DAL-L-005 | MUST 对关键配置参数做最小化防御校验（NULL 检查、引脚范围等），即使 codegen 已校验 |
| DAL-L-006 | **执行器**的 init MUST 使输出处于零能量状态（duty=0 / enable 引脚 inactive），严禁 init 即通电 |

#### `dal_<type>_deinit`

```c
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_deinit(dal_<type>_t *dev);
```

| 规则 ID | 条款 |
|---------|------|
| DAL-L-010 | MUST 幂等：未 init 时返回 `WINK_OK`，不 crash |
| DAL-L-011 | MUST 按 ADR-0024 清场顺序：禁用中断/ISR → 等待 in-flight 回调结束（见 DAL-L-012）→ 释放硬件资源 → 清零句柄（`memset` 清零，含 `initialized=false`） |
| DAL-L-012 | 若驱动使用 ISR，MUST 先禁中断 → 等待 in-flight 回调结束 → 再释放资源（防 use-after-deinit） |
| DAL-L-013 | 共享总线（I2C/SPI）的驱动 deinit MUST 仅释放自身 client claim，MUST NOT 销毁总线（bus-owner 管理） |

### 3.2 按 category 必需 (MUST — 仅当 `is_actuator: true`)

#### `dal_<type>_safe_off`

```c
wink_status_t dal_<type>_safe_off(dal_<type>_t *dev);
```

| 规则 ID | 条款 |
|---------|------|
| DAL-L-020 | 仅当 YAML `is_actuator: true` 时 MUST 实现；`is_actuator: false` 的器件（button, encoder, eeprom, gps, ultrasonic）MUST NOT 实现空壳 `safe_off`（YAML `safe_off_fn: ""` 是正确表达） |
| DAL-L-021 | MUST 不标 `WINK_WARN_UNUSED_RESULT`（应急路径不强制检查返回值）；但返回 `wink_status_t` 以报告成功/失败 |
| DAL-L-022 | MUST 幂等 + 未初始化时安全返回 `WINK_OK` |
| DAL-L-023 | MUST NOT 依赖调度器与堆 |
| DAL-L-024 | SHOULD 满足 ISR-safe（见 [§6](#6-并发isr-与线程安全)） |
| DAL-L-025 | `safe_off` 绑定的具体关断原语由 ADR-0048 逐器件裁决（如 dc_motor 绑定 brake，rc_servo 绑定 duty=0），MUST 在头注释声明具体行为 |

### 3.3 推荐 (SHOULD)

| API | 级别 | 说明 |
|-----|------|------|
| `dal_<type>_reset(dev)` | SHOULD | 软件复位，重置器件内部状态机与缓存。语义因器件而异（encoder 的 reset 含义是计数清零，不是硬件复位）。如实现，MUST 在头注释明确语义 |
| `dal_<type>_get_state(dev, *out_state)` | SHOULD | 返回器件统一状态枚举。如实现，签名 MUST 为 `wink_status_t dal_xxx_get_state(const dal_xxx_t *dev, dal_xxx_state_t *out_state)`，不得直接返回枚举值 |

---

## 4. 函数签名与返回值契约

### 4.1 返回值

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-F-001 | MUST | 所有公开 DAL API MUST 返回 `wink_status_t`（`WINK_OK` 为 0，负数为错误），ADR-0001 |
| DAL-F-002 | MUST | MUST NOT 直接返回 `bool` 作为公开 DAL API 的返回值（lint: `STATUS-NOT-BOOL-PUBLIC`） |
| DAL-F-003 | MUST | 布尔谓词查询 MUST 通过出参传递：`wink_status_t dal_xxx_is_pressed(const dal_xxx_t *dev, bool *out_pressed)` |
| DAL-F-004 | MUST | 除以下豁免白名单外，所有返回 `wink_status_t` 的公开 API MUST 标注 `WINK_WARN_UNUSED_RESULT` |

**DAL-F-004 豁免白名单**：

| 豁免 API | 理由 |
|---------|------|
| `safe_off` | 应急路径不强制检查返回值 |
| `poll` | 每 tick 调用的状态机推进函数，大部分调用点有意忽略返回值（"推进一下，失败了下次再推"）；错误通过 `get_status` 查询。强制检查会在事件循环中制造告警噪音 |
| `deinit` | 幂等 no-op，失败时无恢复动作 |

> **注意**：`toggle` 等操作类 API 不在豁免名单中——`toggle` 失败（如 `ERR_NOT_INITIALIZED`）应被检查。`poll` 的返回值对单测和故障诊断仍有价值（`ERR_NOT_INITIALIZED`, `ERR_DISCONNECTED`），白名单仅豁免 `WINK_WARN_UNUSED_RESULT` 属性，不改变返回类型。

### 4.2 参数约定

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-F-010 | MUST | 第一参数 MUST 为实例句柄指针 |
| DAL-F-011 | MUST | **不修改设备状态**的查询类 API（getter / 谓词 / get_state）MUST 使用 `const dal_<type>_t *dev` |
| DAL-F-012 | MUST | **修改设备状态**的操作类 API MUST 使用 `dal_<type>_t *dev`（非 const） |
| DAL-F-013 | MUST | 出参指针 MUST 以 `out_` 前缀命名（如 `bool *out_pressed`, `float *out_speed`） |
| DAL-F-014 | SHOULD | `init` 的第二参数 SHOULD 为 `const dal_<type>_config_t *cfg` |

### 4.3 错误返回时出参状态契约

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-F-020 | MUST | 公开 API 返回非 `WINK_OK` 时，所有 `out_*` 出参 MUST 保持调用前的值不变（MUST NOT 清零或写入半成品数据）。实现方式：出参只在函数末尾、确定返回 `WINK_OK` 前一次性写入 |
| DAL-F-021 | MUST | 调用方 MUST NOT 在错误返回路径读取 out 参数的值 |
| DAL-F-022 | SHOULD | 调用方 SHOULD 在调用前将结构体出参清零（`= {0}`），避免错误路径读到未初始化的栈垃圾 |

### 4.4 `apply_override` 技术债声明

`dal_rc_servo_apply_override(void *dev, ...)` 和 `dal_ultrasonic_apply_override(void *dev, ...)` 的 `void *dev` 是为适配统一函数指针表 `wink_dev_override_fn` 的已知技术债（违反 DAL-F-010），列入合规矩阵例外。收敛计划：未来 `wink_dev_override_fn` 类型参数化后消除 `void *`。

### 4.5 Micro Profile (8-bit) 函数分发与重入契约

在 8051 / Keil C51 环境下，编译器默认使用静态覆盖分析（Overlay Analysis）分配局部变量内存。函数指针与泛型 `void *` 指针会导致分析失效，强迫参数压入极其狭小的 Hardware Stack。

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-F-001 | MUST | 8 位 Profile 下 **MUST NOT** 使用 `void *dev` 虚分发与函数指针表 (Function Pointer Tables) |
| DAL-8B-F-002 | MUST | 驱动方法 MUST 为具名静态函数或内联函数（如 `dal_led_8b_on(dal_led_8b_t *dev)`），允许编译器进行完整的 Overlay 覆盖分析 |
| DAL-8B-C-001 | MUST | 8 位 Profile 下不使用原子指令，临界区保护统一使用 `EA` 保存/恢复宏：<br/>`#define WINK_8B_CRITICAL_ENTER() do { uint8_t _ea_save = EA; EA = 0;`<br/>`#define WINK_8B_CRITICAL_EXIT() EA = _ea_save; } while(0)` |
| DAL-8B-C-002 | MUST | 临界区内部 **MUST NOT** 调用含有耗时 busy-wait 或复杂状态机推进的代码 |
| DAL-8B-C-010 | MUST | 若某个 DAL API 既可能在 ISR 中被调用，又可能在主循环 (Task) 中被调用，在 Keil C51 环境下该 API **MUST** 加上 `reentrant` 关键字声明，或设计为完全无局部变量的内联宏 |

---

## 5. 动词语义模型与命名规范

### 5.1 函数命名格式

```
dal_<type>_<verb>[_<object>](dal_<type>_t *dev, ...)
```

- `<type>`：器件类型名，与 YAML `type` 字段和目录名一致（`led`, `dc_motor`, `rc_servo`, `button`, `encoder`, `ultrasonic`, `mono_oled`, `eeprom`, `gps`）
- `<verb>`：来自本规范允许的动词集
- `<object>`：可选的操作对象，用于细化语义（如 `set_speed`, `get_count`, `draw_text`）

### 5.2 动词语义三元模型

本仓库刻意区分三种数据访问语义，新增驱动 MUST 遵循：

| 前缀 | 语义 | 是否碰硬件 | 是否可能阻塞 | 示例 |
|------|------|-----------|-------------|------|
| `read_*` | 触发一次物理采样并返回结果 | ✅ | 可能 | `dal_ultrasonic_read(dev, &cm)` |
| `get_*` | 读取已缓存的值或最后设定值，不触发硬件 | ❌ | ❌ | `dal_ultrasonic_get_cached_distance(dev, &cm)`, `dal_dc_motor_get_speed(dev, &speed)` |
| `get_state` / `get_status` | 查询驱动/器件状态机 | ❌ | ❌ | `dal_eeprom_get_status(dev, &state)` |

> **关键契约**: `read_*` 调用后硬件状态改变（已触发测量/采样），`get_*` 调用后硬件状态不变。这是 API 使用者的核心假设，破坏它是安全隐患。

### 5.3 按 category 分组的标准动词库

分组直接复用 codegen YAML 的 `category` 字段（7 类：`actuator`, `output`, `input`, `sensor`, `display`, `storage`, `comm`），不引入新的分类词。

#### 5.3.1 执行器与输出类 (category: actuator / output)

涵盖器件：LED, Relay, DC Motor, RC Servo, Stepper Motor, Buzzer.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `on` | `(dev)` | 开启/点亮 | `dal_led_on(dev)` |
| `off` | `(dev)` | 关闭/熄灭 | `dal_led_off(dev)` |
| `toggle` | `(dev)` | 翻转开关状态 | `dal_led_toggle(dev)` |
| `set` | `(dev, bool on)` | 显式设置开关 | `dal_led_set(dev, true)` |
| `set_<property>` | `(dev, <type> val)` | 设置物理量 | `dal_dc_motor_set_speed(dev, 0.8f)`, `dal_rc_servo_set_angle(dev, 90.0f)` |
| `get_<property>` | `(const dev, <type> *out_val)` | 读回最后设定值（不碰硬件） | `dal_dc_motor_get_speed(dev, &speed)` |
| `is_<pred>` | `(const dev, bool *out_pred)` | 布尔状态查询 | — |
| `brake` | `(dev)` | 执行器制动（电机专用，ADR-0048） | `dal_dc_motor_brake(dev)` |
| `coast` | `(dev)` | 执行器滑行/自由转动 | `dal_dc_motor_coast(dev)` |
| `safe_off` | `(dev)` | 应急关断（§8） | `dal_dc_motor_safe_off(dev)` |

**黑名单**（禁用词）：❌ `turn_on`, `enable_output`, `run_motor`, `spin`, `start_pwm`

#### 5.3.2 传感器与输入类 (category: sensor / input)

涵盖器件：Ultrasonic, Button, Encoder, Temp/Humidity, IMU, Photoelectric.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `read` | `(dev, *out)` | 同步阻塞读取（MUST 标 `WINK_BLOCKING`） | `dal_ultrasonic_read(dev, &cm)` |
| `read_<metric>` | `(dev, *out)` | 读取具体物理量 | `dal_ultrasonic_read_cm(dev, &cm)` |
| `request_<op>` | `(dev)` | 非阻塞请求（三段式首步） | `dal_ultrasonic_request_measurement(dev)` |
| `poll` | `(dev)` | 推进状态机 | `dal_button_poll(dev)`, `dal_gps_poll(dev)` |
| `get_cached_<metric>` | `(const dev, *out)` | 读取缓存结果（不碰硬件） | `dal_ultrasonic_get_cached_distance(dev, &cm)` |
| `get_<property>` | `(const dev, *out)` | 读取缓存值 | `dal_encoder_get_count(dev, &count)`, `dal_button_get_edge_count(dev, &count)` |
| `is_<condition>` | `(const dev, bool *out)` | 快速条件判断 | `dal_button_is_pressed(dev, &pressed)` |
| `was_<condition>` | `(dev, bool *out)` | 边沿事件检测（读后清） | `dal_button_was_pressed(dev, &pressed)` |
| `calibrate` / `zero` | `(dev)` | 校准/清零 | `dal_encoder_reset(dev)` |
| `reset_<counter>` | `(dev)` | 清零计数器 | `dal_button_reset_edge_count(dev)` |

**黑名单**：❌ `fetch_data`, `sample_now`, `get_dist`（缩写不清晰）

> **注意**：`get_value` 不再列入黑名单。`get_*` 系列在本仓库有明确语义（读缓存/读设定值），是合法动词。

#### 5.3.3 显示屏类 (category: display)

涵盖器件：Mono OLED, TFT LCD, Segment LED, E-Paper.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `clear` | `(dev)` | 清空显存 | `dal_mono_oled_clear(dev)` |
| `flush` | `(dev)` | 将显存刷入物理屏幕 | `dal_mono_oled_flush(dev)` |
| `draw_<shape>` | `(dev, ...)` | 绘制基础图形 | `dal_mono_oled_draw_text(dev, col, page, str)` |
| `set_brightness` | `(dev, level)` | 设置背光/亮度 | — |

**黑名单**：❌ `clean_screen`, `refresh_display`, `light_on`

#### 5.3.4 通信与存储类 (category: storage / comm)

涵盖器件：EEPROM, SPI Flash, GPS, LoRa, CAN Node.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `read` / `read_blocking` | `(dev, addr, buf, len)` | 阻塞读取数据块 | `dal_eeprom_read_blocking(dev, addr, buf, len)` |
| `write` / `write_blocking` | `(dev, addr, buf, len)` | 阻塞写入数据块 | `dal_eeprom_write_blocking(dev, addr, buf, len)` |
| `request_read` / `request_write` | `(dev, addr, len)` | 非阻塞请求（三段式） | `dal_eeprom_request_read(dev, addr, len)` |
| `poll` | `(dev)` | 推进状态机 | `dal_eeprom_poll(dev)`, `dal_gps_poll(dev)` |
| `get_<result>` | `(dev, *buf, len)` | 获取异步操作结果 | `dal_eeprom_get_read_result(dev, buf, len)` |
| `get_status` | `(const dev, *out)` | 查询操作状态 | `dal_eeprom_get_status(dev, &state)` |
| `get_position` | `(const dev, *out)` | GPS 定位结果 | `dal_gps_get_position(dev, &pos)` |
| `erase` | `(dev, addr, len)` | 擦除存储扇区 | — |

### 5.4 器件特有 API

当器件有不属于上述标准动词的特有能力时（如 CAN 过滤器配置、IMU 滤波深度）：

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-V-001 | MUST | 使用具名 typed API：`dal_<type>_<specific_verb>(dev, const dal_<type>_<arg>_t *arg)` |
| DAL-V-002 | MUST | 在 YAML 中标记 `device_specific: true`，使其不进入通用 role verb 平面 |
| DAL-V-003 | MUST NOT | MUST NOT 使用 `control(cmd, void *arg)` 形式的 IOCTL 窗口 — 它摧毁类型安全与 codegen 可分析性（违背 ADR-0004 静态分发精神） |

---

## 6. 并发、ISR 与线程安全

本章是安全相关的硬性条款。ESP32-S3 是双核 SMP，`volatile` 不产生 acquire/release 内存屏障。

### 6.0 Task-to-Task 并发默认契约

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-C-040 | MUST | DAL 实例默认**非线程安全**：同一 `dal_xxx_t *dev` 的方法调用、以及 init/deinit 与其他方法之间，MUST 由调用方在外部串行化（同一 mutex / 同一 task / 消息队列） |
| DAL-C-041 | MUST | 仅当驱动在头注释显式声明 `Thread-safe: Yes` 并说明锁/无锁机制时，才允许对同一实例并发调用 |
| DAL-C-042 | MUST | `Thread-safe` Contract 字段缺失时默认按 `No` 解释，lint SHOULD 对公开 API 缺该字段报 warning |
| DAL-C-043 | MAY | **不同** `dal_xxx_t` 实例（不同 dev 指针）之间 MAY 并发调用，前提是它们不共享底层资源（如同一 I2C 总线的两个设备仍需外部串行化——由 PAL 总线锁保证） |

### 6.1 volatile 使用约束

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-C-001 | MUST | 跨 ISR/跨核共享字段仅允许 **"单字宽 + 单写者 + 读者容忍旧值"** 模式 |
| DAL-C-002 | MUST | 任何对 volatile 字段的 read-modify-write（如 `count += delta`）MUST 使用 PAL 原子操作或临界区（`PAL_CRITICAL_SECTION`） |
| DAL-C-003 | MUST NOT | MUST NOT 仅凭 `volatile` 声明就在注释中写"无需临界区"，除非满足 DAL-C-001 的三个条件 |

### 6.2 多字段快照一致性

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-C-010 | MUST | 若驱动句柄含多个跨 ISR/跨核共享的 volatile 字段（如 `ultrasonic` 的 `last_distance` + `last_pulse_us` + `last_status` + `state`），MUST 在头注释中声明读取顺序契约（先读 payload 后读 state，或反之），或使用以下方案之一确保快照一致性 |

推荐方案（按复杂度排列）：

1. **读序契约 + 状态版本号**（最简单）：在头注释声明"先读 state，若 == READY 再读 payload；payload 有效性以读到 state==READY 为准"，并在 ISR 写端保持"先写 payload 后写 state"的顺序。
2. **单原子快照结构 + 版本号**：将相关字段打包为一个结构体，用 `_Atomic` 或临界区做整体交换。
3. **seqlock**：读端循环检查序列号，写端 ISR 在修改前后递增序列号。

### 6.3 ISR 安全

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-C-020 | MUST | ISR 上下文 MUST NOT：分配/释放内存、取互斥锁、调用日志 API、执行阻塞操作 |
| DAL-C-021 | SHOULD | 可在 ISR 上下文安全调用的 API **计划**标注 `WINK_ISR_SAFE` 属性宏（风格对齐 `WINK_BLOCKING`）。该宏当前**尚未在 `wink_status.h` 定义**，待配套 ADR 落地后启用；在此之前以 DAL-C-022 的 Contract 注释声明为准 |
| DAL-C-022 | MUST | API Contract 注释的 `ISR-safe` 字段 MUST 如实声明 |

### 6.4 回调上下文归属

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-C-030 | MUST | 驱动的事件回调在哪个上下文（ISR / task / poll 循环）中被调用 MUST 在头注释中明确声明 |
| DAL-C-031 | MUST | 声明回调内允许调用哪些类别的 API（如"允许 WINK_BLOCKING" 或 "仅允许 ISR-safe API"） |

**现存范例**：`dal_button_event_cb` 的注释明确声明"在 `dal_button_poll()` 的 task 上下文同步调用，非 ISR；允许调用 `WINK_BLOCKING` API 但建议保持短小"。新增驱动的回调 MUST 达到相同声明精度。

### 6.5 deinit 与 ISR 竞态

见 [§3.1 DAL-L-011/012](#31-无条件必需-must--所有驱动)：若驱动使用 ISR，deinit MUST 按顺序 **先禁中断 → 等待 in-flight 回调结束 → 释放硬件资源 → 清零句柄（含 initialized=false）**。

---

## 7. 阻塞、超时与异步模式

### 7.1 阻塞 API 标注

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-B-001 | MUST | 任何可能阻塞调用者的 API MUST 同时满足：(a) 函数名带 `_blocking` 后缀或名字本身暗示阻塞（如 `read`）；(b) 标注 `WINK_BLOCKING` 属性宏 |
| DAL-B-001a | MUST | **命名选择规则**：当同一器件**同时**提供阻塞与非阻塞两种形态时，阻塞变体 MUST 使用 `_blocking` 后缀以明确区分（如 `dal_eeprom_read_blocking` vs `dal_eeprom_request_read`）；当该操作**仅存在阻塞形态**时，允许使用裸动词（如 `read`）。历史遗留：`dal_ultrasonic_read` 同时存在阻塞 `read` 与非阻塞 `request_measurement`，按新规应命名为 `read_blocking`；因其属公开 API，不做破坏性改名，计划在迁移期为其补 `@deprecated`/`WINK_DEPRECATED` 别名并纳入退役轨道（跟踪号待定） |
| DAL-B-002 | MUST | 有 `_blocking` 后缀但未标 `WINK_BLOCKING`，或标了 `WINK_BLOCKING` 但无后缀，均为 lint error |
| DAL-B-003 | MUST | API Contract 注释的 `Blocking` 字段 MUST 给出最坏阻塞时间的**数值上界**（如 "Yes, worst-case ≈ 38ms"），不得仅写 "Yes(ms)" 占位 |
| DAL-B-004 | MUST | 阻塞 API MUST 在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫内声明（ADR-0017 层 2 隔离） |

**现存范例**：

```c
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
// Blocking: Yes. Worst-case ≈ 60ms+.

WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);

WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
```

### 7.2 超时来源与延时规约

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-B-010 | MUST | 超时值 MUST 来自 config 字段或编译期常量宏，MUST NOT 硬编码在函数体内 |
| DAL-B-011 | MUST | 非阻塞 API MUST NOT 内部 busy-wait 超过 100μs，否则必须改为 request/poll 三段式 |
| DAL-B-012 | MUST | 微秒/毫秒级等待 **MUST NOT** 使用空循环忙等（如 `for(int i=0; i<N; i++)`），必须统一调用 PAL 时钟/延时原语（`pal_delay_us()` / `pal_os_get_ms()`），防止 ESP32 (240MHz) 与 8051 (12MHz) 之间产生高达 250 倍的指令耗时漂移 |

### 7.3 异步三段式 (request / poll / get_result)

本仓库的主流异步模式是三段式状态机，优先于回调模式，契合协作式调度与双 target 同源。

#### 命名契约

```
dal_<type>_request_<op>(dev, ...)   → 提交请求，立即返回
dal_<type>_poll(dev)                → 每 tick 调用，推进状态机
dal_<type>_get_<op>_result(dev, *out) / dal_<type>_get_cached_<metric>(dev, *out) → 获取结果
dal_<type>_get_status(dev, *out_state) → 查询状态机
```

#### 状态机基线

```
IDLE ──request──▶ BUSY ──[完成]──▶ DONE/READY ──get_result──▶ IDLE
                    │                                            ▲
                    ├──[失败]──▶ ERROR ──get_result──────────────┘
                    │
                    └──request──▶ return WINK_ERR_BUSY (不改变状态)
```

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-B-020 | MUST | 状态值域 MUST 包含：`IDLE` / `BUSY` / `DONE` (或 `READY`) / `ERROR` |
| DAL-B-021 | MUST | BUSY 时重复 `request_*` MUST 返回 `WINK_ERR_BUSY`，不改变状态 |
| DAL-B-022 | MUST | `poll` 在 IDLE / DONE / ERROR 时 MUST 为 no-op |
| DAL-B-023 | MUST | `get_*_result` 成功读取后 MUST 将状态机重置为 IDLE |
| DAL-B-024 | MUST | 三段式的 `get_cached_*` / `get_*_result` 在从未执行过 `request_*` 时（state == IDLE）MUST 返回 `WINK_ERR_BUSY`（或 `WINK_ERR_EMPTY`），MUST NOT 返回 `WINK_OK`。这确保调用方不会误读初始化时的零值为有效测量结果 |

**现存范例**：`dal_eeprom` (request_read/request_write → poll → get_status/get_read_result) 和 `dal_ultrasonic` (request_measurement → get_cached_distance) 是已验证的参考实现。

### 7.4 与仿真 Asyncify 的关系

阻塞 API 在 Wasm target 下通过 Asyncify 挂起/恢复实现。DAL 作者约束：

- 阻塞函数体内 MUST NOT 使用裸汇编或平台特定的等待原语
- MUST 通过 PAL 层的 `pal_delay_ms` / `pal_os_sleep_ms` / semaphore 实现等待
- 详见 `reviews/2026-06-24-phase1-asyncify-deep-dive`

---

## 8. 失效安全与应急路径

### 8.1 safe_off 语义裁决

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-E-001 | MUST | `safe_off` 不是独立的新语义，它是"绑定到某个具体关断原语"。头注释 MUST 声明具体行为 |
| DAL-E-002 | MUST | `safe_off` 会在 watchdog / panic / assert 失败 / 异常回滚路径被调用，因此 MUST 尽量简单、确定性好 |

**safe_off 的两种代码形态**：

| 形态 | 说明 | 示例 |
|------|------|------|
| **具名 safe_off API** | 驱动暴露独立的 `dal_xxx_safe_off` 函数符号 | `dal_dc_motor_safe_off`, `dal_rc_servo_safe_off` |
| **YAML 绑定的关断函数** | 驱动无独立 safe_off 符号，由 `config.safe_off_fn` 指向某个已有原语，codegen/actuator_registry 消费 | led → `safe_off_fn: dal_led_off`（头文件中不存在 `dal_led_safe_off` 符号） |

两者都是 DAL-E-001 意义上的"绑定到具体关断原语"，但代码形态不同。

**现存语义**（ADR-0048 裁决）：

| 器件 | safe_off 绑定 | 形态 | 物理后果 |
|------|-------------|------|--------|
| dc_motor | brake (H 桥短接制动) | 具名 API | 电机急停 |
| rc_servo | duty=0 (失去保持力) | 具名 API | 舵机 limp = 安全 |
| led | `dal_led_off` | YAML 绑定 | LED 熄灭 |

### 8.2 系统级 safe-all

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-E-010 | SHOULD | 执行器 SHOULD 注册到 `wink_actuator_registry`，使系统能在故障时一次性遍历所有执行器 `safe_off` |

### 8.3 Init 零能量

见 [DAL-L-006](#31-无条件必需-must--所有驱动)：执行器 init 成功后输出 MUST 处于零能量状态。严禁 init 即通电或保持上一次 duty。

**零能量与 Init-to-Ready 的关系**：执行器 init 成功后即**立即接受控制指令**（Init-to-Ready），零能量只是默认输出值而非额外的使能闸门。MUST NOT 出于安全考虑引入 `enable()` / `arm()` 前置调用——否则破坏 Init-to-Ready 契约（DAL-BC-001）。安全关断由 `safe_off` 承担，而非 init 后的待使能态。

---

## 9. 单位、量纲与值域

### 9.1 参数命名

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-U-001 | MUST | 所有物理量参数与出参名 MUST 带单位后缀 |
| DAL-U-002 | MUST | 无量纲归一化参数 MUST 在参数名中体现（`_norm` / `_ratio`），并在注释声明值域 |

**标准后缀**：

| 后缀 | 单位 | 现存使用 |
|------|------|---------|
| `_cm` | 厘米 | `distance_cm` |
| `_mm` | 毫米 | `alt_mm` |
| `_ms` | 毫秒 | `long_press_ms`, `debounce_ms`, `write_time_ms` |
| `_us` | 微秒 | `last_pulse_us` |
| `_hz` | 赫兹 | `pwm_freq_hz` |
| `_deg` | 角度 | `course_deg` |
| `_udeg` | 微度 (1e-6°) | `lat_udeg`, `lon_udeg` |
| `_kmh` | 公里/小时 | `speed_kmh` |
| `_pct` | 百分比 | — |
| `_dps` | 度/秒 (角速度) | — (IMU 预留) |
| `_mps2` | m/s² (加速度) | — (IMU 预留) |
| `_mv` | 毫伏 | — (ADC 预留) |
| `_raw` | 原始 ADC 计数 | — (ADC 预留) |
| `_c` | 摄氏度 | — (温湿度预留) |

> **关于 `_norm`**：`_norm` 后缀不编码正负号区间。有符号归一化（如 dc_motor speed `[-1.0, 1.0]`）与无符号归一化（如 duty `[0, 1.0]`）的值域区分以 API Contract 注释的 `Range` 字段为权威，不在后缀中引入 `_snorm` / `_unorm` 区分。

### 9.2 值域声明

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-U-010 | MUST | API Contract 注释 MUST 在 `Range` 字段声明参数合法值域 |
| DAL-U-011 | MUST | 对执行器的控制量（speed / angle / duty），越界行为 MUST 明确声明为**饱和截断 (saturate)** 或**返回 `WINK_ERR_INVALID_ARG`** 二选一 |

**范例**：

```c
/**
 * @param speed -1.0 (full reverse) … 1.0 (full forward).
 *        0.0 = coast. Out of range → saturate to [-1.0, 1.0].
 *        When config.invert == true, direction sense is swapped.
 */
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);

/**
 * @param angle 0.0 ~ effective_max_angle (度). 超出范围自动钳位 (saturate).
 */
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle);
```

### 9.3 8 位 Micro Profile 整型量纲与缩放规范

在 8 位 Micro Profile 模式下，DAL 公开 API **MUST NOT** 使用 `float` 或 `double` 数据类型（因无 FPU 支持会导致软浮点库膨胀与运行缓慢）。所有物理量与归一化控制量 MUST 转换为固定量纲的整型（`int16_t` / `uint16_t` / `int8_t`）。

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-8B-U-001 | MUST | 8 位 Profile 下公开 API 物理量参数与出参 MUST 使用整型与显式量纲后缀 |
| DAL-8B-U-002 | MUST | 归一化控制量（如速度、占空比）MUST 使用 **千分比 (promille, ‰)** 整数表示，取值范围 `[-1000, 1000]` 或 `[0, 1000]` |
| DAL-8B-U-003 | SHOULD | 角度物理量 SHOULD 使用 **0.1 度 (deci-degree, ddeg)** 或 **1 度** 整数表示 |
| DAL-8B-T-001 | MUST | 8 位 Profile 下时间戳与计数值默认使用 `uint16_t` (最大 65535 ms / us) |
| DAL-8B-T-002 | MUST | 状态标志与布尔值 **MUST** 使用 `uint8_t` 代替 `bool`（在 Keil C51 中 `uint8_t` 直接对应 R0-R7 寄存器，运算效率远高于 `bool`） |

**标准 8 位 Micro Profile 量纲对照表**：

| 物理量 | Full Profile (32-bit) 类型 | Micro Profile (8-bit) 类型 | 单位说明与取值范围 |
|--------|---------------------------|----------------------------|-------------------|
| 速度归一化 | `float speed` (`[-1.0, 1.0]`) | `int16_t speed_promille` | 千分比，`[-1000, 1000]` |
| 占空比 | `float duty` (`[0.0, 1.0]`) | `uint16_t duty_promille` | 千分比，`[0, 1000]` |
| 舵机角度 | `float angle_deg` (`[0.0, 180.0]`) | `uint16_t angle_ddeg` | 0.1 度，`0 ~ 1800` (180.0°) |
| 距离 | `float distance_cm` | `uint16_t distance_mm` | 毫米，`0 ~ 65535 mm` |
| 毫秒延迟/超时 | `uint32_t timeout_ms` | `uint16_t timeout_ms` | 毫秒，`0 ~ 65535 ms` (最大 65.5s) |
| 微秒脉冲 | `uint32_t pulse_us` | `uint16_t pulse_us` | 微秒，`0 ~ 65535 us` |

---

## 10. 事件回调规范

### 10.1 注册 API

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-CB-001 | MUST | 回调注册动词统一使用 `on_event`（与现存 `dal_button_on_event` 一致） |
| DAL-CB-002 | MUST | 上下文参数命名统一使用 `ctx`（不用 `user_data`） |
| DAL-CB-003 | MUST | 传入 `cb = NULL` MUST 注销回调（无需额外 unregister API） |

**签名范式**：

```c
/* 回调类型 */
typedef void (*dal_<type>_event_cb)(dal_<type>_event_t evt, void *ctx);

/* 注册 API */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_on_event(dal_<type>_t *dev, dal_<type>_event_cb cb, void *ctx);
```

### 10.2 回调上下文声明

见 [§6.4 DAL-C-030/031](#64-回调上下文归属)。每个回调注册 API 的头注释 MUST 声明：

1. 回调在哪个上下文被调用（ISR / task / poll 循环）
2. 回调内允许调用的 API 类别
3. 回调是否允许阻塞

### 10.3 全局钩子

如有进程级（非 per-device）的钩子（如 `dal_button_set_irq_hook`），MUST 与 per-device 回调分开声明，并放入 BAL 内部头文件（如 `dal_button_bal.h`），不暴露在公开冻结 API 面。

---

## 11. 裁剪、Stub 与禁用态

### 11.1 编译期裁剪 (WINK_USE_xxx)

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-P-001 | MUST | 每个驱动 MUST 支持 `WINK_USE_<TYPE>` CMake 开关裁剪 |
| DAL-P-002 | MUST | 裁剪禁用时，所有公开 API 声明 MUST 带 `WINK_UNAVAILABLE_MSG(WINK_<TYPE>_DISABLED_MSG)` 标注，使调用方得到友好的编译错误而非链接失败 |
| DAL-P-003 | MUST | `WINK_<TYPE>_DISABLED_MSG` 的文本 MUST 指引用户如何启用（"add a \"xxx\" device to wink-app.json"） |
| DAL-P-004 | MUST | 阻塞 API 的裁剪 stub MUST 同时在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫内（守卫对称性） |

### 11.2 Stub 实现（功能未完成态）

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-P-010 | MUST | Stub 实现的 API MUST 返回 `WINK_ERR_UNSUPPORTED` |
| DAL-P-011 | MUST | Stub MUST NOT claim 任何硬件资源 |
| DAL-P-012 | MUST | Stub MUST NOT 置 `initialized = true`（避免上层误判设备可用） |
| DAL-P-013 | MUST | Stub 的头注释 MUST 标注 `@experimental Stub: returns WINK_ERR_UNSUPPORTED until ... lands` |
| DAL-P-014 | SHOULD | YAML SHOULD 使用 `experimental: true` 标记实现未完成的驱动 |

**现存范例**：`dal_gps` 和 `dal_eeprom` 的 init 均为 stub，注释明确声明"当前 stub 实现将 *dev 清零，dev->initialized=false，不 claim 资源"。

### 11.3 实现成熟度

YAML 中 `experimental: true` 表示"接口可能变动 + 实现可能不完整"。建议未来扩展为：

- `experimental: true` → 接口不稳定，codegen MAY 不生成 role binding
- `implementation_status: stub | partial | complete`（待 ADR 裁决后引入）

---

## 12. 双 Target 一致性

### 12.1 源码层约束

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-T-001 | MUST | `dal/` 层源码 MUST NOT 出现 `#ifdef SIMULATION` / `#ifdef ESP_PLATFORM` / `#ifdef __EMSCRIPTEN__` 等平台宏（下沉到 PAL/targets） |
| DAL-T-002 | MUST | 同一 `.c` 源文件 MUST 同时进入 ESP32 和 Wasm 两个 target 构建，禁止 per-target 分叉源文件 |
| DAL-T-003 | MUST | 时间相关 API MUST 通过 PAL 时钟（`pal_os_get_ms`, `pal_delay_ms`），MUST NOT 直接调用平台 API |

### 12.2 行为差异声明

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-T-010 | SHOULD | 两端行为有差异时（如仿真不模拟 I2C NAK），SHOULD 在头注释新增 `Simulation-parity` 字段声明 |

**范例**（`dal_ultrasonic.h`）：

```
 * Sim 分支：跳过物理 GPIO 配置（旁路最低物理信号层，ADR-0003 决策2），仅置结构状态。
 * ESP32：自动初始化 RMT 硬件脉冲捕获；RMT 失败自动降级到 busy-wait。
```

---

## 13. 向后兼容与演进规则

### 13.1 红线 (MUST)

| 规则 ID | 条款 |
|---------|------|
| DAL-BC-001 | **Init-to-Ready**：`init()` 成功后器件默认进入可用状态。MUST NOT 要求旧调用方显式调用新增的 `start()` 才能工作。**例外**：执行器 init 后 MUST 处于零能量态（可用但不输出） |
| DAL-BC-002 | **句柄末尾追加**：`dal_<type>_t` 新增字段 MUST 追加在结构体末尾，保证 `{0}` 清零初始化不受影响 |
| DAL-BC-003 | **Config 只追加不重排**：`dal_<type>_config_t` 新增成员 MUST 追加在末尾，MUST NOT 重排已有成员（因 `apply_override` 按成员顺序反序列化） |
| DAL-BC-004 | **保持同步 API 存续**：增加异步/回调 API 时，原有的同步 API MUST 完整保留 |
| DAL-BC-005 | **兼容性 > 填充优化**：DAL-BC-003 与"按尺寸降序排列"可能冲突，此时兼容性优先 |

### 13.2 函数级 Deprecation 与退役策略

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-BC-020 | MUST | 废弃公开 DAL 函数 MUST 用 `WINK_DEPRECATED_MSG("use dal_xxx_new() instead; will be removed in vN+2")` 标注旧函数 |
| DAL-BC-021 | MUST | 废弃的同版本 MUST 提供替代 API，并在头注释 `@deprecated` 指向新 API |
| DAL-BC-022 | MUST | 废弃函数 MUST 至少保留**两个 minor 版本**窗口后才可删除（与 button BAL rename 先例一致） |
| DAL-BC-023 | MUST | 删除已废弃函数 MUST 走 ADR 并在 changelog 列出 |

**现存先例**：`WINK_DEPRECATED_MSG` 已在 `wink_status.h` 中定义；`wink_button_events.h` 已使用 `WINK_DEPRECATED("use wink_button_enable_events (ADR-0032 B-class)")` 标注旧 API。

### 13.3 结构体尺寸断言

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-BC-010 | SHOULD | config_t 和 dal_xxx_t SHOULD 添加编译期 ABI 断言。首选 `offsetof(...) == N`（位宽无关，详见 [§2.3](#23-abi-稳定性断言)）；含指针的结构体若用整体 `sizeof` 断言 MUST 按 `INTPTR_MAX` 分 32/64 位两档，避免在 64 位 host 测试上误报 |
| DAL-BC-011 | SHOULD | `apply_override` 的 params 反序列化 SHOULD 有显式长度校验（不仅依赖 `len` 参数隐式判版本） |

---

## 14. 错误码与可观测性

### 14.1 错误码层级

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-EC-001 | MUST | 通用错误码（`WINK_OK`, `WINK_ERR_INVALID_ARG`, `WINK_ERR_NOT_INITIALIZED`, `WINK_ERR_TIMEOUT`, `WINK_ERR_BUSY`, `WINK_ERR_UNSUPPORTED`, `WINK_ERR_IO`）从 `wink_status.h` 全局定义 |
| DAL-EC-002 | MUST | DAL API MUST 优先使用通用错误码 |
| DAL-EC-003 | SHOULD | 如需器件特有错误码（如未来可为 GPS 定义 `WINK_ERR_GPS_NO_FIX`，当前 `wink_status.h` 尚未定义），SHOULD 通过头文件中的 `#define` 宏定义并在 `wink_status.h` 预留数值范围 |

### 14.2 init 幂等性

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-EC-010 | MUST | 对已 `initialized` 的设备调用 `init` MUST 返回 `WINK_ERR_ALREADY_INITIALIZED`，不做任何操作（fail-fast） |
| DAL-EC-011 | MUST | MUST NOT 隐式先 deinit 再 init（有竞态风险） |

### 14.3 日志约定

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-EC-020 | SHOULD | `init` 成功 SHOULD 输出 INFO 级别日志（含 owner 和关键配置） |
| DAL-EC-021 | SHOULD | `init` 失败 SHOULD 输出 WARN 级别日志（含错误码和失败原因） |
| DAL-EC-022 | MUST | ISR 上下文 MUST NOT 调用日志 API |
| DAL-EC-023 | SHOULD | 日志 tag SHOULD 使用 `"dal_<type>"` 格式 |

### 14.4 配置验证职责

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-EC-030 | MUST | `init` 函数 MUST 做最小化防御校验（NULL 检查、关键字段范围），即使 codegen 已在生成时校验 |
| DAL-EC-031 | SHOULD | codegen 层 SHOULD 做完整语义校验（字段约束、值域、枚举合法性）。两层防御 |

---

## 15. API Contract 注释模板

所有公开头文件函数 MUST 标注以下 Contract 元数据。带 ★ 为必填字段：

```c
/**
 * @brief 一句话描述 API 功能。
 *
 * 详细说明（可选）。
 *
 * @param dev  器件实例句柄
 * @param ...  其他参数（含单位、值域）
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: ★ dev 非 NULL；init 已成功。
 *   - Postconditions: ★ 成功时的状态变化。
 *   - Range: ★ 参数值域与单位（如 speed: [-1.0, 1.0] normalized）。
 *   - Blocking: ★ No / Yes (worst-case Xms)。
 *   - Thread-safe: ★ No（默认值；缺失时按 No 解释，见 DAL-C-042）。
 *   - ISR-safe: ★ No / Yes。
 *   - Reentrancy: No / Yes。
 *   - Simulation-parity: 两端行为差异说明（如有）。
 *   - Error-codes: ★ WINK_OK / WINK_ERR_xxx / ...
 */
```

> **Thread-safe 默认值**：`Thread-safe` 字段缺失时默认按 `No` 解释（见 [§6.0 DAL-C-040/042](#60-task-to-task-并发默认契约)）。lint SHOULD 对公开 API 缺该字段报 warning。

**现存最佳范例**：`dal_dc_motor_set_speed`、`dal_ultrasonic_request_measurement`、`dal_button_init` 已覆盖大部分字段。`led` / `encoder` 部分函数缺少完整 Contract 块，属迁移项。

---

## 16. Codegen YAML 集成

### 16.1 SSOT 原则

驱动的全集 SSOT 是 `wink-micro-os/codegen/drivers/*.yaml`（ADR-0046）。本规范不复述 YAML schema，仅说明 API 规范与 YAML 字段的对应关系。

**YAML schema 版本**：当前锁定 `codegen_schema: "1.1"`。

### 16.2 规范与 YAML 字段映射

| 本规范条款 | YAML 字段 | 说明 |
|-----------|----------|------|
| §1.3 Profile 分级支持 | `profiles` | 声明驱动支持的 Profile（如 `[full, micro_8bit]`） |
| §2.4 / §9.3 8位类型重映射 | `profile_overrides.micro_8bit` | 定义 8 位 Micro Profile 专用的 `config_type`、`handle_type` 与整数量纲类型 |
| §3.2 safe_off 按 category | `is_actuator: true/false` + `config.safe_off_fn` | `safe_off_fn: ""` = 该器件无安全关断语义 |
| §5.3 按 category 分组 | `category: actuator/output/input/sensor/display/storage/comm` | 直接复用，不引入新分类词 |
| §5.4 器件特有 API | 未来 `device_specific: true` | 待 codegen 支持 |
| §11.2 Stub | `experimental: true` | 标记实现未完成 |
| §13 兼容性 | `codegen_schema: "1.1"` | schema 版本变更需评审 |

### 16.3 真实 YAML Profile 多 Target 支持参考

`codegen/drivers/*.yaml` 增加 Profile 分级表达式示例（以 `rc_servo.yaml` 为参考）：

```yaml
codegen_schema: "1.1"     # schema 版本号
type: rc_servo             # 器件类型名
category: actuator         # 分类
is_actuator: true          # 执行器标记
experimental: false
default_role: angular_actuator

# 声明支持的 Profile 列表
profiles:
  - full
  - micro_8bit

# 8 位 Profile 专用重映射 (Zero-Copy Flash 配置与整数量纲)
profile_overrides:
  micro_8bit:
    config_type: dal_rc_servo_8b_config_t
    handle_type: dal_rc_servo_8b_t
    value_types:
      angle: { type: uint16_t, unit: deci_deg, range: [0, 1800] }

config:                    # Full Profile 默认 C 类型映射
  c_type: dal_rc_servo_t
  config_type: dal_rc_servo_config_t
  headers: [dal_rc_servo.h]
  deinit_fn: dal_rc_servo_deinit
  safe_off_fn: dal_rc_servo_safe_off

role_bindings:             # Role verb → Jinja 模板 (支持两端分发)
  angular_actuator:
    verbs:
      set_angle:
        template_full: "dal_rc_servo_set_angle(&{{dev}}, {{angle_deg}}f);"
        template_micro_8bit: "dal_rc_servo_8b_set_angle(&{{dev}}, {{angle_ddeg}});"
```

---

## 17. 合规矩阵与迁移策略

### 17.1 现状合规矩阵 (v2.1.0 基线)

图例：✅ 合规 / ❌ 不合规 / — 不适用 / ⚠ 部分合规

> 本基线冻结于 v2.1.0，覆盖本轮新增的 MUST 条款（DAL-C-040、DAL-F-020、DAL-B-024 等）。下方矩阵聚焦生命周期与注释形态；完整逐条合规状态见 [§17.3.1](#1731-规则实施状态)。
>
> 🌟 **Golden Reference (黄金参考驱动样板)**：正式标定 `led` 驱动 (`dal_led.h/c` 与 `dal_led_8b.h/c`) 为双 Profile 黄金参考实现。新增驱动开发与 Code Review 必须以 `led` 驱动的接口范式、ABI 断言、Contract 注释与剪枝守卫格式为标准样板。

| 驱动 | init | deinit | safe_off | const getter | Contract 注释 | WINK_BLOCKING 标注 |
|------|------|--------|----------|-------------|---------------|-------------------|
| led (⭐Golden Ref) | ✅ | ✅ | ⚠ (别名 off) | — | ⚠ 部分缺 | — |
| dc_motor | ✅ | ✅ | ✅ (brake) | ✅ | ✅ | — |
| rc_servo | ✅ | ✅ | ✅ (duty=0) | — | ✅ | — |
| button | ✅ | ✅ | — | ✅ | ⚠ 部分缺 | — |
| encoder | ✅ | ✅ | — | ✅ | ⚠ 缺 Contract | — |
| ultrasonic | ✅ | ✅ | — | ✅ | ✅ | ✅ |
| mono_oled | ✅ | ✅ | — | — | ✅ | — |
| eeprom | ✅ | ✅ | — | ✅ | ✅ | ✅ |
| gps | ✅ | ✅ | — | ✅ | ✅ | ✅ |

### 17.2 迁移策略

| 阶段 | 范围 | 规则模式 | 截止 |
|------|------|---------|------|
| v2.1 发布 | 冻结基线矩阵（见 §17.1） | — | 本文档合并时 |
| 新增驱动 | 新增 YAML + 头文件 | 所有 MUST 规则以 **error** 模式执行 | 立即生效 |
| 存量驱动 | 已有 9 个驱动 | MUST 规则以 **warning** 模式执行 | v2.2 迁移完成 |
| v2.2 | 存量提级 | warning → error | 待定（建议 2 个迭代内） |

### 17.3 规则 ID 与 lint 集成

| 要素 | 约定 |
|------|------|
| 规则 ID 格式 | `DAL-<category>-<number>`（如 `DAL-S-001`, `DAL-L-010`） |
| Lint 引擎 ID | `dal.<snake_case_rule>`（如 `dal.config_owner_first`），与 `wink-tools/tools/lint/` 的 `<pack>.<rule>` 命名法对齐 |
| 所属 pack | DAL 规则使用独立 pack `dal`（`wink lint --pack dal`），不塞进现有 `api` 或 `layering` pack |
| 例外标注 | 代码内：`// lint-allow: DAL-S-001 (reason)`；YAML 内：`lint_exceptions` 字段（待引擎支持） |

### 17.3.1 规则实施状态

每条 MUST/SHOULD 规则按以下级别标注实施状态：

| 级别 | 含义 |
|------|------|
| `lint-enforced` | CI 已有 lint 规则拦截 |
| `review-enforced` | 纯语义约束，靠代码评审保障（如"越界饱和还是报错"） |
| `pending` | 待实现（需给 issue 跟踪号，避免"永远 pending"） |

**核心规则实施状态**（完整表待 `dal` lint pack 落地后补齐）：

| 规则 ID | 条款摘要 | 实施状态 | 跟踪 |
|---------|---------|----------|------|
| DAL-F-001 | 返回 wink_status_t | `lint-enforced` (api pack: `STATUS-NOT-BOOL-PUBLIC`) | — |
| DAL-F-002 | 禁止 bool 返回值 | `lint-enforced` (api pack: `STATUS-NOT-BOOL-PUBLIC`) | — |
| DAL-T-001 | dal/ 禁 #ifdef 平台宏 | `lint-enforced` (layering pack) | — |
| DAL-S-001 | config 首成员 owner | `pending` | `dal` lint pack 首迭代落地（issue 待建） |
| DAL-C-040 | 默认非线程安全 | `review-enforced` | — |
| DAL-U-010 | Range 值域声明 | `review-enforced` | — |
| DAL-E-001 | safe_off 声明具体行为 | `review-enforced` | — |
| DAL-F-020 | 错误返回时出参不变 | `review-enforced` | — |
| DAL-B-012 | 严禁空循环忙等 | `review-enforced` | — |
| DAL-8B-S-001 | 8位禁句柄深拷贝 config | `review-enforced` | — |
| DAL-8B-S-002 | 8位句柄包含 ROM 指针 | `review-enforced` | — |
| DAL-8B-S-020 | 8位模式静态内存禁令 (No-malloc) | `review-enforced` | — |
| DAL-8B-U-001 | 8位 API 禁用 float | `pending` | `dal.8bit` lint pack 跟踪 |
| DAL-8B-U-002 | 归一化量使用千分比 ‰ | `review-enforced` | — |
| DAL-8B-F-001 | 8位禁用 void* 虚分发 | `review-enforced` | — |
| DAL-8B-C-001 | 8位使用 EA 中断保护 | `review-enforced` | — |
| DAL-8B-T-010 | 8位符合 ANSI C89 / SDCC CI 构建检查 | `pending` | CI 集成 SDCC 跟踪 |

### 17.4 已知例外

| 规则 | 违规点 | 原因 | 收敛计划 |
|------|--------|------|---------|
| DAL-F-010 | `dal_rc_servo_apply_override(void *dev, ...)` | 适配 `wink_dev_override_fn` 统一函数指针 | 待 override 类型参数化后消除 |
| DAL-F-010 | `dal_ultrasonic_apply_override(void *dev, ...)` | 同上 | 同上 |
| DAL-S-003 | `dal_dc_motor_config_t` 成员顺序 | 按 config member order 序列化（DAL-S-004 优先） | 不修复，兼容性优先 |

---

## 附录 A. 功耗模式 (Reserved — MUST NOT 实现)

> **状态**：Reserved (Phase N, 待 PM 框架 ADR)

当前无系统级 PM (Power Management) 框架，0 个驱动实现 `suspend` / `resume`。

```c
/* ⚠ RESERVED — 当前 MUST NOT 实现 */
wink_status_t dal_<type>_suspend(dal_<type>_t *dev);
wink_status_t dal_<type>_resume(dal_<type>_t *dev);
```

**为什么不能现在实现**：空壳 `suspend` 比没有 `suspend` 更危险——上层以为已省电/已停止输出，实际器件仍在运行。

**开放设计问题**（待 PM ADR 回答）：

1. `suspend` 期间执行器是否保持输出？
2. `resume` 后是否恢复 suspend 前的 duty/角度？
3. 未 `suspend` 时调 `resume` 返回什么？
4. 与系统级 PM 框架的集成方式？

---

## 附录 B. 源文件编码

| 规则 ID | 级别 | 条款 |
|---------|------|------|
| DAL-ENC-001 | MUST | 所有 DAL 相关源文件（`.h`, `.c`, `.yaml`）MUST 为 **UTF-8 无 BOM** 编码 |

---

## 附录 C. 文档定位与变更治理

本文件定位为 **DAL 开发编码规范**（位于 `dal-development-guide/`），面向驱动开发者和 AI 代码生成器提供可查阅的实操规范。

- **架构真相** → [`docs/design/02-wink-micro-os/01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md)
- **设计决策** → 各 ADR
- **Codegen SSOT** → `codegen/drivers/*.yaml` + `codegen/roles/*.yaml`

### 变更治理

| 变更类型 | 流程 |
|---------|------|
| MUST 条款的新增、删除或语义变更 | MUST 走 ADR |
| SHOULD / MAY 条款变更 | 可由维护者评审合入 |
| 涉及 DAL 架构契约（生命周期、并发模型、分发机制）的 Accepted 变更 | MUST 同步回写 `01-dal-device-abstraction.md` 活规范 |
| 纯实操约定（命名后缀、lint ID 格式） | 不需要回写活规范 |

所有变更 MUST 在本文件头部变更历史中记录版本号与摘要。
