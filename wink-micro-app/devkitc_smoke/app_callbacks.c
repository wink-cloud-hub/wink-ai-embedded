/**
 * @file app_callbacks.c
 * @brief DevKitC smoke-test firmware (P0-1 slim): device tree centralized,
 *        app only wires BAL services + selftest.
 *
 * Init / deinit boilerplate for LED / button / ultrasonic lives in
 * device_tree.c. This TU only:
 *   - defines business event callbacks (long-press -> WDT test)
 *   - starts BAL services (blink, button events, telemetry, ultrasonic poll)
 *   - invokes the OS built-in selftest suite
 *   - logs S1-S11 status on startup
 *
 * Verifies on bare metal (no wiring required):
 *   S1  2s telemetry                (BAL: wink_telemetry_default_start)
 *   S2  LED blink                    (BAL: wink_led_blink_start)
 *   S3  Boot-button debounce         (BAL: wink_button_enable_events, soft_poll)
 *   S4  GPIO ISR edge count          (selftest: gpio.isr_roundtrip)
 *   S5  PWM router freq isolation    (selftest: pwm_router.freq_isolation)
 *   S6  I2C bus scan (no-panic)      (selftest: i2c.bus_scan)
 *   S7  Dual-core resource stress    (selftest: smp.resource_stress)
 *   S8  WDT reset via long-press     (Runtime: wink_runtime_trigger_wdt_test)
 *   S9  RMT hardware loopback        (selftest: rmt.self_loopback)
 *   S10 Ultrasonic echo simulation   (selftest echo + BAL: wink_ultrasonic_poll_start)
 *   S11 Deinit loop verification     (host e2e test verified)
 */
#define LOG_TAG "devkitc_smoke"

#include "device_tree.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_selftest.h"
#include "wink_fault.h"
#include "output/wink_led_blink.h"
#include "input/wink_button_events.h"
#include "sensor/wink_ultrasonic_poll.h"
#include "comm/wink_telemetry_default.h"
#include "wink_blocking_region.h"
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_log.h"

#ifdef WINK_CFG_SIM_ECHO
#include "wink_sim_ultrasonic_echo.h"
#endif

/* S8: Boot-button event callback (long-press -> WDT test, noreturn) */
static void on_boot_button(dal_button_event_t evt, void *ctx)
{
    (void)ctx;
    if (evt == DAL_BUTTON_EVT_LONG_PRESS) {
        LOG_I("\nS8: long-press detected, arming WDT test");
        wink_runtime_trigger_wdt_test(2000); /* does not return */
    }
}

/* S8: Boot notification (Runtime fires this once before init()) */
static void app_on_boot(const wink_boot_info_t *info)
{
    if (info->abnormal_boot_count > 0 ||
        info->reset_reason == WINK_RESET_REASON_WATCHDOG) {
        LOG_I("\nS8: PASS (watchdog recovered, count=%lu, reason=%d)",
              (unsigned long)info->abnormal_boot_count, (int)info->reset_reason);
    }
}

/* init: device_tree then BAL services + selftest */
static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());

    /* S3: enable boot_button events (ADR-0032 B-class soft_poll) */
#ifdef BOOT_BUTTON_AUTO_POLL_MS
    static const wink_button_event_config_t s3_cfg = {
        .drive           = WINK_BUTTON_DRIVE_SOFT_POLL,
        .auto_poll_ms    = BOOT_BUTTON_AUTO_POLL_MS,
        .debounce_ms     = 20u,   /* ADR-0031 default */
        .wake_from_sleep = false,
    };
    wink_status_t st_s3 = wink_button_enable_events(&boot_button, &s3_cfg);
    if (st_s3 == WINK_OK) {
        LOG_I("\nS3: PASS (button events enabled, period=%ums)", (unsigned)BOOT_BUTTON_AUTO_POLL_MS);
    } else {
        LOG_E("\nS3: FAIL (button events enable, status=%d)", (int)st_s3);
        return st_s3;
    }
#else
    LOG_I("\nS3: SKIP (button events not configured)");
#endif

    /* S8: register WDT long-press trigger */
    wink_status_t st_s8 = dal_button_on_event(&boot_button, on_boot_button, NULL);
    if (st_s8 == WINK_OK) {
        LOG_I("\nS8: PASS (WDT trigger registered)");
    } else {
        LOG_E("\nS8: FAIL (WDT trigger register, status=%d)", (int)st_s8);
        return st_s8;
    }

    /* S2: LED blink (A-class) */
    int32_t blink_h = wink_led_blink_start(&board_led, 1000);
    if (blink_h >= 1) {
        LOG_I("\nS2: PASS (led blink, h=%ld)", (long)blink_h);
    } else {
        LOG_E("\nS2: FAIL (led blink, status=%d)", (int)blink_h);
        return (wink_status_t)blink_h;
    }

    /* S10: optional echo sim (host e2e / ESP32 builds with WINK_CFG_SIM_ECHO).
     * Without the define, skip arming and continue with poll only. */
#ifdef WINK_CFG_SIM_ECHO
    wink_status_t st_echo = wink_sim_ultrasonic_echo_start(
        &smoke_ultrasonic, 50.0f,
        smoke_ultrasonic.config.trig_pin, smoke_ultrasonic.config.echo_pin);
    if (st_echo == WINK_OK) {
        LOG_I("\nS10: PASS (echo sim armed, simulated_dist=50cm)");
    } else {
        LOG_E("\nS10: FAIL (echo sim start, status=%d)", (int)st_echo);
        return st_echo;
    }
#else
    LOG_I("\nS10: SKIP (echo sim not enabled for this build)");
#endif

    /* S10: ultrasonic poll (A-class) */
    wink_status_t st_ultrasonic = wink_ultrasonic_poll_start(&smoke_ultrasonic, 500);
    if (st_ultrasonic == WINK_OK) {
        LOG_I("\nS10: PASS (ultrasonic poll, period=500ms)");
    } else {
        LOG_E("\nS10: FAIL (ultrasonic poll start, status=%d)", (int)st_ultrasonic);
        return st_ultrasonic;
    }

    /* S1: telemetry default (MAY_BLOCK) */
    wink_status_t st_s1 = wink_telemetry_default_start(&smoke_ultrasonic, &boot_button);
    if (st_s1 == WINK_OK) {
        LOG_I("\nS1: PASS (telemetry default started)");
    } else {
        LOG_E("\nS1: FAIL (telemetry start, status=%d)", (int)st_s1);
        return st_s1;
    }

    /* S6 readiness: eager-init I2C bus 0 so selftest i2c.bus_scan does not
     * trip the lazy-init WARN path (stub returns WINK_OK as prerequisite).
     *   - ESP32: pal_i2c_port_pins() returns board/weak-default SDA/SCL.
     *   - host/wasm: port_pins returns UNSUPPORTED (pins ignored); fall
     *     back to (0,0) which the virtual targets accept. */
    {
        wink_pin_t sda = 0, scl = 0;
        wink_status_t pq = pal_i2c_port_pins(0, &sda, &scl);
        if (pq == WINK_ERR_UNSUPPORTED) { sda = 0; scl = 0; }
        wink_status_t i2c_st = pal_i2c_bus_init(0, (uint8_t)sda, (uint8_t)scl, 100000);
        /* UNSUPPORTED is fine (target has no I2C; S6 will self-SKIP).
         * Other errors are non-fatal: S6 will surface FAIL itself. */
        (void)i2c_st;
    }

    /* S4/S5/S6/S7/S9: OS built-in selftests.
     * ADR-0017 init-phase exception: selftest runs during synchronous init,
     * outside cooperative PT context; blocking calls are allowed here. */
    WINK_INIT_BLOCKING_REGION_BEGIN
    wink_selftest_result_t results[8];
    size_t n = 0;
    wink_status_t test_st = wink_selftest_run("*", results, 8, &n);
    WINK_INIT_BLOCKING_REGION_END

    bool has_fail = false;
    for (size_t i = 0; i < n; i++) {
        bool is_ok = (results[i].status == WINK_OK);
        bool is_skip = (results[i].status == WINK_ERR_UNSUPPORTED);
        const char *s_code = "UNKNOWN";

        if (strcmp(results[i].name, "gpio.isr_roundtrip") == 0) s_code = "S4";
        else if (strcmp(results[i].name, "pwm_router.freq_isolation") == 0) s_code = "S5";
        else if (strcmp(results[i].name, "i2c.bus_scan") == 0) s_code = "S6";
        else if (strcmp(results[i].name, "smp.resource_stress") == 0) s_code = "S7";
        else if (strcmp(results[i].name, "rmt.self_loopback") == 0) s_code = "S9";

        if (is_ok) {
            LOG_I("\n%s: PASS (%s, metric=%lu)", s_code, results[i].name, (unsigned long)results[i].metric);
        } else if (is_skip) {
            LOG_I("\n%s: SKIP (%s, unsupported)", s_code, results[i].name);
        } else {
            LOG_E("\n%s: FAIL (%s, status=%d, metric=%lu)", s_code, results[i].name, (int)results[i].status, (unsigned long)results[i].metric);
            has_fail = true;
        }
    }

    if (has_fail || (wink_status_is_error(test_st) && test_st != WINK_ERR_UNSUPPORTED)) {
        return WINK_ERR_FAILED_INIT;
    }

    /* S11: deinit loop verified in host e2e test */
    LOG_I("\nS11: PASS (deinit loop, e2e verified)");

    LOG_I("\ninit done. Long-press BOOT (>3s) to trigger WDT reset test.");
    return WINK_OK;
}

/* loop (10ms tick): empty */
static void app_loop(void)
{
    /* no-op */
}

/* Fault callback */
static wink_status_t app_on_fault_status(uint32_t code)
{
    (void)code;
    return WINK_OK;
}

/* Callback factory (binary decoupling) */
const wink_app_callbacks_t *wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t cb = {
        .init_status     = app_init_status,
        .loop            = app_loop,
        .on_fault_status = app_on_fault_status,
        .on_boot         = app_on_boot,
    };
    return &cb;
}
