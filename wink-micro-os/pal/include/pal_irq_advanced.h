/**
 * @file pal_irq_advanced.h
 * @brief PAL 高级系统级中断 API
 *
 * 包含受限的系统级 API（如全局中断锁和 SMP 同步原语）。
 * 此头文件通过 WINK_ALLOW_ADVANCED_IRQ_APIS 宏进行物理隔离，防止普通业务代码
 * 意外调用导致系统时序崩溃。
 */

#ifndef PAL_IRQ_ADVANCED_H
#define PAL_IRQ_ADVANCED_H

#ifndef WINK_ALLOW_ADVANCED_IRQ_APIS
#error "Advanced IRQ APIs are restricted. Define WINK_ALLOW_ADVANCED_IRQ_APIS to include this header."
#endif

#include "pal_irq.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 等待所有正在执行的 ISR 完成（SMP 安全同步原语）
 *
 * ⚠️ SMP 关键同步原语（ADR-0018 高级 API）：
 * 在双核/多核系统中，pal_irq_disable() 返回后，另一个 core
 * 可能仍在执行该中断的 ISR。此时释放 ISR 使用的资源会导致
 * 释放后使用（UAF）崩溃。
 *
 * 典型用法（必须严格遵守此顺序）：
 *   pal_irq_disable(irq_num);
 *   pal_irq_synchronize(irq_num);  // ✅ 等待所有 core 退出 ISR
 *   free(irq_resource);            // 现在可以安全释放
 *
 * @note 普通 App/DAL 代码通常不需要此接口——本项目倾向"启动 init → 运行到停止"的
 *       静态注册模型（见 ADR-0004、ADR-0008），几乎不动态注册/注销 ISR。
 *
 * @param irq_num 逻辑中断号（若为 ~0U 则等待所有中断）
 */
void pal_irq_synchronize(uint32_t irq_num);

/**
 * @brief 禁用所有可屏蔽中断，返回先前的中断状态掩码（全屏蔽最强语义）
 * @return 先前的中断状态，用于传给 pal_irq_restore()
 *
 * 语义保证：返回后，**所有**可屏蔽硬件中断均被禁用（包括最高硬件优先级，
 * 例如 ESP32 上高于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的 Wi-Fi 基带中断）。
 * 此函数支持嵌套调用，必须与 pal_irq_restore() 配对使用。
 *
 * ⚠️ 关键约束（必须严格遵守）：
 * 受此锁保护的临界区代码执行时间 **必须 < 1µs**。
 * 长时间屏蔽所有中断可能破坏 Wi-Fi 基带时序或触发硬件看门狗复位。
 *
 * ⚠️ AI Codegen 使用规则：默认**不应**生成此调用。普通业务临界区请用
 * `PAL_CRITICAL_SECTION`（基于 `pal_irq_save_rtos_safe`）。仅在系统级驱动
 * （Wi-Fi/BT/timer sync 等）明确需要屏蔽 syscall 边界以上中断时才使用。
 *
 * 平台实现细节：
 * - ESP32: XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS) - 最高屏蔽级别
 * - STM32: __disable_irq() - 设置 PRIMASK
 * - WASM: 增加锁计数，持有期间不分发 pending 中断
 * - Host: 增加锁计数，持有期间仅记录 pending，不实际调用 ISR
 */
uint32_t pal_irq_save(void);

/**
 * @brief 最强语义的 RAII 临界区（慎用，仅用于 <1µs 极端原子场景）
 *
 * 屏蔽所有可屏蔽中断（包括 Wi-Fi 基带、看门狗等高硬件优先级源）。
 * 仅用于对原子性要求极高且执行时间 < 1µs 的场景。
 *
 * @note AI Codegen 默认不生成——业务临界区请用 `PAL_CRITICAL_SECTION`。
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

#endif /* PAL_IRQ_ADVANCED_H */
