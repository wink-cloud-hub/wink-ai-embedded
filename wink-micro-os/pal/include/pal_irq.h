/**
 * @file pal_irq.h
 * @brief PAL 统一中断控制器抽象层（v2.0 专家评审版）
 *
 * 契约保证：
 * 1. ✅ ISR 安全接口（可在中断上下文调用）：
 *    - pal_irq_save() / pal_irq_save_rtos_safe() / pal_irq_restore()
 *    - pal_irq_set_pending() / pal_irq_clear_pending()
 *    - pal_irq_synchronize()
 * 2. ⚠️ 非 ISR 安全接口（仅线程上下文调用）：
 *    - pal_irq_enable() / pal_irq_disable()
 *    - pal_irq_direct_connect()
 *    - pal_irq_shared_register()
 *    - pal_gpio_enable_interrupt() / pal_gpio_disable_interrupt()
 *    （内部使用 Flash 函数和动态内存，Cache 禁用时会 Panic）
 * 3. 优先级数值语义统一（所有平台一致）
 * 4. 中断锁可嵌套（save/restore 支持嵌套调用）
 * 5. pal_irq_save() 禁用所有可屏蔽中断，提供最强临界区保护
 * 6. 所有优先级均可安全调用 FreeRTOS FromISR API（REALTIME 除外）
 *
 * 架构设计决策：
 * - ADR-IRQ-001: 中断锁语义选择 - pal_irq_save() 提供全屏蔽最强语义
 * - ADR-IRQ-002: GPIO ISR 清标顺序 - 先清标再调用用户回调
 * - ADR-IRQ-003: 优先级预留安全边界 - HIGHEST 不映射到硬件最大
 * - ADR-IRQ-004: SMP 分发表自旋锁 - 所有读写均需加锁
 * - ADR-IRQ-005: 共享中断责任链 - 不提前终止，始终遍历所有 handler
 * - ADR-IRQ-006: 双等级中断锁语义 - save() 全屏蔽 / save_rtos_safe() 仅屏蔽到边界
 * - ADR-IRQ-007: SMP ISR 执行同步原语 - pal_irq_synchronize()
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
    PAL_IRQ_PRIO_LOWEST   = 0,  /**< 最低优先级，用于非关键外设 */
    PAL_IRQ_PRIO_LOW      = 1,  /**< 低优先级，用于一般通信 */
    PAL_IRQ_PRIO_NORMAL   = 2,  /**< 默认优先级 */
    PAL_IRQ_PRIO_HIGH     = 3,  /**< 高优先级，用于时间敏感外设 */
    PAL_IRQ_PRIO_HIGHEST  = 4,  /**< 最高 RTOS 安全优先级 */
    PAL_IRQ_PRIO_REALTIME = 5,  /**< ⚠️ 非 RTOS 安全！极端硬实时场景专用
                                         严禁调用任何 RTOS API（包括 FromISR 系列）
                                         仅用于电机换向、激光同步等零延迟需求 */
    PAL_IRQ_PRIO_COUNT          /* 优先级数量，用于边界检查 */
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
 * @brief 硬件直连中断（Direct-Connect）处理程序原型
 * @note 因硬件限制，直连中断不能传递任何上下文参数，函数签名必须为 void(*)(void)
 */
typedef void (*pal_direct_isr_t)(void);

/**
 * @brief 共享中断处理程序原型（责任链模式，v2.0 语义修正）
 * @return true = 认领了该中断（仅用于杂散中断统计）；false = 不是我的中断
 *
 * ⚠️ 关键语义修正（v2.0 / ADR-IRQ-005）：
 * 返回值仅用于统计，**不控制遍历流程**。无论返回 true 或 false，
 * 链上所有 handler 都会被调用。这避免了共享中断同时触发时，
 * 先执行的 handler 返回 true 导致后续外设中断未被处理的性能问题。
 *
 * 设计参考：Linux 内核 Shared IRQ 处理机制（已在工业界验证 30 年）。
 *
 * 用于多个外设共享同一硬件中断向量的场景（如 STM32 USB OTG + ETH 共享）。
 * 每个 handler 必须先读取自家外设的状态寄存器，确认是自己的中断后再处理。
 */
typedef bool (*pal_irq_shared_handler_t)(void *arg);

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
 * @def PAL_DIRECT_ISR
 * @brief 跨平台 硬件直连型 ISR 属性注解
 *
 * 用法：static PAL_DIRECT_ISR void motor_direct_isr(void) { ... }
 *
 * 各平台展开为对应属性：
 * - ESP32: IRAM_ATTR (确保直连处理程序驻留 RAM)
 * - STM32: 空 (常规 C 函数作为硬件中断表项)
 * - WASM/Host: 空
 */
#define PAL_DIRECT_ISR PAL_ISR

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
 * @brief 注册并启用硬件直连中断（零软件分发延迟）
 *
 * @param irq_num 逻辑中断号
 * @param handler 直连中断处理函数（必须使用 PAL_DIRECT_ISR 注解，不能接收参数）
 * @return WINK_OK 成功，WINK_ERR_INVALID_ARG 参数非法，WINK_ERR_BUSY 中断已被占用
 *
 * @note 契约保证：直连中断在真机上完全绕过 PAL 软件分发逻辑，由硬件矢量控制器直接跳转，
 *       但不可传递参数，且必须确保不调用任何可能导致线程阻塞/调度的 RTOS 阻塞 API。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_direct_connect(uint32_t irq_num, pal_direct_isr_t handler);

/**
 * @brief 注册共享中断处理程序（责任链模式，v2.0 语义）
 *
 * 用于多个外设共享同一硬件中断向量的场景。当中断触发时，按注册顺序
 * 调用每个 handler，**不提前终止**（v2.0 修正语义）。返回值仅用于统计。
 *
 * @param irq_num 逻辑中断号
 * @param prio 中断优先级（首次注册时生效，后续注册忽略）
 * @param handler 共享中断处理函数，返回 true 表示认领此中断（仅用于统计）
 * @param arg 传递给 handler 的上下文参数
 *
 * @return WINK_OK 成功，WINK_ERR_INVALID_ARG 参数非法，WINK_ERR_NO_MEM 链已满
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_shared_register(uint32_t irq_num,
                                       pal_irq_prio_t prio,
                                       pal_irq_shared_handler_t handler,
                                       void *arg);

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

/**
 * @brief 等待所有正在执行的 ISR 完成（SMP 安全同步原语，v2.0 新增）
 *
 * ⚠️ SMP 关键同步原语（ADR-IRQ-007）：
 * 在双核/多核系统中，pal_irq_disable() 返回后，另一个 core
 * 可能仍在执行该中断的 ISR。此时释放 ISR 使用的资源会导致
 * 释放后使用（UAF）崩溃。
 *
 * 典型用法（必须严格遵守此顺序）：
 *   pal_irq_disable(irq_num);
 *   pal_irq_synchronize(irq_num);  // ✅ 等待所有 core 退出 ISR
 *   free(irq_resource);            // 现在可以安全释放
 *
 * 设计参考：Linux 内核 synchronize_irq()。
 *
 * @param irq_num 逻辑中断号（若为 ~0U 则等待所有中断）
 */
void pal_irq_synchronize(uint32_t irq_num);

/* ─────────────────────────────────────────────────────────
 * 全局中断锁（临界区保护，v2.0 双等级语义）
 * ───────────────────────────────────────────────────────── */

/**
 * @brief 禁用所有可屏蔽中断，返回先前的中断状态掩码（最强语义，ADR-IRQ-006）
 * @return 先前的中断状态，用于传给 pal_irq_restore()
 *
 * 语义保证：返回后，**所有**可屏蔽硬件中断均被禁用（包括最高优先级）。
 * 此函数支持嵌套调用，必须与 pal_irq_restore() 配对使用。
 *
 * ⚠️ 关键约束（必须严格遵守）：
 * 受此锁保护的临界区代码执行时间 **必须 < 1µs**。
 * 长时间屏蔽所有中断可能破坏 Wi-Fi 基带时序或触发硬件看门狗复位。
 *
 * 平台实现细节：
 * - ESP32: XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS) - 最高屏蔽级别
 * - STM32: __disable_irq() - 设置 PRIMASK
 * - WASM: 增加锁计数，持有期间不分发 pending 中断
 * - Host: 增加锁计数，持有期间仅记录 pending，不实际调用 ISR
 *
 * 用法示例：
 *       uint32_t mask1 = pal_irq_save();
 *       uint32_t mask2 = pal_irq_save();  // 合法，支持嵌套
 *       // 临界区代码（< 1µs）
 *       pal_irq_restore(mask2);
 *       pal_irq_restore(mask1);
 */
uint32_t pal_irq_save(void);

/**
 * @brief 仅禁用 RTOS 安全级别的中断（v2.0 新增，推荐默认使用）
 * @return 先前的中断状态，用于传给 pal_irq_restore()
 *
 * 语义保证（ADR-IRQ-006）：仅屏蔽优先级 ≤ configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断。
 * 更高优先级的中断（如 REALTIME 级别、Wi-Fi 基带中断）仍可触发。
 *
 * ✅ 推荐使用场景（绝大多数情况下应使用此函数而非 pal_irq_save()）：
 * - 临界区可能超过 1µs
 * - 需要保护与 RTOS 交互的数据结构
 * - 不希望影响底层硬件协议时序
 *
 * 平台实现细节：
 * - ESP32: XTOS_SET_INTLEVEL(configMAX_SYSCALL_INTERRUPT_PRIORITY)
 * - STM32: __set_BASEPRI(configMAX_SYSCALL_INTERRUPT_PRIORITY)
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

/**
 * @brief 最强语义的 RAII 临界区（慎用，仅用于 <1µs 极端原子场景）
 *
 * 屏蔽所有可屏蔽中断，包括 REALTIME 级别。
 * 仅用于对原子性要求极高且执行时间 < 1µs 的场景。
 */
#define PAL_CRITICAL_SECTION_STRICT(code_block)                   \
    do {                                                           \
        uint32_t __irq_mask = pal_irq_save();                      \
        { code_block }                                             \
        pal_irq_restore(__irq_mask);                               \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PAL_IRQ_H */
