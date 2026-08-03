#ifndef DAL_ENCODER_H
#define DAL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "pal_hal.h"   /* wink_pin_t only (no pal_gpio_mode_t in this API) */

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
 * @brief 编码器引脚输入偏置模式（DAL 语义；不引用 pal_*，ADR-0034）
 *
 * 机械编码器通常需要上拉（默认）；霍尔/光电模块常自带推挽输出可用 NONE。
 * 驱动内部映射到 PAL GPIO 输入模式。
 */
typedef enum {
    DAL_ENCODER_PULL_UP   = 0,  /* default: internal pull-up (mechanical encoders) */
    DAL_ENCODER_PULL_DOWN = 1,  /* internal pull-down */
    DAL_ENCODER_PULL_NONE = 2,  /* floating input (external push-pull driver) */
} dal_encoder_pull_t;

/**
 * @brief 编码器配置结构体
 *
 * x1: A rising edge samples B; B high -> count++, B low -> count--;
 * no pin_b -> increment only (invert is N/A without B).
 * invert: swap A/B direction sense (phase polarity), not get_count negation.
 *
 * No apply_override wire yet. Future serialization follows config member order.
 */
typedef struct {
    const char *owner;      /* 资源占用 owner 静态字符串 */
    wink_pin_t pin_a;       /* 编码器 A 相引脚（必填，>= 0） */
    wink_pin_t pin_b;       /* 编码器 B 相引脚 (可选，若不使用设为 -1) */
    dal_encoder_pull_t pull; /* 引脚输入偏置：0 = PULL_UP（默认） */
    dal_encoder_variant_t variant; /* 0 = x1_rising (today's ISR) */
    bool invert;            /* false = today's A/B sense */
} dal_encoder_config_t;

/**
 * @brief 编码器句柄结构体
 */
typedef struct {
    dal_encoder_config_t config; /* 配置副本 */
    volatile int32_t count;      /* 当前脉冲计数值（ISR 单写者，单字 volatile） */
    bool initialized;            /* 是否初始化 */
    bool isr_registered;         /* 是否注册了中断 */
} dal_encoder_t;

/* ABI stability (spec §2.3 / DAL-BC-010): config MUST remain the first member.
 * 64-bit host (LP64) measured: config=24, handle=32, initialized@28.
 * 32-bit target (ILP32) derived: config=20, handle=28, initialized@24.
 * Recompute with the target compiler if the struct layout changes — enums are
 * int-sized (4B) and wink_pin_t is int16_t (2B). */
_Static_assert(offsetof(dal_encoder_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_encoder_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_encoder_t, initialized) == 24, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_encoder_t) == 28, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_encoder_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_encoder_t, initialized) == 28, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_encoder_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化编码器：校验参数、claim GPIO 引脚、配置输入模式、注册上升沿 ISR。
 *
 * Init-to-Ready（DAL-BC-001）：成功后立即开始计数，无需额外 start/enable 步骤。
 * `initialized` 仅在 ISR 注册成功后置位——失败路径不留半初始化句柄（DAL-L-007）。
 *
 * @param dev 编码器实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL（静态存储）；
 *                    cfg->pin_a >= 0；cfg->variant == X1_RISING；dev 未 initialized。
 *   - Postconditions: WINK_OK 时 dev->initialized=true、dev->isr_registered=true、
 *                     count=0、cfg 已深拷贝；失败时回滚本次 claim 的 GPIO 资源与
 *                     GPIO 配置，dev->initialized 保持 false（可安全 deinit）。
 *   - Range: pin_a >= 0（必填）；pin_b = -1（未使用）或 >= 0；
 *            pull ∈ {PULL_UP=0, PULL_DOWN=1, PULL_NONE=2}；
 *            variant 仅支持 X1_RISING（X2/X4 返回 UNSUPPORTED）。
 *   - Blocking: No。
 *   - Thread-safe: No（默认非线程安全，调用方串行化，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: claim pin_a（及 pin_b 若使用）GPIO 资源；配置 GPIO 输入偏置；
 *                   注册 pin_a 上升沿 ISR；清零 dev->count。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/pin_a<0/pull 越界) /
 *     WINK_ERR_UNSUPPORTED(variant X2/X4) / WINK_ERR_ALREADY_INITIALIZED /
 *     透传 PAL 错误（WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);

/**
 * @brief 获取当前脉冲计数值（读缓存，不触发硬件采样）。
 *
 * @param dev 编码器实例句柄
 * @param out_count 输出当前计数值（只读 volatile 单字，无临界区）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；out_count 非 NULL；dal_encoder_init() 已成功。
 *   - Postconditions: WINK_OK 时 *out_count 为当前计数值；硬件状态不变（get_* 语义）。
 *   - Range: int32_t 全域；正交编码器长期高速运行可能回绕（约 ±2.1e9 脉冲），
 *            转速/圈数换算由上层（BAL）处理。
 *   - Blocking: No。
 *   - Thread-safe: No（单字宽 volatile 读，容忍旧值，DAL-C-001）。
 *   - ISR-safe: Yes（单字 aligned 读，无锁/无阻塞）。
 *   - Side-effects: 无（仅读 dev->count 缓存，不碰硬件）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);

/**
 * @brief 重置脉冲计数值为零（临界区原子清零）。
 *
 * @param dev 编码器实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_encoder_init() 已成功。
 *   - Postconditions: WINK_OK 时 count=0（对 ISR 并发写原子）。
 *   - Blocking: No。
 *   - Thread-safe: No（使用 PAL_CRITICAL_SECTION 原子清零，DAL-C-002）。
 *   - ISR-safe: No（临界区在 task 上下文使用）。
 *   - Side-effects: 在临界区内清零 dev->count；不碰硬件。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_reset(dal_encoder_t *dev);

/**
 * @brief 反初始化编码器：卸 ISR、GPIO reset、释放资源、memset 清零。
 *
 * @param dev 编码器实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: 禁中断 → 等 in-flight ISR 结束（synchronize）→ reset pin_a/pin_b →
 *                     释放 GPIO claim → memset 清零；dev->initialized=false。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No。
 *   - Idempotent: 未 init 时返回 WINK_OK（DAL-L-010）；deinit 后可再次 init。
 *   - Side-effects: 卸载 pin_a ISR、复位 GPIO 引脚、释放资源 claim、清零句柄。
 *   - ADR-0024: 卸 GPIO ISR + synchronize、GPIO reset、释放 resource claim、memset 清零；
 *               释放失败记 LOG_W 但不中断清场（DAL-L-014/015）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）。
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
