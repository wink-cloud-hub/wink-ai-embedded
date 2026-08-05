// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_PT_DEBUG_H
#define WINK_PT_DEBUG_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"

#define WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING 8006u

#ifdef __cplusplus
extern "C" {
#endif

bool wink_pt_in_context(void);

bool wink_soft_timer_in_light_dispatch(void);

#ifdef WINK_PT_DEBUG
#include <assert.h>
extern void wink_trace_fault(uint32_t fault_code);
#define WINK_ASSERT(cond) assert(cond)
#define WINK_ASSERT_NONBLOCKING() do { \
    if (wink_pt_in_context()) { \
        wink_trace_fault((uint32_t)WINK_ERR_PANIC); \
        assert(!wink_pt_in_context() && "Fatal: Blocking API called within Protothread context!"); \
    } \
    if (wink_soft_timer_in_light_dispatch()) { \
        wink_trace_fault(WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING); \
        assert(!wink_soft_timer_in_light_dispatch() && "Fatal: Blocking API called within LIGHT (soft-timer) callback context!"); \
    } \
} while (0)
#else
#define WINK_ASSERT(cond) ((void)0)
#define WINK_ASSERT_NONBLOCKING() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINK_PT_DEBUG_H */
