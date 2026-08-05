// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_trace.c
 * @brief Golden Trace implementation: static ring buffer (zero dynamic allocation).
 */
#include "wink_trace.h"
#include "pal_osal.h"

static uint32_t s_buffer[WINK_TRACE_CAPACITY];
static uint32_t s_count = 0;     /* Total written count (including overwrites) */
static uint32_t s_head = 0;      /* Next write index */
static uint32_t s_warn_count = 0; /* Warning counter */

static inline void s_record_fault_locked(uint32_t fault_code) {
    s_buffer[s_head] = fault_code;
    s_head = (s_head + 1u) % WINK_TRACE_CAPACITY;
    s_count++;
}

void wink_trace_reset(void) {
    uint32_t key = pal_os_critical_enter();
    s_count = 0;
    s_head = 0;
    s_warn_count = 0;
    pal_os_critical_exit(key);
}

void wink_trace_fault(uint32_t fault_code) {
    uint32_t key = pal_os_critical_enter();
    s_record_fault_locked(fault_code);
    pal_os_critical_exit(key);
}

void wink_trace_fault_from_isr(uint32_t fault_code) {
    uint32_t key = pal_os_critical_enter_isr();
    s_record_fault_locked(fault_code);
    pal_os_critical_exit_isr(key);
}

uint32_t wink_trace_count(void) {
    uint32_t key = pal_os_critical_enter();
    uint32_t count = (s_count < WINK_TRACE_CAPACITY) ? s_count : WINK_TRACE_CAPACITY;
    pal_os_critical_exit(key);
    return count;
}

void wink_trace_warn(uint32_t warn_code) {
    (void)warn_code;
    uint32_t key = pal_os_critical_enter();
    s_warn_count++;
    pal_os_critical_exit(key);
}

uint32_t wink_warn_count(void) {
    uint32_t key = pal_os_critical_enter();
    uint32_t count = s_warn_count;
    pal_os_critical_exit(key);
    return count;
}

uint32_t wink_trace_last(void) {
    uint32_t key = pal_os_critical_enter();
    if (s_count == 0) {
        pal_os_critical_exit(key);
        return 0u;
    }
    uint32_t last_idx = (s_head + WINK_TRACE_CAPACITY - 1u) % WINK_TRACE_CAPACITY;
    uint32_t last_val = s_buffer[last_idx];
    pal_os_critical_exit(key);
    return last_val;
}
