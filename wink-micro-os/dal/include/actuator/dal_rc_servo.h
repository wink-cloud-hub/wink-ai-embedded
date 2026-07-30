#ifndef DAL_RC_SERVO_H
#define DAL_RC_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 舵机 PWM 时钟需求（DAL 语义；不引用 pal_*，ADR-0034） */
typedef uint8_t dal_rc_servo_clock_requirement_t;

enum {
    DAL_RC_SERVO_CLOCK_AUTO            = 0,
    DAL_RC_SERVO_CLOCK_STABLE_REQUIRED = 1,
};

/**
 * @brief 舵机构造期配置（dal_rc_servo_init 输入）
 *
 * 字段按 ABI 布局排序（pointer → uint8 → float），非机械尾加（ADR-0034 §2.5）。
 * `resolution_bits` / `clock_requirement` 为 0 (= AUTO) 时行为等同今日
 * `pal_pwm_init(ch, 50)`（13-bit + AUTO）。Flash override wire v1 仍为 9 bytes，
 * **不含** advanced 字段。
 */
typedef struct {
    const char                    *owner;              /* device_tree 实例名，静态存储 */
    uint8_t                        pwm_channel;
    uint8_t                        resolution_bits;    /* 0 = AUTO → 平台默认 13-bit */
    dal_rc_servo_clock_requirement_t  clock_requirement;  /* 0 = AUTO */
    float                          min_pulse_ms;
    float                          max_pulse_ms;
    float                          max_angle;          /* 0 = default 180° travel */
} dal_rc_servo_config_t;

/**
 * @brief 舵机实例（运行期状态；POD，ADR-0004 静态分发）
 *
 * 成员按对齐需求降序排列（c-code.md §4）：float(4B) → uint8_t/bool(1B)，
 * 消除内部 padding。
 *
 * Phase 2 标准化：所有 DAL 设备统一嵌入 `.config` 副本（与 led/button/ultrasonic
 * 一致），便于 codegen 统一遍历、Flash 覆写（ADR-0008）和运行时诊断。
 */
typedef struct {
    dal_rc_servo_config_t config;       /**< 配置副本（owner/pwm_channel/min_pulse_ms/max_pulse_ms），init 从 cfg 深拷贝 */
    float    current_angle;          /**< State: 当前角度（度，钳位后） */
    bool     initialized;            /**< State: init 成功后置 true */
} dal_rc_servo_t;

/**
 * @brief 初始化舵机：校验配置、一次性 PWM init（占用 PWM 通道）、置 initialized。
 * @note config vs device 字段冗余：min/max_pulse_ms 在 config(输入) 与 device(解析后状态) 中
 *       重复是有意的——config 是构造期输入，device 持有运行期解析值；未来 270° 舵机的
 *       max_angle 亦应作 config 传入而非硬编码。
 * @note API Contract:
 *   - Preconditions: dev/cfg 非 NULL；min_pulse_ms > 0；max_pulse_ms > min_pulse_ms。
 *   - Blocking: No（pal_pwm_init 不阻塞）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/非法脉宽) / 透传 PAL 错误
 *     （WINK_ERR_INVALID_ARG channel / WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 *   - Postconditions: WINK_OK 时 dev->initialized=true，PWM 通道已占用。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);

/**
 * @brief 设置舵机偏转角度
 * @param dev 舵机实例句柄
 * @param angle 目标角度 (0.0~effective_max_angle 度，超出范围自动钳位)
 * @return wink_status_t (0=成功，负数=错误码)
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_rc_servo_init() 已成功（!initialized 返回 NOT_INITIALIZED）。
 *   - Blocking: No
 *   - Thread-safe: No (多任务访问需外部互斥)
 *   - ISR-safe: No
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误
 *     （WINK_ERR_INVALID_ARG channel / WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）
 *   - Postconditions: WINK_OK 时 dev->current_angle 更新为钳位后的目标角度
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle);

/**
 * @brief 舵机安全关断（duty=0 → 失保持力 limp = 安全）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_rc_servo_init() 已成功。
 *   - Blocking: No（不 sleep）; Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时该通道 duty=0。
 * ⚠ safe-off 语义边界（架构师红线）：duty=0 对**舵机** = 失去保持力（limp）= 安全（无意外运动）。
 *    但对**未来 DC 电机 DAL**，duty=0 可能是 coast（滑行）而非 brake（制动）——并非通用安全态。
 *    本函数仅适用舵机；其它执行器类型须注册各自语义正确的关断（见 wink_actuator_registry.h），
 *    不得外推为通用执行器关断范式。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);

/**
 * @brief ADR-0008 Flash 覆写：从 16B params 反序列化并改写舵机配置字段。
 * @note params 布局（小端，memcpy 处理非对齐 f32）：pwm_channel:u8@0,
 *       min_pulse_ms:f32@1, max_pulse_ms:f32@5（≥9B）。
 *       **不含** max_angle（本 Phase Non-goal；未来 wire v2 再版本化）。
 *       轻校验(min>0 / max>min / channel<PAL_PWM_CHANNELS) 与 dal_rc_servo_init 权威校验纵深配合。
 *       非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *       void* 签名适配 wink_dev_override_fn 注册表（见 wink_dev_config.h），
 *       dev 在 dal_rc_servo_init 之前被覆写。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief 反初始化舵机：停止 PWM 占空比、反初始化 PWM、GPIO reset、释放资源。
 * @param dev 舵机实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 时返回 WINK_OK。
 *   - ADR-0024: 停 PWM、GPIO reset、释放 resource claim、memset 清零。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06) ────────────────────── */
#if !defined(WINK_USE_RC_SERVO) || !WINK_USE_RC_SERVO
#define WINK_RC_SERVO_DISABLED_MSG \
    "RC servo driver not enabled; add a \"rc_servo\" device to wink-app.json " \
    "(or set -DWINK_USE_RC_SERVO=ON)."
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);
#endif /* !WINK_USE_RC_SERVO */

#endif /* DAL_RC_SERVO_H */
