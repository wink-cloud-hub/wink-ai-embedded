/**
 * @file wink_default_telemetry.c
 * @brief Default 2s telemetry task (samples/common) — uses Runtime
 *        spawn_periodic mechanism; format is a debug-policy choice, not OS.
 */
#define LOG_TAG "telem"

#include "wink_default_telemetry.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "wink_trace.h"
#include "pal_log.h"
#include "dal_ultrasonic.h"
#include "dal_button.h"

/* ── per-task context (one-shot, static) ────────────────────────────────── */
typedef struct {
    const dal_ultrasonic_t *sonar;
    const dal_button_t     *btn;
} telem_ctx_t;

static void telem_fn(void *ctx_arg)
{
    const telem_ctx_t *ctx = (const telem_ctx_t *)ctx_arg;
    wink_runtime_stats_t st;
    wink_runtime_get_stats(&st);

    float         dist_cm  = -1.0f;
    wink_status_t sonar_st = WINK_ERR_UNSUPPORTED;
    uint32_t      isrs     = 0u;

    if (ctx->sonar != NULL) {
        sonar_st = dal_ultrasonic_get_cached_distance(ctx->sonar, &dist_cm);
    }
    if (ctx->btn != NULL) {
        uint32_t cnt = 0u;
        WINK_IGNORE_RESULT(dal_button_get_edge_count(ctx->btn, &cnt));
        isrs = cnt;
    }

    LOG_I("uptime=%lums heap=%lu stack_min=%lu faults=%lu warns=%lu "
          "isrs=%lu sonar_st=%d dist=%.2fcm reset=%d abn=%lu",
          (unsigned long)st.uptime_ms,
          (unsigned long)st.free_heap,
          (unsigned long)st.min_free_stack,
          (unsigned long)st.fault_count,
          (unsigned long)st.warn_count,
          (unsigned long)isrs,
          (int)sonar_st, dist_cm,
          (int)st.last_reset_reason,
          (unsigned long)st.abnormal_boot_count);
}

wink_status_t wink_default_telemetry_start(const dal_ultrasonic_t *sonar,
                                           const dal_button_t     *btn)
{
    /* One static context — we don't expect more than one telemetry task per
     * sample (this is a debug helper, not a generic pub-sub system). */
    static telem_ctx_t s_ctx;
    s_ctx.sonar = sonar;
    s_ctx.btn   = btn;

    /* Period 2000ms, stack 2048, low priority (1), any core. */
    return wink_runtime_spawn_periodic(
        "default_telem", 2048u, 2000u,
        telem_fn, &s_ctx,
        /*priority=*/1, PAL_OS_CORE_ANY);
}
