// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx full-port level-change interrupt model (Stage4 S4-1 Step 2,
// PLAN-20260911-MCS51-S4, CPL-07).
//
// Standard 8051 external interrupts are INT0/INT1 only (generic core). This
// silicon adds per-pin port interrupts (P0EXTIE/P0EXTIF + EICFG edge select,
// vectors 7..10) and pin-share selectors for INT0/INT1 (PS_INT0/PS_INT1).
// Moved verbatim from the generic extint TU; sampling state lives in the
// chip pool (S2-2 D6 handover, S4-D2: no cross-reset baseline preserve).
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_event.h"

#include <stdint.h>

extern "C" {
uint8_t js_pal_gpio_read_state(uint16_t pin);
}

namespace {

constexpr uint8_t SFR_IE   = 0xA8;
constexpr uint8_t SFR_PCON = 0x87;  // PCON.1 = Power-Down (GAP-17' wake gate)

constexpr uint8_t IE_EA = 7u;  // IE.7: global interrupt enable

constexpr uint8_t EXT_LOW  = 0u;
constexpr uint8_t EXT_HIGH = 1u;

constexpr uint64_t SAMPLE_PERIOD_US = 10000ull;

constexpr uint8_t SFR_P0EXTIE = 0xACu;  // enable block P0..P3EXTIE
// Flag base: CMS8S_SFR_P0EXTIF + p (shared chip map).

constexpr uint8_t PORT_VECTORS[4] = {7u, 8u, 9u, 10u};

constexpr uint16_t PORT_EICFG_BASE[4] = {
    0xF080u,  // P0EICFG0 @ 0xF080..0xF087
    0xF088u,  // P1EICFG0 @ 0xF088..0xF08F
    0xF090u,  // P2EICFG0 @ 0xF090..0xF095
    0xF098u,  // P3EICFG0 @ 0xF098..0xF09B
};

constexpr uint16_t XSFR_PS_INT0 = CMS8S_XSFR_PS_INT0;
constexpr uint16_t XSFR_PS_INT1 = CMS8S_XSFR_PS_INT1;
constexpr uint8_t  PS_RESET     = CMS8S_XSFR_PS_RESET;

// Write-0-to-clear (W0C), same semantics as T2IF/EIF2: writing 0 clears the
// bit, writing 1 leaves it unchanged (GAP-12). Without this, an `|=` clear
// or a multi-pin clear would wipe still-pending flags from other pins.
// M3: C language linkage for the C-ABI hook table.
extern "C" void sfr_write_hook_port_extif(struct Mcu51Context* ctx,
                                          uint8_t addr, uint8_t old_val,
                                          uint8_t new_val) {
    if (ctx) {
        ctx->sfr_shadow[addr] = old_val & new_val;
    }
}

// Patch the generic INT0/INT1 line descriptors with this silicon's pin-share
// selector addresses (core lines default to hardwired pins, ps none).
void patch_line_selectors(Mcu51Context* ctx) {
    ctx->extint.lines[0].ps_addr = XSFR_PS_INT0;
    ctx->extint.lines[1].ps_addr = XSFR_PS_INT1;
}

// S4-H2 (reset-rebuilds-registration contract): shared by init and reset
// (trap_register overwrites the same slots — idempotent).
void install_trap_hooks(void) {
    // GAP-12: port interrupt flags are write-0-to-clear.
    mcs51_trap_register_sfr_write(CMS8S_SFR_P0EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P1EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P2EXTIF, sfr_write_hook_port_extif);
    mcs51_trap_register_sfr_write(CMS8S_SFR_P3EXTIF, sfr_write_hook_port_extif);
}

void poll_port_ints(Mcu51Context* ctx, bool force, uint64_t now) {
    Cms8sPortExtIntState& port = cms8s_priv(ctx)->port_extint;
    if (!force && (now - port.port_last_sample_us) < SAMPLE_PERIOD_US) {
        return;
    }
    port.port_last_sample_us = now;

    uint8_t ie = ctx->sfr_shadow[SFR_IE];
    bool ea = (ie & (1u << IE_EA)) != 0;
    // Loop bound from the descriptor (reduced families sample only
    // existing pins, full-pin families all 32).
    const mcs51_family_desc_t* fam = mcs51_family_desc(ctx->family);

    for (uint8_t p = 0; p < 4u; ++p) {
        uint8_t extie = ctx->sfr_shadow[SFR_P0EXTIE + p];
        uint8_t extif = ctx->sfr_shadow[CMS8S_SFR_P0EXTIF + p];
        uint8_t npins = mcs51_family_port_pin_count(fam, p);

        for (uint8_t b = 0; b < npins; ++b) {
            uint16_t pin = static_cast<uint16_t>((p << 3) | b);
            uint8_t st = js_pal_gpio_read_state(pin);
            uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;
            Mcu51PortPinState& ps = port.port_pins[p][b];
            bool was_low = (ps.last_level == EXT_LOW);
            bool now_low = (level == EXT_LOW);
            bool have = ps.have_sample;
            ps.last_level = level;
            ps.have_sample = true;

            if ((extie & (1u << b)) != 0 && have) {
                const uint16_t eicfg_addr =
                    static_cast<uint16_t>(PORT_EICFG_BASE[p] + b);
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

// Forward: init delegates to reset below (S4-H2 follow-up).
void cms8s_extint_reset(struct Mcu51Context* ctx);

void cms8s_extint_init(struct Mcu51Context* ctx) {
    // S4-H2 follow-up: init delegates to reset so a standalone init (test
    // harnesses, future chip add-ons) leaves a fully coherent context —
    // selector seeds and hooks included. Full context reset runs both; the
    // second pass overwrites the same slots (idempotent).
    cms8s_extint_reset(ctx);
}

void cms8s_extint_reset(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    patch_line_selectors(ctx);
    for (uint8_t p = 0; p < 4u; ++p) {
        ctx->sfr_shadow[CMS8S_SFR_P0EXTIF + p] = 0u;
    }
    ctx->xdata_shadow[XSFR_PS_INT0] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_INT1] = PS_RESET;
    Cms8sPortExtIntState& port = cms8s_priv(ctx)->port_extint;
    port.port_last_sample_us = 0;
    port.sample_due = true;
    port.in_poll = false;
    install_trap_hooks();
}

void cms8s_port_extint_poll(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!cms8s_hook_armed(ctx)) {
        return;  // review hardening: unbound/classic
    }
    Cms8sPortExtIntState& port = cms8s_priv(ctx)->port_extint;
    if (port.in_poll) {
        return;
    }
    port.in_poll = true;
    bool force = port.sample_due;
    port.sample_due = false;
    poll_port_ints(ctx, force, wink_mcs51_virtual_us());
    port.in_poll = false;
}

uint64_t cms8s_port_extint_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!cms8s_hook_armed(ctx)) {
        return UINT64_MAX;
    }
    uint64_t next =
        cms8s_priv(ctx)->port_extint.port_last_sample_us + SAMPLE_PERIOD_US;
    return (next > ctx->virtual_us) ? next : ctx->virtual_us;
}

}  // extern "C"
