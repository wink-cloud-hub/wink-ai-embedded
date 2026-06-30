#ifndef PAL_HAL_H
#define PAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_irq.h"  /* 统一中断抽象：pal_irq_prio_t */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS 8
#endif

#ifndef PAL_I2C_PORTS
#define PAL_I2C_PORTS 2
#endif

typedef enum {
    PAL_GPIO_INPUT               = 0,
    PAL_GPIO_INPUT_PULLUP        = 1,
    PAL_GPIO_INPUT_PULLDOWN      = 2,
    PAL_GPIO_OUTPUT_PUSH_PULL    = 3,
    PAL_GPIO_OUTPUT_OPEN_DRAIN   = 4,
} pal_gpio_mode_t;

typedef enum {
    PAL_GPIO_INTR_DISABLE         = 0,
    PAL_GPIO_INTR_RISING_EDGE     = 1,
    PAL_GPIO_INTR_FALLING_EDGE    = 2,
    PAL_GPIO_INTR_ANY_EDGE        = 3,
    PAL_GPIO_INTR_LOW_LEVEL       = 4,  /* 新增：电平触发 */
    PAL_GPIO_INTR_HIGH_LEVEL      = 5,  /* 新增：电平触发 */
} pal_gpio_intr_t;

/**
 * @brief 统一引脚编号类型
 * @note 使用 int16_t 确保 GPIO_NUM_NC (-1) 不被截断为 65535
 *       兼容 ESP-IDF gpio_num_t 的符号语义
 */
typedef int16_t wink_pin_t;

extern const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS];

/* I2C 物理引脚路由：[port][0] = SDA, [port][1] = SCL */
extern const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);

void pal_pwm_deinit(uint8_t channel);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode);

void pal_gpio_write(wink_pin_t pin, bool level);

bool pal_gpio_read(wink_pin_t pin);

typedef void (*pal_gpio_isr_t)(void *arg);

/**
 * @brief 启用 GPIO 引脚中断（扩展版，支持指定优先级）
 *
 * @param pin 引脚号
 * @param intr_type 中断触发类型
 * @param prio 中断优先级（统一抽象）
 * @param callback ISR 回调（遵守 ISR 契约）
 * @param arg 回调参数
 *
 * @note ✅ 此接口在所有平台都存在，不再需要 #ifdef 包裹
 * @note 不支持的平台（如 Host）返回 WINK_ERR_UNSUPPORTED，由调用方处理
 *
 * ⚠️ 实现契约（GPIO ISR Wrapper 必须遵守）：
 * 1. 第一时间清除中断标志 —— 防止重入和中断风暴
 * 2. SMP 安全：持有自旋锁，原子性读取回调指针和参数
 * 3. 最后调用用户 ISR
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg);

/* 保留原接口用于向后兼容，内部默认 NORMAL 优先级 */
static inline wink_status_t
pal_gpio_enable_interrupt(wink_pin_t pin, pal_gpio_intr_t intr_type,
                           pal_gpio_isr_t callback, void *arg)
{
    return pal_gpio_enable_interrupt_ex(pin, intr_type,
                                         PAL_IRQ_PRIO_NORMAL,
                                         callback, arg);
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us,
                                 uint32_t *pulse_us);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                               const uint8_t *write_buf, uint32_t write_len,
                               uint8_t *read_buf, uint32_t read_len);

#ifdef __cplusplus
}
#endif

#endif
