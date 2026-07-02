/**
 * @file pal_irq.h
 * @brief PAL 统一中断控制器抽象层（Track F 升级版）
 *
 * 契约保证：
 * 1. ✅ ISR 安全接口（可在中断上下文调用）：
 *    - pal_irq_save_rtos_safe() / pal_irq_restore()
 *    - pal_irq_set_pending() / pal_irq_clear_pending()
 * 2. ⚠️ 非 ISR 安全接口（仅线程上下文调用）：
 *    - pal_irq_enable() / pal_irq_disable()
 *    - pal_gpio_enable_interrupt() / pal_gpio_disable_interrupt()
 *    （内部使用 Flash 函数，Cache 禁用时会 Panic）
 * 3. 优先级数值语义统一（3级：LOW/NORMAL/HIGH）
 * 4. 中断锁可嵌套（save/restore 支持嵌套调用）
 * 5. 裸机环境下，pal_irq_save_rtos_safe() 安全降级为关全局中断锁
 * 6. 本文件屏蔽了易滥用的全量关中断锁和同步机制（见 pal_irq_advanced.h）
 *
 * 架构设计决策：
 * - ADR-0018: PAL IRQ API 收窄与安全隔离
 */

#ifndef PAL_IRQ_H
#define PAL_IRQ_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────
 * 中断优先级统一抽象（v2.0 6 级含 REALTIME）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 统一中断优先级枚举
 *
 * 语义保证：所有平台下，HIGHEST 优先级的中断会抢占 LOWEST 优先级的中断。
 * 各平台内部映射到硬件优先级数值（注意：不同芯片优先级数值方向可能相反）。
 *
 * ⚠️ FreeRTOS 安全约束（v2.0）：
 * LOWEST ~ HIGHEST 级别的中断均可安全调用 xQueueSendFromISR 等 FreeRTOS API。
 * REALTIME 级别例外 —— 该级别高于 RTOS 临界区保护边界，严禁调用任何 RTOS API。
 * 仅用于电机换向、激光同步等零延迟、零 OS 依赖的硬实时场景。
 */
typedef enum {
    PAL_IRQ_PRIO_LOW      = 1,  /**< 低优先级，用于一般通信 */
    PAL_IRQ_PRIO_NORMAL   = 2,  /**< 默认优先级 */
    PAL_IRQ_PRIO_HIGH     = 3,  /**< 高优先级，用于时间敏感外设 */
    PAL_IRQ_PRIO_COUNT
} pal_irq_prio_t;

/* ─────────────────────────────────────────────────────────
 * ISR 类型与属性注解
 * ───────────────────────────────────────────────────────── */

/**
 * @brief ISR 函数原型（完全平台无关）
 * @param arg 注册时传入的上下文指针
 *
 * @contract ISR 必须遵守：
 * 1. 执行时间 < 10us (或 < 最高优先级 tick 周期的 10%)
 * 2. 不调用任何可能阻塞的函数
 * 3. 栈使用 < 128 字节（含嵌套调用）
 * 4. 不触发任何可能导致 Flash 访问的操作
 * 5. 仅调用后缀为 FromISR 的 RTOS API（REALTIME 级除外，不可调用）
 */
typedef void (*pal_isr_t)(void *arg);

/**
 * @def PAL_ISR
 * @brief 跨平台 软件分发型 ISR 属性注解
 *
 * 用法：static PAL_ISR void my_isr(void *arg) { ... }
 *
 * 各平台展开为对应属性：
 * - ESP32: IRAM_ATTR (确保分发回调代码驻留 RAM，不因 Flash Cache Miss 延迟)
 * - STM32/ARM: 空 (ARM Cortex-M 硬件自动压栈，常规 C 函数即可作为 ISR)
 * - WASM/Host: 空 (普通函数)
 */
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define PAL_ISR  IRAM_ATTR
#else
#define PAL_ISR  /* 无特殊属性 */
#endif



/**
 * @def PAL_DEFINE_ISR
 * @brief 类型安全的 ISR 定义宏
 *
 * 自动生成类型转换包装，避免 ISR 中手动进行 (struct xxx *)arg 强制转换，
 * 消除类型转换带来的潜在 Bug。
 *
 * 用法：
 *   PAL_DEFINE_ISR(my_button_isr, struct button_state, state) {
 *       state->press_count++;  // ✅ 类型安全，不需要强制转换
 *       state->last_press_time = pal_get_tick_count();
 *   }
 *
 *   pal_gpio_enable_interrupt(pin, edge, my_button_isr, &my_button_state);
 */
#define PAL_DEFINE_ISR(name, arg_type, arg_name)  \
    static PAL_ISR void name##_typed(arg_type *arg_name);  \
    static PAL_ISR void name(void *arg) {  \
        name##_typed((arg_type *)arg);  \
    }  \
    static PAL_ISR void name##_typed(arg_type *arg_name)

/* ─────────────────────────────────────────────────────────
 * 中断控制器核心接口
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 启用并注册软件分发中断（支持传递上下文参数）
 *
 * @param irq_num 逻辑中断号（由 device tree 定义）
 * @param prio 中断优先级
 * @param handler ISR 处理函数（必须遵守 ISR 契约，使用 PAL_ISR 修饰）
 * @param arg 传递给 ISR 的上下文参数
 *
 * @return WINK_OK 成功，WINK_ERR_INVALID_ARG 参数非法，WINK_ERR_BUSY 中断已被占用
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg);



/**
 * @brief 禁用并注销中断
 *
 * @param irq_num 逻辑中断号
 * @return WINK_OK 成功，WINK_ERR_INVALID_ARG 参数非法
 *
 * @note SMP 安全注意：调用 pal_irq_disable() 后，另一个核心可能仍在执行该 ISR。
 *       若需要释放 ISR 使用的资源，**必须**在其后调用 pal_irq_synchronize() 等待。
 *       典型用法：pal_irq_disable() → pal_irq_synchronize() → free(irq_resource)
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_disable(uint32_t irq_num);

/**
 * @brief 设置中断 pending 状态（软件触发中断）
 * @param irq_num 逻辑中断号
 */
void pal_irq_set_pending(uint32_t irq_num);

/**
 * @brief 清除中断 pending 状态
 * @param irq_num 逻辑中断号
 */
void pal_irq_clear_pending(uint32_t irq_num);



/* ─────────────────────────────────────────────────────────
 * 全局中断锁（临界区保护，ADR-0018 修订）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 禁用 RTOS 安全级别的中断
 * @return 先前的中断状态，用于传给 pal_irq_restore()
 *
 * 语义保证：屏蔽底层应用外设优先级的中断。
 * 
 * 裸机（Baremetal）降级策略：
 * 如果底层没有 RTOS 或不可控制中断嵌套（如部分简单 Cortex-M 裸机），
 * 则此接口自动安全降级为全局关中断（如 `__disable_irq()`）。
 * 
 * 平台实现细节：
 * - ESP32: XTOS_SET_INTLEVEL(configMAX_SYSCALL_INTERRUPT_PRIORITY)
 * - Cortex-M (RTOS): __set_BASEPRI(configMAX_SYSCALL_INTERRUPT_PRIORITY)
 * - Cortex-M (Baremetal): __disable_irq() 降级
 */
uint32_t pal_irq_save_rtos_safe(void);

/**
 * @brief 恢复中断状态
 * @param mask 由 pal_irq_save() 或 pal_irq_save_rtos_safe() 返回的掩码
 *
 * 注意：必须严格按照 save 的逆序调用 restore。
 */
void pal_irq_restore(uint32_t mask);

/**
 * @brief RAII 风格的临界区包裹（C 语言模拟，推荐默认）
 *
 * 自动处理配对的 save/restore，避免遗漏导致的死锁。
 * 使用 pal_irq_save_rtos_safe()（RTOS 安全语义，不影响高优先级硬件中断）。
 *
 * 用法：
 *   PAL_CRITICAL_SECTION({
 *       // 受保护的代码，无中断抢占
 *       shared_var++;
 *   });
 */
#define PAL_CRITICAL_SECTION(code_block)                          \
    do {                                                           \
        uint32_t __irq_mask = pal_irq_save_rtos_safe();            \
        { code_block }                                             \
        pal_irq_restore(__irq_mask);                               \
    } while(0)



#ifdef __cplusplus
}
#endif

#endif /* PAL_IRQ_H */
