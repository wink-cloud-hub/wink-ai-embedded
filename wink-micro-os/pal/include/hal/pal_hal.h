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
    /* Bidirectional: driver + input buffer both enabled. Needed when a pin
     * must be written (software drive) AND read/sensed (RMT input / GPIO ISR
     * on own edge). Maps to GPIO_MODE_INPUT_OUTPUT on ESP32; host/wasm stubs
     * treat it identically to other modes (read/write always allowed). */
    PAL_GPIO_INPUT_OUTPUT        = 5,
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

/**
 * @brief 查询 PWM 通道映射到的物理 GPIO。
 *
 * P1-P4 (2026-07-04): 取代过去公开 `extern const wink_pin_t pal_pwm_pin_map[]` 的
 * 做法。数组现在各 target 私有（static const 或 weak），只能通过 getter 访问。
 * 好处：
 *  1. 消费者不再看到"数组"，禁用了直接读越界索引导致的 UB；
 *  2. target-side 可选择走非数组的路由（例如 wasm 走 JS 桥）而不改 API；
 *  3. 上层代码统一走 wink_status_t 错误传播，符合 ADR-0001。
 *
 * @param channel 通道号 [0, PAL_PWM_CHANNELS)。
 * @param out_pin 输出：该通道当前路由到的 GPIO 引脚（成功时写入）。
 * @return
 *   WINK_OK              查询成功；*out_pin 有效。
 *   WINK_ERR_INVALID_ARG channel >= PAL_PWM_CHANNELS 或 out_pin == NULL。
 *   WINK_ERR_UNSUPPORTED target 无 PWM 引脚路由（如 wasm 纯虚拟外设）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin);

/**
 * @brief 查询 I2C 端口映射到的 SDA/SCL 物理 GPIO。
 *
 * P1-P4 (2026-07-04): 取代过去公开 `extern const wink_pin_t pal_i2c_pin_map[][2]` 的
 * 做法，理由同 pal_pwm_channel_pin。
 *
 * ⚠️ 顺序约定：out_sda 对应 [port][0]，out_scl 对应 [port][1]
 *   （沿用 esp32 target 的历史布局；board_config.c 强定义时按此顺序）。
 *
 * @param port    I2C 端口号 [0, PAL_I2C_PORTS)。
 * @param out_sda 输出：SDA 引脚（成功时写入）。可传 NULL 表示不关心。
 * @param out_scl 输出：SCL 引脚（成功时写入）。可传 NULL 表示不关心。
 * @return
 *   WINK_OK              查询成功。
 *   WINK_ERR_INVALID_ARG port >= PAL_I2C_PORTS 或 out_sda/out_scl 皆为 NULL。
 *   WINK_ERR_UNSUPPORTED target 无 I2C 引脚路由（如 wasm 纯虚拟外设）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl);

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
 *   WINK_ERR_UNSUPPORTED 平台不支持 GPIO ISR
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

/**
 * @brief 等待指定 GPIO pin 的 in-flight ISR 完成后返回（SMP 安全同步原语）
 *
 * ⚠️ SMP 关键同步原语（P1-P5-10，配合 pal_gpio_disable_interrupt 使用）：
 * SMP 系统中 `pal_gpio_disable_interrupt(pin)` 返回后，另一个 core 可能仍在
 * 执行该 pin 的 ISR。若立刻释放 ISR 使用的资源（例如 arg 指向的堆结构），
 * 会导致 UAF (Use-After-Free)。此接口忙等待该 pin 的 in-flight ISR 计数
 * 归 0（含超时保护），返回后即可安全释放资源。
 *
 * 典型用法（必须严格遵守此顺序）：
 *   pal_gpio_disable_interrupt(pin);
 *   pal_gpio_synchronize_interrupt(pin);  // ✅ 等待所有 core 退出 ISR
 *   free(isr_arg);                        // 现在可以安全释放
 *
 * @note 单核 target（host / wasm 仿真）为 no-op；ESP32 上映射到
 *       target-private 的 GPIO in-flight 计数忙等待（带超时）。
 * @note 普通 App/DAL 代码一般不需要——静态注册 + 运行到停止的模式无需此调用；
 *       仅当需要动态注销并释放 ISR 使用的资源时才使用。
 *
 * @param pin 引脚号（越界返回 WINK_ERR_INVALID_ARG）
 * @return WINK_OK 成功（含目标 target 上等价于 no-op 的情形）；
 *         WINK_ERR_INVALID_ARG pin 非法
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin);

/**
 * @brief 启用硬件信号自环/回环测试接口。
 *
 * 用于测试环境（特别是裸开发板）。它在底层实现将输出引脚 pin_out 产生的信号
 * （如 GPIO 软件翻转电平或 PWM 信号）回环到输入引脚 pin_in 上（如 GPIO 输入或 RMT 输入）。
 *
 * @note 各 target 平台下的工作机制：
 *   - ESP32:  利用芯片内部的 GPIO Matrix (信号交换矩阵) 将输出信号路由到输入，无需物理导线。
 *   - Host:   在软件层面建立虚拟连接，使得对 pin_out 的写入（如 pal_gpio_write 或 pal_pwm_set_duty）
 *             可以被 pin_in 正常读出（如 pal_gpio_read 或 pal_gpio_pulse_in 仿真数据获取）。
 *   - Wasm:   与 Host 类似，进行仿真数据回环。
 *   - STM32/其他: 若硬件不支持内部自环，直接返回 WINK_ERR_UNSUPPORTED，此时测试必须通过物理接线短接。
 *
 * @param pin_out 输出信号引脚
 * @param pin_in  输入信号引脚
 * @return
 *   WINK_OK              回环连接成功
 *   WINK_ERR_INVALID_ARG 引脚越界或非法
 *   WINK_ERR_UNSUPPORTED 当前硬件平台不支持内部信号矩阵自环
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in);

/**
 * @brief 关闭硬件信号自环/回关测试接口。
 *
 * 清除之前通过 pal_test_enable_hardware_loopback 建立的内部信号回环。
 *
 * @param pin_out 输出信号引脚
 * @param pin_in  输入信号引脚
 * @return
 *   WINK_OK              清理成功
 *   WINK_ERR_INVALID_ARG 引脚越界或非法
 */
wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief GPIO 脉冲宽度测量（阻塞 busy-wait / RMT 等待）。
 * @note Blocking: Yes（最坏 timeout_us 微秒忙等或 FreeRTOS 信号量等待）。
 *       Not available under WINK_STRICT_NONBLOCKING (ADR-0017).
 *       非阻塞替代路径：pal_rmt_pulse_capture_init + 异步 poll。
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us,
                                 uint32_t *pulse_us);

/**
 * @brief I2C 总线同步传输 (write + optional read)。
 * @note Blocking: Yes（总线 ACK/NACK 等待 + 传输时间；ESP32 走 driver 事件循环，
 *       host/wasm 通常是即时返回，但语义仍为"直到完成"）。
 *       Not available under WINK_STRICT_NONBLOCKING (ADR-0017).
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                               const uint8_t *write_buf, uint32_t write_len,
                               uint8_t *read_buf, uint32_t read_len);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif
