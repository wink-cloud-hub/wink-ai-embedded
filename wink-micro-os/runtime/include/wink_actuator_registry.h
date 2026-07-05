/**
 * @file wink_actuator_registry.h
 * @brief 执行器统一关断注册表（Actuator Registry，静态表，零动态分配）。
 *
 * runtime fault/panic/on_fault 路径统一调 wink_actuator_safe_off_all 关断所有执行器
 * （review P0-4 / Phase 5）。各执行器 DAL 在 init 阶段注册**自己语义正确**的 safe-off 回调。
 *
 * ⚠ safe-off 语义边界：duty=0 对舵机=limp=安全，但对未来 DC 电机可能是 coast（滑行）而非
 *    brake（制动）。故「各自定义 safe-off」模型——电机须注册制动/断使能回调，而非简单 duty=0。
 *    本 registry 不假设通用关断范式（dal_servo_safe_off 的适用范围见 dal_servo.h）。
 *
 * ⚠ 复位期间硬件级默认安全态（引脚 Hi-Z 弱拉、执行器使能脚默认关断、电源门控）**必须由板级
 *    电路保证**——软件无法覆盖 HardFault/总线死锁/CPU 卡死/WDT 硬复位瞬间。本 registry 只补软件闭环。
 */
#ifndef WINK_ACTUATOR_REGISTRY_H
#define WINK_ACTUATOR_REGISTRY_H

#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册表容量（静态分配，可按平台 -D 调整） */
#ifndef WINK_ACTUATOR_REGISTRY_CAPACITY
#define WINK_ACTUATOR_REGISTRY_CAPACITY 16
#endif

/** @brief 执行器关断回调原型（ctx 为执行器实例指针等上下文） */
typedef wink_status_t (*wink_actuator_safe_off_fn)(void *ctx);

/**
 * @brief 注册一个执行器关断回调
 * @param fn 关断回调（NULL → WINK_ERR_INVALID_ARG）
 * @param ctx 传给回调的上下文（通常为执行器实例指针）
 * @return WINK_OK / WINK_ERR_INVALID_ARG(fn NULL) / WINK_ERR_RESOURCE_EXHAUSTED(表满)
 * @note 幂等：重复 (fn, ctx) → WINK_OK。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx);

/** @brief 清空注册表（测试隔离 / 启动重置用） */
void wink_actuator_registry_reset(void);

/**
 * @brief 关断所有已注册执行器。
 * @note 即使单个关断失败也继续遍历全部；失败项 wink_trace_fault 记录。
 *       **不得阻塞**（fault 路径须尽快进入安全态）。
 */
void wink_actuator_safe_off_all(void);

/**
 * @brief Define a type-adapting thunk for registering DAL actuator off
 *        functions with the registry.
 *
 * DAL safe-off APIs have typed signatures (e.g.
 * `wink_status_t dal_led_off(dal_led_t*)`) while the registry stores
 * `wink_status_t (*)(void*)`.  ISO C forbids nested functions, so we
 * cannot define the adapter at the registration call site.  This macro
 * declares a file-scoped static thunk with the correct signature.
 *
 * Usage at **file scope** (not inside a function body):
 * @code
 *     WINK_DEFINE_ACTUATOR_THUNK(board_led_safe_off, dal_led_off, dal_led_t)
 *     // ... in app_init:
 *     WINK_IGNORE_RESULT(wink_actuator_register(board_led_safe_off, &board_led));
 * @endcode
 *
 * @param thunk_name  Name for the generated static function (must be
 *                    unique within the translation unit).
 * @param fn          The typed DAL function to wrap (e.g. dal_led_off).
 * @param dev_type    The DAL device pointer type (e.g. dal_led_t).
 */
#define WINK_DEFINE_ACTUATOR_THUNK(thunk_name, fn, dev_type) \
    static wink_status_t thunk_name(void *_ctx) { return fn((dev_type *)_ctx); }

#ifdef __cplusplus
}
#endif

#endif /* WINK_ACTUATOR_REGISTRY_H */
