/**
 * @file pal_hal_ultrasonic.c
 * @brief ESP32 平台的超声波传感器 PAL 实现
 *
 * 使用 ESP-IDF RMT 外设进行高精度脉冲捕获。
 * 此文件仅在 ESP32 target 编译，WASM/host 构建完全不可见。
 *
 * 代码复用策略：
 * - 内部复用 pal_hal_esp32_rmt.c 中已实现的 pal_rmt_ultrasonic_* 函数
 * - 降级路径：pal_gpio_pulse_in（来自 pal_hal_esp32.c）
 * - 仅做接口适配，不重复实现算法逻辑
 */

#include "hal/pal_ultrasonic.h"
#include "hal/pal_hal_rmt.h"  /* 复用现有的 RMT 实现 */
#include "pal_hal.h"           /* pal_gpio_* 等 HAL 接口 */
#include "pal_osal.h"          /* pal_os_busy_wait_us 等 OSAL 接口 */

/* 全局状态：RMT 是否已初始化（单实例超声波） */
static bool s_rmt_initialized = false;

wink_status_t pal_hal_ultrasonic_init(uint16_t echo_pin) {
    if (s_rmt_initialized) {
        return WINK_OK;
    }
    /* 复用 pal_hal_esp32_rmt.c 的 RMT 硬件初始化 */
    wink_status_t status = pal_rmt_ultrasonic_init(echo_pin);
    if (!wink_status_is_error(status)) {
        s_rmt_initialized = true;
    }
    return status;
}

wink_status_t pal_hal_ultrasonic_trigger(uint16_t trigger_pin) {
    /* TRIG 时序：输出 10us 高电平脉冲
     * 使用标准 PAL GPIO 接口，与平台无关 */
    wink_status_t status = pal_gpio_write(trigger_pin, true);
    if (wink_status_is_error(status)) { return status; }
    pal_os_busy_wait_us(10);
    return pal_gpio_write(trigger_pin, false);
}

wink_status_t pal_hal_ultrasonic_measure_pulse_us(
    uint16_t echo_pin,
    uint32_t timeout_us,
    uint32_t *pulse_us
) {
    if (pulse_us == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 优先使用 RMT 硬件捕获（非阻塞，不消耗 CPU）
     * 如果 RMT 未初始化（或初始化失败），降级到 GPIO busy-wait */
    if (s_rmt_initialized) {
        return pal_rmt_ultrasonic_measure(timeout_us, pulse_us);
    } else {
        /* 降级路径：使用标准 PAL GPIO 脉冲测量接口 */
        return pal_gpio_pulse_in(echo_pin, true, timeout_us, pulse_us);
    }
}

void pal_hal_ultrasonic_deinit(void) {
    if (s_rmt_initialized) {
        pal_rmt_ultrasonic_deinit();
        s_rmt_initialized = false;
    }
}
