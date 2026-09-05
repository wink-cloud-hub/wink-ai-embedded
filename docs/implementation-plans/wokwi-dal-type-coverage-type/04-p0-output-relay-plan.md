# DAL 外设执行分计划：output/relay

| 项 | 内容 |
|---|---|
| **计划名称** | `output/relay` 继电器外设驱动与 Codegen 落地计划 |
| **所属批次** | P0 批次 (首个无欠账零 PAL 依赖外设) |
| **映射 Wokwi 组件** | `ks2e-m-dc5` (继电器开关模块) |
| **驱动文件路径** | `wink-micro-os/dal/include/output/dal_relay.h`<br>`wink-micro-os/dal/src/output/dal_relay.c` |
| **Codegen 描述** | `wink-micro-os/codegen/drivers/relay.yaml`（**ADR-0051：YAML 为 SSOT**） |
| **关联规范** | [`dal-role-architecture-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md) (SSOT)、[`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3)、[`dal-best-practices.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3 (拓扑枚举原则)、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、[ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) |
| **前置依赖状态** | 零欠账！底座仅依赖已存在的 `pal_gpio_*` 与 `pal_resource`，**可立即执行** |
| **计划状态** | ✅ 已实现（2026-08-05，经 ADR-0058 加固） |

> **实现偏差说明（2026-08-05，ADR-0058；2026-08-11 SSOT v2.3.3 规范对齐）**：以下计划为初稿草案；最终落地以 ADR-0058 与 SSOT v2.3.3 为准，关键偏差：
> 1. **API 重命名对齐 §5.3 动词规范**：`dal_relay_turn_on/off/set_state` → `dal_relay_on/off/set`（与 `dal_led_on/off/set` 一致；`turn_*` 为黑名单动词）。
> 2. **变体精简与 SSR 降级为 Alias（SSOT v2.3.3 Boundary B 遵循）**：固态继电器 (SSR) 引脚拓扑与 `direct_gpio` 100% 一致 (`[VCC, GND, IN]`)，按 Boundary B 从 C 语言变体枚举中降级为 Codegen 市场型号别名（`G3MB-202P` / `FOTEK-SSR-25DA` 自动映射至 `direct_gpio`）；变体仅保留 `DIRECT_GPIO` 与 `LATCHING_DUAL_PIN`。
> 3. **新增 `dal_relay_safe_off`** 且 `is_actuator: true`，codegen 自动注册到 `wink_actuator_registry`（故障安全关断）。
> 4. **磁保持 poll 自动注册**：`relay.yaml` 设 `config.poll_fn: dal_relay_poll`，codegen 生成 `WINK_DEFINE_POLL_THUNK` 并 `wink_runtime_register_poll`（runtime tick 自动清脉冲），无需 App 手动调用。
> 5. **init 对磁保持按 `initial_state` 发一次 SET/RESET 脉冲建立已知态**；deinit 文档如实声明磁保持不保证物理触点断开（非阻塞路径 RESET 脉宽不足）。
> 6. **脉宽校验**：`pulse_duration_ms` 0→默认 50ms；>1000ms 拒绝（防 uint16 65s 烧线圈）；`set_state` 磁保持 break-before-make。
> 7. 新增 `DAL_RELAY_DEFAULT_PULSE_MS`/`DAL_RELAY_MAX_PULSE_MS` 常量、`dal_relay_get_last_status()` getter、完整 `@note API Contract` 块；host 新增 `pal_host_get_gpio_level` 电平捕获钩子；16 项单测全绿。

---

## 1. 需求分析与硬件语义映射

### 1.1 Wokwi 元件与控制语义
- **`ks2e-m-dc5`** 属于数字电磁继电器开关模块。
- **底层物理控制**：GPIO 高/低电平信号控制继电器线圈吸合与断开。
- **DAL 抽象控制语义**：**开关二值状态控制 (Binary State Switch)**。
  - 吸合 (`turn_on` / `activate`)：闭合常开触点 (NO-COM)，继电器导通。
  - 断开 (`turn_off` / `deactivate`)：释放触点，继电器断开。
  - 翻转 (`toggle`)：反转当前继电器状态。

### 1.2 电平极性与初始状态配置
- 支持高电平触发与低电平触发（通过配置 `active_low` 消化拓扑差异，遵照 `drive_mode` 拓扑避风港原则）。
- 支持配置上电默认初始状态 (`initial_state`)。

---

## 2. 硬件拓扑分类与 Variant 架构分析 (依据 `dal-best-practices.md` §3)

### 2.1 架构设计原则
遵照 [`dal-best-practices.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3「同类芯片 / 模块：语义不变 + 拓扑枚举」设计心法：
1. **对外 API 绝对冻结**：统一为 `turn_on` / `turn_off` / `toggle` / `set_state` / `is_on`，绝不因具体硬件型号改变 API。
2. **拓扑为一等公民 (`variant`)**：使用内部拓扑枚举 `dal_relay_variant_t` 消化驱动逻辑差异。禁止在 C 代码中出现任何具体芯片型号名宏（如禁写 `#ifdef WINK_USE_SRD05VDC`）。
3. **Codegen 别名映射**：市面具体芯片/模块名仅作为 Codegen 设备树别名，由 `app_codegen` 自动映射为 `variant` + 默认引脚极性。

### 2.2 市面主流继电器产品/系列全量统计与拓扑盘点

| 硬件拓扑分类 (Variant) | 市面典型芯片/模块型号 | 电气驱动机制 | 引脚需求 | 静态功耗特性 |
|---|---|---|---|---|
| **`DIRECT_GPIO` (单脚直驱/光耦)** | `KS2E-M-DC5`, `SRD-05VDC-SL-C` (松乐), `HK4100F`, `HF32F`, `OMRON G5Q`, Arduino 1/2/4/8路光耦继电器板 (EL817+8550) | MCU GPIO 直驱 NPN/PNP 或通过光耦隔离驱动单线圈 | 1× GPIO (`pin`) | 较高 (吸合时线圈持续消耗 50-100mA 电流) |
| **`SSR` (固态继电器)** | `OMRON G3MB-202P`, `Fotek SSR-25DA`, `MOC3041` (光耦可控硅) | 内部光耦触发双向可控硅/MOSFET，过零/零点交叉触发，无机械触点 | 1× GPIO (`pin`) | 极低 (控制段仅需 ~5mA 光耦电流)，零机械磨损与火花 |
| **`LATCHING_DUAL_PIN` (双线圈磁保持/双稳态)** | `Hongfa HFE10/HFE20`, `Panasonic TX-S-L2`, `OMRON G6K-2F-L2` | 内部永磁铁保持，通过 Set 脉冲线圈吸合，Reset 脉冲线圈释放 (需 30~50ms 脉冲) | 2× GPIO (`pin` + `reset_pin`) | **零静态功耗** (仅脉冲瞬间耗电，掉电后物理触点状态保持) |
| **`LATCHING_SINGLE_PIN` (单线圈 H 桥磁保持)** | `Panasonic TX-1`, `OMRON G6K-2F-L`, `Hongfa HFE9` | 单线圈正向脉冲电流吸合，反向脉冲电流释放 (需要 H 桥或双 GPIO 极性反转) | 2× GPIO (`pin` + `reset_pin`) | **零静态功耗** (通过电流方向决定 Set/Reset) |

### 2.3 Codegen 市场型号别名映射表 (Market Alias Mapping)

Codegen 编译器根据 `wink-app.json` 中配置的 `driver_ic` 别名，自动推导 DAL `variant` 与参数配置：

```json
{
  "aliases": {
    "KS2E-M-DC5":        { "variant": "direct_gpio", "active_low": false },
    "SRD-05VDC-SL-C":   { "variant": "direct_gpio", "active_low": true },
    "G3MB-202P":        { "variant": "ssr",         "active_low": false },
    "FOTEK-SSR-25DA":   { "variant": "ssr",         "active_low": false },
    "HFE10-L2":         { "variant": "latching_dual_pin", "pulse_duration_ms": 50 },
    "G6K-2F-L2":        { "variant": "latching_dual_pin", "pulse_duration_ms": 30 }
  }
}
```

---

## 3. 数据结构设计 (`dal_relay.h`)

遵循 [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) §2 结构体规范及 `DAL-S-001` / `DAL-S-006` / `DAL-S-011` / `DAL-S-014` / `DAL-HDR-NO-HAL`：

```c
#ifndef DAL_RELAY_H
#define DAL_RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
/* 严格遵循 DAL-HDR-NO-HAL: 禁止 include "pal_hal.h" */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 继电器电气拓扑枚举 (Topology Variant - 一等公民)
 */
typedef enum {
    DAL_RELAY_VARIANT_DIRECT_GPIO         = 0, /**< 单 GPIO 直驱/光耦隔离 (经典单线圈, 默认) */
    DAL_RELAY_VARIANT_SSR                 = 1, /**< 固态继电器 (SSR, 零机械触点) */
    DAL_RELAY_VARIANT_LATCHING_DUAL_PIN   = 2, /**< 双线圈磁保持继电器 (双脚脉冲触发, 零静态功耗) */
    DAL_RELAY_VARIANT_LATCHING_SINGLE_PIN = 3, /**< 单线圈 H 桥磁保持继电器 (正反向脉冲) */
} dal_relay_variant_t;

/**
 * @brief 继电器配置结构体 (POD config_t)
 * 成员按对齐降序排列：owner 指针 → uint16_t → int16_t → enum → bool 标志
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称 (DAL-S-001: 必须为首成员指针) */
    uint16_t pin;                   /**< 主控制 / Set 引脚 (DAL-S-006: 必填 uint16_t) */
    int16_t reset_pin;              /**< Reset 引脚 (磁保持拓扑专用; 可选 → int16_t, -1 表示未绑定) */
    uint16_t pulse_duration_ms;     /**< 磁保持脉冲宽度 (ms, 默认 50ms) */
    dal_relay_variant_t variant;    /**< 拓扑变体枚举 (DAL-S-001) */
    bool active_low;                /**< 触发极性: false=高有效, true=低有效 */
    bool initial_state;             /**< init 后的初始状态: true=默认吸合, false=默认断开 */
} dal_relay_config_t;

/**
 * @brief 继电器句柄结构体 (POD instance_t)
 * 支持 Flash 动态覆写 (ADR-0008)
 */
typedef struct {
    dal_relay_config_t config;      /**< 配置副本 (DAL-S-011: 值副本且 offsetof == 0) */
    uint32_t pulse_start_ms;        /**< 磁保持脉冲输出起始时间 (用于非阻塞关脉冲) */
    bool is_on;                     /**< 当前逻辑开关状态: true=吸合/导通, false=断开 */
    bool pulse_active;              /**< 磁保持脉冲是否处于输出中 */
    bool initialized;               /**< 初始化状态标记 (DAL-L-004) */
    volatile wink_status_t last_status; /**< 最近一次操作错误码 (DAL-B-025 可观测性) */
} dal_relay_t;

/* DAL-S-014: 首成员偏移静态断言 */
_Static_assert(offsetof(dal_relay_t, config) == 0,
               "config must be the first member");

#ifdef __cplusplus
}
#endif

#endif /* DAL_RELAY_H */
```

---

## 4. C 驱动核心 API 接口设计

遵循 `DAL-F-*` 命名范式与 `wink_status_t` 错误码契约：

```c
/**
 * @brief 初始化继电器外设并设置为初始状态
 * @param dev 继电器句柄指针
 * @param cfg 静态配置指针（内部拷贝为值副本）
 * @return WINK_OK 成功；
 *         WINK_ERR_INVALID_ARG 参数为空；
 *         WINK_ERR_ALREADY_INITIALIZED 重复初始化 (DAL-L-004)；
 *         WINK_ERR_BUSY GPIO 被其它外设占用
 */
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);

/**
 * @brief 释放继电器外设资源，并自动断开线圈（安全状态）
 * @param dev 继电器句柄指针
 * @return WINK_OK 成功
 */
wink_status_t dal_relay_deinit(dal_relay_t *dev);

/**
 * @brief 设置继电器开关状态
 * @param dev 继电器句柄指针
 * @param on  true=吸合/导通, false=断开
 * @return WINK_OK 成功；WINK_ERR_NOT_INITIALIZED 未初始化
 */
wink_status_t dal_relay_set_state(dal_relay_t *dev, bool on);

/**
 * @brief 吸合/导通继电器
 * @param dev 继电器句柄指针
 * @return WINK_OK
 */
wink_status_t dal_relay_turn_on(dal_relay_t *dev);

/**
 * @brief 断开/释放继电器
 * @param dev 继电器句柄指针
 * @return WINK_OK
 */
wink_status_t dal_relay_turn_off(dal_relay_t *dev);

/**
 * @brief 翻转继电器开关状态
 * @param dev 继电器句柄指针
 * @return WINK_OK
 */
wink_status_t dal_relay_toggle(dal_relay_t *dev);

/**
 * @brief 查询继电器当前是否处于吸合状态
 * @param dev    继电器句柄指针
 * @param out_on 输出状态指针
 * @return WINK_OK
 */
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);

/**
 * @brief 轮询继电器脉冲定时器（针对磁保持拓扑，非阻塞清除脉冲）
 * @param dev 继电器句柄指针
 * @return WINK_OK
 */
wink_status_t dal_relay_poll(dal_relay_t *dev);
```

---

## 5. 运行时防线 (5 Safety Guards) 落实

| 防线 | 条款标准 | 落地实现方案 |
|---|---|---|
| **Guard A: 低功耗** | 关断省电 | 普通线圈继电器吸合时持续消耗 50-100mA。`deinit()` 时强制调用 `dal_relay_turn_off` 断开线圈。针对磁保持 (`LATCHING_*`) 拓扑，脉冲输出 `pulse_duration_ms` 后自动关断控制脚，物理触点保持，静态功耗直接归零 |
| **Guard B: Bus 共享** | 资源独占 | `init()` 时调用 `pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner)` 锁定主引脚；若为磁保持拓扑，同步锁定 `cfg->reset_pin` |
| **Guard C: Zero-as-Default** | 零值推导 | 默认 `variant = DAL_RELAY_VARIANT_DIRECT_GPIO`；`pulse_duration_ms == 0` 时推导默认 50ms；`initial_state = false` 确保上电安全断开 |
| **Guard D: 硬件托底** | 物理输出驱动 | 彻底封装 `pal_gpio_write()`，磁保持脉冲通过 `dal_relay_poll()` 状态机非阻塞清脉冲 |
| **Guard E: 非阻塞** | `WINK_STRICT_NONBLOCKING` | 严禁使用 `delay_ms()` 死等磁保持脉冲！`turn_on`/`turn_off` 仅拉高引脚并记录 `pulse_start_ms`，后续由 `poll()` 检查时间戳拉低，严格无阻塞 |

---

## 6. Codegen 驱动描述 (`relay.yaml`)

新建 `wink-micro-os/codegen/drivers/relay.yaml`（ Schema 1.1 ）：

```yaml
codegen_schema: "1.1"
type: relay
category: output
is_actuator: false
experimental: false
default_role: binary_indicator

# 物理量量纲标记 (ADR-0056 / DAL-U-021)
quantity: binary
quantity_class: actuator_command

fields:
  owner:
    tier: stable
    type: string
    emit: none
  gpio_pin:
    tier: advanced
    type: int
    required: true
    c: pin
  reset_pin:
    tier: advanced
    type: int
    default: -1
  pulse_duration_ms:
    tier: advanced
    type: int
    default: 50
  variant:
    tier: advanced
    type: enum
    values: [direct_gpio, ssr, latching_dual_pin, latching_single_pin]
    default: direct_gpio
  active_low:
    tier: advanced
    type: bool
    default: false
  initial_state:
    tier: advanced
    type: bool
    default: false

quantities:
  owner:             { quantity_class: actuator_command }
  gpio_pin:          { quantity_class: actuator_command }
  reset_pin:         { quantity_class: actuator_command }
  pulse_duration_ms: { quantity_class: actuator_command }
  variant:           { quantity_class: actuator_command }
  active_low:        { quantity_class: actuator_command }
  initial_state:     { quantity_class: actuator_command }

config:
  c_type: dal_relay_t
  config_type: dal_relay_config_t
  headers: [dal_relay.h]
  init_fn: dal_relay_init
  deinit_fn: dal_relay_deinit
  safe_off_fn: ""                 # is_actuator: false → 必须为空字符串 (DAL-L-020)

role_bindings:
  binary_indicator:
    headers: []
    verbs:
      activate:
        template: "static inline void {{ name }}_activate(void) { WINK_IGNORE_RESULT(dal_relay_turn_on(&{{ name }})); }"
      deactivate:
        template: "static inline void {{ name }}_deactivate(void) { WINK_IGNORE_RESULT(dal_relay_turn_off(&{{ name }})); }"
      toggle:
        template: "static inline void {{ name }}_toggle(void) { WINK_IGNORE_RESULT(dal_relay_toggle(&{{ name }})); }"
```

---

## 7. Wasm / Wokwi 仿真映射契约

- **Wokwi 元件**：`ks2e-m-dc5`
- **引脚映射**：`IN` 引脚连接 MCU 控制 GPIO。
- **仿真行为**：
  - WASM PAL 接收到 GPIO 电平变更时，通过 `js_pal_gpio_read/write` 驱动前端 `ks2e-m-dc5` 动画显示（触点吸合/断开发声与颜色变化）。

---

## 8. 交付 Checklist 与验证步骤

- [ ] **1. 脚手架生成**：
  ```bash
  python wink-tools/wink.py create dal relay --category output --role binary_indicator
  ```
- [ ] **2. C 驱动实现**：编写 `dal_relay.h` 和 `dal_relay.c`，实现多拓扑 (`DIRECT_GPIO`, `SSR`, `LATCHING_*`) 及非阻塞脉冲状态机。
- [ ] **3. YAML 驱动描述**：创建 `wink-micro-os/codegen/drivers/relay.yaml`，确保与 `dal-role-architecture-spec.md` 及 `codegen/roles/binary_indicator.yaml` 契约完全一致（`fire_and_forget` / `void` 返回值）。
- [ ] **4. 单元测试**：编写 `wink-micro-os/test/unit/dal/test_dal_relay.c`，测试直驱模式、SSR 模式以及磁保持双脚脉冲模式。
- [ ] **5. ABI 静态断言**：运行 `python wink-tools/wink.py lint arch --pack abi` 回填 `_Static_assert` 的 `sizeof` / `offsetof` 验证。
- [ ] **6. 自动化静态 Lint**：
  ```bash
  python wink-tools/wink.py lint arch --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface --baseline lint-baseline.json
  ```
- [ ] **7. 三 Target 编译**：通过 Host (CTest)、WASM 及 ESP32 交叉编译，验证 `device_tree.h` 生成的 `binary_indicator` 门面函数。
- [ ] **8. 闭环状态更新**：在 [`00-master-execution-plan.md`](./00-master-execution-plan.md) 中将 `relay` 的进度更新为 `✅ Complete`。

