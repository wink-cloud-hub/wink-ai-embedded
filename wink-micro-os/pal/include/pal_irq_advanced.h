// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_irq_advanced.h
 * @brief PAL Advanced System-Level Interrupt APIs.
 *
 * Restricted system-level APIs (e.g., global hardware interrupt lock and SMP synchronization primitives).
 * Physically isolated via WINK_ALLOW_ADVANCED_IRQ_APIS guard macro to prevent accidental usage.
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
 * @brief Wait for all active ISR executions to finish (SMP safe synchronization primitive)
 *
 * SMP Critical Primitive (ADR-0018):
 * In dual-core/multi-core systems, after pal_irq_disable() returns, another core
 * may still be executing the ISR handler. Releasing ISR context memory prematurely
 * would cause Use-After-Free (UAF) crashes.
 *
 * Usage pattern:
 *   pal_irq_disable(irq_num);
 *   pal_irq_synchronize(irq_num);  // Wait for all cores to exit ISR
 *   free(irq_resource);            // Safe to release
 *
 * @param[in] irq_num Logical interrupt ID (pass ~0U to wait for all interrupts)
 */
void pal_irq_synchronize(uint32_t irq_num);

/**
 * @brief Disable all maskable interrupts and return previous interrupt mask
 *
 * Guarantees that ALL maskable hardware interrupts are disabled (including highest hardware priority).
 * Supports nested calls; must be paired with pal_irq_restore().
 *
 * Critical Constraint:
 * Code inside this critical section MUST execute in < 1µs to prevent Wi-Fi baseband jitter or watchdog reset.
 *
 * Target Implementation Details:
 * - ESP32: XTOS_SET_INTLEVEL(XCHAL_NUM_INTLEVELS)
 * - STM32: __disable_irq()
 * - WASM: Increment lock counter; record pending IRQs without dispatching
 * - Host: Increment lock counter; defer dispatching
 *
 * @return Previous interrupt state mask to be passed to pal_irq_restore()
 */
uint32_t pal_irq_save(void);

/**
 * @brief Strict RAII critical section wrapper (<1µs extreme atomic scope)
 *
 * Masks all maskable interrupts including high-priority hardware sources.
 * Use with caution for extreme <1µs atomic operations.
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
