#ifndef DAL_ENCODER_H
#define DAL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Quadrature decode mode (Phase 1: x1 only).
 */
typedef enum {
    DAL_ENCODER_VARIANT_X1_RISING = 0, /* default; A rising samples B */
    DAL_ENCODER_VARIANT_X2 = 1,        /* reserved -> init UNSUPPORTED */
    DAL_ENCODER_VARIANT_X4 = 2,        /* reserved -> init UNSUPPORTED */
} dal_encoder_variant_t;

/**
 * @brief 编码器配置结构体
 *
 * x1: A rising edge samples B; B high -> count++, B low -> count--;
 * no pin_b -> increment only.
 * invert: swap A/B direction sense (phase polarity), not get_count negation.
 *
 * No apply_override wire yet. Future serialization follows config member order.
 */
typedef struct {
    const char *owner;      /* 资源占用 owner 静态字符串 */
    wink_pin_t pin_a;       /* 编码器 A 相引脚 */
    wink_pin_t pin_b;       /* 编码器 B 相引脚 (可选，若不使用设为 -1) */
    pal_gpio_mode_t pull;   /* 引脚输入上拉/下拉模式：PAL_GPIO_INPUT_PULLUP 等 */
    dal_encoder_variant_t variant; /* 0 = x1_rising (today's ISR) */
    bool invert;            /* false = today's A/B sense */
} dal_encoder_config_t;

/**
 * @brief 编码器句柄结构体
 */
typedef struct {
    dal_encoder_config_t config; /* 配置副本 */
    volatile int32_t count;      /* 当前脉冲计数值 */
    bool initialized;            /* 是否初始化 */
    bool isr_registered;         /* 是否注册了中断 */
} dal_encoder_t;

/**
 * @brief 初始化编码器
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);

/**
 * @brief 获取当前脉冲计数值
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);

/**
 * @brief 重置脉冲计数值为零
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_reset(dal_encoder_t *dev);

/**
 * @brief 反初始化编码器：卸 ISR、GPIO reset、释放资源。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 时返回 WINK_OK。
 *   - ADR-0024: 卸 GPIO ISR + synchronize、GPIO reset、释放 resource claim、memset 清零。
 */
wink_status_t dal_encoder_deinit(dal_encoder_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs ── */
#if !defined(WINK_USE_ENCODER) || !WINK_USE_ENCODER
#define WINK_ENCODER_DISABLED_MSG \
    "Encoder driver not enabled; add an \"encoder\" device to wink-app.json " \
    "(or set -DWINK_USE_ENCODER=ON)."
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_reset(dal_encoder_t *dev);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG)
wink_status_t dal_encoder_deinit(dal_encoder_t *dev);
#endif /* !WINK_USE_ENCODER */

#endif /* DAL_ENCODER_H */
