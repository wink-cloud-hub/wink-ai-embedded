#ifndef DAL_LED_H
#define DAL_LED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 配置结构体（标准化 config_t 模式，便于 Codegen 设备树生成）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 *
 * 成员按对齐降序排列（uint16_t → bool）：自然对齐，无填充。
 */
typedef struct {
    const char *owner;     /* 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint16_t pin;          /* 逻辑 GPIO 引脚 */
    bool active_high;      /* true: 高电平点亮；false: 低电平点亮（active low） */
} dal_led_config_t;

/**
 * @brief LED 逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config → dal_xxx_apply_override
 *   2. 运行时诊断：可直接打印当前生效的配置
 *
 * 成员按对齐降序排列（config_t → bool ×2）：自然对齐，无填充。
 */
typedef struct {
    dal_led_config_t config; /* 配置副本（pin, active_high），由 init 从 cfg 拷贝 */
    bool is_on;            /* 缓存当前点亮状态 */
    bool initialized;      /* init 成功后置 true */
} dal_led_t;

/* ABI stability (spec §2.3 / DAL-BC-010): config MUST remain the first
 * member. Offsets are compiler-verified: config has no enum/float, so the
 * layout is ptr(4/8) + uint16 + bool + tail padding. Recompute with the
 * target compiler if the struct layout changes — do not derive constants
 * from "follows the previous member" intuition (DAL-S-014). */
_Static_assert(offsetof(dal_led_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_led_config_t) == 8,  "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_led_t, initialized) == 9,  "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_led_t) == 12, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_led_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_led_t, initialized) == 17, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_led_t) == 24, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化 LED：校验、claim GPIO、配置推挽输出并显式写入熄灭电平。
 *
 * Init-to-Ready（DAL-BC-001）：成功后立即可接收 on/off/set/toggle，无需额外
 * arm/enable 步骤。Init 后输出处于**零能量态**（DAL-L-006）——无论 active_high
 * 还是 active_low，LED 均保证熄灭（显式写 off 电平，不依赖 GPIO 复位默认值）。
 *
 * @param dev LED 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL；dev 未 initialized。
 *                    （cfg->pin 为 uint16_t，下界天然满足；上界由 pal_gpio_init 校验，
 *                    非法引脚返回透传错误并回滚 claim。）
 *   - Postconditions: WINK_OK 时 dev->initialized=true、dev->is_on=false；GPIO 配置
 *                     为推挽输出并写入熄灭电平（active_high→LOW，active_low→HIGH）；
 *                     cfg 深拷贝到 dev->config。失败时回滚本次 claim 的 GPIO 资源、
 *                     不置 initialized。
 *   - Range: pin 为有效逻辑 GPIO（>= 0；上界由 PAL/GPIO_NUM_MAX 把关）。
 *   - Blocking: No（PAL claim + GPIO init，无 busy-wait）。
 *   - Thread-safe: No（调用方串行化 init 与其它方法，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: claim 一个 GPIO 引脚资源；配置 GPIO 方向并写熄灭电平。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（NULL、pin<0）/
 *     WINK_ERR_ALREADY_INITIALIZED / 透传 PAL 错误
 *     （WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED / WINK_ERR_IO / WINK_ERR_HARDWARE）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg);

/**
 * @brief 点亮 LED
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_led_init() 已成功。
 *   - Postconditions: WINK_OK 时 LED 点亮且 dev->is_on=true。
 *   - Blocking: No.
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: 写 GPIO 电平（按 active_high 推导）；更新 dev->is_on。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_on(dal_led_t *dev);

/**
 * @brief 熄灭 LED
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_led_init() 已成功。
 *   - Postconditions: WINK_OK 时 LED 熄灭且 dev->is_on=false。
 *   - Blocking: No.
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: 写 GPIO 电平（按 active_high 推导）；更新 dev->is_on。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_off(dal_led_t *dev);

/**
 * @brief 设置 LED 显式开关状态
 * @param on true=点亮；false=熄灭
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_led_init() 已成功。
 *   - Postconditions: WINK_OK 时 dev->is_on == on。
 *   - Blocking: No.
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: 写 GPIO 电平（on=true 按 active_high 点亮，on=false 熄灭）；更新 dev->is_on。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_set(dal_led_t *dev, bool on);

/**
 * @brief 翻转 LED 状态（on↔off）
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_led_init() 已成功。
 *   - Postconditions: WINK_OK 时 dev->is_on 取反。
 *   - Blocking: No.
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: 写 GPIO 电平为当前 is_on 的反；更新 dev->is_on。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_toggle(dal_led_t *dev);

/**
 * @brief 应急/故障关断（ADR-0048）：绑定 off（熄灭 LED）。
 *
 * 与普通 dal_led_off() 的区别：本函数用于 watchdog/panic/assert 失败/异常回滚路径，
 * 由 wink_actuator_safe_off_all() 遍历调用。因此：
 *   - 不要求 dev 已初始化（未初始化即"无物可关"，返回 WINK_OK，DAL-L-022）；
 *   - 不标 WINK_WARN_UNUSED_RESULT（应急路径不强制检查返回值，DAL-L-021）；
 *   - 尽力熄灭（best-effort），不依赖调度器与堆。
 *
 * @param dev LED 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。不要求已 init。
 *   - Postconditions: best-effort 熄灭 LED（若已初始化则写 off 电平并置 is_on=false）。
 *   - Blocking: No。
 *   - Thread-safe: No。
 *   - ISR-safe: No（调 pal_gpio_write；保守声明）。
 *   - Reentrancy: Yes（幂等，可重复调用）。
 *   - Side-effects: 若已初始化则写 GPIO 熄灭电平、置 dev->is_on=false。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）。
 */
wink_status_t dal_led_safe_off(dal_led_t *dev);

/**
 * @brief 反初始化 LED：熄灭输出、GPIO reset、释放资源、memset 清零。
 * @note 可在未 init 的 dev 上安全调用（直接返回 WINK_OK，no-op，DAL-L-010）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: best-effort 熄灭→pal_gpio_reset_pin（Hi-Z、清 reservation）→
 *                     释放 GPIO resource claim→memset(dev,0)；dev->initialized=false。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No。
 *   - Idempotent: 未 init 时返回 WINK_OK；deinit 后可再次 init。
 *   - Side-effects: 写 GPIO off、复位引脚、释放资源、清零句柄。
 *   - ADR-0024: GPIO reset + 释放 resource claim；释放失败记 LOG_W 但不中断清场。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）。
 */
wink_status_t dal_led_deinit(dal_led_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06) ──────────────────────
 * When WINK_USE_LED=OFF (CMake static pruning), the driver source is not
 * compiled. The declarations below are kept visible (outside the #if
 * guard) but tagged WINK_UNAVAILABLE_MSG so accidental calls produce a
 * friendly compile error that points the caller at the fix, instead of
 * an opaque "undefined reference" at link time.
 *
 * Adding a new public API? Mirror its signature inside the block.
 */
#if !defined(WINK_USE_LED) || !WINK_USE_LED
#define WINK_LED_DISABLED_MSG \
    "LED driver not enabled; add a \"led\" device to wink-app.json " \
    "(or set -DWINK_USE_LED=ON)."
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_on(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_off(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_set(dal_led_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_toggle(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG)
wink_status_t dal_led_safe_off(dal_led_t *dev);
WINK_UNAVAILABLE_MSG(WINK_LED_DISABLED_MSG)
wink_status_t dal_led_deinit(dal_led_t *dev);
#endif /* !WINK_USE_LED */

#endif /* DAL_LED_H */
