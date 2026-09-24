// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx system-clock output (CLO) peripheral model (ADR-0004 static
// dispatch, buzzer-isomorphic).
//
// Behavioral model notes (honesty record, see
// PLAN-20260924-CMS8S78XX-SYSCLOCK-FIDELITY D3):
//   * CLO has no enable register on silicon: selecting P13CFG == 0x05 routes
//     Fsys/64 to P1.3 unconditionally. Frequency always follows the live
//     ctx->clock_hz (maintained by the CLKDIV hook, reset seed 24 MHz).
//   * Microsecond quantization: the true half period T_half = 32e6/Fsys us
//     (1.333... us at 24 MHz) is not an integer number of virtual
//     microseconds. A pure-integer Bresenham phase accumulator (step_us +
//     rem_step/rem_accum over clock_hz_last) emits the [1, 1, 2, ...] us
//     toggle sequence whose macroscopic average is exactly 375000 Hz at
//     24 MHz (0% frequency error); the microscopic edge placement is
//     quantized to whole microseconds by construction.
#include "cms8s_clo.h"

#include "cms8s_priv.h"
#include "mcs51_context.h"
#include "wink_mcs51_gpio.h"
#include <cstdint>

namespace {

constexpr uint16_t XSFR_P13CFG = 0xF013;
constexpr uint8_t  GPIO_P13_MUX_CLO = 0x05;
constexpr uint16_t CLO_PIN = 11u;  // P1.3: (1 << 3) | 3

// Advance the phase accumulator once, returning this step's whole-us delta.
static uint32_t clo_advance(Cms8sCloState* c) {
    uint32_t delta = c->step_us;
    c->rem_accum += c->rem_step;
    if (c->rem_accum >= c->clock_hz_last) {
        c->rem_accum -= c->clock_hz_last;
        delta += 1u;
    }
    return delta;
}

void update_clo_state(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();

    const uint8_t p13cfg = ctx->xdata_shadow[XSFR_P13CFG];
    const bool should_run = (p13cfg == GPIO_P13_MUX_CLO);

    Cms8sCloState* c = &cms8s_priv(ctx)->clo;
    if (should_run) {
        const uint32_t clk_hz =
            ctx->clock_hz ? ctx->clock_hz : 24000000u;
        if (clk_hz != c->clock_hz_last) {
            // First start or a runtime CLKDIV change: re-derive the
            // Bresenham step from T_half = 32e6/Fsys us.
            c->clock_hz_last = clk_hz;
            c->step_us = static_cast<uint32_t>(32000000ull / clk_hz);
            c->rem_step = static_cast<uint32_t>(32000000ull % clk_hz);
            c->rem_accum = 0u;
            if (c->step_us == 0u && c->rem_step == 0u) {
                c->step_us = 1u;
            }
        }
        if (!c->running) {
            c->running = true;
            c->pin_level = 1u;
            js_pal_gpio_write(CLO_PIN, true, MCS51_DRIVE_SUPPLY);
            const uint32_t delta = clo_advance(c);
            c->next_toggle_us = ctx->virtual_us +
                (delta == 0u ? 1u : delta);
        }
    } else {
        if (c->running) {
            c->running = false;
            c->next_toggle_us = UINT64_MAX;
            c->pin_level = 0u;
            js_pal_gpio_write(CLO_PIN, false, MCS51_DRIVE_SUPPLY);
        }
    }
}

}  // namespace

extern "C" {

void cms8s_clo_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    Cms8sCloState* c = &cms8s_priv(ctx)->clo;
    const bool was_running = c->running;
    c->running = false;
    c->pin_level = 0u;
    c->step_us = 0u;
    c->rem_step = 0u;
    c->rem_accum = 0u;
    c->clock_hz_last = 0u;
    c->next_toggle_us = UINT64_MAX;
    c->toggle_count = 0u;
    // Reset-in-flight: a stop is a stop — park the pin at the safe level
    // only when it was actually driving, so a cold init stays silent.
    if (was_running) {
        js_pal_gpio_write(CLO_PIN, false, MCS51_DRIVE_SUPPLY);
    }
    // No SFR/XSFR hooks to rebuild (poll-only mux gate, S4-H2 vacuous).
}

void cms8s_clo_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // bind BEFORE any pool deref (ordering invariant)
    cms8s_clo_reset(ctx);
}

void cms8s_clo_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) {
        return;  // review hardening: unbound/classic
    }

    update_clo_state(ctx);

    Cms8sCloState* c = &cms8s_priv(ctx)->clo;
    if (!c->running || c->next_toggle_us == UINT64_MAX) {
        return;
    }

    constexpr uint32_t MAX_TOGGLES_PER_POLL = 1000u;
    uint32_t toggles = 0u;
    while (c->running && ctx->virtual_us >= c->next_toggle_us) {
        c->pin_level ^= 1u;
        js_pal_gpio_write(CLO_PIN, c->pin_level != 0, MCS51_DRIVE_SUPPLY);
        c->toggle_count++;
        const uint32_t delta = clo_advance(c);
        c->next_toggle_us += (delta == 0u ? 1u : delta);
        if (++toggles >= MAX_TOGGLES_PER_POLL) {
            if (ctx->virtual_us >= c->next_toggle_us) {
                const uint32_t catchup = clo_advance(c);
                c->next_toggle_us = ctx->virtual_us +
                    (catchup == 0u ? 1u : catchup);
            }
            break;
        }
    }
}

uint64_t cms8s_clo_next_event_us(struct Mcu51Context* ctx) {
    // Review hardening: never crash on an unbound context (neutral = idle).
    Cms8sCloState* c = nullptr;
    if (ctx != nullptr && cms8s_hook_armed(ctx)) {
        c = &cms8s_priv(ctx)->clo;
    }
    if (c != nullptr && c->running && c->next_toggle_us != UINT64_MAX) {
        return c->next_toggle_us;
    }
    return UINT64_MAX;
}

// Test observability without a ctx parameter: reads the active context.
// Review hardening: neutral on unbound (never crash).
bool cms8s_clo_is_running(void) {
    Cms8sPriv* priv = cms8s_priv(nullptr);
    if (priv == nullptr || !cms8s_hook_armed(nullptr)) {
        return false;
    }
    return priv->clo.running;
}

uint32_t cms8s_clo_toggle_count(void) {
    Cms8sPriv* priv = cms8s_priv(nullptr);
    if (priv == nullptr || !cms8s_hook_armed(nullptr)) {
        return 0u;
    }
    return priv->clo.toggle_count;
}

uint32_t cms8s_clo_half_period_us(void) {
    Cms8sPriv* priv = cms8s_priv(nullptr);
    if (priv == nullptr || !cms8s_hook_armed(nullptr)) {
        return 0u;
    }
    return priv->clo.step_us;
}

}  // extern "C"
