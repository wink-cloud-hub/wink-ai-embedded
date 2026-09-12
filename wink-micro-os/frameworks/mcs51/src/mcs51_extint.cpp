// SPDX-License-Identifier: Apache-2.0
// MCS-51 External Interrupt (INT0/INT1) standard model.
#include "wink_mcs51_extint.h"

#include <cstdint>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_sfr_map.h"
#include "mcs51_trap.h"
#include "wink_event.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

// Stage4 CPL-07: full-port interrupts + pin-share selectors live in the chip
// package. The core keeps the standard INT0/INT1 lines; each line carries a
// generic pin-selector address slot (ps_addr, 0xFFFF = hardwired classic
// pins) that the owning chip init patches with its selector — the core reads
// the VALUE, never a vendor address.

extern "C" {
uint8_t js_pal_gpio_read_state(uint16_t pin);
}

namespace {

constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_IE   = 0xA8;
constexpr uint8_t SFR_PCON = 0x87;  // PCON.1 = Power-Down (GAP-17' wake gate)

constexpr uint8_t TCON_IT0 = 0u;  // TCON.0: 0=low level, 1=falling edge
constexpr uint8_t TCON_IE0 = 1u;  // TCON.1: INT0 edge flag
constexpr uint8_t TCON_IT1 = 2u;  // TCON.2: 0=low level, 1=falling edge
constexpr uint8_t TCON_IE1 = 3u;  // TCON.3: INT1 edge flag

constexpr uint8_t IE_EX0 = 0u;  // IE.0: INT0 enable
constexpr uint8_t IE_EX1 = 2u;  // IE.2: INT1 enable
constexpr uint8_t IE_EA  = 7u;  // IE.7: global interrupt enable

constexpr uint8_t VECTOR_INT0 = 0u;
constexpr uint8_t VECTOR_INT1 = 2u;

constexpr uint16_t PIN_INT0 = 26u;  // P3.2 = (3<<3)|2
constexpr uint16_t PIN_INT1 = 27u;  // P3.3 = (3<<3)|3

constexpr uint8_t EXT_LOW  = 0u;
constexpr uint8_t EXT_HIGH = 1u;

constexpr uint64_t SAMPLE_PERIOD_US = 10000ull;

// No pin-share selector: the line is bond-fixed to its fallback pins
// (classic P3.2/P3.3). Chip inits patch real selector addresses.
constexpr uint16_t SELECTOR_NONE = 0xFFFFu;

uint16_t resolve_int_pin(Mcu51Context* ctx, uint16_t ps_addr, uint16_t fallback_pin) {
    if (ps_addr == SELECTOR_NONE) {
        return fallback_pin;  // hardwired classic pins, no selector
    }
    uint8_t sel  = ctx->xdata_shadow[ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit  = sel & 0x0Fu;
    if (mcs51_family_pin_valid(mcs51_family_desc(ctx->family), port, bit)) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback_pin;
}

void poll_line(Mcu51Context* ctx, Mcu51ExtIntLine& ln) {
    uint16_t pin = resolve_int_pin(ctx, ln.ps_addr, ln.fallback_pin);
    bool mux_changed = (pin != ln.pin);
    if (mux_changed) {
        ln.pin = pin;
        ln.last_level = 0xFFu;
        ln.have_sample = false;
    }
    uint8_t st = js_pal_gpio_read_state(ln.pin);
    if (st != EXT_LOW && st != EXT_HIGH) {
        return;  // HiZ / conflict: no external driver — treat as "no change"
    }
    uint8_t level = st;
    bool was_low = (ln.last_level == EXT_LOW);
    bool now_low = (level == EXT_LOW);
    bool falling = !mux_changed && ln.have_sample && !was_low && now_low;
    ln.last_level = level;
    ln.have_sample = true;

    uint8_t tcon = ctx->sfr_shadow[SFR_TCON];
    bool edge_mode = (tcon & (1u << ln.it_bit)) != 0;

    if (edge_mode) {
        if (falling) {
            ctx->sfr_shadow[SFR_TCON] =
                static_cast<uint8_t>(tcon | (1u << ln.ie_bit));
            tcon = ctx->sfr_shadow[SFR_TCON];
        }
        bool pending = (tcon & (1u << ln.ie_bit)) != 0;
        if (pending) {
            mcs51_raise_irq(ln.vector == VECTOR_INT0 ? IRQ_SOURCE_INT0 : IRQ_SOURCE_INT1);
        }
    } else {
        if (now_low) {
            mcs51_raise_irq(ln.vector == VECTOR_INT0 ? IRQ_SOURCE_INT0 : IRQ_SOURCE_INT1);
        }
    }
}

}  // namespace

extern "C" {

void mcs51_extint_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    ctx->extint.lines[0].pin = PIN_INT0;
    ctx->extint.lines[0].fallback_pin = PIN_INT0;
    ctx->extint.lines[0].ps_addr = SELECTOR_NONE;
    ctx->extint.lines[0].vector = VECTOR_INT0;
    ctx->extint.lines[0].it_bit = TCON_IT0;
    ctx->extint.lines[0].ie_bit = TCON_IE0;
    ctx->extint.lines[0].ex_bit = IE_EX0;
    if (!ctx->extint.lines[0].have_sample) {
        ctx->extint.lines[0].last_level = 0xFFu;
    }

    ctx->extint.lines[1].pin = PIN_INT1;
    ctx->extint.lines[1].fallback_pin = PIN_INT1;
    ctx->extint.lines[1].ps_addr = SELECTOR_NONE;
    ctx->extint.lines[1].vector = VECTOR_INT1;
    ctx->extint.lines[1].it_bit = TCON_IT1;
    ctx->extint.lines[1].ie_bit = TCON_IE1;
    ctx->extint.lines[1].ex_bit = IE_EX1;
    if (!ctx->extint.lines[1].have_sample) {
        ctx->extint.lines[1].last_level = 0xFFu;
    }

    ctx->extint.sample_due = true;
    ctx->extint.in_poll = false;
}

void mcs51_extint_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (ctx->extint.lines[0].fallback_pin == 0) {
        mcs51_extint_init(ctx);
    }
    for (Mcu51ExtIntLine& ln : ctx->extint.lines) {
        ln.last_sample_us = 0;
    }
    ctx->extint.sample_due = true;
    ctx->sfr_shadow[SFR_TCON] &=
        static_cast<uint8_t>(~((1u << TCON_IE0) | (1u << TCON_IE1)));
    wink_mcs51_clear_irq(IRQ_SOURCE_INT0);
    wink_mcs51_clear_irq(IRQ_SOURCE_INT1);
}

void wink_mcs51_extint_reset(void) {
    mcs51_extint_reset(mcs51_get_context());
}

void mcs51_extint_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (ctx->extint.lines[0].fallback_pin == 0) {
        mcs51_extint_init(ctx);
    }
    if (ctx->extint.in_poll) {
        return;
    }
    ctx->extint.in_poll = true;
    bool force = ctx->extint.sample_due;
    ctx->extint.sample_due = false;
    uint64_t now = wink_mcs51_virtual_us();
    for (Mcu51ExtIntLine& ln : ctx->extint.lines) {
        if (!force && ln.have_sample &&
            (now - ln.last_sample_us) < SAMPLE_PERIOD_US) {
            continue;
        }
        ln.last_sample_us = now;
        poll_line(ctx, ln);
    }
    ctx->extint.in_poll = false;
}

void wink_mcs51_extint_poll(void) {
    mcs51_extint_poll(mcs51_get_context());
}

uint64_t mcs51_extint_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    // INT0/INT1 are sampled once per SAMPLE_PERIOD_US slice (poll_line
    // throttles on last_sample_us). Advertise the earliest next sample so
    // IDLE (PCON.0) Mode A keeps step-pumping and microsteps keep polling
    // the lines; returning UINT64_MAX here would drop a classic part into
    // Mode B (event-queue wait) with no scheduled wake source when no timer
    // is active. PD (PCON.1) ignores schedules by design and stays
    // event-driven (mcs51_calc_next_event_us is_pd path).
    uint64_t earliest = UINT64_MAX;
    for (const Mcu51ExtIntLine& ln : ctx->extint.lines) {
        const uint64_t next = ln.last_sample_us + SAMPLE_PERIOD_US;
        if (next < earliest) {
            earliest = next;
        }
    }
    if (earliest == UINT64_MAX) {
        return UINT64_MAX;
    }
    return (earliest > ctx->virtual_us) ? earliest : ctx->virtual_us;
}

}  // extern "C"
