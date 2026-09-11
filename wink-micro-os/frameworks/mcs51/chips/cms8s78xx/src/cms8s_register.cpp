// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx chip-package register entry (Stage4 S4-2 Step 3,
// PLAN-20260911-MCS51-S4, CPL-10).
//
// The single place that names cms8s_* peripheral symbols: core dispatch
// loops never reference them (total §3.1 one-way rule); production glue
// (generated mcs51_family_select.h, stage6) and the test harness call
// cms8s78xx_register() explicitly. Idempotent (registry dedups by name).
//
// This TU also owns the chip BSS pool (scheme A, audit §3 criterion 1):
// one Cms8sPriv slot per context instance, bound through soc_priv by
// self-binding chip inits. Pool lives with the register entry (not with any
// single model) so no model TU depends sideways on another (S4-2 Step 3
// review package: adc/buzzer no longer resolve the pool via the sys TU).
#include "mcs51_peripheral.h"

#include <stddef.h>
#include <string.h>

#include "cms8s_priv.h"
#include "mcs51_context.h"

extern "C" {

void cms8s_adc_init(struct Mcu51Context* ctx);
void cms8s_adc_model_reset(struct Mcu51Context* ctx);
void cms8s_adc_poll(struct Mcu51Context* ctx);
uint64_t cms8s_adc_next_event_us(struct Mcu51Context* ctx);

void cms8s_buzzer_init(struct Mcu51Context* ctx);
void cms8s_buzzer_reset(struct Mcu51Context* ctx);
void cms8s_buzzer_poll(struct Mcu51Context* ctx);
uint64_t cms8s_buzzer_next_event_us(struct Mcu51Context* ctx);

void cms8s_sys_init(struct Mcu51Context* ctx);
void cms8s_sys_reset(struct Mcu51Context* ctx);
void cms8s_sys_poll(struct Mcu51Context* ctx);
uint64_t cms8s_sys_next_event_us(struct Mcu51Context* ctx);

void cms8s_gpio_init(struct Mcu51Context* ctx);
void cms8s_gpio_reset(struct Mcu51Context* ctx);

void cms8s_extint_init(struct Mcu51Context* ctx);
void cms8s_extint_reset(struct Mcu51Context* ctx);
void cms8s_port_extint_poll(struct Mcu51Context* ctx);
uint64_t cms8s_port_extint_next_event_us(struct Mcu51Context* ctx);

void cms8s_uart_init(struct Mcu51Context* ctx);
void cms8s_uart_reset(struct Mcu51Context* ctx);

}  // extern "C"

namespace {

// Moved verbatim from the core static table (stage4 CPL-10): same names,
// same phases, same family mask — only the owner changed.
const mcs51_peripheral_desc_t kCms8sDescs[] = {
    {
        "cms8s_adc",
        cms8s_adc_init,
        cms8s_adc_model_reset,
        cms8s_adc_poll,
        cms8s_adc_next_event_us,
        MCS51_PHASE_ADC,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        "cms8s_buzzer",
        cms8s_buzzer_init,
        cms8s_buzzer_reset,
        cms8s_buzzer_poll,
        cms8s_buzzer_next_event_us,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        "cms8s_sys",
        cms8s_sys_init,
        cms8s_sys_reset,
        cms8s_sys_poll,
        cms8s_sys_next_event_us,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        // S4-1 Step 1: enhanced GPIO = hook installer (no poll/next-event;
        // NULL entries are skipped by the dispatch loops).
        "cms8s_gpio",
        cms8s_gpio_init,
        cms8s_gpio_reset,
        nullptr,
        nullptr,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        // S4-1 Step 2: full-port interrupts + INT line selector patching.
        "cms8s_extint",
        cms8s_extint_init,
        cms8s_extint_reset,
        cms8s_port_extint_poll,
        cms8s_port_extint_next_event_us,
        MCS51_PHASE_EXTINT,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        // S4-2 Step 1: UART source selection = hook installer (the TX
        // engine stays core; NULL poll/next-event are skipped).
        "cms8s_uart",
        cms8s_uart_init,
        cms8s_uart_reset,
        nullptr,
        nullptr,
        MCS51_PHASE_RX_DRAIN,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
};

}  // namespace

// S2-1 chip BSS pool (scheme A): one Cms8sPriv slot per context instance.
// Core never names it (§3.1 one-way rule); binding is self-service through
// cms8s_soc_bind below.
static Cms8sPriv s_cms8s_priv_pool[MCS51_MAX_INSTANCES];

extern "C" {

void cms8s_soc_bind(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;  // review hardening: null-active never crashes
    // Indexing clamps, never asserts (review finding): the loud fuse lives
    // in mcs51_context_reset; here NDEBUG-proof containment wins — worst
    // case aliases the last slot deterministically, never OOB.
    const uint8_t idx = (ctx->instance_index < MCS51_MAX_INSTANCES)
                            ? ctx->instance_index
                            : (MCS51_MAX_INSTANCES - 1u);
    Cms8sPriv* slot = &s_cms8s_priv_pool[idx];
    if (ctx->soc_priv == slot) {
        return;  // idempotent: already bound (later inits in the same reset)
    }
    memset(slot, 0, sizeof(*slot));
    ctx->soc_priv = slot;
}

void cms8s78xx_register(void) {
    for (size_t i = 0;
         i < sizeof(kCms8sDescs) / sizeof(kCms8sDescs[0]); ++i) {
        mcs51_peripheral_register(&kCms8sDescs[i]);
    }
}

}  // extern "C"
