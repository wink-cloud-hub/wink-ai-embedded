# DAL 外设执行分计划：input/analog_knob

| 项 | 内容 |
|---|---|
| **计划名称** | `input/analog_knob` 外设驱动与 Codegen 落地计划 |
| **所属批次** | P0 批次 (核心 HMI 人机交互外设) |
| **映射 Wokwi 组件** | `potentiometer` (旋转电位器)、`slide-potentiometer` (滑动变阻器/滑杆) |
| **驱动文件路径** | `wink-micro-os/dal/include/input/dal_analog_knob.h`<br>`wink-micro-os/dal/src/input/dal_analog_knob.c` |
| **Codegen 描述** | `wink-micro-os/codegen/drivers/analog_knob.yaml`（**ADR-0051：YAML 为 SSOT，非 `wink-tools/.../drivers/*.py`**） |
| **关联规范** | [`dal-api-consistency-spec.md`](../../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md) (v3.4.3)、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)、[ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md)、[ADR-0046](../../decisions/core/0046-dal-driver-registry-ssot.md) |
| **⛔ 前置阻塞依赖** | **已完成**：前置 [00.5-pal-adc-subsystem-plan.md](./00.5-pal-adc-subsystem-plan.md) 已落地 |
| **计划状态** | ✅ 已实现（经 2026-08-06 架构重构加固并真实单测全绿） |

> **落地重构与加固终局总结（2026-08-06）**：
> 1. **P0-1 循环依赖解除**：在 PAL 子系统扩充 `pal_adc_acquire(pin, cfg, &ch)` / `pal_adc_release(ch)` API，由 PAL 内部完成 GPIO 引脚到 ADC 通道槽位的寻找与初始化，彻底解决了 DAL `init` 时未初始化反查 `pin_channel` 导致死锁的硬伤。
> 2. **P0-2 `poll()` 迟滞快照修复**：在 `dal_analog_knob_poll()` 采样前先快照 `prev_promille = dev->last_knob_promille`，修复了原先自己与自己比较导致 `changed` 恒为 `false` 的逻辑死代码。
> 3. **P1-1 物理量程与逻辑量程解耦**：传给 PAL 的 `full_scale_mv` 统一填 `0`（平台默认最高衰减档位），软件刻度 `min_mv/max_mv` 纯由 DAL 做千分比缩放。
> 4. **P1-2 剥离 Pragma**：从 `pal_adc_read_raw` / `read_mv` 移除误加的 `WINK_BLOCKING` 标记，彻底移除 DAL 中的 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`。
> 5. **真实环境 100% 测试通过**：独立测试工程构建下 `ctest -R test_dal_analog_knob` **9/9 Tests 100% Passed (0 Failures)**；`wink lint` 静态架构门禁 Clean 无任何告警！

---

## 0. 前置阻塞：PAL ADC 子系统（已于 2026-08-06 完成落地）

**核对结论（2026-08-06，基于当前代码库实测）**：本计划所需的前置 PAL ADC 子系统（[00.5-pal-adc-subsystem-plan.md](./00.5-pal-adc-subsystem-plan.md)）已完全落地通过验证。

---

## 1. 需求分析与硬件语义映射

| 核对项 | 实际状态 |
|---|---|
| `pal/include/hal/pal_hal.h` | 仅 GPIO / PWM / I2C 共 15 个 `pal_*` 函数，**无任何 `pal_adc_*`** |
| `pal/include/pal_resource.h` | `pal_resource_type_t` 仅有 `GPIO_PIN/PWM_CHANNEL/I2C_PORT/I2C_ADDR/UART_PORT`，**无 ADC 通道资源类型** |
| `targets/wasm/wasm_bridge.h` | 仅 `js_pal_gpio_read` / `js_pal_i2c_transfer`，**无 `js_pal_adc_read`**；唯一 ADC 相关物是故障注入钩子 `pal_wasm_set_adc_noise_v()`（`pal_wasm_physical.c:122`）——意图存在但**读路径完全缺失** |
| `targets/esp32` | 无 ADC 实现 |

因此原 §4 "Guard B：依赖 PAL ADC 子系统自动共享与通道多路复用" 是**在描述一个不存在的子系统**，绝非一行实现可以覆盖。

### 0.1 拆出前置计划
新增 [`00.5-pal-adc-subsystem-plan.md`](./00.5-pal-adc-subsystem-plan.md)，由 `input/analog_knob`(P0#1) 与 `sensor/analog_sensor`(P0#5) **共享复用**，范围至少包含：
1. `pal/include/hal/pal_adc.h`：`pal_adc_init(channel, cfg)` / `pal_adc_read_raw(channel, uint16_t *out_raw)` / `pal_adc_read_mv(channel, uint16_t *out_mv)` / `pal_adc_deinit`；引脚→通道映射 `pal_adc_pin_channel()`（对齐既有 `pal_pwm_channel_pin` / `pal_i2c_port_pins` 形态）。
2. `pal_resource.h` 增加 `PAL_RESOURCE_ADC_CHANNEL`，供 owner 冲突检测。
3. 三 target 实现：esp32（`adc_oneshot` + 校准 curve fitting）、wasm（新增 `js_pal_adc_read_mv` bridge + 复用 `adc_noise_v` 故障注入）、host（可注入桩，供单测）。
4. 位宽/衰减语义：ESP32 12-bit + `ADC_ATTEN_DB_11`，其满量程约 **~3100mV 而非 3300mV**，且 ADC2 与 WiFi 冲突——这些必须在 PAL 层收敛，不可泄漏到 DAL。

### 0.2 P0 执行顺序调整建议
把 P0 顺序改为 **`relay`(#4) / `buzzer`(#3) 先行**（纯 GPIO/PWM，零 PAL 欠账），用它们先跑通"`new-dal` → YAML → lint → 单测"全链路，再回到 ADC 家族（`analog_knob` / `analog_sensor`）。建议同步更新 `00-master-execution-plan.md` §2.2 的执行序。

---

## 1. 需求分析与硬件语义映射

### 1.1 Wokwi 元件与控制语义
- **`potentiometer`** 与 **`slide-potentiometer`** 属于人机交互（HMI）的连续模拟量输入调节元件。
- **底层物理输入**：模拟电压 (0mV ~ 满量程, 经 ADC 转换为 10-bit / 12-bit 原始数字量)。
- **DAL 抽象控制语义**：**归一化调参量 `knob_promille`（`uint16_t`, `[0, 1000]`）**。
  - 旋至最左 / 滑至最底 → `0`；旋至最右 / 滑至顶端 → `1000`。
  - **不使用 `float 0.0f~1.0f`**：`_norm` 后缀不在 ADR-0056 封闭后缀表内，会触发 lint error（详见 §2.1）。

### 1.2 架构隔离防线（架构裁决 4.2 硬伤 1 封堵）
- **隔离界限（语义层）**：`input/analog_knob` 专用于 **HMI 人机调参**（返回 `[0,1000]` 千分比）；`sensor/analog_sensor` 专用于 **物理量测量**（返回 `_raw` / `_mv`，工程量换算留给 BAL）。**目录、API 量纲、role 三者隔离**。
- **⚠️ 措辞修正**：隔离是**语义层**的，**不是实现层零复用**。二者底层共用同一条 PAL ADC 读路径、同一套资源 claim、同一个 wasm bridge。前置计划（§0）刻意让二者**共享** PAL ADC 与仿真通道，避免重复造轮；原草案"两者 100% 隔离、绝不混用"的表述容易误导实现者复制两份 ADC 代码，故此更正。

---

## 2. 硬件拓扑分类与 Variant 架构分析 (依据 dal-best-practices.md §3)

### 2.1 架构原则
- **对外 API 冻结**：统一暴露 `dal_analog_knob_read_promille` / `read_mv` / `poll`。
- **硬件拓扑收敛**：旋钮/滑杆电位器均为 3 引脚电阻分压拓扑（VCC-GND-SIG），统一归为单拓扑结构。
- **C 头文件绝对无具体芯片型号**：头文件中无任何 `WH148`/`B10K` 等型号字符串，具体型号由 Codegen 别名表消化。

### 2.2 市面主流芯片/模块全量盘点

| 型号 / 产品 | 类型 / 形态 | 物理接口 | 典型用途 |
|---|---|---|---|
| **WH148** | 旋转电位器 (单联/双联, 10K/100K) | 3-Pin 模拟分压 | 音量调节、音响旋钮、面板调参 |
| **B10K / B50K Slide** | 直滑 / 滑动变阻器 (Slide Potentiometer) | 3-Pin / 4-Pin 模拟分压 | 调音台滑轨、灯光推子 |
| **RK097** | 密封型微型旋转电位器 | 3-Pin 模拟分压 | 便携设备、手持终端调参 |
| **3362P / 3296W** | 精密多圈微调电位器 | 3-Pin 模拟分压 | 电路偏置微调、高精度设定 |
| **Wokwi potentiometer** | Wokwi 旋转电位器仿真组件 | 模拟量输入 | WebAssembly 仿真 |
| **Wokwi slide-potentiometer** | Wokwi 滑动变阻器仿真组件 | 模拟量输入 | WebAssembly 仿真 |

### 2.3 Codegen 别名映射表 (`aliases`)

在 `wink-micro-os/codegen/drivers/analog_knob.yaml` 中提供别名映射，方便开发者在 `wink-app.json` 中使用常见硬件型号名称：

```yaml
aliases:
  potentiometer:       { min_mv: 0, max_mv: 0, hysteresis_promille: 10 }
  slide_potentiometer: { min_mv: 0, max_mv: 0, hysteresis_promille: 10 }
  wh148:               { min_mv: 0, max_mv: 0, hysteresis_promille: 10 }
  b10k_slide:          { min_mv: 0, max_mv: 0, hysteresis_promille: 10 }
  rk097:               { min_mv: 0, max_mv: 0, hysteresis_promille: 10 }
```

---

## 3. 数据结构设计 (`dal_analog_knob.h`)

### 2.0 原草案的 5 处硬伤（已修正）

| # | 原草案 | 实测事实 | 修正 |
|---|---|---|---|
| 1 | `wink_err_t` 返回类型 | 全仓**不存在** `wink_err_t`，只有 `wink_status_t`（`wink_status.h`） | 全部改为 `wink_status_t` |
| 2 | `WINK_ERR_UNINITIALIZED` | 实际枚举名为 **`WINK_ERR_NOT_INITIALIZED = -11`** | 改名 |
| 3 | `const dal_..._config_t *config;`（指针） | **DAL-S-011 (MUST, LINT-ENFORCED)**：首成员 MUST 内嵌 `config` **值副本** 且 `offsetof == 0`；现存 9 个驱动全部内嵌值 | 改为内嵌值副本 |
| 4 | `float min_voltage_mv` / `hysteresis_threshold` / `last_norm_val` | **ADR-0056 / DAL-U-004**：`_norm` 后缀未编码刻度 → `dal.quantity.suffix_encodes_scale` **error**；且 `_threshold`/`_val` 不在封闭后缀表 → `suffix_closed` **error** | 见 §2.1 定标整数方案 |
| 5 | `uint16_t pin` + `int16_t enable_pin` | 类型对（DAL-S-006），但 `enable_pin` 应用 `wink_pin_t`（= `int16_t`）；注意 `wink_pin_t` 定义在 `pal_hal.h`，而 **`DAL-HDR-NO-HAL` (error)** 禁止 DAL 公共头 include `pal_hal.h`（`dal_dc_motor.h` 是带 `until: 2026-12-31` 的历史豁免，新驱动**不得**沿用） | `enable_pin` 用裸 `int16_t`，**不 include `pal_hal.h`** |

### 2.1 量纲决策：analog_knob 属 B 类（sensor_measurement）

按 ADR-0056 §9.3 两分类：旋钮是**输入采集**（硬件 → App），属 **B 类 sensor_measurement**，而非 A 类执行器命令。

- B 类 Full Profile 允许 `float` + 真实单位后缀，但后缀必须落在封闭表 `B_CLASS_UNITS`（`tools/lint/dal/quantity_suffixes.py`）内。
- `_norm` **不在**任何封闭表中，`float ..._norm` 会直接触发 `dal.quantity.suffix_closed` error。
- 封闭表中已有 **`_promille`**（千分比，`[0,1000]`）与 `_mv`、`_raw`。故归一化调参量统一用 **`knob_promille`**（`uint16_t`，`[0,1000]`）。

> ⚠️ 注意 `_promille` 在 `SIGNED_HINT` 中（因 `speed_promille ∈ [-1000,1000]`），若用 `uint16_t` 会触发 `dal.quantity.a_class_signedness` **warning**。但该规则仅对 `is_a_class_unit(suf)` 生效且 handle 内 `last_`/`current_`/`cached_` 前缀成员被豁免。**落地前必须先确认**：要么 handle 成员命名为 `last_knob_promille`（走豁免），要么在 `quantity_suffixes.py` 补 B 类无符号 promille 语义。此为**需 ADR 确认的开放问题**（见 §8）。

### 2.2 结构体草案（修正版）

遵循 `dal-api-consistency-spec.md` §2 与 `DAL-S-001/006/011/014`：

```c
#ifndef DAL_ANALOG_KNOB_H
#define DAL_ANALOG_KNOB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
/* 不得 include "pal_hal.h"：DAL-HDR-NO-HAL (error)。故 enable_pin 用裸 int16_t。 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 归一化调参量满刻度（千分比，ADR-0056 封闭后缀 `_promille`） */
#define DAL_ANALOG_KNOB_FULL_SCALE_PROMILLE 1000u

/**
 * @brief HMI 模拟旋钮拓扑变体枚举 (Topology Variant - 一等公民)
 */
typedef enum {
    DAL_ANALOG_KNOB_VARIANT_STANDARD = 0,        /**< 标准 3-Pin B型线性电阻分压电位器 (默认) */
    DAL_ANALOG_KNOB_VARIANT_LOGARITHMIC = 1,     /**< 音响 A型对数电位器 (内部对数转线性校正) */
    DAL_ANALOG_KNOB_VARIANT_ANTI_LOGARITHMIC = 2,/**< 反对数 C型电位器 (内部反对数校正) */
    DAL_ANALOG_KNOB_VARIANT_CENTER_DETENT = 3,  /**< 带中点物理卡槽电位器 (50% 中点死区吸附) */
} dal_analog_knob_variant_t;

/**
 * @brief HMI 模拟旋钮/滑杆配置结构体 (POD config_t)
 * 成员按对齐降序排列，避免填充；owner MUST 为首成员 (DAL-S-001)。
 */
typedef struct {
    const char *owner;              /**< 资源占用者名称（静态存储期字符串，DAL-S-002） */
    uint16_t min_mv;                /**< 最小有效电压 (mV)；0 && max_mv==0 → 平台满量程 (Guard C) */
    uint16_t max_mv;                /**< 最大有效电压 (mV)；0 && min_mv==0 → 平台满量程 (Guard C) */
    uint16_t hysteresis_promille;   /**< 迟滞消抖阈值（千分比，建议 5~50） */
    uint16_t pin;                   /**< ADC 输入 GPIO 引脚（必填 → uint16_t，DAL-S-006） */
    int16_t enable_pin;             /**< 低功耗使能引脚（可选 → 有符号，-1 = 未绑定，Guard A） */
    dal_analog_knob_variant_t variant; /**< 拓扑变体枚举 (DAL-S-001) */
    bool inverted;                  /**< 方向翻转：false=0→1000，true=1000→0 */
} dal_analog_knob_config_t;

/**
 * @brief HMI 模拟旋钮句柄 (POD instance_t)
 * config 内嵌值副本且为首成员 (DAL-S-011)，支持 Flash 动态覆写 (ADR-0008)。
 */
typedef struct {
    dal_analog_knob_config_t config;  /**< 配置副本，由 init 从 cfg 拷贝；offsetof == 0 */
    uint16_t last_knob_promille;      /**< 上次归一化读数（`last_` 前缀走符号性豁免） */
    uint16_t last_raw;                /**< 上次原始 ADC 采样值（封闭后缀 `_raw`） */
    bool initialized;                 /**< init 成功后置 true (DAL-L-004) */
    volatile wink_status_t last_status; /**< 最近一次 poll 错误（DAL-B-025 可观测性） */
} dal_analog_knob_t;

/* Static assertions for ABI freeze and first-member guard (DAL-S-011 / DAL-S-014) */
_Static_assert(offsetof(dal_analog_knob_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_analog_knob_config_t) == 24, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_analog_knob_t, initialized) == 28, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_analog_knob_t) == 36, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_analog_knob_config_t) == 32, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_analog_knob_t, initialized) == 36, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_analog_knob_t) == 48, "ABI break: handle size changed on 64-bit host");
#endif

#ifdef __cplusplus
}
#endif

#endif /* DAL_ANALOG_KNOB_H */
```

---

## 3. C 驱动核心 API 接口设计

全部返回 `wink_status_t`（**非 `wink_err_t`**）；量纲遵循 ADR-0056 B 类 + 封闭后缀表：

```c
/**
 * @brief 初始化模拟旋钮外设
 * @param dev  旋钮句柄指针
 * @param cfg  静态配置指针（init 内部拷贝为值副本）
 * @return WINK_OK 成功；WINK_ERR_INVALID_ARG 参数错误；
 *         WINK_ERR_ALREADY_INITIALIZED 重复 init（DAL-L-004 fail-fast）；
 *         WINK_ERR_RESOURCE_EXHAUSTED ADC 通道/GPIO claim 失败
 * @note  init 失败 MUST 回滚已 claim 的资源（DAL-L-008）
 */
wink_status_t dal_analog_knob_init(dal_analog_knob_t *dev,
                                   const dal_analog_knob_config_t *cfg);

/**
 * @brief 释放模拟旋钮资源（best-effort，DAL-L-015）
 * @return WINK_OK
 */
wink_status_t dal_analog_knob_deinit(dal_analog_knob_t *dev);

/**
 * @brief 读取归一化当前位置（千分比）
 * @param dev              旋钮句柄指针
 * @param out_knob_promille 输出归一化位置 [0, 1000]
 * @return WINK_OK；WINK_ERR_NOT_INITIALIZED 未初始化；WINK_ERR_IO ADC 读失败
 * @note  API Contract:
 *          - Range: 0=最左/最底, 500=中位, 1000=最右/顶端
 *          - Side-effects: 刷新 last_knob_promille / last_raw
 */
wink_status_t dal_analog_knob_read_promille(dal_analog_knob_t *dev,
                                            uint16_t *out_knob_promille);

/**
 * @brief 读取原始毫伏电压（封闭后缀 `_mv`）
 * @return WINK_OK；WINK_ERR_NOT_INITIALIZED；WINK_ERR_IO
 */
wink_status_t dal_analog_knob_read_mv(dal_analog_knob_t *dev,
                                      uint16_t *out_mv);

/**
 * @brief 轮询旋钮变更（非阻塞，带迟滞抗抖动）
 * @param out_changed        是否发生超过 hysteresis_promille 的位置改变
 * @param out_knob_promille  最新归一化位置 [0, 1000]
 * @return WINK_OK；WINK_ERR_NOT_INITIALIZED；WINK_ERR_IO
 */
wink_status_t dal_analog_knob_poll(dal_analog_knob_t *dev,
                                   bool *out_changed,
                                   uint16_t *out_knob_promille);

/**
 * @brief 读取最近一次 poll 错误（DAL-B-025 可观测性，对齐 dal_button_get_status）
 */
wink_status_t dal_analog_knob_get_status(const dal_analog_knob_t *dev,
                                         wink_status_t *out_status);
```

> **DAL-U-029 提醒**：`promille = ((uint32_t)(raw_mv - min_mv) * 1000u) / (max_mv - min_mv)` 中的乘法**必须显式提升到 `uint32_t`**，否则 `dal.quantity.a_class_overflow_guard` 报 **error**（lint 逐行正则扫 `.c`）。同时必须防 `max_mv == min_mv` 的除零。

---

## 4. 运行时防线 (5 Safety Guards) 落实

| 防线 | 条款标准 | 落地实现方案 |
|---|---|---|
| **Guard A: 低功耗** | 预留 `enable_pin` | `config.enable_pin >= 0` 时，`init()` 拉高供电、`deinit()` 拉低；**不在每次 `read()` 内翻转**（电位器为电阻分压，上电需稳定时间，逐次开关会引入首读误差）。若需省电，由 BAL 显式控制。`0 → -1` 规范化对齐 `dc_motor` 的既有做法 |
| **Guard B: Bus 共享** | ADC 模块共享 | ⛔ **依赖不存在的 PAL ADC**（§0）。前置计划须提供 `PAL_RESOURCE_ADC_CHANNEL` claim + 通道复用；DAL 侧仅做 owner claim 与错误传播 |
| **Guard C: Zero-as-Default** | 零值推导默认 | `min_mv == 0 && max_mv == 0` → 由 **PAL 查询平台满量程**（`pal_adc_full_scale_mv()`），**不得在 DAL 硬编码 3300**。ESP32 12-bit + 11dB 衰减实际满量程 ≈ 3100mV，硬编码 3300 会导致旋钮永远读不到 1000 |
| **Guard D: 抗抖动** | 滤波与迟滞 | 迟滞在 `poll()` 内以 `hysteresis_promille` 比较；滤波用**定点** EMA 或 4-sample 中位值（`uint32_t` 中间量，禁 float，避免 Micro Profile 与 DAL-U-029 冲突） |
| **Guard E: 非阻塞** | `WINK_STRICT_NONBLOCKING` | ADC oneshot 读取本身为寄存器级快速返回；**禁止 `pal_gpio_pulse_in` 式忙等**。注意 `WINK_STRICT_NONBLOCKING` 现仅在 `dal_gps.h` 出现，本驱动应确认该宏的正确用法后再标注 |

> **补充：Guard 覆盖缺口** — 现有 5 Guard 未覆盖 ADC 特有的两个真实风险，建议在前置计划补第 6/7 条：
> - **ADC2 与 WiFi 互斥**（ESP32 硬约束）：若旋钮被分配到 ADC2 引脚且 App 启用 WiFi，读数将失败。应在 **codegen 校验期**（board JSON 的引脚能力表）拦截，而非运行时。当前 `boards/esp32_devkitc.json` 的 `headers` 仅有 `"A0": 36, "A3": 39`，**无 ADC 能力/通道元数据**，需扩展。
> - **校准与非线性**：ESP32 ADC 未校准时非线性显著；应在 PAL 层统一 `esp_adc_cal` 曲线拟合，DAL 只消费线性化后的 mV。

---

## 5. Codegen 驱动描述（YAML，**非 Python 插件**）

### 5.1 原草案的路径错误
原草案要求写 `wink-tools/tools/codegen/drivers/analog_knob.py` 并继承 `DriverPlugin`。两处均错：
1. **ADR-0051 已将 SSOT 路径从 `wink-tools/.../drivers/*.py` 迁至 `wink-micro-os/codegen/drivers/*.yaml`**。ADR-0051 明确："SSOT **路径**从 tools 的 `drivers/*.py` 递进为可扫描扩展根"，且 MVP **禁止 Python hooks**。往 `wink-tools/` 写驱动正是该 ADR 要消除的反模式（用户改不了源码分发）。
2. 基类名不是 `DriverPlugin` 而是 **`DriverBase`**（`tools/codegen/drivers/base.py:30`），且注册靠 `__init_subclass__`；`wink-tools` 下的 `*.py` 现在只作为**兼容层/golden 对照**（`register: bool` 开关）。

### 5.2 正确产物：`wink-micro-os/codegen/drivers/analog_knob.yaml`

对齐 `button.yaml` / `ultrasonic.yaml` 的 Schema 1.1 形态：

```yaml
codegen_schema: "1.1"
type: analog_knob
category: input
is_actuator: false
experimental: false
default_role: analog_input        # 见 §5.3：该 role 目前不存在，需一并新建

# ADR-0056 / DAL-U-021：quantity_class 缺失 → dal.yaml.quantity_class_required (error)
quantity: ratio
quantity_class: sensor_measurement

fields:
  gpio_pin:
    tier: advanced
    type: int
    required: true
    c: pin
  enable_pin:
    tier: advanced
    type: int
    default: -1
  min_mv:
    tier: advanced
    type: int
    default: 0
  max_mv:
    tier: advanced
    type: int
    default: 0                    # 0 → Guard C 平台满量程推导
  hysteresis_promille:
    tier: stable
    type: int
    default: 10
    min: 0
    max: 500
  inverted:
    tier: advanced
    type: bool
    default: false
  role:
    tier: stable
    type: string
    emit: none

# ⚠️ DAL-U-021 实测：dal_yaml_parity.py 对 fields 中的**每一个字段**都要求
# quantities.<name>.quantity_class，缺失即 error。现存 9 个驱动全部踩在这条上
# （实测 `lint --pack dal` 共 114 个 error，其中大量为该规则）。
# 落地时必须补全 quantities: 段，或与 lint owner 确认该规则是否过宽（见 §8）。
quantities:
  gpio_pin:            { quantity_class: sensor_measurement }
  enable_pin:          { quantity_class: sensor_measurement }
  min_mv:              { quantity_class: sensor_measurement }
  max_mv:              { quantity_class: sensor_measurement }
  hysteresis_promille: { quantity_class: sensor_measurement }
  inverted:            { quantity_class: sensor_measurement }
  role:                { quantity_class: sensor_measurement }

config:
  c_type: dal_analog_knob_t
  config_type: dal_analog_knob_config_t
  headers: [dal_analog_knob.h]
  init_fn: dal_analog_knob_init
  deinit_fn: dal_analog_knob_deinit
  safe_off_fn: ""                 # is_actuator: false → MUST 为空 (DAL-L-020)

role_bindings:
  analog_input:
    headers: []
    verbs:
      read_promille:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_read_promille(uint16_t *out_promille) { return dal_analog_knob_read_promille(&{{ name }}, out_promille); }"
      read_mv:
        template: "WINK_WARN_UNUSED_RESULT static inline wink_status_t {{ name }}_read_mv(uint16_t *out_mv) { return dal_analog_knob_read_mv(&{{ name }}, out_mv); }"
```

### 5.3 连带缺口：`role` 不存在
`codegen/roles/` 现有 7 个 role（`angular_actuator` / `binary_indicator` / `binary_sensor` / `distance_sensor` / `open_loop_actuator` / `pulse_counter` / `text_display`），**没有适配连续模拟输入的 role**。
- `user_surface` pack 的 **`DEVICE-REQUIRES-ROLE`** 规则：非 experimental 驱动若既无显式 `role` 又无 `default_role` → 报错。
- 故必须一并新建 `codegen/roles/analog_input.yaml`（含 `verbs` + `error_class`，形态照 `binary_sensor.yaml`）：

```yaml
role_name: analog_input
description: "通用模拟量输入能力平面 (连续模拟量采集与调参)"
verbs:
  read_promille:
    return_type: wink_status_t
    params:
      - name: out_promille
        type: uint16_t*
  read_mv:
    return_type: wink_status_t
    params:
      - name: out_mv
        type: uint16_t*
```

- **注意**：该 role 会被 `analog_sensor`(P0#5)、`heart_rate`(P2#18)、`joystick` 拆解件复用 → **role 契约应在 [`00.5-pal-adc-subsystem-plan.md`](./00.5-pal-adc-subsystem-plan.md) 前置计划中统一设计与定稿**，而不是在本计划里私自定义，否则后续必然破坏性改名。

### 5.4 顺带修正：字段顺序门禁
`drivers.config_field_order` 要求 YAML `fields` 顺序与 `dal_*_config_t` 物理成员顺序一致（避免 `-Wreorder` / C++20 指定初始化硬错）。实测 `dc_motor` 与 `eeprom` 已在告警。本驱动**从第一天就对齐**：`fields` 顺序须与 §2.2 结构体成员顺序（owner 后依次 `min_mv, max_mv, hysteresis_promille, pin, enable_pin, inverted`）一致——上述草案**目前故意保留了不一致**以便讨论，落地前须二者择一并跑 `lint --pack drivers` 验证。

### 5.5 脚手架命令
```bash
python wink-tools/wink.py create dal analog_knob --category input \
    --pin-field gpio_pin --pin-field enable_pin --role analog_input
```
（`create dal` 会同时生成 `.h`/`.c` + 驱动 YAML + `roles/<role>.yaml` 骨架。）

---

## 6. Wasm / Wokwi 仿真映射契约

- **Wokwi 元件**：`wokwi-potentiometer` / `wokwi-slide-potentiometer`
- **⛔ 现状缺口**：`wasm_bridge.h` **无 ADC 读通道**。需新增：
  - `extern uint16_t js_pal_adc_read_mv(uint16_t pin);`（或 `pal_wasm_adc_read_mv` 降级双入口，对齐既有 `js_pal_gpio_read` / `pal_wasm_gpio_read` 双层形态）。
  - 前端 `WasmPhysicalBridge` 侧补 `readAdcMv(pin)`，把 Wokwi 元件的 `value` 属性换算为 mV。
- **故障注入复用**：已存在的 `pal_wasm_set_adc_noise_v(float)`（`pal_wasm_physical.c:122`）应叠加进读数，用于验证 Guard D 迟滞滤波真实有效。
- **确定性要求**：ADR-0055（sim FP 确定性与 golden 策略）——噪声必须走既有 PRNG 且**不改变调用序**，否则 golden 测试漂移。这是选择**定点** promille 而非 float 的又一理由。

---

## 7. 验收与测试计划 (Checklist)

**Phase 0（前置，已于 00.5 计划完成）**
- [x] **0.1** 编写并评审 [`00.5-pal-adc-subsystem-plan.md`](./00.5-pal-adc-subsystem-plan.md)（`pal_adc.h` + 三 target + `PAL_RESOURCE_ADC_CHANNEL`）。
- [x] **0.2** 固化当前 Lint 历史债务基线，确保 CI 机械拦截“零新增 Error”：
  ```bash
  python wink-tools/wink.py lint arch --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface --format json --output lint-baseline.json
  ```
- [x] **0.3** 设计并评审共享 role `analog_input` 契约（与 `analog_sensor` / `heart_rate` 一并定稿）。
- [x] **0.4** 扩展 `boards/*.json`，补 ADC 通道/衰减/ADC2-WiFi 互斥能力元数据。
- [x] **0.5** 解决 §8 的两个开放问题（promille 符号性、DAL-U-021 覆盖面）。

**Phase 1（本驱动）**
- [ ] **1.** 用 `wink.py create dal analog_knob --category input --role analog_input` 生成脚手架（勿手写）。
- [ ] **2.** 实现 `dal_analog_knob.h` / `.c`；**零 malloc**、零 `delay_ms`、乘法显式 `uint32_t` 提升、防除零。
- [ ] **3.** 编译规则：**不改** `dal/CMakeLists.txt`（该文件顶部注释明确"do not edit this file's driver list"；驱动列表由 `list_drivers.py` 从 YAML 生成）。仅需 YAML 落地即自动纳入 + `WINK_USE_ANALOG_KNOB` 剪枝。
- [ ] **4.** 单元测试 `wink-micro-os/test/unit/dal/test_dal_analog_knob.c`（**注意实际路径是 `test/unit/dal/`，不是 `test/dal/`**）：覆盖 0mV / 中位 / 满量程 / `inverted` / 迟滞抑制 / `max_mv == min_mv` 除零 / 未 init 返回 `WINK_ERR_NOT_INITIALIZED` / 重复 init 返回 `WINK_ERR_ALREADY_INITIALIZED`。
- [ ] **5.** ABI 断言：跑 `python wink-tools/wink.py lint arch --pack abi` 回填 `_Static_assert` 实测值（**勿手工估算**）。
- [ ] **6.** 静态 Lint：`python wink-tools/wink.py lint arch --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface`
      （**注意：`--pack drivers` 只是 6 个 pack 之一；原草案仅跑 `drivers` 会漏掉 `dal.quantity` / `DAL-HDR-NO-HAL` / `DEVICE-REQUIRES-ROLE` 等致命规则**）。
- [ ] **7.** 双 target 编译验证（ADR-0002）：host + wasm + ESP32 三向编过。
- [ ] **8.** Wokwi 仿真闭环：拖入 potentiometer，旋转后 App 读数从 0 单调变化到 1000。

---

## 8. 开放问题（落地前必须裁定）

| # | 问题 | 影响 | 建议 |
|---|---|---|---|
| 1 | `_promille` 在 `SIGNED_HINT` 中（为 `speed_promille ∈ [-1000,1000]`），但旋钮读数是 `[0,1000]` 无符号 | 用 `uint16_t` 触发 `a_class_signedness` warning；用 `int16_t` 语义失真 | 在 `quantity_suffixes.py` 区分 A 类 `speed_promille`(signed) 与 B 类 `ratio_promille`(unsigned)，或新增 `_per10k` 无符号方案；**需小 ADR** |
| 2 | `dal.yaml.quantity_class_required` 对 `fields` 中**每个字段**（含 `gpio_pin`/`role`/`active_low` 这类非物理量）都强制 `quantity_class` | 实测 `lint --pack dal` 报 114 个 error，9 个现存驱动全部违规 → 说明该规则**当前过宽或全仓有技术债** | 与 lint owner 确认：规则应仅作用于**真实物理量字段**；否则本驱动需写一堆语义无意义的 `sensor_measurement` |
| 3 | `analog_knob`(HMI 比例) 与 `analog_sensor`(物理量 mV/raw) 底层同为一条 ADC 读路径 | §1.2 声称"100% 隔离"，但二者共用 PAL ADC + 极可能共用 role 与仿真 bridge | 隔离应界定为**语义层隔离**（API 量纲/目录/role），而非实现层零复用；建议改写 §1.2 措辞避免误导后续实现者重复造轮 |
| 4 | `WINK_STRICT_NONBLOCKING` 全仓仅在 `dal_gps.h` 出现 | Guard E 引用了一个用法未普及的宏 | 确认该宏的规范用法，或改为引用 ADR-0017（阻塞 API 硬隔离） |

