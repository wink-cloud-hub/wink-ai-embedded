#ifndef DAL_RC_SERVO_H
#define DAL_RC_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
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
 * A 类执行器命令按 ADR-0056 全 Profile 使用定标整数：
 * - 脉宽用 µs（`_us`），角度用 0.1°（`_ddeg`），无 float。
 * - `min_pulse_us`/`max_pulse_us` 为硬件脉宽范围（典型 500~2500µs）。
 * - `max_angle_ddeg` 为行程上限（0 = 默认 180.0° = 1800 ddeg）。
 *
 * 成员按尺寸降序排列（pointer → uint16×3 → uint8×3）。
 */
typedef struct {
    const char                    *owner;              /**< device_tree 实例名，静态存储 */
    uint16_t                       min_pulse_us;       /**< 最小脉宽 µs（典型 500） */
    uint16_t                       max_pulse_us;       /**< 最大脉宽 µs（典型 2500） */
    uint16_t                       max_angle_ddeg;     /**< 行程上限 0.1°（0=默认 1800=180.0°） */
    uint8_t                        pwm_channel;        /**< PWM 通道号 [0, PAL_PWM_CHANNELS) */
    uint8_t                        resolution_bits;    /**< 0 = AUTO → 平台默认 13-bit */
    dal_rc_servo_clock_requirement_t clock_requirement; /**< 0 = AUTO */
} dal_rc_servo_config_t;

/**
 * @brief 舵机实例（运行期状态；POD，ADR-0004 静态分发）
 *
 * Phase 2 标准化：所有 DAL 设备统一嵌入 `.config` 副本，便于 codegen 统一遍历、
 * Flash 覆写（ADR-0008）和运行时诊断。
 */
typedef struct {
    dal_rc_servo_config_t config;          /**< 配置副本，init 从 cfg 深拷贝 */
    uint16_t              current_angle_ddeg; /**< State: 当前角度（0.1°，钳位后） */
    bool                  initialized;     /**< State: init 成功后置 true */
} dal_rc_servo_t;

/* ABI stability: config MUST remain the first member (DAL-S-011).
 * 64-bit host (LP64) measured: config=24, handle=32, initialized@26.
 * 32-bit target (ILP32) derived: config=16, handle=20, initialized@18.
 * Recompute with the target compiler if the struct layout changes. */
_Static_assert(offsetof(dal_rc_servo_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_rc_servo_config_t) == 16, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_rc_servo_t, initialized) == 18, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_rc_servo_t) == 20, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_rc_servo_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_rc_servo_t, initialized) == 26, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_rc_servo_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief 初始化舵机：校验配置、一次性 PWM init（占用 PWM 通道）、写零占空比、置 initialized。
 *
 * Init-to-Ready（DAL-BC-001）：成功后立即可接收 set_angle，无需额外 arm/enable 步骤。
 * Init 后输出零能量（PWM duty=0，舵机 limp）。
 *
 * @param dev   舵机实例句柄
 * @param cfg   配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev/cfg 非 NULL；cfg->owner 非 NULL；cfg->pwm_channel < PAL_PWM_CHANNELS；
 *                    dev 未 initialized；min_pulse_us > 0；max_pulse_us > min_pulse_us。
 *   - Postconditions: WINK_OK → dev->initialized=true；config 深拷贝并规范化脉宽；
 *                     PWM 通道已 claim 且初始化为 50Hz；duty=0（limp 零能量）。
 *                     失败时回滚 PWM claim，不置 initialized。
 *   - Range: pwm_channel [0, PAL_PWM_CHANNELS)；min_pulse_us/max_pulse_us uint16（典型 500~2500）；
 *            max_angle_ddeg 0（=默认 1800）或正整数。
 *   - Blocking: No（PAL claim + PWM init，无 busy-wait）。
 *   - Thread-safe: No（调用方串行化 init 与其它方法，DAL-C-040）。
 *   - ISR-safe: No。
 *   - Side-effects: claim PWM 通道资源；初始化 PWM 定时器（50Hz）；写 duty=0。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（NULL/channel 越界/脉宽非法）/
 *     WINK_ERR_ALREADY_INITIALIZED / WINK_ERR_UNSUPPORTED（stable clock）/
 *     透传 PAL 错误（WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED / WINK_ERR_IO）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);

/**
 * @brief 设置舵机偏转角度（A 类执行器命令，全 Profile 定标整数）。
 *
 * @param dev          舵机实例句柄
 * @param angle_ddeg   目标角度，单位 0.1°（ddeg）。900 = 90.0°，1800 = 180.0°。
 *                     超出 [0, effective_max_angle_ddeg] 自动钳位饱和（DAL-U-011）。
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_rc_servo_init() 已成功。
 *   - Postconditions: WINK_OK → 硬件 PWM 占空比已更新；dev->current_angle_ddeg 更新为钳位后的角度。
 *   - Range: angle_ddeg [0, effective_max_angle_ddeg]，单位 0.1°。
 *     具名示例：0=0°（最小脉宽位），900=90.0°（中位），1800=180.0°（最大脉宽位）。
 *   - Blocking: No。
 *   - Thread-safe: No（调用方串行化，DAL-C-040）。
 *   - ISR-safe: No（调 pal_pwm_set_duty）。
 *   - Side-effects: 更新 PWM 占空比；写 dev->current_angle_ddeg（仅在硬件成功后）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）/ WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, uint16_t angle_ddeg);

/**
 * @brief 舵机安全关断（duty=0 → 失保持力 limp = 安全）。
 *
 * 与普通 dal_rc_servo_off 的区别：本函数用于 watchdog/panic/assert 失败/异常回滚路径，
 * 由 wink_actuator_safe_off_all() 遍历调用。因此：
 *   - 不要求 dev 已初始化（未初始化即"无物可关"，返回 WINK_OK，DAL-L-022）；
 *   - 不标 WINK_WARN_UNUSED_RESULT（应急路径不强制检查返回值，DAL-L-021）；
 *   - 尽力关断（best-effort），不依赖调度器与堆。
 *
 * @param dev 舵机实例句柄
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。不要求已 init。
 *   - Postconditions: best-effort 写 duty=0（limp）；若已初始化则 current_angle_ddeg 不变（角度缓存保留，
 *                     但硬件无输出）。
 *   - Blocking: No; Thread-safe: No; ISR-safe: No（调 pal_pwm_set_duty）。
 *   - Reentrancy: Yes（幂等，可重复调用）。
 *   - Side-effects: 写 PWM duty=0。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）。
 *
 * ⚠ safe-off 语义边界（架构师红线）：duty=0 对**舵机** = 失去保持力（limp）= 安全（无意外运动）。
 *    但对**DC 电机**，duty=0 可能是 coast（滑行）而非 brake（制动）——并非通用安全态。
 *    本函数仅适用舵机；其它执行器须注册各自语义正确的关断，不得外推。
 */
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);

/**
 * @brief ADR-0008 Flash 覆写：从 params 反序列化并改写舵机配置字段。
 *
 * 仅允许在 dal_rc_servo_init **之前**调用（DAL-S-015 config 不可变性）。
 * init 后调用返回 WINK_ERR_INVALID_ARG，不修改任何字段。
 *
 * @note params 布局（小端，memcpy 处理非对齐整数）：
 *       pwm_channel:u8@0, min_pulse_us:u16@1, max_pulse_us:u16@3,
 *       max_angle_ddeg:u16@5（共 7 字节）。
 *       轻校验（channel<PAL_PWM_CHANNELS / min>0 / max>min）与 init 权威校验纵深配合。
 *       非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *       void* 签名适配 wink_dev_override_fn 注册表。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief 反初始化舵机：safe_off → 停 PWM → GPIO reset → 释放资源 → memset 清零。
 *
 * @param dev 舵机实例句柄
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Postconditions: best-effort 关断（duty=0 limp）→ pal_pwm_deinit → 查询路由引脚并
 *                     pal_gpio_reset_pin → 释放 PWM channel claim → memset 清零；
 *                     dev->initialized=false。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No。
 *   - Idempotent: 未 init 时返回 WINK_OK（DAL-L-010）；deinit 后可再次 init。
 *   - Side-effects: 停 PWM、复位 GPIO、释放资源、清零句柄。
 *   - ADR-0024: 停 PWM、GPIO reset、释放 resource claim、memset 清零。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG（dev NULL）。
 */
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs ── */
#if !defined(WINK_USE_RC_SERVO) || !WINK_USE_RC_SERVO
#define WINK_RC_SERVO_DISABLED_MSG \
    "RC servo driver not enabled; add a \"rc_servo\" device to wink-app.json " \
    "(or set -DWINK_USE_RC_SERVO=ON)."
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_init(dal_rc_servo_t *dev, const dal_rc_servo_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, uint16_t angle_ddeg);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG)
wink_status_t dal_rc_servo_safe_off(dal_rc_servo_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_RC_SERVO_DISABLED_MSG)
wink_status_t dal_rc_servo_deinit(dal_rc_servo_t *dev);
#endif /* !WINK_USE_RC_SERVO */

#endif /* DAL_RC_SERVO_H */
