/* SPDX-License-Identifier: Apache-2.0
 * PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706 Task 5: appliance safety host
 * vehicle for the production mcs51_health_pot firmware.
 *
 * mcs51_health_pot ships wasm-sim only (its app CMakeLists produces no host
 * target), so this driver links the transpiled REAL app source into the same
 * cooperative fiber runtime used by every mcs51 host test. The physical safety
 * triplet is asserted directly on model state:
 *   - relay latch   : P2.0 bit in the SFR shadow (sbit HEATER);
 *   - state/fault   : the app XDATA telemetry slots (0x10..0x15);
 *   - display frame : P1 write hook ring, scanned as 4-digit frames.
 *
 * Scenario injection: each wink_runtime_run() call is one power cycle (the
 * framework init resets the models and restarts the app fiber from main()).
 * The post-init hook re-arms the AN0 injection rail after that reset and
 * registers a cooperative injector task: the Keil super-loop never returns to
 * the runtime tick loop, so scheduled pin drives must come from a peer
 * scheduler task sleeping 10 ms between steps (one step per master tick).
 *
 * Covered:
 *   - cold boot standby + ON -> HEAT smoke (vehicle sanity);
 *   - POST stuck-key suppression at power-up and its release recovery;
 *   - boot-time hot-probe (>= 45 C) 60 s cooldown latch, denial, display
 *     alternation and expiry;
 *   - runtime E-03 -> acknowledge -> cooldown denial -> expiry -> HEAT.
 */
#include <stdint.h>
#include <stdio.h>

#include "wink_runtime.h"
#include "wink_app.h"
#include "wink_status.h"
#include "mcs51_adc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_trap.h"
#include "wink_sim_scheduler.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);

/* ── App observability map (health_pot.c telemetry slots / pins / font) ──── */
#define TLM_TEMP         0x0010u
#define TLM_STATE        0x0011u
#define TLM_HEATER       0x0012u
#define TLM_FAULT        0x0013u
#define P1_SFR_ADDR      0x90u
#define P2_SFR_ADDR      0xA0u
#define HEATER_BIT       0x01u
#define BTN_ONOFF_PIN    4u
#define BTN_FUNC_PIN     5u

#define ST_OFF           0u
#define ST_HEAT          1u
#define ST_FAULT         3u

/* health_pot NTC anchors (ntc_lut_raw/ntc_lut_temp): 241 -> 25 C, 131 -> 40 C
 * (stable => slope dry-fire in ~20 s), 89 -> 50 C (hot power-up). */
#define COLD_CODE        241u
#define DRY_CODE         131u
#define HOT_CODE         89u

/* Display font patterns (common cathode): 'C','O','L','-','0','5'. */
#define SEG_C            0x39u
#define SEG_O            0x3Fu
#define SEG_L            0x38u
#define SEG_DASH         0x40u
#define SEG_0            0x3Fu
#define SEG_5            0x6Du

#define ON_PRESS_TICK    3u
#define ON_RELEASE_TICK  6u

#define P1_LOG_SIZE      64u
#define MAX_PIN_STEPS    16u

typedef struct {
    uint32_t tick;
    uint8_t  pin;
    uint8_t  level; /* 0 = drive low (pressed), 1 = drive high (released) */
} pin_step_t;

static pin_step_t s_pin_steps[MAX_PIN_STEPS];
static unsigned   s_pin_step_count;
static uint32_t   s_tick;
static uint8_t    s_pin_levels[32];
static uint16_t   s_boot_adc = COLD_CODE;
static uint8_t    s_boot_onoff_low;

static uint8_t    s_p1_log[P1_LOG_SIZE];
static uint32_t   s_p1_count;

static int g_fails;

static void check(int cond, const char *msg) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", msg);
        g_fails++;
    }
}

static void scenario_reset(void) {
    s_pin_step_count = 0u;
    s_tick = 0u;
    s_boot_onoff_low = 0u;
    s_p1_count = 0u;
    for (unsigned i = 0u; i < 32u; i++) {
        s_pin_levels[i] = 1u;
    }
}

static void schedule_pin(uint32_t tick, uint8_t pin, uint8_t level) {
    if (s_pin_step_count >= MAX_PIN_STEPS) {
        return;
    }
    s_pin_steps[s_pin_step_count].tick = tick;
    s_pin_steps[s_pin_step_count].pin = pin;
    s_pin_steps[s_pin_step_count].level = level;
    s_pin_step_count++;
}

/* P1 display write hook: the Timer0 scan writes one digit per 10 ms tick, so
 * four consecutive writes form one 4-digit frame. */
static void p1_write_hook(struct Mcu51Context *ctx, uint8_t addr,
                          uint8_t old_val, uint8_t new_val) {
    (void)ctx;
    (void)addr;
    (void)old_val;
    s_p1_log[s_p1_count % P1_LOG_SIZE] = new_val;
    s_p1_count++;
}

static uint8_t p1_at(uint32_t idx) {
    uint32_t start = (s_p1_count > P1_LOG_SIZE) ? (s_p1_count % P1_LOG_SIZE) : 0u;
    return s_p1_log[(start + idx) % P1_LOG_SIZE];
}

static void sort4(uint8_t *v) {
    for (unsigned i = 0u; i < 3u; i++) {
        for (unsigned j = 0u; j + 1u < 4u - i; j++) {
            if (v[j] > v[j + 1u]) {
                uint8_t t = v[j];
                v[j] = v[j + 1u];
                v[j + 1u] = t;
            }
        }
    }
}

/* True when any consecutive 4-write window equals the expected multiset. */
static int frame_present(const uint8_t want[4]) {
    uint8_t w[4] = { want[0], want[1], want[2], want[3] };
    sort4(w);
    if (s_p1_count < 4u) {
        return 0;
    }
    uint32_t n = (s_p1_count > P1_LOG_SIZE) ? P1_LOG_SIZE : s_p1_count;
    for (uint32_t i = 0u; i + 3u < n; i++) {
        uint8_t g[4] = { p1_at(i), p1_at(i + 1u), p1_at(i + 2u), p1_at(i + 3u) };
        sort4(g);
        if (g[0] == w[0] && g[1] == w[1] && g[2] == w[2] && g[3] == w[3]) {
            return 1;
        }
    }
    return 0;
}

/* Cooperative injector task: one step per 10 ms master tick. The Keil
 * super-loop never returns to the runtime tick loop, so this peer task is the
 * only way to apply time-based input changes inside a single power cycle. */
static void injector_task(void *arg) {
    (void)arg;
    while (1) {
        unsigned i;
        for (i = 0u; i < s_pin_step_count; i++) {
            if (s_pin_steps[i].tick == s_tick) {
                wink_mcs51_host_set_ext_pin(s_pin_steps[i].pin, s_pin_steps[i].level);
            }
        }
        s_tick++;
        wink_app_delay_ms(10u);
    }
}

/* Framework post-init hook: runs after the ADR-0077 power-on reset/port seed
 * and the ADC rail reset on every run, so the scripted boot state survives;
 * sim_scheduler_reset() wiped all tasks, so re-register the injector here. */
static void boot_inject(void) {
    uint32_t task_id;
    s_tick = 0u;
    s_p1_count = 0u;
    mcs51_adc_set_value(0u, s_boot_adc);   /* AN0 -> Pin 0 rail key */
    for (unsigned i = 0u; i < 32u; i++) {
        wink_mcs51_host_set_ext_pin((uint16_t)i, s_pin_levels[i]);
    }
    wink_mcs51_host_set_ext_pin(BTN_ONOFF_PIN,
                                s_boot_onoff_low ? 0u : s_pin_levels[BTN_ONOFF_PIN]);
    mcs51_trap_register_sfr_write(P1_SFR_ADDR, p1_write_hook);
    (void)sim_scheduler_register(injector_task, NULL, "hp_inject", 5,
                                 PAL_OS_CORE_ANY, 32u * 1024u, &task_id);
}

static uint8_t tlm_state(void) {
    return mcs51_get_context()->xdata_shadow[TLM_STATE];
}

static uint8_t tlm_temp(void) {
    return mcs51_get_context()->xdata_shadow[TLM_TEMP];
}

static uint8_t tlm_heater(void) {
    return mcs51_get_context()->xdata_shadow[TLM_HEATER];
}

static uint8_t tlm_fault(void) {
    return mcs51_get_context()->xdata_shadow[TLM_FAULT];
}

static uint8_t heater_pin(void) {
    return (uint8_t)(mcs51_get_context()->sfr_shadow[P2_SFR_ADDR] & HEATER_BIT);
}

static int run_scenario(uint32_t ticks) {
    wink_status_t st = wink_runtime_run(wink_app_get_callbacks(), ticks);
    if (st != WINK_OK) {
        printf("[mcs51] FAIL: runtime run returned %d\n", (int)st);
        return 1;
    }
    return 0;
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (cb == NULL || cb->loop == NULL) {
        printf("[mcs51] FAIL: callbacks/loop not bound\n");
        return 1;
    }

    /* On-chip CMS8S silicon: the app drives ADC/BUZ/LED peripherals. */
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);

    mcs51_framework_set_post_init_hook(boot_inject);

    /* ── 1) Cold standby: no phantom event, relay open, ST_OFF ───────────── */
    scenario_reset();
    s_boot_adc = COLD_CODE;
    if (run_scenario(40u)) {
        return 1;
    }
    check(tlm_state() == ST_OFF, "cold boot: state != ST_OFF");
    check(tlm_heater() == 0u, "cold boot: heater latch != 0");
    check(heater_pin() == 0u, "cold boot: P2.0 != 0");
    check(tlm_fault() == 0u, "cold boot: fault != 0");

    /* ── 2) Smoke: ON press at 30 ms -> HEAT with relay energised ────────── */
    scenario_reset();
    s_boot_adc = COLD_CODE;
    schedule_pin(ON_PRESS_TICK, BTN_ONOFF_PIN, 0u);
    schedule_pin(ON_RELEASE_TICK, BTN_ONOFF_PIN, 1u);
    if (run_scenario(80u)) {
        return 1;
    }
    check(tlm_state() == ST_HEAT, "ON press: state != ST_HEAT");
    check(tlm_heater() == 1u, "ON press: heater latch != 1");
    check(heater_pin() == 1u, "ON press: P2.0 != 1");
    check(tlm_fault() == 0u, "ON press: fault != 0");

    /* ── 3) POST stuck-key: key held low at power-up generates no event ──── */
    scenario_reset();
    s_boot_adc = COLD_CODE;
    s_boot_onoff_low = 1u;
    if (run_scenario(100u)) {
        return 1;
    }
    check(tlm_state() == ST_OFF, "POST jam: phantom start (state != ST_OFF)");
    check(tlm_heater() == 0u, "POST jam: heater latch != 0");
    check(heater_pin() == 0u, "POST jam: P2.0 != 0");

    /* ── 4) POST recovery: release + fresh press is honoured ────────────── */
    scenario_reset();
    s_boot_adc = COLD_CODE;
    s_boot_onoff_low = 1u;
    schedule_pin(50u, BTN_ONOFF_PIN, 1u);   /* release */
    schedule_pin(80u, BTN_ONOFF_PIN, 0u);   /* fresh press */
    if (run_scenario(140u)) {
        return 1;
    }
    check(tlm_state() == ST_HEAT, "POST recovery: state != ST_HEAT");
    check(heater_pin() == 1u, "POST recovery: P2.0 != 1");

    /* ── 5) Hot power-up latch: >= 45 C re-locks 60 s + COOL display ─────── */
    scenario_reset();
    s_boot_adc = HOT_CODE;
    if (run_scenario(450u)) {
        return 1;
    }
    check(tlm_temp() == 50u, "hot boot: telemetry temp != 50 C");
    check(tlm_state() == ST_OFF, "hot boot: state != ST_OFF");
    check(tlm_heater() == 0u, "hot boot: heater latch != 0");
    check(heater_pin() == 0u, "hot boot: P2.0 != 0");
    {
        const uint8_t want_cool[4] = { SEG_C, SEG_O, SEG_O, SEG_L };
        const uint8_t want_temp[4] = { SEG_DASH, SEG_5, SEG_0, SEG_DASH };
        check(frame_present(want_cool), "hot boot: COOL frame never displayed");
        check(frame_present(want_temp), "hot boot: temperature frame never displayed");
    }

    /* Denied start while the boot lock is active. */
    scenario_reset();
    s_boot_adc = HOT_CODE;
    schedule_pin(ON_PRESS_TICK, BTN_ONOFF_PIN, 0u);
    schedule_pin(ON_RELEASE_TICK, BTN_ONOFF_PIN, 1u);
    if (run_scenario(80u)) {
        return 1;
    }
    check(tlm_state() == ST_OFF, "hot boot: start accepted during lock");
    check(heater_pin() == 0u, "hot boot: P2.0 energised during lock");

    /* Lock expiry: press at 63 s (boot latched at 0 s) releases to HEAT. */
    scenario_reset();
    s_boot_adc = HOT_CODE;
    schedule_pin(6300u, BTN_ONOFF_PIN, 0u);
    schedule_pin(6320u, BTN_ONOFF_PIN, 1u);
    if (run_scenario(6700u)) {
        return 1;
    }
    check(tlm_state() == ST_HEAT, "hot boot: lock never expired");
    check(heater_pin() == 1u, "hot boot: P2.0 != 1 after expiry");

    /* ── 6) Runtime E-03: latch, acknowledge, denial, expiry ─────────────── */
    /* 6a) Stable 40 C heating trips the slope detector around 20-21 s. */
    scenario_reset();
    s_boot_adc = DRY_CODE;
    schedule_pin(ON_PRESS_TICK, BTN_ONOFF_PIN, 0u);
    schedule_pin(ON_RELEASE_TICK, BTN_ONOFF_PIN, 1u);
    if (run_scenario(3000u)) {
        return 1;
    }
    check(tlm_state() == ST_FAULT, "dryfire: state != ST_FAULT");
    check(tlm_fault() == 3u, "dryfire: fault != E-03");
    check(heater_pin() == 0u, "dryfire: P2.0 energised");

    /* 6b) Acknowledge at 24 s then re-press at 26 s: still locked out. */
    scenario_reset();
    s_boot_adc = DRY_CODE;
    schedule_pin(ON_PRESS_TICK, BTN_ONOFF_PIN, 0u);
    schedule_pin(ON_RELEASE_TICK, BTN_ONOFF_PIN, 1u);
    schedule_pin(2400u, BTN_ONOFF_PIN, 0u);   /* fault acknowledge */
    schedule_pin(2420u, BTN_ONOFF_PIN, 1u);
    schedule_pin(2600u, BTN_ONOFF_PIN, 0u);   /* denied restart */
    schedule_pin(2620u, BTN_ONOFF_PIN, 1u);
    if (run_scenario(3600u)) {
        return 1;
    }
    check(tlm_state() == ST_OFF, "cooldown: restart accepted inside 60 s lock");
    check(tlm_heater() == 0u, "cooldown: heater latch != 0");
    check(heater_pin() == 0u, "cooldown: P2.0 != 0");

    /* 6c) Cooldown expiry at ~81 s: a press at 83 s starts heating again. */
    scenario_reset();
    s_boot_adc = DRY_CODE;
    schedule_pin(ON_PRESS_TICK, BTN_ONOFF_PIN, 0u);
    schedule_pin(ON_RELEASE_TICK, BTN_ONOFF_PIN, 1u);
    schedule_pin(2400u, BTN_ONOFF_PIN, 0u);   /* fault acknowledge */
    schedule_pin(2420u, BTN_ONOFF_PIN, 1u);
    schedule_pin(8300u, BTN_ONOFF_PIN, 0u);   /* after lock expiry */
    schedule_pin(8320u, BTN_ONOFF_PIN, 1u);
    if (run_scenario(8600u)) {
        return 1;
    }
    check(tlm_state() == ST_HEAT, "cooldown: lock never expired (state != HEAT)");
    check(heater_pin() == 1u, "cooldown: P2.0 != 1 after expiry");

    mcs51_framework_set_post_init_hook(NULL);

    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: mcs51_health_pot safety host vehicle — cold standby, "
           "ON->HEAT, POST suppression + recovery, hot-boot 60 s lock with "
           "COOL display, runtime E-03 lock/ack/deny/expiry\n");
    return 0;
}
