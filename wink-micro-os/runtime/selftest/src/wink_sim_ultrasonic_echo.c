/**
 * @file wink_sim_ultrasonic_echo.c
 * @brief Ultrasonic echo-pulse simulator (S10 shadow task) — wraps the
 *        TRIG-ISR + sem + core-pinned mock task + GPIO direction bookkeeping
 *        that devkitc_smoke used to inline.
 *
 * Lives under runtime/selftest/ because it is a bringup/test helper, NOT a
 * product DAL feature. ADR-0023 Stage 2.4: migrated here from samples/common/
 * so that samples/common can shrink to an INTERFACE forwarding-shim library.
 *
 * Design:
 *   - A GPIO ISR on TRIG (rising edge) gives a binary semaphore.
 *   - A dedicated task pinned to core 1 (priority 3 — above background stress,
 *     below the runtime task at prio 5 per [[memory:freertos-same-priority-pulse-stretch]])
 *     waits on the sem, then drives ECHO high for pulse_us, then low.
 *   - ECHO pin is promoted to INPUT_OUTPUT via pal_gpio_set_direction so the
 *     DAL's RMT capture peripheral can listen while the mock task drives the
 *     same wire (host/wasm targets degrade to a no-op because direction set
 *     and GPIO ISR init return UNSUPPORTED there, which we swallow).
 *
 * Timing: HC-SR04 round-trip formula  pulse_us = cm / 0.01715 ≈ 58.3 µs/cm.
 * A 100µs dead-time precedes the echo pulse to model acoustic ring-down.
 *
 * STRICT_NONBLOCKING (ADR-0017 layer-2): the entire TU compiles out under
 * -DWINK_STRICT_NONBLOCKING=1; this helper is never part of a strict
 * non-blocking firmware image.
 */
#define LOG_TAG "sim_echo"

#include "wink_sim_ultrasonic_echo.h"

/* STRICT_NONBLOCKING: entire body compiles out. Header already stubs the
 * declarations so #including TUs see nothing to call. */
#ifndef WINK_STRICT_NONBLOCKING

#include "dal_ultrasonic.h"
#include "wink_status.h"
#include "pal_log.h"
#include "pal_hal.h"
#include "pal_irq.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "internal/pal_test_loopback.h"

#if defined(ESP_PLATFORM)
#include "esp_rom_sys.h"   /* esp_rom_printf for ISR-safe immediate diagnostics */
#endif

/* ADR-0017 layer-1 exception: this file is a bringup/test helper and
 * legitimately calls blocking APIs (pal_os_sem_take / pal_os_task_create /
 * pal_os_busy_wait_us).  Silence -Wdeprecated-declarations for strict
 * builds. */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ── Per-instance state ─────────────────────────────────────────────────── */
typedef struct {
    dal_ultrasonic_t *dev;          /* back-pointer for stop() */
    uint16_t          trig_pin;
    uint16_t          echo_pin;
    uint32_t          pulse_us;     /* echo high-time for simulated_cm */
    pal_os_sem_t      sem;
    volatile uint32_t trig_count;   /* diagnostic: TRIG edges seen */
    bool              armed;
} sim_echo_state_t;

/* Only one simulator active at a time — S10 is a bringup pattern on a single
 * ultrasonic, no multi-instance need in samples.  Static keeps the ISR/Task
 * shim stateless. */
static sim_echo_state_t *s_active = NULL;

/* Diagnostic counters (extern'd for PAL debug reads after timeouts). */
volatile uint32_t g_sim_echo_trig_count = 0;
volatile uint32_t g_sim_echo_pulse_done = 0;

/* ── ISR (TRIG rising edge) ────────────────────────────────────────────── */
static PAL_ISR void sim_trig_isr(void *arg)
{
    (void)arg;
    if (s_active != NULL) {
        s_active->trig_count++;
        g_sim_echo_trig_count = s_active->trig_count;
        /* ISR heartbeat — first 20 edges to cover init + post-S9 steady state.
         * No cap beyond 20 keeps boot-time noise bounded while still letting
         * us see whether the ISR dies after S7 task deletion at ~2s. */
        if (s_active->trig_count <= 20) {
#if defined(ESP_PLATFORM)
            esp_rom_printf("[sim_echo] ISR TRIG#%lu\n",
                           (unsigned long)s_active->trig_count);
#endif
        }
        if (s_active->sem != NULL) {
            WINK_IGNORE_RESULT(pal_os_sem_give_isr(s_active->sem));
        }
    }
}

/* ── Mock echo task ─────────────────────────────────────────────────────── */
static void sim_echo_task(void *arg)
{
    sim_echo_state_t *st = (sim_echo_state_t *)arg;
    (void)arg;
    bool first_pulse = true;
    for (;;) {
        if (pal_os_sem_take(st->sem, WINK_MUTEX_WAIT_FOREVER) != WINK_OK) {
            continue;
        }

        /* Acoustic dead-time (~100µs) before echo fires. */
        pal_os_busy_wait_us(100);

        bool before = false, after_hi = false, after_lo = false;
        WINK_IGNORE_RESULT(pal_gpio_read(st->echo_pin, &before));
        wink_status_t wh_st = pal_gpio_write(st->echo_pin, true);
        WINK_IGNORE_RESULT(pal_gpio_read(st->echo_pin, &after_hi));

        /* First-pulse diagnostic: log readback to verify pad actually drives. */
        if (first_pulse) {
            first_pulse = false;
            LOG_I("sim_echo: first fire wh=%d rd: before=%d after_hi=%d pulse=%luus trig_count=%lu",
                  (int)wh_st, (int)before, (int)after_hi,
                  (unsigned long)st->pulse_us,
                  (unsigned long)st->trig_count);
        }

        pal_os_busy_wait_us(st->pulse_us);
        WINK_IGNORE_RESULT(pal_gpio_write(st->echo_pin, false));
        WINK_IGNORE_RESULT(pal_gpio_read(st->echo_pin, &after_lo));
        if (st->trig_count == 1) {
            /* one extra diag on first pulse: readback after LOW */
            LOG_I("sim_echo: after pulse: after_lo=%d", (int)after_lo);
        }
        /* Pulse-completion heartbeat — first 20 pulses. Proves the TRIG ISR
         * is firing and the mock echo task is actually driving the pin. Critical
         * for diagnosing "RMT receives no edges after S9/S7 teardown". */
        g_sim_echo_pulse_done = st->trig_count;
        if (st->trig_count <= 20) {
            esp_rom_printf("[sim_echo] pulse#%lu done: wh=%d before=%d hi=%d lo=%d pulse=%luus\n",
                           (unsigned long)st->trig_count,
                           (int)wh_st, (int)before, (int)after_hi, (int)after_lo,
                           (unsigned long)st->pulse_us);
        }
    }
    /* Unreachable, but MISRA/fallthrough hygiene: */
    pal_os_task_delete(NULL);
}

/* ── Public API ─────────────────────────────────────────────────────────── */
wink_status_t wink_sim_ultrasonic_echo_start(dal_ultrasonic_t *dev,
                                             float simulated_cm,
                                             uint16_t trig_pin,
                                             uint16_t echo_pin)
{
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Clamp to HC-SR04 legal range (2cm – 400cm). */
    if (simulated_cm < 2.0f)    simulated_cm = 2.0f;
    if (simulated_cm > 400.0f)  simulated_cm = 400.0f;

    static sim_echo_state_t s_state;
    s_state = (sim_echo_state_t){
        .dev       = dev,
        .trig_pin  = trig_pin,
        .echo_pin  = echo_pin,
        .pulse_us  = (uint32_t)(simulated_cm / 0.01715f + 0.5f),
        .trig_count = 0u,
        .armed     = false,
    };

    /* Host/wasm: GPIO ISR init and set_direction will return UNSUPPORTED —
     * that's fine; those targets inject echo timing via sim_set_echo_timing()
     * in unit tests, and we simply return OK so the call site doesn't need
     * #ifdefs.  We create the sem anyway (it's a no-op free on host). */
    s_state.sem = pal_os_sem_create();
    if (s_state.sem == NULL) {
        return WINK_ERR_NO_MEM;
    }

    /* Promote ECHO to INPUT_OUTPUT + GPIO-out signal routing so the mock task
     * can drive it while RMT listens on the same pin. Same mechanism as S9
     * (pal_test_enable_hardware_loopback) -- plain gpio_set_direction is
     * insufficient because RMT init routes the output mux away from the GPIO
     * peripheral. Only done ONCE here; per-pulse re-configuration races with
     * rmt_receive() and breaks captures -- pal_gpio_pulse_in() re-applies
     * loopback routing after any lazy RMT re-init (e.g. after S9 rmt.self_loopback
     * deinit/re-init on a different pin). */
    WINK_IGNORE_RESULT(pal_test_enable_hardware_loopback(echo_pin, echo_pin));

    /* Publish s_active BEFORE arming the ISR so sim_trig_isr sees it. */
    s_active = &s_state;

    wink_status_t st = pal_gpio_enable_interrupt(
        trig_pin, PAL_GPIO_INTR_RISING_EDGE, sim_trig_isr, NULL);
    if (wink_status_is_error(st) && st != WINK_ERR_UNSUPPORTED) {
        s_active = NULL;
        LOG_E("sim_echo: TRIG ISR setup failed: %d", (int)st);
        return st;
    }

    /* Priority 3, core 1 — see design note at file top. */
    st = pal_os_task_create(sim_echo_task, "sim_echo", 4096u,
                            &s_state, /*prio=*/3, PAL_OS_CORE_1, NULL);
    if (wink_status_is_error(st) && st != WINK_ERR_UNSUPPORTED) {
        WINK_IGNORE_RESULT(pal_gpio_disable_interrupt(trig_pin));
        s_active = NULL;
        LOG_E("sim_echo: mock task create failed: %d", (int)st);
        return st;
    }

    s_state.armed = true;
    LOG_I("sim_echo: armed (trig=%u echo=%u sim=%.1fcm pulse=%luus)",
          (unsigned)trig_pin, (unsigned)echo_pin,
          simulated_cm, (unsigned long)s_state.pulse_us);
    return WINK_OK;
}

void wink_sim_ultrasonic_echo_stop(dal_ultrasonic_t *dev)
{
    (void)dev;
    if (s_active == NULL || !s_active->armed) {
        return;
    }
    WINK_IGNORE_RESULT(pal_gpio_disable_interrupt(s_active->trig_pin));
    /* Note: we cannot reliably kill the FreeRTOS task from outside without
     * storing its handle.  For smoke / bringup this is fine — the sim runs
     * for the lifetime of the app.  Setting s_active=NULL prevents the ISR
     * from giving the sem, so the task blocks forever on sem_take (no CPU
     * use). */
    s_active->armed = false;
    s_active = NULL;
}

#endif /* WINK_STRICT_NONBLOCKING */
