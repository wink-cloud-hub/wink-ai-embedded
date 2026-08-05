// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_trace.h
 * @brief Golden Trace - Fault and event logging subsystem.
 */
#ifndef WINK_TRACE_H
#define WINK_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Ring buffer capacity (statically allocated) */
#ifndef WINK_TRACE_CAPACITY
#define WINK_TRACE_CAPACITY 32
#endif

/**
 * @brief Reset and clear trace ring buffer
 */
void wink_trace_reset(void);

/**
 * @brief Record a fault code (TASK context)
 * @param[in] fault_code Application/runtime fault code
 */
void wink_trace_fault(uint32_t fault_code);

/**
 * @brief Record a fault code from ISR (ISR context, ADR-0016)
 * @param[in] fault_code Application/runtime fault code
 */
void wink_trace_fault_from_isr(uint32_t fault_code);

/**
 * @brief Get total recorded fault entries count (<= WINK_TRACE_CAPACITY)
 * @return Current fault entry count
 */
uint32_t wink_trace_count(void);

/**
 * @brief Record a warning event code (performance / budget warning)
 * @param[in] warn_code Warning event code
 */
void wink_trace_warn(uint32_t warn_code);

/**
 * @brief Get total recorded warning count since boot or reset
 * @return Total warning count
 */
uint32_t wink_warn_count(void);

/**
 * @brief Get the most recently recorded fault code (0 if empty)
 * @return Last fault code or 0
 */
uint32_t wink_trace_last(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_TRACE_H */
