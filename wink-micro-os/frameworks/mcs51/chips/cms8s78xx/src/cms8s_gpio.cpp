// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx enhanced GPIO model (Stage4 S4-1 Step 1, PLAN-20260911-MCS51-S4,
// CPL-03).
//
// Standard 8051 ports are quasi-bidirectional (weak pull-up, always drive).
// This silicon adds per-pin direction (PxTRIS), open-drain (PxOD), internal
// pull-up (PxUP) and analog mux (PxxCFG). The model serves the generic core
// through the per-context gpio_hooks (mounted by cms8s_gpio_init/reset);
// the core fast-paths standard parts on caps_cache and never names an
// address here.
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "cms8s_priv.h"
#include "wink_mcs51_gpio.h"

#include <stdint.h>

namespace {

// A-05 TRIS polarity: TRIS bit 1 = OUTPUT, 0 = INPUT (all-input out of
// reset, SFR shadows memset to 0x00).
uint8_t tris_addr(uint8_t port) {
    switch (port) {
        case 0: return 0x9Au;  // P0TRIS
        case 1: return 0xA1u;  // P1TRIS
        case 2: return 0xA2u;  // P2TRIS
        case 3: return 0xA3u;  // P3TRIS
        default: return 0xFFu;
    }
}

// P3 (PLAN-20260912-MCS51-P3-TRIS): TRIS SFR address -> port index (0xFF =
// not a TRIS register). The write hooks are per address; this is the reverse
// of tris_addr().
uint8_t port_from_tris_addr(uint8_t addr) {
    switch (addr) {
        case 0x9Au: return 0u;
        case 0xA1u: return 1u;
        case 0xA2u: return 2u;
        case 0xA3u: return 3u;
        default: return 0xFFu;
    }
}

uint16_t up_addr(uint8_t port) {
    switch (port) {
        case 0: return 0xF00Au;  // P0UP
        case 1: return 0xF01Au;  // P1UP
        case 2: return 0xF02Au;  // P2UP
        case 3: return 0xF03Au;  // P3UP
        default: return 0xFFFFu;
    }
}

uint16_t od_addr(uint8_t port) {
    switch (port) {
        case 0: return 0xF009u;  // P0OD
        case 1: return 0xF019u;  // P1OD
        case 2: return 0xF029u;  // P2OD
        case 3: return 0xF039u;  // P3OD
        default: return 0xFFFFu;
    }
}

uint16_t cfg_addr(uint8_t port, uint8_t bit) {
    if (port >= 4u || bit >= 8u) {
        return 0xFFFFu;
    }
    return (uint16_t)(0xF000u + (port * 0x10u) + bit);  // PxxCFG
}

inline void decode_pin(uint16_t pin, uint8_t* port, uint8_t* bit) {
    *port = (uint8_t)((pin >> 3) & 0x07u);
    *bit = (uint8_t)(pin & 0x07u);
}

// M3: C language linkage for the C-ABI hook table (see the sys model).
extern "C" bool cms8s_gpio_may_drive(struct Mcu51Context* ctx, uint16_t pin,
                                      uint8_t level) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return true;
    }
    uint8_t port = 0u;
    uint8_t bit = 0u;
    decode_pin(pin, &port, &bit);
    if (port >= 4u || bit >= 8u) {
        return true;
    }
    const uint8_t tris = ctx->sfr_shadow[tris_addr(port)];
    if (((tris >> bit) & 1u) == 0u) {
        return false;  // INPUT direction: latch holds, no drive
    }
    const uint16_t od = od_addr(port);
    if (od != 0xFFFFu && ((ctx->xdata_shadow[od] >> bit) & 1u) != 0u &&
        level != 0u) {
        return false;  // open-drain release: HiZ, no drive
    }
    return true;
}

extern "C" bool cms8s_gpio_is_analog(struct Mcu51Context* ctx, uint16_t pin) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return false;
    }
    uint8_t port = 0u;
    uint8_t bit = 0u;
    decode_pin(pin, &port, &bit);
    const uint16_t cfg = cfg_addr(port, bit);
    return cfg != 0xFFFFu && ctx->xdata_shadow[cfg] == 0x01u;
}

extern "C" uint8_t cms8s_gpio_pullup(struct Mcu51Context* ctx, uint16_t pin) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return 0u;
    }
    uint8_t port = 0u;
    uint8_t bit = 0u;
    decode_pin(pin, &port, &bit);
    if (port >= 4u || bit >= 8u) {
        return 0u;
    }
    // HiZ input with internal pull-up defaults to 1 (A-05).
    const uint8_t tris = ctx->sfr_shadow[tris_addr(port)];
    const uint16_t up = up_addr(port);
    if (((tris >> bit) & 1u) == 0u && up != 0xFFFFu &&
        ((ctx->xdata_shadow[up] >> bit) & 1u) != 0u) {
        return 1u;
    }
    return 0u;
}

void install_hooks(struct Mcu51Context* ctx) {
    ctx->gpio_hooks.may_drive = cms8s_gpio_may_drive;
    ctx->gpio_hooks.is_analog = cms8s_gpio_is_analog;
    ctx->gpio_hooks.pullup = cms8s_gpio_pullup;
}

// ── P3 (PLAN-20260912-MCS51-P3-TRIS): direction-change drive semantics ─────
// Silicon latches the pad the moment TRIS flips to output (the previously
// suppressed latch value appears immediately); flipping to input releases
// the driver to HiZ. The SFR proxy stores the new TRIS value first and then
// invokes the hook, so new_val is authoritative; walk old^new.
inline uint16_t pin_key(uint8_t port, uint8_t bit) {
    return static_cast<uint16_t>((port << 3) | bit);
}

inline uint8_t latch_bit(Mcu51Context* ctx, uint8_t port, uint8_t bit) {
    return static_cast<uint8_t>(
        (ctx->sfr_shadow[0x80u + (port * 0x10u)] >> bit) & 1u);
}

inline bool od_active(Mcu51Context* ctx, uint8_t port, uint8_t bit) {
    const uint16_t od = od_addr(port);
    return (od != 0xFFFFu) &&
           (((ctx->xdata_shadow[od] >> bit) & 1u) != 0u);
}

// Drive the pad from the latched value (TRIS 0->1). Analog-configured pins
// have no digital driver; open-drain latch=1 is a HiZ release, not a high
// drive (may_drive hook parity).
void drive_from_latch(Mcu51Context* ctx, uint8_t port, uint8_t bit) {
    const uint16_t pin = pin_key(port, bit);
    if (cms8s_gpio_is_analog(ctx, pin)) {
        return;  // PxxCFG=AN: no digital drive
    }
    if (latch_bit(ctx, port, bit) == 0u) {
        js_pal_gpio_write(pin, false, MCS51_DRIVE_SUPPLY);
        return;
    }
    if (od_active(ctx, port, bit)) {
        js_pal_gpio_release_mcu(pin);
        return;
    }
    js_pal_gpio_write(pin, true, MCS51_DRIVE_WEAK);
}

// P3 hook: definition must carry C language linkage (mcs51_trap.h); the
// anonymous namespace supplies internal linkage, same as cms8s_gpio_may_drive.
extern "C" void on_tris_write(struct Mcu51Context* ctx, uint8_t addr,
                              uint8_t old_val, uint8_t new_val) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx || ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    const uint8_t port = port_from_tris_addr(addr);
    if (port >= 4u) {
        return;
    }
    const uint8_t changed = static_cast<uint8_t>(old_val ^ new_val);
    for (uint8_t bit = 0u; bit < 8u; ++bit) {
        if ((changed & static_cast<uint8_t>(1u << bit)) == 0u) {
            continue;
        }
        if (((new_val >> bit) & 1u) != 0u) {
            drive_from_latch(ctx, port, bit);  // input -> output: re-drive
        } else {
            js_pal_gpio_release_mcu(pin_key(port, bit));  // output -> input
        }
    }
}

void install_tris_hooks(void) {
    mcs51_trap_register_sfr_write(0x9Au, on_tris_write);  // P0TRIS
    mcs51_trap_register_sfr_write(0xA1u, on_tris_write);  // P1TRIS
    mcs51_trap_register_sfr_write(0xA2u, on_tris_write);  // P2TRIS
    mcs51_trap_register_sfr_write(0xA3u, on_tris_write);  // P3TRIS
}

}  // namespace

extern "C" {

void cms8s_gpio_init(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;  // belt-and-braces: the registry mask already filters
    }
    cms8s_soc_bind(ctx);  // bind BEFORE installing (ordering invariant)
    install_hooks(ctx);
    install_tris_hooks();
}

void cms8s_gpio_reset(struct Mcu51Context* ctx) {
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
    // P3: silicon reset leaves every pin an input (TRIS=0) with no drive.
    // Release any MCU driver left from the previous run; the bridge re-seeds
    // WEAK-HIGH after context reset (framework init order), so the net
    // power-on baseline is unchanged.
    for (uint16_t pin = 0u; pin < 32u; ++pin) {
        js_pal_gpio_release_mcu(pin);
    }
    install_hooks(ctx);  // reset memset cleared the table: reinstall
    install_tris_hooks();
}

}  // extern "C"
