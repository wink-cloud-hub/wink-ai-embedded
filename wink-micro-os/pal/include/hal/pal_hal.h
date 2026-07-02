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

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_write(wink_pin_t pin, bool level);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);

typedef void (*pal_gpio_isr_t)(void *arg);

/**
 * @brief 启用 GPIO 引脚中断（扩展版，首次注册时锁定优先级）
 *
 * @param pin 引脚号
 * @param intr_type 中断触发类型
 * @param prio 中断优先级 —— ⚠️ v2.2（2026-07-01，ADR-0012 落地）：**首次锁定语义**
 * @param callback ISR 回调（遵守 ISR 契约）
 * @param arg 回调参数
 *
 * @note ✅ 此接口在所有平台都存在，不再需要 #ifdef 包裹
 * @note 不支持的平台（编译时存根）返回 WINK_ERR_UNSUPPORTED，由调用方处理
 *
 * ⚠️ v2.2 契约（2026-07-01，取代 v2.1 的 "静默忽略 prio"）：
 * GPIO 中断在各 target 上共享一个 dispatch service；因此 prio 的语义为
 * **进程生命周期内首次注册时锁定**：
 *
 *   · 首次注册：底层 install service，硬件优先级绑定到映射后的 flag
 *     - ESP32:  ESP_INTR_FLAG_LEVELn | ESP_INTR_FLAG_IRAM
 *     - host/wasm: 单线程模型无实际调度效果，仅记录状态
 *   · 后续注册：
 *       - prio 与首次一致 → 正常注册 pin handler
 *       - prio 与首次不一致 → 返回 WINK_ERR_INVALID_ARG（本次拒接）
 *
 * ⚠️ 一旦锁定，本接口不提供解锁 API —— 即使 disable 所有 pin 后仍保持锁定。
 * 拒绝"disable(last_pin) → uninstall"的方案，因为：
 *   (1) 存在 TOCTOU race（disable→uninstall→enable(new prio) 之间存在窗口）；
 *   (2) SMP 下 ISR 可能仍在另一核执行（见 ADR-IRQ-007），uninstall 会 UAF；
 *   (3) 心智模型复杂化，且 ESP-IDF 官方语义就是"进程级 one-shot 全局服务"。
 * 若未来真需要 per-pin 独立优先级（按钮抢占传感器等场景），会新增
 * `pal_gpio_enable_interrupt_dedicated()` 独立中断源接口。
 *
 * ⚠️ REALTIME 全 target 拒接：所有 target 上 prio == PAL_IRQ_PRIO_REALTIME 均返回
 * WINK_ERR_UNSUPPORTED（v2.2 起 host/wasm 也拒接，与 ESP32 对齐；host/wasm 可通过
 * 编译期宏 WINK_HOST_ALLOW_REALTIME_FOR_TESTING opt-in 放行，仅供静态校验测试）。
 *
 * ⚠️ 错误码选择说明：
 * - INVALID_ARG：语义准确（参数与当前系统状态不合法），符合"本次调用参数不对"的直觉。
 * - BUSY：暗示可重试，误导性（重试永远不会成功）。
 * - UNSUPPORTED：暗示整个能力缺失，不准确（能力存在，只是当前 prio 已被别的值占用）。
 *
 * ⚠️ 实现契约（GPIO ISR Wrapper 必须遵守）：
 * 1. 第一时间清除中断标志 —— 防止重入和中断风暴
 * 2. SMP 安全：持有自旋锁，原子性读取回调指针和参数
 * 3. 最后调用用户 ISR
 *
 * @return
 *   WINK_OK              首次或与首次锁定值一致的后续注册成功
 *   WINK_ERR_INVALID_ARG pin 越界 / callback NULL / prio 越界 /
 *                         prio 与首次锁定值不一致（本次拒接）
 *   WINK_ERR_UNSUPPORTED prio == PAL_IRQ_PRIO_REALTIME（或平台不支持 GPIO ISR）
 *   WINK_ERR_HARDWARE    底层 install / register 失败（ESP-IDF 返回错）
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
