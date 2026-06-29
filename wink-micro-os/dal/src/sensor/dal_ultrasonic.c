#include "dal_ultrasonic.h"
#include "hal/pal_ultrasonic.h"  /* PAL 超声波接口契约（全平台统一） */
#include "pal_hal.h"
#include "pal_osal.h"

#include <string.h>   /* memcpy（ADR-0008 apply_override 反序列化） */

#define ULTRASONIC_TIMEOUT_US 30000u   /* 30ms 超时保护 */
#define ULTRASONIC_CM_PER_US  0.017f   /* 声速换算系数 (340m/s, 往返折半) */

/* ---- 两端共享：脉宽(us) -> 距离(cm) ----
 * 非 static 以便单元测试 extern 访问（例外：无副作用纯函数，风险可控）。 */
float dal_pulse_us_to_cm(uint32_t pulse_us) {
    return (float)pulse_us * ULTRASONIC_CM_PER_US;
}

wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len) {
    dal_ultrasonic_t *u = (dal_ultrasonic_t *)dev;
    if (u == NULL || params == NULL) { return WINK_ERR_INVALID_ARG; }
    if (len < 4u) { return WINK_ERR_INVALID_ARG; }   /* u16@0 + u16@2 → ≥4B */

    uint16_t trig_pin;
    uint16_t echo_pin;
    memcpy(&trig_pin, params + 0, 2);
    memcpy(&echo_pin, params + 2, 2);

    if (trig_pin == echo_pin) { return WINK_ERR_INVALID_ARG; }   /* 非法不写 */

    u->config.trig_pin = trig_pin;
    u->config.echo_pin = echo_pin;
    return WINK_OK;
}

wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->trig_pin == cfg->echo_pin) { return WINK_ERR_INVALID_ARG; }

    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_ultrasonic_config_t));
    dev->last_distance = 0.0f;
    dev->state = DAL_ULTRASONIC_IDLE;
    dev->last_status = WINK_OK;
    dev->last_pulse_us = 0u;

    /* 统一使用 PAL 接口初始化超声波硬件。
     * - WASM 仿真：pal_hal_ultrasonic_init 是空操作，仅返回 WINK_OK
     * - ESP32 真机：内部初始化 RMT（如果 enable）或降级到 GPIO busy-wait
     * - 无需任何平台条件编译，编译期静态分发 */
    wink_status_t status = pal_hal_ultrasonic_init(cfg->echo_pin);
    if (wink_status_is_error(status)) {
        /* PAL 初始化失败，降级到仅 GPIO 模式
         * （pal_hal_ultrasonic_measure_pulse_us 会自动选择可用实现） */
        dev->config.use_rmt = false;
    }

    /* GPIO 配置（TRIG 输出，ECHO 输入）
     * WASM 仿真下这两个函数也是空操作（pal_hal_wasm.c 实现） */
    status = pal_gpio_init(cfg->trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) { return status; }
    status = pal_gpio_init(cfg->echo_pin, PAL_GPIO_INPUT);
    if (wink_status_is_error(status)) { return status; }

    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. 触发超声波（TRIG 时序）
     * - WASM 仿真：内部委托 js_sim_trigger_ultrasonic 旁路
     * - ESP32 真机：输出 10us GPIO 脉冲
     * 统一 PAL 接口，无平台条件编译 */
    pal_hal_ultrasonic_trigger(dev->config.trig_pin);
    dev->state = DAL_ULTRASONIC_MEASURING;

    /* 2. 捕获 echo 脉宽
     * - WASM 仿真：内部委托 js_sim_measure_echo_pulse_us 物理模拟
     * - ESP32 真机：RMT 硬件捕获（优先）或 GPIO busy-wait
     * PAL 内部处理平台差异，DAL 层透明 */
    uint32_t pulse_us = 0;
    wink_status_t cap = pal_hal_ultrasonic_measure_pulse_us(
        dev->config.echo_pin,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );

    if (wink_status_is_error(cap)) {
        dev->last_status = cap;
        dev->state = DAL_ULTRASONIC_ERROR;
    } else {
        dev->last_pulse_us = pulse_us;
        dev->last_distance = dal_pulse_us_to_cm(pulse_us);
        dev->last_status = WINK_OK;
        dev->state = DAL_ULTRASONIC_READY;
    }
    return WINK_OK;   /* request 成功（已触发）；结果经 get_cached 读 */
}

wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    switch (dev->state) {
        case DAL_ULTRASONIC_READY:
            *distance_cm = dev->last_distance;
            return WINK_OK;
        case DAL_ULTRASONIC_MEASURING:
            return WINK_ERR_BUSY;
        case DAL_ULTRASONIC_ERROR:
            return dev->last_status;
        case DAL_ULTRASONIC_IDLE:
        default:
            return WINK_ERR_BUSY;   /* 无测量数据：当作未就绪 */
    }
}

/* @deprecated @blocking —— 见头文件契约；App 10ms tick 禁用，迁移至 request_measurement + get_cached_distance。
 * 所有平台共用同一份代码：统一使用 PAL 接口，无平台条件编译。
 * - WASM 仿真：PAL 内部委托 js_sim_* 物理量旁路
 * - ESP32 真机：PAL 内部用 RMT 或 GPIO busy-wait
 * 单位换算、超时判定与业务逻辑全平台同源（ADR-0003 决策2）。 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. 触发超声波（TRIG 时序） */
    pal_hal_ultrasonic_trigger(dev->config.trig_pin);

    /* 2. 测量 ECHO 脉宽（平台差异由 PAL 内部处理） */
    uint32_t pulse_us = 0;
    wink_status_t status = pal_hal_ultrasonic_measure_pulse_us(
        dev->config.echo_pin,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );
    if (wink_status_is_error(status)) {
        return status;  /* WINK_ERR_TIMEOUT 或其它硬件错误 */
    }

    /* 3. 单位换算：全平台同源代码（ADR-0003 决策2） */
    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *distance_cm = dev->last_distance;
    return WINK_OK;
}
