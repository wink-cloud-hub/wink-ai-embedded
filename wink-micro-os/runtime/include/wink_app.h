// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_app.h
 * @brief Application lifecycle callback contract + Protothread coroutine macros.
 */
#ifndef WINK_APP_H
#define WINK_APP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "wink_config.h"
#include "wink_status.h"
#include "wink_event.h"

typedef enum {
    WINK_RESET_REASON_UNKNOWN    = 0,
    WINK_RESET_REASON_POWER_ON   = 1,
    WINK_RESET_REASON_WATCHDOG   = 2,
    WINK_RESET_REASON_PANIC      = 3,
    WINK_RESET_REASON_SOFTWARE   = 4,
    WINK_RESET_REASON_BROWNOUT   = 5,
} wink_reset_reason_t;

#ifndef static_assert
#if defined(__cplusplus)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   define static_assert(expr, msg) _Static_assert(expr, msg)
#else
#   define static_assert(expr, msg) typedef char static_assert_failed[(expr)?1:-1]
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t line;               /**< Line number for switch-case jump */
    uint32_t delay_ticks;        /**< Remaining ticks for delay */
} wink_pt_t;

#ifdef WINK_PT_DEBUG
#define WINK_PT_POISON_STACK() do { \
    volatile uint32_t _poison[16]; \
    for (int _i = 0; _i < 16; _i++) _poison[_i] = 0xDEADBEEF; \
    (void)_poison[0]; \
} while(0)
#else
#define WINK_PT_POISON_STACK() ((void)0)
#endif

#define WINK_PT_INIT(pt) do { \
    (pt)->line = 0; \
    (pt)->delay_ticks = 0; \
} while(0)

#define WINK_PT_BEGIN(pt) switch((pt)->line) { case 0:

#define WINK_PT_END(pt) } WINK_PT_INIT(pt); return WINK_OK

#define WINK_PT_EXIT(pt) do { \
    (pt)->line = 0xFFFF; \
    return WINK_OK; \
} while(0)

#define WINK_PT_YIELD(pt) do { \
    WINK_PT_POISON_STACK(); \
    (pt)->line = __LINE__; \
    return WINK_ERR_BUSY; \
    case __LINE__:; \
} while(0)

#define WINK_PT_WAIT_UNTIL(pt, cond) do { \
    (pt)->line = __LINE__; \
    case __LINE__: \
    if (!(cond)) return WINK_ERR_BUSY; \
} while(0)

#define WINK_PT_WAIT_WHILE(pt, cond) WINK_PT_WAIT_UNTIL(pt, !(cond))

#define WINK_PT_DELAY_MS(pt, ms) do { \
    (pt)->delay_ticks = ((ms) + WINK_RUNTIME_TICK_MS - 1) / WINK_RUNTIME_TICK_MS; \
    while ((pt)->delay_ticks > 0) { \
        (pt)->delay_ticks--; \
        WINK_PT_YIELD(pt); \
    } \
} while(0)

#define WINK_PT_STATE_BEGIN(name) \
    struct name##_state {

#define WINK_PT_STATE_END(name) \
        uint32_t _magic; \
    }; \
    /* Compile-time validation: state struct must be POD-compatible */ \
    static_assert(sizeof(struct name##_state) >= sizeof(uint32_t), \
        #name "_state must be at least large enough for _magic field")

#define WINK_PT_STATE_USE(name) \
    /* Compile-time: verify pt + state layout has no unexpected padding */ \
    static_assert(offsetof(struct name##_state, _magic) == \
        sizeof(struct name##_state) - sizeof(uint32_t), \
        #name "_state _magic field must be the last member"); \
    /* Runtime: initialize state on first use with magic validation */ \
    struct name##_state *state = (struct name##_state *)((uint8_t *)(pt) + sizeof(wink_pt_t)); \
    if (state->_magic != 0x50545354UL) { /* "PTST" - Protothread State */ \
        memset(state, 0, sizeof(*state)); \
        state->_magic = 0x50545354UL; \
    }

#define WINK_PT_STATE_INIT(pt, name) do { \
    struct name##_state *s = (struct name##_state *)((uint8_t *)(pt) + sizeof(wink_pt_t)); \
    memset(s, 0, sizeof(*s)); \
    s->_magic = 0x50545354UL; \
} while(0)

#define WINK_PT_STATE_VALID(pt, name) \
    (((const struct name##_state *)((const uint8_t *)(pt) + sizeof(wink_pt_t)))->_magic \
        == 0x50545354UL)

typedef wink_status_t (*wink_pt_func_t)(wink_pt_t *pt);

#define WINK_PT_TIMER_WRAPPER(name, coroutine) \
    static wink_status_t name(void *arg) { \
        wink_pt_t *pt = (wink_pt_t *)arg; \
        wink_status_t status = coroutine(pt); \
        return (status == WINK_ERR_BUSY) ? WINK_OK : status; \
    }

typedef struct {
    wink_reset_reason_t reset_reason;      /**< Reason for boot */
    uint32_t abnormal_boot_count;          /**< Consecutive abnormal resets count */
    bool     is_healthy_recovery;          /**< True if healthy recovery */
    uint32_t uptime_ms;                    /**< Uptime in ms */
} wink_boot_info_t;

typedef struct wink_app_callbacks {
    /* Legacy void-returning callbacks (kept for ABI compat). */
    void (*init)(void);
    void (*loop)(void);
    void (*on_fault)(uint32_t fault_code);

    /* Extended lifecycle hooks (added Wave 2, 2026-07). */
    void          (*on_boot)(const wink_boot_info_t *info);
    wink_status_t (*init_status)(void);            /* return non-OK -> auto raise_fault */
    wink_status_t (*on_fault_status)(uint32_t fault_code); /* OK=recovered, LOCKED=halt */
    void          (*on_event)(const wink_event_t *evt);   /* callback for handling asynchronous events */
} wink_app_callbacks_t;

void wink_app_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* WINK_APP_H */
