/**
 * @file wink_servo_sweep.c
 * @brief BAL servo helper ??sweep and set angles for DAL servos.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.servo"

#include "actuator/wink_servo_sweep.h"
#include "wink_tasks.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include "wink_status.h"

#include <stddef.h>

/* DAL pruning (WINK_UNAVAILABLE): force the MAX==0 stub path when the
 * servo driver is off so ESP32 still links wink_bal (ADR-0039). */
#if !defined(WINK_USE_SERVO) || !(WINK_USE_SERVO)
#  undef WINK_SERVO_SWEEP_MAX
#  define WINK_SERVO_SWEEP_MAX 0
#endif

/* ADR-0017 BAL-exception: helper ???? wink_periodic MAY_BLOCK ???? WINK_BLOCKING API */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

#if WINK_SERVO_SWEEP_MAX > 0

typedef struct {
    dal_servo_t            *servo;
    wink_periodic_handle_t  period_h;
    float                   min_angle;
    float                   max_angle;
    float                   curr_angle;
    float                   step;
} servo_ctx_t;

static servo_ctx_t s_slots[WINK_SERVO_SWEEP_MAX];

/* ?? internal helpers ?????????????????????????????????????????? */

static pal_os_core_id_t map_core(wink_bal_core_t c) {
    switch (c) {
        case WINK_BAL_CORE_0: return PAL_OS_CORE_0;
        case WINK_BAL_CORE_1: return PAL_OS_CORE_1;
        case WINK_BAL_CORE_ANY:        /* fallthrough */
        case WINK_BAL_CORE_INVALID:    /* fallthrough */
        default:                      return PAL_OS_CORE_ANY;
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < WINK_SERVO_SWEEP_MAX; i++) {
        if (s_slots[i].servo == NULL) {
            return i;
        }
    }
    return -1;
}

static int find_slot_by_servo(dal_servo_t *servo) {
    for (int i = 0; i < WINK_SERVO_SWEEP_MAX; i++) {
        if (s_slots[i].servo == servo) {
            return i;
        }
    }
    return -1;
}

static void servo_tick(void *arg) {
    servo_ctx_t *ctx = (servo_ctx_t *)arg;
    if (ctx->servo == NULL) {
        return;
    }
    float next_angle = ctx->curr_angle + ctx->step;
    if (next_angle >= ctx->max_angle) {
        next_angle = ctx->max_angle;
        ctx->step = -ctx->step;
    } else if (next_angle <= ctx->min_angle) {
        next_angle = ctx->min_angle;
        ctx->step = -ctx->step;
    }
    ctx->curr_angle = next_angle;
    WINK_IGNORE_RESULT(dal_servo_set_angle(ctx->servo, next_angle));
}

/* ?? public API ???????????????????????????????????????????????? */

wink_status_t wink_servo_sweep_start_ex(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms,
                                        const wink_bal_opts_t *opts)
{
    if (servo == NULL || period_ms == 0u || min_angle > max_angle) {
        LOG_D("start: invalid arg");
        return WINK_ERR_INVALID_ARG;
    }
    if (find_slot_by_servo(servo) >= 0) {
        LOG_D("start: servo already running");
        return WINK_ERR_INVALID_STATE;
    }
    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("start: out of slots");
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_bal_opts_t effective = WINK_BAL_OPTS_DEFAULT;
    if (opts != NULL) {
        effective = *opts;
    }

    uint32_t flags = effective.flags;
    if (flags == 0u) {
        flags = WINK_PERIODIC_MAY_BLOCK;
    }

    uint32_t stack = (effective.stack_bytes != 0u) ? effective.stack_bytes : 2048u;
    int32_t prio = (effective.priority >= 0) ? effective.priority : 3;
    pal_os_core_id_t core = map_core(effective.core_id);

    servo_ctx_t *ctx = &s_slots[free_idx];
    ctx->servo = servo;
    ctx->min_angle = min_angle;
    ctx->max_angle = max_angle;
    ctx->curr_angle = min_angle;
    ctx->step = 5.0f; /* default step 5 degrees per tick */
    ctx->period_h = WINK_PERIODIC_INVALID;

    wink_status_t st = dal_servo_set_angle(servo, min_angle);
    if (wink_status_is_error(st)) {
        LOG_D("start: initial angle set failed");
        ctx->servo = NULL;
        return st;
    }

    wink_periodic_handle_t h = wink_periodic_start_ex(
        "servo", stack, period_ms, servo_tick, ctx, flags, prio, core);
    if (h < 0) {
        ctx->servo = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

wink_status_t wink_servo_sweep_start(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms) {
    return wink_servo_sweep_start_ex(servo, min_angle, max_angle, period_ms, NULL);
}

wink_status_t wink_servo_sweep_stop(dal_servo_t *servo) {
    if (servo == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_servo(servo);
    if (idx < 0) {
        return WINK_OK;
    }
    servo_ctx_t *ctx = &s_slots[idx];
    if (ctx->period_h > 0) {
        wink_periodic_stop(ctx->period_h);
    }
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->servo = NULL;
    return WINK_OK;
}

wink_status_t wink_servo_sweep_set_period(dal_servo_t *servo, uint32_t period_ms) {
    if (servo == NULL || period_ms == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_servo(servo);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    servo_ctx_t *ctx = &s_slots[idx];
    return wink_periodic_change_period(ctx->period_h, period_ms);
}

bool wink_servo_sweep_is_running(dal_servo_t *servo) {
    if (servo == NULL) {
        return false;
    }
    return (find_slot_by_servo(servo) >= 0);
}

wink_status_t wink_servo_set_angle(dal_servo_t *servo, float angle) {
    return dal_servo_set_angle(servo, angle);
}

void wink_servo_sweep_reset(void) {
    for (int i = 0; i < WINK_SERVO_SWEEP_MAX; i++) {
        if (s_slots[i].servo != NULL) {
            if (s_slots[i].period_h > 0) {
                wink_periodic_stop(s_slots[i].period_h);
            }
            s_slots[i].period_h = WINK_PERIODIC_INVALID;
            s_slots[i].servo = NULL;
        }
    }
}

#else /* WINK_SERVO_SWEEP_MAX == 0 */

wink_status_t wink_servo_sweep_start_ex(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms,
                                        const wink_bal_opts_t *opts)
{
    (void)servo; (void)min_angle; (void)max_angle; (void)period_ms; (void)opts;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t wink_servo_sweep_start(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms) {
    (void)servo; (void)min_angle; (void)max_angle; (void)period_ms;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t wink_servo_sweep_stop(dal_servo_t *servo) {
    (void)servo;
    return WINK_OK;
}

wink_status_t wink_servo_sweep_set_period(dal_servo_t *servo, uint32_t period_ms) {
    (void)servo; (void)period_ms;
    return WINK_ERR_UNSUPPORTED;
}

bool wink_servo_sweep_is_running(dal_servo_t *servo) {
    (void)servo;
    return false;
}

wink_status_t wink_servo_set_angle(dal_servo_t *servo, float angle) {
    (void)servo;
    (void)angle;
    return WINK_ERR_UNSUPPORTED;
}

void wink_servo_sweep_reset(void) {
    /* No-op on stub */
}

#endif /* WINK_SERVO_SWEEP_MAX > 0 */

WINK_INTERNAL_BLOCKING_REGION_END
