/**
 * @file pal_hal_rmt.h
 * @brief PAL RMT 硬件脉冲捕获扩展（ESP32 专用）。
 *
 * 这是 PAL 层的扩展 API，仅在 ESP32 target 下可用。
 * 使用 RMT (Remote Control) 外设实现非阻塞超声波脉冲测量，
 * 替代 pal_gpio_pulse_in 的 busy-wait 实现，不阻塞 tick。
 *
 * TODO Phase 4: 将此接口标准化为 PAL 通用非阻塞脉冲捕获 API。
 */

#ifndef PAL_HAL_RMT_H
#define PAL_HAL_RMT_H

#include <stdint.h>
#include "wink_status.h"

/**
 * @brief 初始化 RMT 超声波脉冲捕获通道
 * @param echo_pin ECHO 信号输入引脚
 * @return WINK_OK 成功；其它错误码失败
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_ultrasonic_init(uint16_t echo_pin);

/**
 * @brief 执行一次超声波脉宽测量（非阻塞，由 RMT 硬件完成）
 * @param timeout_us 超时时间（微秒），通常 HC-SR04 取 30000us
 * @param pulse_us 输出参数，返回测量到的脉冲宽度（微秒）
 * @return WINK_OK 测量成功；WINK_ERR_TIMEOUT 超时；其它错误码失败
 *
 * 使用方法：
 *   pal_gpio_write(trig_pin, true);
 *   pal_delay_us(10);
 *   pal_gpio_write(trig_pin, false);
 *   uint32_t pulse_us;
 *   if (pal_rmt_ultrasonic_measure(30000, &pulse_us) == WINK_OK) {
 *       float distance_mm = pulse_us * 0.343f / 2.0f;  // 声速 343m/s
 *   }
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_ultrasonic_measure(uint32_t timeout_us, uint32_t *pulse_us);

/**
 * @brief 反初始化 RMT 超声波捕获通道
 */
void pal_rmt_ultrasonic_deinit(void);

#endif /* PAL_HAL_RMT_H */
