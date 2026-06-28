#ifndef DAL_ULTRASONIC_H
#define DAL_ULTRASONIC_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 非阻塞测量状态机（Phase 4） */
typedef enum {
    DAL_ULTRASONIC_IDLE      = 0,
    DAL_ULTRASONIC_MEASURING = 1,
    DAL_ULTRASONIC_READY     = 2,
    DAL_ULTRASONIC_ERROR     = 3,
} dal_ultrasonic_state_t;

/**
 * 成员按对齐需求降序排列（c-code.md §4）：4B(float/uint32/enum) → 2B(uint16) → 1B(bool)，
 * 消除内部 padding（28B → 24B）。仅重排顺序、未改字段名，designated initializer 与
 * 所有 `dev->xxx` 访问均不受影响（非破坏性）。
 */
typedef struct {
    /* —— 4B —— */
    float                   last_distance;   ///< 最近一次测量距离 (cm)
    uint32_t                last_pulse_us;   ///< Phase 4：上次 echo 脉宽 μs
    wink_status_t           last_status;     ///< Phase 4：上次测量结果状态（ERROR 时为具体错误码）
    dal_ultrasonic_state_t  state;           ///< Phase 4：非阻塞测量状态机
    /* —— 2B —— */
    uint16_t                trig_pin;
    uint16_t                echo_pin;
    /* —— 1B —— */
    bool                    initialized;     ///< Phase 2：dal_ultrasonic_init 成功后置 true
    bool                    use_rmt;         ///< ESP32：true=RMT 硬件捕获，false=busy-wait 降级
} dal_ultrasonic_t;

/**
 * @brief 初始化超声波：校验引脚、配置 GPIO 方向（真机）、置 initialized。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；trig_pin != echo_pin。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/同 pin) / 透传 PAL 错误
 *     （真机：WINK_ERR_IO / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED）。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；trig/echo 方向已配置（真机）。
 *   - Sim 分支：跳过物理 GPIO 配置（旁路最低物理信号层，ADR-0003 决策2），仅置结构状态。
 *   - ESP32：自动初始化 RMT 硬件脉冲捕获；RMT 失败自动降级到 busy-wait。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, uint16_t trig_pin, uint16_t echo_pin);

/**
 * @brief 请求一次测量（触发后立即返回；非阻塞）。
 * @note host：测量在 request 内经 pal_gpio_pulse_in 同步完成（虚拟时间下单 tick 即 READY），
 *       表现为「单 tick ready」——这是可接受的仿真保真（host 测状态机契约，非真实 wall-clock 异步）。
 *       ESP32：经 RMT 硬件测量，CPU 仅阻塞在信号量等待（不消耗 CPU，由 FreeRTOS 调度）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Blocking: Yes (≈ 测量时间 + 调度开销)，but RMT version is not busy-waiting.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK(请求已发出) / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 *   - Postconditions: 触发已发出；结果（READY/ERROR + last_status）经 get_cached_distance 读。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);

/**
 * @brief 非阻塞读取上次测量的缓存距离/状态。
 * @note API Contract:
 *   - Preconditions: dev/distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Blocking: No.
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK(READY，*distance_cm 有效) / WINK_ERR_BUSY(MEASURING/IDLE) /
 *     last_status(ERROR) / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 *   - Postconditions: WINK_OK 时 *distance_cm 为缓存距离。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm);

/**
 * @brief 获取障碍物距离 (cm) —— 阻塞 busy-wait，**@deprecated**。
 * @deprecated Runtime/BAL 10ms tick 不得调用本 API；保留仅供过渡/单测，BAL 完全迁移到非阻塞
 *             且 host 协作推进重构后移除（Phase 4 follow-up）。
 * @note Blocking: Yes. Worst-case ≈ 2 * ULTRASONIC_TIMEOUT_US + trigger pulse (≈ 60ms+)。
 *       Not allowed in cooperative runtime loop.
 * @note API Contract:
 *   - Preconditions: dev/distance_cm 非 NULL；dal_ultrasonic_init() 已成功。
 *   - Thread-safe: No; ISR-safe: No (含阻塞 delay/polling)
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_TIMEOUT
 *   - Postconditions: dev->last_distance 在 WINK_OK 时更新
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);

/**
 * @brief ADR-0008 Flash 覆写：从 16B params 反序列化并改写超声波 trig/echo 引脚。
 * @note params 布局（小端）：trig_pin:u16@0, echo_pin:u16@2（≥4B）。
 *       轻校验(trig≠echo) 与 dal_ultrasonic_init 权威校验纵深配合。
 *       非法 → 不写任何字段，返 WINK_ERR_INVALID_ARG。
 *       void* 签名适配 wink_dev_override_fn 注册表（见 wink_dev_config.h），
 *       dev 在 dal_ultrasonic_init 之前被覆写。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* DAL_ULTRASONIC_H */
