// SPDX-License-Identifier: GPL-3.0-only
/**
 * @file pal_pwm.h
 * @brief PAL PWM Interface Subsystem with Basis Points, Dynamic Pin Routing, and Zero Soft-FP (ADR-0066).
 */

#ifndef PAL_PWM_H
#define PAL_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_target_caps.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    struct_size;       /**< Size of struct for forward ABI compatibility */
    wink_pin_t                  pin;               /**< Target physical GPIO pin (WINK_PIN_NC = use channel default) */
    uint32_t                    freq_hz;           /**< PWM base frequency in Hz */
    uint8_t                     resolution_bits;   /**< 0 = AUTO -> target optimal default */
    pal_pwm_clock_requirement_t clock_requirement; /**< Clock source stability requirement */
} pal_pwm_config_t;

/* ======================================================================
 * PWM Basis-Points (bp) 定点算法 SSOT (ADR-0066, PLAN-20260912)
 *
 * 量纲：万分比 0..10000 = 0.00%..100.00% (Basis Points, 非 permille)。
 * 所有 target 的 bp->counter 换算必须语义对齐到本 helper，禁止各 target
 * 私自内联公式（esp32/host/wasm 历史三分裂见本计划 P2）。
 *
 * 8/16 位 target 接入约束 (T2.5)：必须在编译命令行或 board 头中定义
 *   PAL_PWM_DUTY_TOP_LIMIT <= 65535（mcs51 不得走 uint64 路径）；
 * 否则默认全范围保留 64 位路径。target 亦可完全覆写本 helper (D2)。
 * 8 位 PWM (top<=255, 如未来 mcs51/PCA)：可使用 32 位分支，或 256 项
 * LUT（Backlog B-2）。
 * ====================================================================== */

/**
 * 32位无符号整数乘法安全阈值：(9999u * PAL_PWM_TOP_32BIT_MAX + 5000u) <= UINT32_MAX (4,294,967,295)
 * 消除裸写 429496u 魔法数字，明确数学推导依据。
 * 推导：9999 * 429496 = 4294530504；+5000 = 4294535504 < 4294967295。
 */
#define PAL_PWM_TOP_32BIT_MAX 429496u

#ifndef PAL_PWM_DUTY_TOP_LIMIT
#define PAL_PWM_DUTY_TOP_LIMIT 0xFFFFFFFFu  /* 全范围：保留 64 位路径 */
#endif

/**
 * @brief bp->counter 通用四舍五入防溢出算法（底层算术 helper）。
 * @param[in] bp 万分比占空比 [0, 10000]；>=10000 安全钳位返回 top。
 * @param[in] top 定时器满量程计数（(1<<bits)-1）；0 保护返回 0。
 * @return 四舍五入后的 counter，钳位 <= top。
 * @note 防御性分工：本 helper 对 bp>=10000 实行安全钳位；而公共 PAL 接口
 *       pal_pwm_set_duty_bp 严格履行 ADR-0012 合约诚实，当 bp > 10000u 时
 *       返回 WINK_ERR_INVALID_ARG（见下）。
 * @note PAL_PWM_DUTY_TOP_LIMIT <= PAL_PWM_TOP_32BIT_MAX 时仅编译 32 位分支
 *       （纯 32 位乘除，MCS51 / Cortex-M0 编译期剔除 __udivdi3），否则
 *       runtime top<=PAL_PWM_TOP_32BIT_MAX 快速路径 + uint64_t 满范围路径。
 */
static inline uint32_t pal_pwm_calc_duty_counter(uint16_t bp, uint32_t top)
{
    if ((bp == 0u) || (top == 0u)) { return 0u; }
    if (bp >= 10000u) { return top; }
#if (PAL_PWM_DUTY_TOP_LIMIT <= PAL_PWM_TOP_32BIT_MAX)
    {
        uint32_t prod = (uint32_t)bp * top;
        uint32_t count = (prod + 5000u) / 10000u;
        return (count > top) ? top : count;
    }
#else
    if (top <= (uint32_t)PAL_PWM_TOP_32BIT_MAX) {
        uint32_t prod = (uint32_t)bp * top;
        uint32_t count = (prod + 5000u) / 10000u;
        return (count > top) ? top : count;
    } else {
        uint64_t product = (uint64_t)bp * (uint64_t)top;
        uint32_t count = (uint32_t)((product + 5000ull) / 10000ull);
        return (count > top) ? top : count;
    }
#endif
}

/* --- 防手抖 helper 与宏常量（宏 + inline 双轨支持） --- */

/* 常量表达式安全宏（满足 C99 全局/静态配置结构体初值初始化） */
#define PAL_PWM_DUTY_PCT(p)       ((uint16_t)((uint32_t)(p) >= 100u ? 10000u : (uint32_t)(p) * 100u))
#define PAL_PWM_DUTY_PERMILLE(pm) ((uint16_t)((uint32_t)(pm) >= 1000u ? 10000u : (uint32_t)(pm) * 10u))
#define PAL_PWM_DUTY_OFF          (0u)
#define PAL_PWM_DUTY_HALF         (5000u)
#define PAL_PWM_DUTY_FULL         (10000u)

/**
 * @brief 整数百分比 -> bp（运行时 helper，带钳位）。
 * @note 小数百分比（如 7.5%）使用规范：PAL_PWM_DUTY_PCT 仅用于纯整数百分比；
 *       对于 RC 舵机中位等带小数的占空比（如 7.5% = 1.5ms/20ms），必须使用
 *       千分比宏 PAL_PWM_DUTY_PERMILLE(75) 或直接书写 750u。
 *       严禁传入 pal_pwm_duty_pct(7.5)（整型隐式截断为 700 bp，
 *       引发约 9 度舵机机械偏角）。
 */
/* 运行时内联辅助函数（带参数类型约束与边界钳位） */
static inline uint16_t pal_pwm_duty_pct(uint8_t pct) {
    return (pct >= 100u) ? 10000u : (uint16_t)(pct * 100u);
}
static inline uint16_t pal_pwm_duty_permille(uint16_t pm) {
    return (pm >= 1000u) ? 10000u : (uint16_t)(pm * 10u);
}

/**
 * @brief Query physical GPIO mapped to specified PWM channel
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[out] out_pin Output pointer for mapped GPIO pin
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds, WINK_ERR_UNSUPPORTED if target lacks routing
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin);

/**
 * @brief Legacy basic init wrapper (default pin + auto clock)
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[in] frequency_hz PWM frequency in Hz
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

/**
 * @brief Extended init: dynamic pin routing + freq + resolution + clock requirement
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNEL_MAX)
 * @param[in] cfg Extended PWM configuration struct
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);

/**
 * @brief Set PWM duty cycle in Basis Points (0..10000 = 0.00%..100.00%)
 * @note ISR-Safe. Guarantees 0 soft-float library overhead and zero 32-bit overflow.
 * @note 四舍五入语义：counter = (bp*top + 5000) / 10000（见 pal_pwm_calc_duty_counter）；
 *       bp==0/top==0 返回 0；bp 全量程直返 top。helper 层钳位，API 层对
 *       bp > 10000u 严格返回 WINK_ERR_INVALID_ARG（ADR-0012 合约诚实）。
 * @param[in] channel PWM channel ID
 * @param[in] basis_points Duty cycle in basis points [0, 10000]
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of range (>10000)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty_bp(uint8_t channel, uint16_t basis_points);

#ifndef PAL_PWM_HIDE_FLOAT_API
/**
 * @brief Legacy floating-point duty cycle setter (0.0f..1.0f)
 * @deprecated Use pal_pwm_set_duty_bp instead to eliminate soft-fp code bloat (ADR-0066).
 * @param[in] channel PWM channel ID
 * @param[in] duty Duty cycle in range [0.0f, 1.0f]
 * @return WINK_OK on success, error status code otherwise
 */
WINK_DEPRECATED_MSG("Use pal_pwm_set_duty_bp instead to eliminate soft-fp library overhead")
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);
#endif /* PAL_PWM_HIDE_FLOAT_API */

/**
 * @brief Dynamically adjust PWM frequency on active channel
 * @param[in] channel PWM channel ID
 * @param[in] freq_hz New frequency in Hz
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz);

/**
 * @brief Deinitialize specified PWM channel and release claimed pin & channel resources
 * @param[in] channel PWM channel ID
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_deinit(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PWM_H */
