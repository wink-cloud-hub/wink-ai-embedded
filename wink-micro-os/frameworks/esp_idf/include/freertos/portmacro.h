/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;

#define portMAX_DELAY           (TickType_t) 0xffffffffUL
#define portTICK_PERIOD_MS      ((TickType_t) 1000 / configTICK_RATE_HZ)
#define portNUM_PROCESSORS      1

#define portNOP()               ((void)0)

/* Cooperative simulation yield and ISR preemption request primitives (Phase 3 ISSUE-04) */
void sim_scheduler_yield_context(void);
void esp_freertos_request_isr_yield(void);

#define portYIELD()                  sim_scheduler_yield_context()
#define portYIELD_FROM_ISR(woken)    do { if (woken) { esp_freertos_request_isr_yield(); } } while(0)
#define portEND_SWITCHING_ISR(woken) portYIELD_FROM_ISR(woken)

/* ── Phase 2: portMUX_TYPE spinlock state tracking (ISSUE-02) ──────────────
 * In single-virtual-core cooperative scheduling, portMUX_TYPE acts as a
 * mutual-exclusion marker and critical-section boundary declaration. Physical
 * SMP races are impossible, but we must faithfully track owner/depth so that
 * illegal yields inside critical sections are caught (ADR-0012 contract
 * honesty, ADR-0014 single-core deterministic scheduling).            */
#define portMUX_NO_OWNER        0xB33FFFFFUL

typedef struct {
    uint32_t owner;  /* task slot ID of current holder, or portMUX_NO_OWNER */
    uint32_t count;  /* re-entrant nesting depth (same owner may acquire N times) */
} portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED { .owner = portMUX_NO_OWNER, .count = 0 }

/* Task-context critical section API */
void vPortEnterCritical(portMUX_TYPE *mux);
void vPortExitCritical(portMUX_TYPE *mux);

/* Safe variants (callable from both task and ISR context) */
void vPortEnterCriticalSafe(portMUX_TYPE *mux);
void vPortExitCriticalSafe(portMUX_TYPE *mux);

/* M1: ISR variants kept as independent function bodies (not macro aliases).
 * Phase 2 single-virtual-core: logic re-uses task variant internally, but
 * independent symbols preserve the Phase 3 ISR event pump divergence point. */
void vPortEnterCritical_ISR(portMUX_TYPE *mux);
void vPortExitCritical_ISR(portMUX_TYPE *mux);

#define portENTER_CRITICAL(mux)      vPortEnterCritical(mux)
#define portEXIT_CRITICAL(mux)       vPortExitCritical(mux)
#define portENTER_CRITICAL_ISR(mux)  vPortEnterCritical_ISR(mux)
#define portEXIT_CRITICAL_ISR(mux)   vPortExitCritical_ISR(mux)
#define portENTER_CRITICAL_SAFE(mux) vPortEnterCriticalSafe(mux)
#define portEXIT_CRITICAL_SAFE(mux)  vPortExitCriticalSafe(mux)

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
