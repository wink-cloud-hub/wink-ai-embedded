/**
 * @file wink_app.h
 * @brief App 回调契约 + 无栈协程（Protothread）宏定义。
 *
 * 回调注入（非 extern）：runtime 库不持有对外部 app_* 符号的强依赖，
 * 达成二进制级解耦（见 03-directory-architecture.md §7）。target entry 实例化
 * 本结构体并调用 wink_runtime_run。
 *
 * ⚠️  EXTREMELY IMPORTANT — Protothread Footgun Prevention ⚠️
 *
 * NEVER use automatic (stack) variables inside a protothread function!
 * When WINK_PT_YIELD/WINK_PT_DELAY_MS returns, ALL stack variables are DESTROYED.
 *
 * ❌ WRONG (causes random Heisenbugs):
 *     int i = 0;              // Stack variable - LOST after yield!
 *     WINK_PT_DELAY_MS(pt, 100);
 *     printf("%d", i);        // i is GARBAGE now!
 *
 * ✅ CORRECT:
 *     static int i = 0;       // Static - persists across yields
 *     // OR use WINK_PT_STATE_* macros for per-instance state
 *
 * THIS IS THE #1 CAUSE OF HEISENBUGS IN PROTOTHREAD CODE.
 * YOU HAVE BEEN WARNED.
 */
#ifndef WINK_APP_H
#define WINK_APP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "wink_config.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Wink Protothread - Stackless Cooperative Coroutine
 * ============================================================ */

/**
 * @brief Protothread control block
 *
 * Rules for using WINK_PT macros:
 * 1. Variables persisting across yields MUST be static OR in WINK_PT_STATE.
 * 2. WINK_PT_BEGIN must be first statement in the function.
 * 3. WINK_PT_END must be last statement in the function.
 * 4. Never use switch statements inside a protothread (they conflict).
 */
typedef struct {
    uint16_t line;               /**< Line number for switch-case jump */
    uint32_t delay_ticks;        /**< Remaining ticks for delay */
} wink_pt_t;

/* ============================================================
 *  FOOTGUN PROTECTION: Poison stack on yield in debug mode
 *  This ensures stack variables are IMMEDIATELY invalid after
 *  yield, eliminating "it happens to work" false positives.
 * ============================================================ */
#ifdef WINK_PT_DEBUG
#define WINK_PT_POISON_STACK() do { \
    volatile uint32_t _poison[16]; \
    for (int _i = 0; _i < 16; _i++) _poison[_i] = 0xDEADBEEF; \
    (void)_poison[0]; \
} while(0)
#else
#define WINK_PT_POISON_STACK() ((void)0)
#endif

/**
 * @brief Initialize protothread control block
 */
#define WINK_PT_INIT(pt) do { \
    (pt)->line = 0; \
    (pt)->delay_ticks = 0; \
} while(0)

/**
 * @brief Begin protothread function
 */
#define WINK_PT_BEGIN(pt) switch((pt)->line) { case 0:

/**
 * @brief End protothread function (restartable from beginning)
 */
#define WINK_PT_END(pt) } WINK_PT_INIT(pt); return WINK_OK

/**
 * @brief Exit protothread (complete - will not restart)
 */
#define WINK_PT_EXIT(pt) do { \
    (pt)->line = 0xFFFF; \
    return WINK_OK; \
} while(0)

/**
 * @brief Yield control back to scheduler
 */
#define WINK_PT_YIELD(pt) do { \
    WINK_PT_POISON_STACK(); /* Footgun defense */ \
    (pt)->line = __LINE__; \
    return WINK_ERR_BUSY; \
    case __LINE__:; \
} while(0)

/**
 * @brief Wait until condition becomes true
 */
#define WINK_PT_WAIT_UNTIL(pt, cond) do { \
    (pt)->line = __LINE__; \
    case __LINE__: \
    if (!(cond)) return WINK_ERR_BUSY; \
} while(0)

/**
 * @brief Wait while condition is true (inverse of WAIT_UNTIL)
 */
#define WINK_PT_WAIT_WHILE(pt, cond) WINK_PT_WAIT_UNTIL(pt, !(cond))

/**
 * @brief Delay for specified milliseconds
 *
 * Note: Uses WINK_RUNTIME_TICK_MS granularity. Actual delay may be up
 * to one tick longer than requested.
 *
 * Caller must ensure this macro is only invoked while the tick counter
 * advances (typically from main loop dispatch, not raw interrupt context).
 */
#define WINK_PT_DELAY_MS(pt, ms) do { \
    (pt)->delay_ticks = ((ms) + WINK_RUNTIME_TICK_MS - 1) / WINK_RUNTIME_TICK_MS; \
    while ((pt)->delay_ticks > 0) { \
        (pt)->delay_ticks--; \
        WINK_PT_YIELD(pt); \
    } \
} while(0)

/* ============================================================
 *  STATEFUL PROTOTHREAD MACROS - API design guides correct usage
 *  Developers using these macros naturally put state in structs,
 *  completely avoiding the auto variable footgun by design.
 * ============================================================ */

/**
 * @brief Begin declaration of per-protothread state struct
 *
 * Usage:
 *   WINK_PT_STATE_BEGIN(my_coroutine)
 *       int counter;         // ← Automatically persistent!
 *       float temperature;   // ← No static needed, per-instance
 *   WINK_PT_STATE_END()
 */
#define WINK_PT_STATE_BEGIN(name) \
    struct name##_state {

#define WINK_PT_STATE_END() \
        uint32_t _magic; \
    };

/**
 * @brief Use state struct inside coroutine
 *
 * State is stored immediately following wink_pt_t in memory,
 * so each coroutine instance has its own state (no static!).
 *
 * Usage inside coroutine function:
 *   wink_status_t my_coroutine(wink_pt_t *pt) {
 *       WINK_PT_STATE_USE(my_coroutine);
 *
 *       WINK_PT_BEGIN(pt);
 *
 *       state->counter = 0;  // ✅ SAFE! In per-instance struct
 *       while (state->counter < 5) {
 *           WINK_PT_DELAY_MS(pt, 100);  // yield
 *           state->counter++;  // ✅ Value preserved across yield
 *       }
 *
 *       WINK_PT_END(pt);
 *   }
 */
#define WINK_PT_STATE_USE(name) \
    struct name##_state *state = (struct name##_state *)((uint8_t *)(pt) + sizeof(wink_pt_t)); \
    if (state->_magic != 0x50545354UL) { /* "PTST" */ \
        memset(state, 0, sizeof(*state)); \
        state->_magic = 0x50545354UL; \
    }

/**
 * @brief Protothread function type signature
 */
typedef wink_status_t (*wink_pt_func_t)(wink_pt_t *pt);

/**
 * @brief Helper wrapper to run protothread as a soft timer
 *
 * Registers the protothread for 1-tick periodic dispatch so that
 * WINK_PT_DELAY_MS and WINK_PT_YIELD progress automatically.
 *
 * Usage:
 *   // Define your coroutine wrapper (copy-paste this pattern)
 *   static wink_status_t my_coroutine_wrapper(void *arg) {
 *       wink_pt_t *pt = (wink_pt_t *)arg;
 *       wink_status_t status = my_coroutine(pt);
 *       return (status == WINK_ERR_BUSY) ? WINK_OK : status;
 *   }
 *
 *   // Then create timer:
 *   wink_timer_handle_t h = wink_soft_timer_create(
 *       my_coroutine_wrapper, &my_pt_ctx,
 *       WINK_TIMER_PERIODIC, WINK_RUNTIME_TICK_MS
 *   );
 */
#define WINK_PT_TIMER_WRAPPER(name, coroutine) \
    static wink_status_t name(void *arg) { \
        wink_pt_t *pt = (wink_pt_t *)arg; \
        wink_status_t status = coroutine(pt); \
        return (status == WINK_ERR_BUSY) ? WINK_OK : status; \
    }

/**
 * @brief App 生命周期回调集合（各字段允许为 NULL，runtime 跳过）
 *  - init:    启动时调用一次
 *  - loop:    每个 tick 调用
 *  - on_fault: 故障时调用（fault_code 由 runtime 或 App 上报）
 */
typedef struct {
    void (*init)(void);
    void (*loop)(void);
    void (*on_fault)(uint32_t fault_code);
} wink_app_callbacks_t;

/** @brief App 侧周期延时（内部转 PAL pal_delay_ms，语义由 target 实现） */
void wink_app_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* WINK_APP_H */
