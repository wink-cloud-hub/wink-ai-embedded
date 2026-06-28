#include "dal_ultrasonic.h"
#include "pal_hal.h"
#include "pal_osal.h"

#include <string.h>   /* memcpy（ADR-0008 apply_override 反序列化） */
#ifdef SIMULATION
#include "wasm_bridge.h"   /* js_sim_* 旁路（sim 分支 request_measurement / read 引用） */
#endif
#if defined(ESP_PLATFORM)
#include "pal_hal_rmt.h"   /* ESP32 RMT 硬件脉冲捕获 */
#endif

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

    u->trig_pin = trig_pin;
    u->echo_pin = echo_pin;
    return WINK_OK;
}

wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, uint16_t trig_pin, uint16_t echo_pin) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (trig_pin == echo_pin) { return WINK_ERR_INVALID_ARG; }
    dev->trig_pin = trig_pin;
    dev->echo_pin = echo_pin;
    dev->last_distance = 0.0f;
    dev->state = DAL_ULTRASONIC_IDLE;
    dev->last_status = WINK_OK;
    dev->last_pulse_us = 0u;
#ifdef SIMULATION
    /* 仿真：跳过物理 GPIO 配置（旁路最低物理信号层，ADR-0003 决策2），仅置结构状态 */
    dev->initialized = true;
    return WINK_OK;
#else
    /* 真机：配置 GPIO 方向（Phase 3 status 透传；失败含 BUSY/EXHAUSTED）。 */
    wink_status_t status = pal_gpio_init(trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(status)) { return status; }
    status = pal_gpio_init(echo_pin, PAL_GPIO_INPUT);
    if (wink_status_is_error(status)) { return status; }

#if defined(ESP_PLATFORM)
    /* ESP32：初始化 RMT 硬件脉冲捕获（替代 busy-wait） */
    status = pal_rmt_ultrasonic_init(echo_pin);
    if (wink_status_is_error(status)) {
        /* RMT 失败：降级到 pal_gpio_pulse_in busy-wait */
    } else {
        dev->use_rmt = true;
    }
#endif

    dev->initialized = true;
    return WINK_OK;
#endif
}

wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. 触发（物理信号层；sim 旁路） */
#ifdef SIMULATION
    js_sim_trigger_ultrasonic(dev->trig_pin);
#else
    pal_gpio_write(dev->trig_pin, true);
    pal_delay_us(10);
    pal_gpio_write(dev->trig_pin, false);
#endif
    dev->state = DAL_ULTRASONIC_MEASURING;

    /* 2. 捕获 echo 脉宽 */
    uint32_t pulse_us = 0;
    wink_status_t cap;

#if defined(ESP_PLATFORM)
    /* ESP32：优先 RMT 硬件捕获（非阻塞，不消耗 CPU） */
    if (dev->use_rmt) {
        cap = pal_rmt_ultrasonic_measure(ULTRASONIC_TIMEOUT_US, &pulse_us);
    } else {
        /* 降级：pal_gpio_pulse_in busy-wait */
        cap = pal_gpio_pulse_in(dev->echo_pin, true, ULTRASONIC_TIMEOUT_US, &pulse_us);
    }
#else
    /* host / 其它平台：pal_gpio_pulse_in */
    cap = pal_gpio_pulse_in(dev->echo_pin, true, ULTRASONIC_TIMEOUT_US, &pulse_us);
#endif

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

#ifdef SIMULATION
/* --- 仿真模式：仅旁路底层物理量来源（trigger + echo 脉宽），
       换算与超时与真机同源 (ADR-0003 决策2 / c-code.md §2)。
       extern 签名抄 wasm_bridge.h（SSOT 闭合）。 --- */
#include "wasm_bridge.h"

/* @deprecated @blocking —— 见头文件契约；App 10ms tick 禁用，迁移至 request_measurement + get_cached_distance。 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 1. trigger 时序旁路（真机侧为 GPIO 10us 脉冲） */
    js_sim_trigger_ultrasonic(dev->trig_pin);

    /* 2. echo 脉宽测量旁路（真机侧为 while 循环测高电平） */
    uint32_t pulse_us = js_sim_measure_echo_pulse_us(dev->trig_pin);
    if (pulse_us >= ULTRASONIC_TIMEOUT_US) { return WINK_ERR_TIMEOUT; }

    /* 3. 换算：两端同源 */
    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *distance_cm = dev->last_distance;
    return WINK_OK;
}

#else
/* --- 真实芯片模式 --- */
/* @deprecated @blocking —— 真机最坏 ≈60ms busy-wait，破坏 10ms tick/WCET；禁从 App 调用。
 * 保留仅供过渡/单测；App 应使用 request_measurement + get_cached_distance（Phase 4）。 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    pal_gpio_write(dev->trig_pin, true);
    pal_delay_us(10);
    pal_gpio_write(dev->trig_pin, false);

    uint64_t wait_start = pal_get_us();
    while (!pal_gpio_read(dev->echo_pin)) {
        if (pal_get_us() - wait_start > ULTRASONIC_TIMEOUT_US) { return WINK_ERR_TIMEOUT; }
    }

    uint64_t echo_start = pal_get_us();
    while (pal_gpio_read(dev->echo_pin)) {
        if (pal_get_us() - echo_start > ULTRASONIC_TIMEOUT_US) { return WINK_ERR_TIMEOUT; }
    }
    uint32_t pulse_us = (uint32_t)(pal_get_us() - echo_start);

    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *distance_cm = dev->last_distance;
    return WINK_OK;
}
#endif
