#ifndef DAL_LED_H
#define DAL_LED_H

#include <stdint.h>
#include <stdbool.h>
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

/**
 * @brief 初始化 LED：校验引脚、配置 GPIO 推挽输出、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 * 旧 API（pin + active_high 分离参数）已迁移至此。
 *
 * @param dev LED 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL。
 *   - Blocking: No（pal_gpio_init 不阻塞）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL) / 透传 PAL 错误
 *     （WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；GPIO 方向已配置（真机）；
 *                     cfg 的内容已深拷贝到 dev->config。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_init(dal_led_t *dev, const dal_led_config_t *cfg);

/**
 * @brief 点亮 LED
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_led_init() 已成功。
 *   - Blocking: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_on(dal_led_t *dev);

/**
 * @brief 熄灭 LED
 * @note API Contract: 同 dal_led_on。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_off(dal_led_t *dev);

/**
 * @brief 设置 LED 显式开关状态
 * @param on true=点亮；false=熄灭
 * @note API Contract: 同 dal_led_on。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_set(dal_led_t *dev, bool on);

/**
 * @brief 翻转 LED 状态（on↔off）
 * @note API Contract: 同 dal_led_on。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_led_toggle(dal_led_t *dev);

/**
 * @brief 反初始化 LED：关闭输出、释放 GPIO 资源、置 initialized=false。
 * @note 可在未 init 的 dev 上安全调用（直接返回 WINK_OK，no-op）。
 * @return WINK_OK
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
wink_status_t dal_led_deinit(dal_led_t *dev);
#endif /* !WINK_USE_LED */

#endif /* DAL_LED_H */
