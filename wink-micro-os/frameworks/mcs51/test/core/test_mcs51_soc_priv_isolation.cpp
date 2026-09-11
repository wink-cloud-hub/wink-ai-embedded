// SPDX-License-Identifier: Apache-2.0
// Stage2 S2-1 (PLAN-20260911-MCS51-S2): soc_priv scheme-A isolation.
//
// Two contexts on different instance slots must never share chip-private
// state (XSFR-driven ADC conversions land in different pool slots);
// instance_index out-of-range clamps; per-context gpio hooks are isolated
// across families; classic binds soc_priv = nullptr.
#include <stdint.h>
#include <stdio.h>

#include "cms8s_adc.h"
#include "cms8s_buzzer.h"
#include "mcs51_adc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_proxy.hpp"

namespace {

WinkSfr ADCON0(0xDFu);
WinkSfr ADCON1(0xDEu);
WinkSfr ADCCHS(0xD9u);

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", msg);
        ++g_fails;
    }
}

void seed_common(void) {
    // LDO on + VSEL=3V + AN0 mux on the ACTIVE context; clear rail.
    mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
    mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
    mcs51_adc_reset();
    cms8s_adc_model_reset(mcs51_get_context());
}

void convert_an0_right(void) {
    ADCCHS = 0u;
    ADCON1 = 0x80u;  // ADEN
    ADCON0 = 0x40u;  // ADFM right
    ADCON0 = static_cast<unsigned>((uint8_t)ADCON0 | 0x02u);  // ADGO
}

bool hook_a_called = false;
bool hook_b_called = false;

bool hook_a(struct Mcu51Context *, uint16_t) {
    hook_a_called = true;
    return true;
}

bool hook_b(struct Mcu51Context *, uint16_t) {
    hook_b_called = true;
    return true;
}

static Mcu51Context s_ctx_a = {};
static Mcu51Context s_ctx_b = {};
static Mcu51Context s_ctx_clamp = {};

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
extern "C" void cms8s_soc_bind(struct Mcu51Context *ctx);

int main(void) {
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);

    // ── instance slots: 0/1 + out-of-range clamp ─────────────────────────
    mcs51_context_init(&s_ctx_a, 0u);
    mcs51_context_init(&s_ctx_b, 1u);
    mcs51_context_init(&s_ctx_clamp, 99u);
    check(s_ctx_a.instance_index == 0u, "slot A must be 0");
    check(s_ctx_b.instance_index == 1u, "slot B must be 1");
    check(s_ctx_clamp.instance_index == MCS51_MAX_INSTANCES - 1u,
          "out-of-range idx must clamp to last slot");
    mcs51_context_reset(&s_ctx_clamp);  // clamped slot resets cleanly

    // ── review hardening: wild index never escapes the pool ─────────────
    // A hand-built context bypassing init (idx 99, NDEBUG-proof path):
    // bind must contain it to the last slot — functional, no OOB.
    static Mcu51Context s_ctx_wild = {};
    s_ctx_wild.instance_index = 99u;
    mcs51_set_active_context(&s_ctx_wild);
    cms8s_soc_bind(&s_ctx_wild);
    check(s_ctx_wild.soc_priv != nullptr, "wild idx must bind, not crash");
    check(s_ctx_wild.soc_priv == s_ctx_clamp.soc_priv,
          "wild idx must alias the clamped last slot deterministically");

    // ── per-instance reset binds distinct pool slots ─────────────────────
    mcs51_set_active_context(&s_ctx_a);
    mcs51_context_reset(&s_ctx_a);
    mcs51_set_active_context(&s_ctx_b);
    mcs51_context_reset(&s_ctx_b);
    check(s_ctx_a.soc_priv != nullptr, "CMS8S slot A must bind soc_priv");
    check(s_ctx_b.soc_priv != nullptr, "CMS8S slot B must bind soc_priv");
    check(s_ctx_a.soc_priv != s_ctx_b.soc_priv,
          "slots A/B must not share the pool slot");

    // ── conversions on A never leak into B ───────────────────────────────
    mcs51_set_active_context(&s_ctx_a);
    seed_common();
    mcs51_adc_set_value(0, 0x0ABCu);  // Pin 0 injection (AN0)
    convert_an0_right();
    check(cms8s_adc_conversion_count() == 1u, "A: conversion must count");
    mcs51_set_active_context(&s_ctx_b);
    seed_common();
    check(cms8s_adc_conversion_count() == 0u,
          "B: pool slot must start clean (no A leakage)");
    mcs51_adc_set_value(0, 0x0123u);
    convert_an0_right();
    check(cms8s_adc_conversion_count() == 1u, "B: conversion must count");
    mcs51_set_active_context(&s_ctx_a);
    check(cms8s_adc_conversion_count() == 1u,
          "A: count must be untouched by B");

    // ── per-context hooks isolated across families ───────────────────────
    s_ctx_a.gpio_hooks.may_drive = hook_a;
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    mcs51_set_active_context(&s_ctx_b);
    mcs51_context_reset(&s_ctx_b);  // classic: binds nullptr, zeroes hooks
    check(s_ctx_b.soc_priv == nullptr, "classic must bind soc_priv=nullptr");
    check(s_ctx_b.gpio_hooks.may_drive == nullptr,
          "fresh reset must zero hooks");
    check(s_ctx_a.gpio_hooks.may_drive == hook_a,
          "A hooks must survive B reset + family switch");
    s_ctx_b.gpio_hooks.may_drive = hook_b;
    (void)hook_a_called;
    (void)hook_b_called;
    check(s_ctx_a.gpio_hooks.may_drive != s_ctx_b.gpio_hooks.may_drive,
          "hooks are per-context, never shared");

    // ── review lock-in: classic-context foreign poll/observer neutrality ─
    // Chip polls on an unbound classic context must bail silently (no bind,
    // no crash); observers report neutral. Guards cms8s_hook_armed against
    // future accidental deletion. (Merely reaching here proves no crash.)
    cms8s_adc_poll(&s_ctx_b);
    cms8s_buzzer_poll(&s_ctx_b);
    check(s_ctx_b.soc_priv == nullptr,
          "classic must remain unbound after foreign polls");
    check(!cms8s_buzzer_is_running(),
          "classic-context buzzer must report not running");
    check(cms8s_adc_conversion_count() == 0u,
          "unbound observer must report neutral zero");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);  // leave clean
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);  // leave clean
    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: soc_priv per-instance isolation (slots/clamp/hooks/classic-null)\n");
    return 0;
}
