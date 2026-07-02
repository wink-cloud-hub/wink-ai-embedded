#ifndef DAL_SERVO_H
#define DAL_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 舵机构造期配置（dal_servo_init 输入） */
typedef struct {
    const char *owner;       /* 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint8_t pwm_channel;
    float min_pulse_ms;
    float max_pulse_ms;
} dal_servo_config_t;

/**
 * @brief 舵机实例（运行期状态；POD，ADR-0004 静态分发）
 *
 * 成员按对齐需求降序排列（c-code.md §4）：float(4B) → uint8_t/bool(1B)，
 * 消除内部 padding（20B → 16B）。仅重排顺序、未改字段名，故 designated
 * initializer 与所有 `dev->xxx` 访问均不受影响（非破坏性）。
 */
typedef struct {
    float    min_pulse_ms;   /* Config: 最小脉宽 ms */
    float    max_pulse_ms;   /* Config: 最大脉宽 ms */
    float    current_angle;  /* State:  当前角度（度，钳位后） */
    uint8_t  pwm_channel;    /* Config: PWM 通道 */
    bool     initialized;    /* State:  init 成功后置 true */
} dal_servo_t;

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
wink_status_t dal_servo_init(dal_servo_t *dev, const dal_servo_config_t *cfg);

/**
 * @brief 设置舵机偏转角度
 * @param dev 舵机实例句柄
 * @param angle 目标角度 (0.0~180.0 度，超出范围自动钳位)
 * @return wink_status_t (0=成功，负数=错误码)
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_servo_init() 已成功（!initialized 返回 NOT_INITIALIZED）。
 *   - Blocking: No
 *   - Thread-safe: No (多任务访问需外部互斥)
 *   - ISR-safe: No
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误
 *     （WINK_ERR_INVALID_ARG channel / WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）
 *   - Postconditions: WINK_OK 时 dev->current_angle 更新为钳位后的目标角度
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_servo_set_angle(dal_servo_t *dev, float angle);

/**
 * @brief 舵机安全关断（duty=0 → 失保持力 limp = 安全）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_servo_init() 已成功。
 *   - Blocking: No（不 sleep）; Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(dev NULL) / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时该通道 duty=0。
 * ⚠ safe-off 语义边界（架构师红线）：duty=0 对**舵机** = 失去保持力（limp）= 安全（无意外运动）。
 *    但对**未来 DC 电机 DAL**，duty=0 可能是 coast（滑行）而非 brake（制动）——并非通用安全态。
 *    本函数仅适用舵机；其它执行器类型须注册各自语义正确的关断（见 wink_actuator_registry.h），
 *    不得外推为通用执行器关断范式。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_servo_safe_off(dal_servo_t *dev);

/**
 * @brief ADR-0008 Flash 覆写：从 16B params 反序列化并改写舵机配置字段。
 * @note params 布局（小端，memcpy 处理非对齐 f32）：pwm_channel:u8@0,
 *       min_pulse_ms:f32@1, max_pulse_ms:f32@5（≥9B）。
 *       轻校验(min>0 / max>min / channel<PAL_PWM_CHANNELS) 与 dal_servo_init 权威校验纵深配合。
 *       非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *       void* 签名适配 wink_dev_override_fn 注册表（见 wink_dev_config.h），
 *       dev 在 dal_servo_init 之前被覆写。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* DAL_SERVO_H */
