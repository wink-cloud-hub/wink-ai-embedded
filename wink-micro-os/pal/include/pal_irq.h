// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_irq.h
 * @brief PAL Unified Interrupt Controller Abstraction Layer (Track F).
 *
 * Contract guarantees:
 * 1. ISR-safe interfaces (callable from ISR context):
 *    - pal_irq_save_rtos_safe() / pal_irq_restore()
 *    - pal_irq_set_pending() / pal_irq_clear_pending()
 * 2. Non-ISR safe interfaces (callable from thread context only):
 *    - pal_irq_enable() / pal_irq_disable()
 *    - pal_gpio_enable_interrupt() / pal_gpio_disable_interrupt()
 *      (Uses internal Flash operations; invoking with Cache disabled will trigger Panic)
 * 3. Unified 3-tier priority levels (LOW/NORMAL/HIGH)
 * 4. Nested interrupt lock support (save/restore support recursive calls)
 * 5. In baremetal environments, pal_irq_save_rtos_safe() safely degrades to global interrupt lock
 * 6. High-risk global disable APIs are isolated in pal_irq_advanced.h
 *
 * Architectural decisions:
 * - ADR-0018: PAL IRQ API Narrowing and Security Isolation
 */

#ifndef PAL_IRQ_H
#define PAL_IRQ_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Unified Interrupt Priority Abstraction (ADR-0018: 3 Tiers)
 * --------------------------------------------------------- */

/**
 * @brief Unified interrupt priority levels (3 Tiers)
 *
 * Guaranteed semantics: On all platforms, HIGH priority interrupts preempt LOW priority interrupts.
 * Internal target implementations map these tiers to physical hardware priority values.
 *
 * FreeRTOS Safety Guarantee:
 * LOW, NORMAL, and HIGH tiers all reside within configMAX_SYSCALL_INTERRUPT_PRIORITY,
 * allowing safe invocation of FromISR FreeRTOS APIs (e.g., xQueueSendFromISR).
 */
typedef enum {
    PAL_IRQ_PRIO_LOW      = 1,  /**< Low priority, for general communication peripherals */
    PAL_IRQ_PRIO_NORMAL   = 2,  /**< Default priority */
    PAL_IRQ_PRIO_HIGH     = 3,  /**< High priority, for timing-critical peripherals */
    PAL_IRQ_PRIO_COUNT
} pal_irq_prio_t;

/* ---------------------------------------------------------
 * ISR Signature & Attribute Annotations
 * --------------------------------------------------------- */

/**
 * @brief ISR function signature (Platform agnostic)
 * @param arg Context pointer passed during handler registration
 *
 * ISR Contract Constraints:
 * 1. Execution duration < 10us (or < 10% of highest priority tick period)
 * 2. No blocking function calls permitted
 * 3. Stack usage < 128 bytes (including call frame)
 * 4. No operations triggering Flash Cache access allowed
 * 5. Call only FromISR RTOS APIs (safe across LOW/NORMAL/HIGH tiers)
 */
typedef void (*pal_isr_t)(void *arg);

/**
 * @def PAL_ISR
 * @brief Cross-platform dispatch ISR attribute annotation
 *
 * Usage: static PAL_ISR void my_isr(void *arg) { ... }
 *
 * Target attributes:
 * - ESP32: IRAM_ATTR (Ensures handler resides in RAM to prevent Flash cache miss delays)
 * - STM32/ARM: Empty (Cortex-M hardware handles stack frame; standard C function serves as ISR)
 * - WASM/Host: Empty (Standard function)
 */
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define PAL_ISR  IRAM_ATTR
#else
#define PAL_ISR  /* No special attribute */
#endif

/**
 * @def PAL_DEFINE_ISR
 * @brief Type-safe ISR definition macro
 *
 * Automatically generates a typed wrapper to eliminate manual `(struct xxx *)arg` casts inside ISRs.
 *
 * Usage:
 *   PAL_DEFINE_ISR(my_button_isr, struct button_state, state) {
 *       state->press_count++;
 *       state->last_press_time = pal_get_tick_count();
 *   }
 *
 *   pal_gpio_enable_interrupt(pin, edge, my_button_isr, &my_button_state);
 */
/* Fwd-decl of name##_typed does NOT carry PAL_ISR: on xtensa gcc 15,
 * applying IRAM_ATTR on both fwd-decl and definition triggers a
 * -Wattributes error ("section '.iram1.N' conflicts with previous").
 * Placement is determined solely by the definition's attribute. */
#define PAL_DEFINE_ISR(name, arg_type, arg_name)  \
    static void name##_typed(arg_type *arg_name);  \
    static PAL_ISR void name(void *arg) {  \
        name##_typed((arg_type *)arg);  \
    }  \
    static PAL_ISR void name##_typed(arg_type *arg_name)

/* ---------------------------------------------------------
 * Interrupt Controller Core Interfaces
 * --------------------------------------------------------- */

/**
 * @brief Enable and register software-dispatched interrupt with context argument
 *
 * @param[in] irq_num Logical interrupt ID (defined by device tree)
 * @param[in] prio Interrupt priority tier
 * @param[in] handler ISR handler function (must satisfy ISR contract, annotated with PAL_ISR)
 * @param[in] arg Context argument pointer passed to ISR
 *
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on invalid parameter, WINK_ERR_BUSY if IRQ already claimed
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg);

/**
 * @brief Disable and unregister interrupt
 *
 * @param[in] irq_num Logical interrupt ID
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on invalid parameter
 *
 * @note SMP Resource Reclamation: In SMP systems, after disable returns, another core may still be executing the ISR.
 *       For explicit synchronization, use `pal_irq_synchronize()` (defined in `pal_irq_advanced.h`).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_irq_disable(uint32_t irq_num);

/**
 * @brief Set interrupt pending status (software triggered interrupt)
 * @param[in] irq_num Logical interrupt ID
 */
void pal_irq_set_pending(uint32_t irq_num);

/**
 * @brief Clear interrupt pending status
 * @param[in] irq_num Logical interrupt ID
 */
void pal_irq_clear_pending(uint32_t irq_num);

/* ---------------------------------------------------------
 * Global Interrupt Locking (Critical Section Protection)
 * --------------------------------------------------------- */

/**
 * @brief Disable interrupts up to RTOS-safe priority level
 * @return Previous interrupt mask, to be passed into pal_irq_restore()
 *
 * Guarantees masking of application peripheral interrupts.
 *
 * Baremetal Fallback Strategy:
 * If no RTOS is running, this function safely degrades to global interrupt lock (e.g. __disable_irq()).
 */
uint32_t pal_irq_save_rtos_safe(void);

/**
 * @brief Restore interrupt mask
 * @param[in] mask Interrupt mask returned by pal_irq_save_rtos_safe()
 *
 * Note: Must be invoked in strict reverse order of save.
 */
void pal_irq_restore(uint32_t mask);

/**
 * @brief RAII-style critical section block wrapper
 *
 * Automatically handles paired save/restore calls to prevent deadlocks.
 * Uses pal_irq_save_rtos_safe() for RTOS-safe critical section scope.
 *
 * Usage:
 *   PAL_CRITICAL_SECTION({
 *       // Critical section code block
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
