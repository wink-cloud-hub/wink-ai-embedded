#ifndef DAL_ENCODER_H
#define DAL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
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

/* ABI stability: config MUST remain the first member (DAL-S-011). */
_Static_assert(offsetof(dal_encoder_t, config) == 0, "config must be the first member");

/**
 * @brief 初始化编码器：校验参数、claim GPIO 引脚、配置输入模式、注册上升沿 ISR。
 *
 * @param dev 编码器实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL（静态存储）；cfg->pin_a >= 0。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；count=0；ISR 已注册；cfg 已深拷贝。
 *   - Blocking: No。
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/越界) / WINK_ERR_UNSUPPORTED(variant) /
 *     透传 PAL 错误（WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);

/**
 * @brief 获取当前脉冲计数值（读缓存，不触发硬件采样）。
 * @param dev 编码器实例句柄
 * @param out_count 输出当前计数值（只读 volatile 单字，无临界区）
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；out_count 非 NULL；dal_encoder_init() 已成功。
 *   - Postconditions: WINK_OK 时 *out_count 为当前计数值。
 *   - Blocking: No。
 *   - Thread-safe: No（单字宽 volatile 读，容忍旧值，DAL-C-001）。
 *   - ISR-safe: Yes（单字 aligned 读，无锁/无阻塞）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);

/**
 * @brief 重置脉冲计数值为零（临界区原子清零）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_encoder_init() 已成功。
 *   - Postconditions: WINK_OK 时 count=0。
 *   - Blocking: No。
 *   - Thread-safe: No（使用 PAL_CRITICAL_SECTION 原子清零，DAL-C-002）。
 *   - ISR-safe: No（临界区在 task 上下文使用）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
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
