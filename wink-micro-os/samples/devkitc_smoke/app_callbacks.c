/**
 * @file app_callbacks.c
 * @brief DevKitC smoke-test firmware (rewrite Wave 6): init → helper → selftest
 *        in ~120 lines.  All ISR/task/sem/12-step-RMT/telemetry boilerplate
 *        that used to live here has moved into Runtime selftest or
 *        samples/common helpers.
 *
 * Verifies on bare metal (no wiring required):
 *   S1  2s telemetry                (common: wink_default_telemetry_start)
 *   S2  LED blink                    (common: wink_led_blink_start)
 *   S3  Boot-button debounce         (DAL: dal_button_poll)
 *   S4  GPIO ISR edge count          (DAL: dal_button_enable_isr_counter)
 *   S5  PWM router freq isolation    (selftest: pwm_router.freq_isolation)
 *   S6  I2C bus scan (no-panic)      (selftest: i2c.bus_scan)
 *   S7  Dual-core resource stress    (selftest: smp.resource_stress)
 *   S8  WDT reset via long-press     (Runtime: wink_runtime_trigger_wdt_test)
 *   S9  RMT hardware loopback        (selftest: rmt.self_loopback)
 *   S10 Ultrasonic echo simulation   (common: wink_sim_ultrasonic_echo_start)
 */
#define LOG_TAG "devkitc_smoke"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_selftest.h"
#include "wink_fault.h"
#include "wink_actuator_registry.h"
#include "wink_blink_helper.h"
#include "wink_default_telemetry.h"
#include "wink_sim_ultrasonic_echo.h"
#include "pal_log.h"

/* ADR-0017 layer-1 exception: app init legitimately calls WINK_BLOCKING APIs
 * (selftest i2c_scan/rmt_wait sleep, ultrasonic request_measurement). */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ── Pin definitions for S10 ultrasonic sim (consumed by common helper) ──── */
#define SMOKE_TRIG_PIN  18u
#define SMOKE_ECHO_PIN  19u

/* ── Per-app state ──────────────────────────────────────────────────────── */
static dal_ultrasonic_t s_sonar;

/* ── S8: Boot-button event callback (long-press → WDT test, noreturn) ───── */
static void on_boot_button(dal_button_event_t evt, void *ctx)
{
    (void)ctx;
    if (evt == DAL_BUTTON_EVT_LONG_PRESS) {
        LOG_I("S8: long-press detected, arming WDT test");
        wink_runtime_trigger_wdt_test(2000); /* does not return */
    }
}

/* ── S8: Boot notification (Runtime fires this once before init()) ──────── */
static void app_on_boot(const wink_boot_info_t *info)
{
    if (info->abnormal_boot_count > 0 ||
        info->reset_reason == WINK_RESET_REASON_WATCHDOG) {
        LOG_I("watchdog: PASS (recovered after abnormal reset, count=%lu, reason=%d)",
              (unsigned long)info->abnormal_boot_count, (int)info->reset_reason);
    }
}

/* ── S10: periodic ultrasonic measurement (runs in its own task) ────────── */
static void sonar_poll_task(void *ctx)
{
    WINK_IGNORE_RESULT(dal_ultrasonic_request_measurement((dal_ultrasonic_t *)ctx));
}

/* ── Actuator safe-off thunks (ISO C: no nested functions) ──────────────── */
WINK_DEFINE_ACTUATOR_THUNK(board_led_safe_off, dal_led_off, dal_led_t)

/* ── init: declare devices, wire helpers, run selftest ──────────────────── */
static void app_init(void)
{
    /* ── S2/S3: LED + boot button ─────────────────────────────────────── */
    static const dal_led_config_t led_cfg = {
        .owner = "board_led", .pin = BOARD_LED_PIN, .active_high = true
    };
    WINK_CHECK(dal_led_init(&board_led, &led_cfg), WINK_FAULT_DAL_LED);
    WINK_IGNORE_RESULT(wink_actuator_register(board_led_safe_off, &board_led));

    static const dal_button_config_t btn_cfg = {
        .owner = "boot_button", .pin = BOOT_BUTTON_PIN, .active_low = true
    };
    WINK_CHECK(dal_button_init(&boot_button, &btn_cfg), WINK_FAULT_DAL_BUTTON);
    WINK_IGNORE_RESULT(dal_button_on_event(&boot_button, on_boot_button, NULL));
    WINK_IGNORE_RESULT(dal_button_set_long_press_ms(&boot_button, 3000));
    WINK_IGNORE_RESULT(dal_button_enable_isr_counter(&boot_button));          /* S4 */

    WINK_IGNORE_RESULT(wink_led_blink_start(&board_led, 1000));                /* S2 */

    /* ── S10: Ultrasonic + ECHO-loopback sim (samples/common helper) ──── */
    static const dal_ultrasonic_config_t sonar_cfg = {
        .owner = "smoke_sonar",
        .trig_pin = SMOKE_TRIG_PIN, .echo_pin = SMOKE_ECHO_PIN, .use_rmt = true,
    };
    WINK_CHECK(dal_ultrasonic_init(&s_sonar, &sonar_cfg), WINK_FAULT_DAL_ULTRASONIC);
    WINK_IGNORE_RESULT(wink_sim_ultrasonic_echo_start(
        &s_sonar, 50.0f, SMOKE_TRIG_PIN, SMOKE_ECHO_PIN));
    WINK_IGNORE_RESULT(wink_runtime_spawn_periodic(
        "sonar_poll", 2048, 500, sonar_poll_task, &s_sonar, 1, PAL_OS_CORE_ANY));

    /* ── S1: Default telemetry (samples/common helper) ────────────────── */
    WINK_IGNORE_RESULT(wink_default_telemetry_start(&s_sonar, &boot_button));

    /* ── S5/S6/S7/S9/S4-isr: OS built-in selftest (one call).
     *    selftest_core already logs PASS/SKIP/FAIL per entry; we only need to
     *    raise a fault if anything hard-failed (UNSUPPORTED = skip ≠ fail). */
    wink_selftest_result_t results[8];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", results, 8, &n));
    for (size_t i = 0; i < n; i++) {
        if (results[i].status != WINK_OK &&
            results[i].status != WINK_ERR_UNSUPPORTED) {
            LOG_E("%s: FAIL (metric=%lu)", results[i].name,
                  (unsigned long)results[i].metric);
            wink_trace_fault(WINK_FAULT_APP(1));
        }
    }

    LOG_I("init done. Long-press BOOT (>3s) to trigger WDT reset test.");
}

/* ── loop (10ms tick): button poll; everything else is background tasks ── */
static void app_loop(void)
{
    WINK_IGNORE_RESULT(dal_button_poll(&boot_button));
}

/* ── Fault callback (Runtime already ran safe-off-all before this) ─────── */
static void app_on_fault(uint32_t code)
{
    (void)code;
}

/* ── Callback factory (binary decoupling) ──────────────────────────────── */
const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init     = app_init,
        .loop     = app_loop,
        .on_fault = app_on_fault,
        .on_boot  = app_on_boot,
    };
    return &cb;
}
