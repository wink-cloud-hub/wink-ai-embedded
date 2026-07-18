#define LOG_TAG "bal.cl_motor"

#include "control/wink_closed_loop_motor.h"
#include "wink_tasks.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include "pal_osal.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include <string.h>

#ifndef WINK_CLOSED_LOOP_MOTOR_HELPER_MAX
#  ifdef WINK_APP_MAX_MOTOR_INSTANCES
#    define WINK_CLOSED_LOOP_MOTOR_HELPER_MAX WINK_APP_MAX_MOTOR_INSTANCES
#  else
#    define WINK_CLOSED_LOOP_MOTOR_HELPER_MAX 4
#  endif
#endif

#if WINK_CLOSED_LOOP_MOTOR_HELPER_MAX > 0

typedef struct {
    dal_motor_t            *motor;
    dal_encoder_t          *encoder;
    wink_pid_t              pid;
    uint32_t                period_ms;
    uint32_t                timeout_ms;
    float                   counts_per_rev;
    
    volatile float          target_speed;
    volatile float          current_speed;
    int32_t                 last_encoder_count;
    uint64_t                last_time_us;
    uint64_t                last_pulse_time_ms;
    
    wink_periodic_handle_t  period_h;
} motor_ctx_t;

static motor_ctx_t s_slots[WINK_CLOSED_LOOP_MOTOR_HELPER_MAX];

/* ── internal helpers ────────────────────────────────────────── */

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
    for (int i = 0; i < WINK_CLOSED_LOOP_MOTOR_HELPER_MAX; i++) {
        if (s_slots[i].motor == NULL) {
            return i;
        }
    }
    return -1;
}

static int find_slot_by_motor(dal_motor_t *motor) {
    for (int i = 0; i < WINK_CLOSED_LOOP_MOTOR_HELPER_MAX; i++) {
        if (s_slots[i].motor == motor) {
            return i;
        }
    }
    return -1;
}

static void motor_tick(void *arg) {
    motor_ctx_t *ctx = (motor_ctx_t *)arg;
    if (ctx->motor == NULL || ctx->encoder == NULL) {
        return;
    }

    uint64_t now_us = pal_os_get_us();
    uint64_t now_ms = pal_os_get_ms();

    // 1. Read current encoder count
    int32_t current_count = 0;
    if (wink_status_is_error(dal_encoder_get_count(ctx->encoder, &current_count))) {
        return;
    }

    // Calculate delta time
    float dt = (float)(now_us - ctx->last_time_us) / 1000000.0f;
    if (dt <= 0.0f) {
        return;
    }

    // Calculate delta ticks
    int32_t delta_ticks = current_count - ctx->last_encoder_count;
    
    // Check if encoder ticks changed to update safety heartbeat
    if (delta_ticks != 0) {
        ctx->last_pulse_time_ms = now_ms;
        ctx->last_encoder_count = current_count;
    }
    ctx->last_time_us = now_us;

    // 2. Read target speed thread-safely
    float target = ctx->target_speed;

    // 3. Fail-safe timeout check
    if (target != 0.0f && (now_ms - ctx->last_pulse_time_ms) > ctx->timeout_ms) {
        // Stop motor output immediately (Fail-safe)
        WINK_IGNORE_RESULT(dal_motor_safe_off(ctx->motor));
        // Reset PID states
        wink_pid_reset(&ctx->pid);
        ctx->current_speed = 0.0f;
        // Raise feedback loss fault
        wink_trace_fault(WINK_FAULT_MOTOR_FEEDBACK_LOSS);
        return;
    }

    // 4. Calculate actual feedback speed (counts/s)
    float feedback_speed = (float)delta_ticks / dt;
    ctx->current_speed = feedback_speed;

    // 5. Update PID controller
    float control_output = 0.0f;
    wink_status_t status = wink_pid_update(&ctx->pid, target, feedback_speed, dt, &control_output);
    if (wink_status_is_error(status)) {
        return;
    }

    // 6. Set motor speed (PID outputs -1.0f to 1.0f)
    if (control_output > 1.0f) {
        control_output = 1.0f;
    } else if (control_output < -1.0f) {
        control_output = -1.0f;
    }
    WINK_IGNORE_RESULT(dal_motor_set_speed(ctx->motor, control_output));
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_closed_loop_motor_start_ex(dal_motor_t *motor, 
                                              dal_encoder_t *encoder,
                                              const wink_closed_loop_motor_config_t *cfg,
                                              const wink_bal_opts_t *opts)
{
    if (motor == NULL || encoder == NULL || cfg == NULL || cfg->period_ms == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (find_slot_by_motor(motor) >= 0) {
        return WINK_ERR_INVALID_STATE;
    }
    int free_idx = find_free_slot();
    if (free_idx < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_bal_opts_t effective = WINK_BAL_OPTS_DEFAULT;
    if (opts != NULL) {
        effective = *opts;
    }

    uint32_t flags = effective.flags;
    if (flags == 0u) {
        flags = WINK_PERIODIC_LIGHT; /* PID loop should run as LIGHT */
    }

    uint32_t stack = (effective.stack_bytes != 0u) ? effective.stack_bytes : 2048u;
    int32_t prio = (effective.priority >= 0) ? effective.priority : 4;
    pal_os_core_id_t core = map_core(effective.core_id);

    motor_ctx_t *ctx = &s_slots[free_idx];
    ctx->motor = motor;
    ctx->encoder = encoder;
    ctx->period_ms = cfg->period_ms;
    ctx->timeout_ms = cfg->timeout_ms > 0 ? cfg->timeout_ms : 500u; // Default 500ms timeout
    ctx->counts_per_rev = cfg->counts_per_rev > 0.0f ? cfg->counts_per_rev : 1.0f;
    ctx->target_speed = 0.0f;

    // Read initial encoder counts and timestamp
    int32_t initial_count = 0;
    wink_status_t st = dal_encoder_get_count(encoder, &initial_count);
    if (wink_status_is_error(st)) {
        ctx->motor = NULL;
        return st;
    }
    ctx->last_encoder_count = initial_count;
    ctx->last_time_us = pal_os_get_us();
    ctx->last_pulse_time_ms = pal_os_get_ms();

    // Initialize the PID algorithm state
    wink_pid_init(&ctx->pid, &cfg->pid_cfg);

    // Initial stop
    st = dal_motor_safe_off(motor);
    if (wink_status_is_error(st)) {
        ctx->motor = NULL;
        return st;
    }

    // Start periodic task
    wink_periodic_handle_t h = wink_periodic_start_ex(
        "cl_motor", stack, cfg->period_ms, motor_tick, ctx, flags, prio, core);
    if (h < 0) {
        ctx->motor = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

wink_status_t wink_closed_loop_motor_start(dal_motor_t *motor, 
                                           dal_encoder_t *encoder,
                                           const wink_closed_loop_motor_config_t *cfg)
{
    return wink_closed_loop_motor_start_ex(motor, encoder, cfg, NULL);
}

wink_status_t wink_closed_loop_motor_stop(dal_motor_t *motor)
{
    if (motor == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_motor(motor);
    if (idx < 0) {
        return WINK_OK; // Idempotent stop
    }
    motor_ctx_t *ctx = &s_slots[idx];
    
    // Stop the periodic task
    if (ctx->period_h > 0) {
        wink_periodic_stop(ctx->period_h);
    }
    ctx->period_h = WINK_PERIODIC_INVALID;

    // Safety shutdown of the motor
    WINK_IGNORE_RESULT(dal_motor_safe_off(ctx->motor));

    // Release slot
    ctx->motor = NULL;
    ctx->encoder = NULL;

    return WINK_OK;
}

wink_status_t wink_closed_loop_motor_set_speed(dal_motor_t *motor, float target_speed)
{
    if (motor == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_motor(motor);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    
    motor_ctx_t *ctx = &s_slots[idx];
    
    // Thread-safe update using critical section
    PAL_CRITICAL_SECTION({
        ctx->target_speed = target_speed;
        // Also update the pulse time to prevent premature timeout trigger when starting from zero
        ctx->last_pulse_time_ms = pal_os_get_ms();
    });

    return WINK_OK;
}

wink_status_t wink_closed_loop_motor_get_speed(dal_motor_t *motor, float *out_speed)
{
    if (motor == NULL || out_speed == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_motor(motor);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    
    motor_ctx_t *ctx = &s_slots[idx];
    PAL_CRITICAL_SECTION({
        *out_speed = ctx->current_speed;
    });
    return WINK_OK;
}

void wink_closed_loop_motor_reset(void)
{
    for (int i = 0; i < WINK_CLOSED_LOOP_MOTOR_HELPER_MAX; i++) {
        if (s_slots[i].motor != NULL) {
            if (s_slots[i].period_h > 0) {
                wink_periodic_stop(s_slots[i].period_h);
            }
            WINK_IGNORE_RESULT(dal_motor_safe_off(s_slots[i].motor));
            s_slots[i].period_h = WINK_PERIODIC_INVALID;
            s_slots[i].motor = NULL;
            s_slots[i].encoder = NULL;
        }
    }
}

#else /* WINK_CLOSED_LOOP_MOTOR_HELPER_MAX == 0 */

wink_status_t wink_closed_loop_motor_start_ex(dal_motor_t *motor, 
                                              dal_encoder_t *encoder,
                                              const wink_closed_loop_motor_config_t *cfg,
                                              const wink_bal_opts_t *opts)
{
    (void)motor; (void)encoder; (void)cfg; (void)opts;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_closed_loop_motor_start(dal_motor_t *motor, 
                                           dal_encoder_t *encoder,
                                           const wink_closed_loop_motor_config_t *cfg)
{
    (void)motor; (void)encoder; (void)cfg;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_closed_loop_motor_stop(dal_motor_t *motor)
{
    (void)motor;
    return WINK_OK;
}

wink_status_t wink_closed_loop_motor_set_speed(dal_motor_t *motor, float target_speed)
{
    (void)motor; (void)target_speed;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_closed_loop_motor_get_speed(dal_motor_t *motor, float *out_speed)
{
    (void)motor; (void)out_speed;
    return WINK_ERR_UNAVAILABLE;
}

void wink_closed_loop_motor_reset(void)
{
}

#endif /* WINK_CLOSED_LOOP_MOTOR_HELPER_MAX > 0 */
