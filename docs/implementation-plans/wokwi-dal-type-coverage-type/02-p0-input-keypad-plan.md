# DAL 外设执行计划：input/keypad

| 项 | 内容 |
|---|---|
| **计划名称** | `input/keypad` 矩阵键盘外设驱动与 Codegen 落地计划 |
| **所属批次** | P0 批次 (核心 HMI 人机交互按键矩阵外设) |
| **映射 Wokwi 组件** | `membrane-keypad` (4x4 / 3x4 薄膜矩阵键盘) |
| **驱动文件路径** | `wink-micro-os/dal/include/input/dal_keypad.h`<br>`wink-micro-os/dal/src/input/dal_keypad.c` |
| **Codegen 描述** | `wink-micro-os/codegen/drivers/keypad.yaml`（**ADR-0051：YAML 为 SSOT**） |
| **关联规范** | [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3)、[`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) §4 (#3 `keypad` $\rightarrow$ `binary_sensor`)、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、[ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) |
| **前置依赖状态** | 零欠账！底座仅依赖已存在的 `pal_gpio_*` 与 `pal_resource`，**已完全落地** |
| **计划状态** | ✅ 已实现（2026-08-06 完全落地与通过 100% 真实测试验证） |

> **落地加固总结（2026-08-06）**：
> 1. **驱动与拓扑落地**：完成了 `dal_keypad.h` 与 `dal_keypad.c`，引入 `dal_keypad_variant_t`（`MATRIX_4X4` 与 `MATRIX_3X4`），实现非阻塞逐列低电平扫描与软件消抖。
> 2. **Codegen 规范对齐**：按 Schema 1.1 新建 `keypad.yaml` 与 `keypad_init.c.j2`，绑定 `binary_sensor` 默认 Role，支持 `membrane_keypad_4x4` / `membrane_keypad_3x4` 市场型号别名。
> 3. **ABI 冻结实测**：回填实测 `_Static_assert`（32位 32/40/48 字节，64位 40/48/56 字节），并在 `test_dal_abi_freeze.c` 中完成冻结断言。
> 4. **自动化测试 100% 通过**：独立 Host 构建下 `ctest -R test_dal_keypad` 6/6 Tests 100% Passed。
> 5. **静态架构门禁**：`wink lint` 六向架构门禁 Clean 无任何告警！

---

## 1. 需求与硬件映射

### 1.1 Wokwi 组件与控制语义
- **`membrane-keypad`** 属于 HMI 矩阵键盘输入组件（4 行 4 列 / 4 行 3 列）。
- **底层物理输入**：GPIO 逐列低电平扫描 + 行引脚上拉输入检测 (Row-Column Scanning)。
- **DAL 抽象控制语义**：**按键字符/键值读取与非阻塞消抖检测 (Keypad Matrix Scanning & Debouncing)**。
  - `get_key` / `read_key`：读取当前按下的 ASCII 字符（如 `'1'`, `'A'`, `'*'`，无按键按下返回 `'\0'`）。
  - `is_pressed`：查询是否有任意按键按下（映射 Role `binary_sensor` 契约）。
  - `poll`：非阻塞时间戳轮询扫描并完成防抖 (Debounce)。

### 1.2 量纲与 A/B 分类（ADR-0056）
- `keypad` 属于 **B 类 sensor_measurement**（硬件输入采集 $\rightarrow$ App）。
- 离散按键信号，量纲标记为：
  - `quantity: binary`
  - `quantity_class: sensor_measurement`

---

## 2. 硬件拓扑分类与 Variant 架构分析 (依据 dal-best-practices.md §3)

### 2.1 架构设计原则
1. **对外 API 绝对冻结**：统一为 `dal_keypad_get_key` / `dal_keypad_is_pressed` / `dal_keypad_poll`，绝不因 4x4 或 3x4 变体改变函数签名。
2. **拓扑为一等公民 (`variant`)**：使用内部拓扑枚举 `dal_keypad_variant_t` 消化行列数与键值映射表差异。
3. **C 头文件绝对无具体芯片/型号名**：头文件中无任何型号字符串，具体型号与键值地图由 Codegen 别名表与配置覆盖。

### 2.2 市面主流矩阵键盘全量盘点

| 硬件拓扑分类 (Variant) | 市面典型产品/模块型号 | 物理结构与引脚 | 默认键值映射字符矩阵 |
|---|---|---|---|
| **`MATRIX_4X4` (4x4 矩阵, 默认)** | Wokwi `membrane-keypad`, 4x4 膜按键模块, 4x4 微动开关矩阵板 | 4 行 + 4 列 (8 引脚) | `1 2 3 A`<br>`4 5 6 B`<br>`7 8 9 C`<br>`* 0 # D` |
| **`MATRIX_3X4` (3x4 电话矩阵)** | 3x4 电话按键面板, 3x4 金属门禁键盘 | 4 行 + 3 列 (7 引脚) | `1 2 3`<br>`4 5 6`<br>`7 8 9`<br>`* 0 #` |

### 2.3 Codegen 市场型号别名映射表 (Market Alias Mapping)

```yaml
aliases:
  membrane_keypad_4x4: { variant: matrix_4x4, num_rows: 4, num_cols: 4, debounce_ms: 10 }
  membrane_keypad_3x4: { variant: matrix_3x4, num_rows: 4, num_cols: 3, debounce_ms: 10 }
```

---

## 3. 数据结构设计 (`dal_keypad.h`)

遵照 [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) §2 及 `DAL-S-001` / `DAL-S-006` / `DAL-S-011` / `DAL-HDR-NO-HAL`：

```c
#ifndef DAL_KEYPAD_H
#define DAL_KEYPAD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
/* 严格遵循 DAL-HDR-NO-HAL: 禁止 include "pal_hal.h" */

#ifdef __cplusplus
extern "C" {
#endif

#define DAL_KEYPAD_MAX_ROWS 4
#define DAL_KEYPAD_MAX_COLS 4

/**
 * @brief 矩阵键盘拓扑变体枚举 (Topology Variant - 一等公民)
 */
typedef enum {
    DAL_KEYPAD_VARIANT_MATRIX_4X4 = 0, /**< 4x4 矩阵键盘 (默认) */
    DAL_KEYPAD_VARIANT_MATRIX_3X4 = 1, /**< 3x4 电话矩阵键盘 */
} dal_keypad_variant_t;

/**
 * @brief 矩阵键盘配置结构体 (POD config_t)
 * 成员按对齐降序排列：owner 指针 → uint16_t → int16_t → enum → bool/uint8
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称 (DAL-S-001: 必须为首成员指针) */
    uint16_t debounce_ms;           /**< 消抖时间毫秒 (默认 10ms) */
    int16_t row_pins[DAL_KEYPAD_MAX_ROWS]; /**< 行 GPIO 引脚数组 (必填) */
    int16_t col_pins[DAL_KEYPAD_MAX_COLS]; /**< 列 GPIO 引脚数组 (未用引脚为 -1) */
    dal_keypad_variant_t variant;   /**< 拓扑变体枚举 */
    uint8_t num_rows;               /**< 实际行数 (1~4, 默认 4) */
    uint8_t num_cols;               /**< 实际列数 (1~4, 默认 4) */
    bool active_low;                /**< 扫描极性: true=内部上拉+低电平列扫描 (默认) */
} dal_keypad_config_t;

/**
 * @brief 矩阵键盘句柄结构体 (POD instance_t)
 */
typedef struct {
    dal_keypad_config_t config;     /**< 配置副本 (DAL-S-011: 值副本且 offsetof == 0) */
    uint32_t last_scan_ms;          /**< 上次有效按键扫描时间戳 */
    char last_key;                  /**< 当前按下的字符 ('\0' 表示无键) */
    uint8_t last_row;               /**< 当前按下的行号 (0xFF 表示无键) */
    uint8_t last_col;               /**< 当前按下的列号 (0xFF 表示无键) */
    bool is_pressed;                /**< 当前是否有有效键按下 */
    bool initialized;               /**< 初始化状态标记 (DAL-L-004) */
    volatile wink_status_t last_status; /**< 最近一次操作错误码 (DAL-B-025 可观测性) */
} dal_keypad_t;

/* Static assertions for ABI freeze and first-member guard (DAL-S-011 / DAL-S-014) */
_Static_assert(offsetof(dal_keypad_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_keypad_config_t) == 32, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_keypad_t, initialized) == 40, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_keypad_t) == 48, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_keypad_config_t) == 40, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_keypad_t, initialized) == 48, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_keypad_t) == 56, "ABI break: handle size changed on 64-bit host");
#endif

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_init(dal_keypad_t *dev, const dal_keypad_config_t *cfg);

wink_status_t dal_keypad_deinit(dal_keypad_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_key(dal_keypad_t *dev, char *out_key);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_is_pressed(const dal_keypad_t *dev, bool *out_pressed);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_poll(dal_keypad_t *dev, bool *out_changed, char *out_key);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_status(const dal_keypad_t *dev, wink_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* DAL_KEYPAD_H */
```

---

## 4. C 驱动核心逻辑接口与行/列扫描算法 (`dal_keypad.c`)

### 4.1 初始化与资源独占
- 在 `dal_keypad_init()` 中：
  - 循环遍历 `cfg->row_pins` 与 `cfg->col_pins`，通过 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, owner)` 独占全部引脚。
  - 将所有行引脚配置为 `PAL_GPIO_INPUT_PULLUP`（输入上拉）。
  - 将所有列引脚配置为 `PAL_GPIO_OUTPUT_PUSH_PULL` 并拉高（高电平空闲）。

### 4.2 非阻塞逐列扫描算法 (Column Scan Algorithm)
- 在 `dal_keypad_poll()` / `dal_keypad_get_key()` 中：
  1. 依次将 `col_pins[c]` 拉低 (LOW)。
  2. 读取各行 `row_pins[r]` 的电平：若拉低（LOW），说明行 `r` 与列 `c` 之间的按键被闭合按下。
  3. 扫描完列 `c` 后立即将 `col_pins[c]` 恢复拉高 (HIGH)。
  4. 结合非阻塞毫秒时间戳 `dal_os_get_ms()` 进行 `debounce_ms` 软件消抖判定。
  5. 依据 `variant` 查找 4x4 或 3x4 字符表，返回对应字符（如 `'1'`, `'A'`, `'*'`）。

---

## 5. 运行时防线 (5 Safety Guards) 落实

| 防线 | 条款标准 | 落地实现方案 |
|---|---|---|
| **Guard A: 低功耗** | 省电机制 | 扫描完成后将所有列引脚拉高/High-Z，无静态电流消耗 |
| **Guard B: Bus 共享** | 资源独占 | `init()` 时循环索取所有有效行脚与列脚的 `PAL_RESOURCE_GPIO_PIN`，任意一脚冲突自动全量解绑并滚回 |
| **Guard C: Zero-as-Default** | 零值推导 | 默认 `variant = DAL_KEYPAD_VARIANT_MATRIX_4X4`；`debounce_ms == 0` 推导默认 10ms |
| **Guard D: 硬件托底** | 物理输出驱动 | 逐列恢复拉高，防止多列同时拉低引发对接短路 |
| **Guard E: 非阻塞** | `WINK_STRICT_NONBLOCKING` | 严禁使用 `delay_ms()` 死等消抖！使用非阻塞时间戳判定 |

---

## 6. Codegen 驱动描述 (`keypad.yaml`)

新建 `wink-micro-os/codegen/drivers/keypad.yaml`（ Schema 1.1 ）：

```yaml
codegen_schema: "1.1"
type: keypad
category: input
is_actuator: false
experimental: false
default_role: binary_sensor

aliases:
  membrane_keypad_4x4: { variant: matrix_4x4, num_rows: 4, num_cols: 4, debounce_ms: 10 }
  membrane_keypad_3x4: { variant: matrix_3x4, num_rows: 4, num_cols: 3, debounce_ms: 10 }

quantity: binary
quantity_class: sensor_measurement

fields:
  owner:
    tier: stable
    type: string
    emit: none
  debounce_ms:
    tier: advanced
    type: int
    default: 10
  variant:
    tier: advanced
    type: enum
    enum: [matrix_4x4, matrix_3x4]
    default: matrix_4x4
    map:
      matrix_4x4: DAL_KEYPAD_VARIANT_MATRIX_4X4
      matrix_3x4: DAL_KEYPAD_VARIANT_MATRIX_3X4
  active_low:
    tier: advanced
    type: bool
    default: true
  role:
    tier: stable
    type: string
    emit: none

quantities:
  owner:       { quantity_class: sensor_measurement }
  debounce_ms: { quantity_class: sensor_measurement }
  variant:     { quantity_class: sensor_measurement }
  active_low:  { quantity_class: sensor_measurement }
  role:        { quantity_class: sensor_measurement }

config:
  c_type: dal_keypad_t
  config_type: dal_keypad_config_t
  headers: [input/dal_keypad.h]
  init_fn: dal_keypad_init
  deinit_fn: dal_keypad_deinit
  init_template_file: templates/keypad_init.c.j2
  safe_off_fn: ""

role_bindings:
  binary_sensor:
    headers: []
    verbs:
      is_active:
        template: "WINK_WARN_UNUSED_RESULT static inline bool {{ name }}_is_active(void) { bool p = false; (void)dal_keypad_is_pressed(&{{ name }}, &p); return p; }"
      is_active_status:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_is_active_status(bool *out_active) { return dal_keypad_is_pressed(&{{ name }}, out_active); }"
```

---

## 7. Wasm / Wokwi 仿真映射契约

- **Wokwi 元件**：`membrane-keypad`
- **引脚映射**：R1-R4、C1-C4 引脚连接 MCU GPIO。
- **仿真行为**：
  - WASM PAL 通过 `js_pal_gpio_read/write` 模拟行列扫描状态，响应 Wokwi 前端 `membrane-keypad` 的按键按下事件。

---

## 8. 交付 Checklist 与验证步骤

- [x] **1. 脚手架生成与文件搭建**：创建 `dal_keypad.h` / `dal_keypad.c` / `keypad.yaml` / `keypad_init.c.j2`。
- [x] **2. C 驱动实现**：实现 4x4 / 3x4 逐列扫描、多 Pin 独占申请与消抖判定。
- [x] **3. 单元测试编写**：编写 `wink-micro-os/test/unit/dal/test_dal_keypad.c`。
- [x] **4. 编译与测试验证**：运行 `python wink-tools/wink.py build host`，并在真实二进制下跑通 `ctest -R test_dal_keypad` 100% Pass。
- [x] **5. 静态 Lint 门禁**：运行 `wink lint` 六向架构门禁全绿无告警。
- [x] **6. 闭环状态更新**：更新 `00-master-execution-plan.md` 进度状态。

