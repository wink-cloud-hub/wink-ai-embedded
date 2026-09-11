// SPDX-License-Identifier: Apache-2.0
// MCS-51 External Interrupt (INT0/INT1 + Port-pin GPIO interrupts) model.
#include "wink_mcs51_extint.h"

#include <cstdint>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_sfr_map.h"
#include "cms8s_sfr_map.h"  // S3-1 transition: EXTIF/PS_* moved here with
                             // CMS8S_ prefixes; this TU includes the chip map
                             // until its code moves to chips/ in stage4.
#include "mcs51_trap.h"
#include "wink_event.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

// Port-pin interrupt flags P0..P3EXTIF: CMS8S_SFR_PxEXTIF (cms8s_sfr_map.h).

// Write-0-to-clear (W0C), same semantics as T2IF/EIF2: writing 0 clears the
// bit, writing 1 leaves it unchanged (GAP-12). Without this, an `|=` clear
// or a multi-pin clear would wipe still-pending flags from other pins.
// M3: C language linkage for the C-ABI hook table (see cms8s_sys.cpp).
extern "C" void sfr_write_hook_port_extif(struct Mcu51Context* ctx, uint8_t addr,
                               uint8_t old_val, uint8_t new_val) {
    (void)addr;
    if (ctx) {
        ctx->sfr_shadow[addr] = old_val & new_val;
    }
}

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

constexpr uint8_t SFR_P0EXTIE = 0xACu;  // extint-local (enable block P0..P3EXTIE)
// Flag base: CMS8S_SFR_P0EXTIF + p (cms8s_sfr_map.h, shared).
// (S2-2: no hardcoded pin-count table here anymore — legality reads the
// family descriptor per call site, so reduced families tighten nonexistent
// pins to fallback while full-pin families keep every pin.)
constexpr uint8_t PORT_VECTORS[4] = {7u, 8u, 9u, 10u};

constexpr uint16_t PORT_EICFG_BASE[4] = {
    0xF080u,  // P0EICFG0 @ 0xF080..0xF087
    0xF088u,  // P1EICFG0 @ 0xF088..0xF08F
    0xF090u,  // P2EICFG0 @ 0xF090..0xF095
    0xF098u,  // P3EICFG0 @ 0xF098..0xF09B
};

// M5: shared selectors alias the single source (cms8s_sfr_map.h, S3-1).
constexpr uint16_t XSFR_PS_INT0 = CMS8S_XSFR_PS_INT0;
constexpr uint16_t XSFR_PS_INT1 = CMS8S_XSFR_PS_INT1;
constexpr uint8_t  PS_RESET     = CMS8S_XSFR_PS_RESET;

uint16_t resolve_int_pin(Mcu51Context* ctx, uint16_t ps_addr, uint16_t fallback_pin) {
    uint8_t sel  = ctx->xdata_shadow[ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit  = sel & 0x0Fu;
    const uint8_t* pin_masks = mcs51_family_desc(ctx->family)->port_pin_masks;
    if (port < 4u && bit < pin_masks[port]) {
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

void poll_port_ints(Mcu51Context* ctx, bool force, uint64_t now) {
    if (!force && (now - ctx->extint.port_last_sample_us) < SAMPLE_PERIOD_US) {
        return;
    }
    ctx->extint.port_last_sample_us = now;

    uint8_t ie = ctx->sfr_shadow[SFR_IE];
    bool ea = (ie & (1u << IE_EA)) != 0;
    // S2-2: loop bound from the descriptor (reduced families sample only
    // existing pins, full-pin families all 32).
    const uint8_t* pin_masks = mcs51_family_desc(ctx->family)->port_pin_masks;

    for (uint8_t p = 0; p < 4u; ++p) {
        uint8_t extie = ctx->sfr_shadow[SFR_P0EXTIE + p];
        uint8_t extif = ctx->sfr_shadow[CMS8S_SFR_P0EXTIF + p];
        uint8_t npins = pin_masks[p];

        for (uint8_t b = 0; b < npins; ++b) {
            uint16_t pin = static_cast<uint16_t>((p << 3) | b);
            uint8_t st = js_pal_gpio_read_state(pin);
            uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;
            Mcu51PortPinState& ps = ctx->extint.port_pins[p][b];
            bool was_low = (ps.last_level == EXT_LOW);
            bool now_low = (level == EXT_LOW);
            bool have = ps.have_sample;
            ps.last_level = level;
            ps.have_sample = true;

            if ((extie & (1u << b)) != 0 && have) {
                uint16_t eicfg_addr = static_cast<uint16_t>(PORT_EICFG_BASE[p] + b);
                uint8_t mode = ctx->xdata_shadow[eicfg_addr] & 0x03u;
                bool match = false;
                if (mode == 1u) {
                    match = was_low && !now_low;        // Rising
                } else if (mode == 2u) {
                    match = !was_low && now_low;        // Falling
                } else if (mode == 3u) {
                    match = (was_low != now_low);       // Both edges
                }
                if (match) {
                    extif |= static_cast<uint8_t>(1u << b);
                    ctx->sfr_shadow[CMS8S_SFR_P0EXTIF + p] = extif;
                    // GAP-17': STOP (Power-Down) wake via GPIO port
                    // interrupt (ref manual §5.4.1). Mirrors the INT0/INT1
                    // PD clause in mcs51_raise_irq: EA-gated, PD-bit-gated,
                    // one post per edge (match fires once per transition,
                    // so a held level cannot flood the event queue).
                    // WUT/LSE/SWE/LVD have no model and stay unwakeable
                    // (redline §4.7).
                    if (ea && (ctx->sfr_shadow[SFR_PCON] & 0x02u) != 0u) {
                        wink_event_t evt = {0};
                        (void)wink_event_post(&evt);
                    }
                }
            }
        }

        if (ea && (extif & extie) != 0) {
            (void)wink_mcs51_dispatch_vector(PORT_VECTORS[p]);
        }
    }
}

}  // namespace

extern "C" {

void mcs51_extint_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    ctx->extint.lines[0].pin = PIN_INT0;
    ctx->extint.lines[0].fallback_pin = PIN_INT0;
    ctx->extint.lines[0].ps_addr = XSFR_PS_INT0;
    ctx->extint.lines[0].vector = VECTOR_INT0;
    ctx->extint.lines[0].it_bit = TCON_IT0;
    ctx->extint.lines[0].ie_bit = TCON_IE0;
    ctx->extint.lines[0].ex_bit = IE_EX0;
    if (!ctx->extint.lines[0].have_sample) {
        ctx->extint.lines[0].last_level = 0xFFu;
    }

    ctx->extint.lines[1].pin = PIN_INT1;
    ctx->extint.lines[1].fallback_pin = PIN_INT1;
    ctx->extint.lines[1].ps_addr = XSFR_PS_INT1;
    ctx->extint.lines[1].vector = VECTOR_INT1;
    ctx->extint.lines[1].it_bit = TCON_IT1;
    ctx->extint.lines[1].ie_bit = TCON_IE1;
    ctx->extint.lines[1].ex_bit = IE_EX1;
    if (!ctx->extint.lines[1].have_sample) {
        ctx->extint.lines[1].last_level = 0xFFu;
    }

    for (uint8_t p = 0; p < 4u; ++p) {
        for (uint8_t b = 0; b < 8u; ++b) {
            if (!ctx->extint.port_pins[p][b].have_sample) {
                ctx->extint.port_pins[p][b].last_level = 0xFFu;
            }
        }
    }
    ctx->extint.sample_due = true;
    ctx->extint.in_poll = false;
    ctx->extint.port_last_sample_us = 0;

    // GAP-12: port interrupt flags are write-0-to-clear.
    mcs51_trap_register_sfr_write(CMS8S_SFR_P0EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P1EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P2EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P3EXTIF, sfr_write_hook_port_extif);
}

void mcs51_extint_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (ctx->extint.lines[0].fallback_pin == 0) {
        mcs51_extint_init(ctx);
    }
    for (Mcu51ExtIntLine& ln : ctx->extint.lines) {
        ln.last_sample_us = 0;
    }
    ctx->extint.port_last_sample_us = 0;
    ctx->extint.sample_due = true;
    ctx->sfr_shadow[SFR_TCON] &=
        static_cast<uint8_t>(~((1u << TCON_IE0) | (1u << TCON_IE1)));
    wink_mcs51_clear_irq(IRQ_SOURCE_INT0);
    wink_mcs51_clear_irq(IRQ_SOURCE_INT1);
    for (uint8_t p = 0; p < 4u; ++p) {
        ctx->sfr_shadow[CMS8S_SFR_P0EXTIF + p] = 0;
    }
    ctx->xdata_shadow[XSFR_PS_INT0] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_INT1] = PS_RESET;
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
    poll_port_ints(ctx, force, now);
    ctx->extint.in_poll = false;
}

void wink_mcs51_extint_poll(void) {
    mcs51_extint_poll(mcs51_get_context());
}

uint64_t mcs51_extint_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    uint64_t next = ctx->extint.port_last_sample_us + SAMPLE_PERIOD_US;
    return (next > ctx->virtual_us) ? next : ctx->virtual_us;
}

}  // extern "C"
