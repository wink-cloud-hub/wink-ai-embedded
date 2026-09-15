// SPDX-License-Identifier: GPL-3.0-only
// MCS-51 GPIO dual-read path and pin arbitration implementation (Task R0).
#include "wink_mcs51_gpio.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "absacc.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "mcs51_context.h"
#include "wink_mcs51_ext_bus.h"

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

extern "C" {
void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength);
uint8_t js_pal_gpio_read_state(uint16_t pin);
}

namespace {

inline int mcs51_ext_pin_level(uint8_t port, uint8_t bit) {
    const uint8_t st = js_pal_gpio_read_state(
        static_cast<uint16_t>((static_cast<uint16_t>(port) << 3) | bit));
    return st == 1u ? 1 : (st == 0u ? 0 : -1);
}

inline uint16_t gpio_pin_key(uint8_t port, uint8_t bit) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(port) << 3) | bit);
}

// Stage4 CPL-03: enhanced-IO behavior (direction/open-drain/analog/pull-up)
// lives in the chip package behind per-context hooks. Standard parts take
// the caps_cache fast path (ADR-0004: zero indirection); unhooked enhanced
// contexts (pre-stage6 production without family glue) degrade permissively
// to classic quasi-bidirectional behavior, never suppress.
inline bool gpio_may_drive(Mcu51Context* mcu, uint8_t port, uint8_t bit,
                           uint8_t level) {
    if (mcu == nullptr ||
        (mcu->caps_cache & MCS51_CAP_ENHANCED_IO) == 0u) {
        return true;
    }
    const mcs51_gpio_may_drive_fn_t fn = mcu->gpio_hooks.may_drive;
    if (fn == nullptr) {
        return true;
    }
    return fn(mcu, gpio_pin_key(port, bit), level);
}

inline bool gpio_is_analog(Mcu51Context* mcu, uint8_t port, uint8_t bit) {
    if (mcu == nullptr ||
        (mcu->caps_cache & MCS51_CAP_ENHANCED_IO) == 0u) {
        return false;
    }
    const mcs51_gpio_is_analog_fn_t fn = mcu->gpio_hooks.is_analog;
    if (fn == nullptr) {
        return false;
    }
    return fn(mcu, gpio_pin_key(port, bit));
}

inline bool gpio_pullup_active(Mcu51Context* mcu, uint8_t port, uint8_t bit) {
    if (mcu == nullptr ||
        (mcu->caps_cache & MCS51_CAP_ENHANCED_IO) == 0u) {
        return false;
    }
    const mcs51_gpio_pullup_fn_t fn = mcu->gpio_hooks.pullup;
    if (fn == nullptr) {
        return false;
    }
    return fn(mcu, gpio_pin_key(port, bit)) != 0u;
}

// Diagnostic counters (file-static, GAP-10 pattern). STRICT aborts on
// analog-read before counting; suppressed-output counts in both modes
// (STRICT counting path is reachable since suppression is not fatal).
uint32_t s_gpio_suppressed = 0u;
uint32_t s_gpio_analog_read = 0u;
#ifndef WINK_MCS51_STRICT
bool s_gpio_suppressed_warned = false;
bool s_gpio_analog_warned = false;
#endif

inline void gpio_suppressed_policy(void) {
#ifdef WINK_MCS51_STRICT
    if (s_gpio_suppressed < 0xFFFFFFFFu) {
        ++s_gpio_suppressed;
    }
#else
    if (s_gpio_suppressed < 0xFFFFFFFFu) {
        ++s_gpio_suppressed;
    }
    if (!s_gpio_suppressed_warned) {
        s_gpio_suppressed_warned = true;
        pal_log_w("MCS51", "GPIO write to input-direction pin suppressed (TRIS/OD)");
    }
#endif
}

inline void gpio_analog_read_policy(void) {
#ifdef WINK_MCS51_STRICT
    assert(0 && "GPIO digital read of analog-configured pin (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (s_gpio_analog_read < 0xFFFFFFFFu) {
        ++s_gpio_analog_read;
    }
    if (!s_gpio_analog_warned) {
        s_gpio_analog_warned = true;
        pal_log_w("MCS51", "GPIO digital read of analog-configured pin (PxxCFG=AN)");
    }
#endif
}

// True when an output latch edge may drive the external pin. Enhanced
// families decide through the may_drive hook; classic parts (and unhooked
// contexts) drive unconditionally (legacy quasi-bidirectional).
//
// NOTE: the pre-stage4 TRIS/OD gating implementation moved verbatim to the
// chip package (enhanced-GPIO model); this dispatcher carries no addresses.

}  // namespace

extern "C" {

uint8_t mcs51_gpio_read_latch(uint8_t port) {
    if (port >= 4u) {
        return 0u;
    }
    const uint8_t addr = static_cast<uint8_t>(0x80u + (port * 0x10u));
    return mcs51_get_context()->sfr_shadow[addr];
}

uint8_t mcs51_gpio_bit_read_latch(uint8_t port, uint8_t bit) {
    if (port >= 4u || bit >= 8u) {
        return 0u;
    }
    return static_cast<uint8_t>((mcs51_gpio_read_latch(port) >> bit) & 1u);
}

uint8_t mcs51_gpio_bit_read_pin(uint8_t port, uint8_t bit) {
    if (port >= 4u || bit >= 8u) {
        return 0u;
    }
    Mcu51Context* mcu = mcs51_get_context();
    // A-05: analog-configured pin has no valid digital input on silicon.
    if (gpio_is_analog(mcu, port, bit)) {
        gpio_analog_read_policy();
        return mcs51_gpio_bit_read_latch(port, bit);
    }
    // Priority 1: internal on_read trap (e.g. ADC0832 DO line)
    const mcs51_pin_trap_t& trap = mcu->pin_traps[port][bit];
    if (trap.on_read != nullptr) {
        return trap.on_read(trap.read_ctx) ? 1u : 0u;
    }
    // Priority 2: UniSim channel-1 external level (js_pal_gpio_read_state)
    const int ext = mcs51_ext_pin_level(port, bit);
    if (ext >= 0) {
        return static_cast<uint8_t>(ext);
    }
    // A-05: HiZ input with internal pull-up defaults to 1 (hook decides).
    if (gpio_pullup_active(mcu, port, bit)) {
        return 1u;
    }
    // Priority 3: HiZ / Conflict fallback to port latch shadow
    return mcs51_gpio_bit_read_latch(port, bit);
}

uint8_t mcs51_gpio_read_pin(uint8_t port) {
    if (port >= 4u) {
        return 0u;
    }
    Mcu51Context* mcu = mcs51_get_context();
    uint8_t val = mcs51_gpio_read_latch(port);
    for (uint8_t b = 0; b < 8u; ++b) {
        // A-05: analog pins return latch (counted in bit path).
        if (gpio_is_analog(mcu, port, b)) {
            gpio_analog_read_policy();
            continue;
        }
        const mcs51_pin_trap_t& trap = mcu->pin_traps[port][b];
        uint8_t pin_level;
        if (trap.on_read != nullptr) {
            pin_level = trap.on_read(trap.read_ctx) ? 1u : 0u;
        } else {
            const int ext = mcs51_ext_pin_level(port, b);
            if (ext < 0) {
                // A-05: HiZ + pull-up defaults to 1 (hook decides).
                if (gpio_pullup_active(mcu, port, b)) {
                    pin_level = 1u;
                } else {
                    continue;  // HiZ/conflict: keep the latch bit
                }
            } else {
                pin_level = static_cast<uint8_t>(ext);
            }
        }
        val = pin_level ? static_cast<uint8_t>(val | (1u << b))
                        : static_cast<uint8_t>(val & ~(1u << b));
    }
    return val;
}

void mcs51_gpio_bit_write(uint8_t port, uint8_t bit, uint8_t level) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    Mcu51Context* mcu = mcs51_get_context();
    // GAP-24: any firmware use of a bus pin counts toward the classic
    // MOVX/GPIO conflict verdict (idempotent writes included).
    mcs51_ext_bus_notify_gpio(mcu, port, static_cast<uint8_t>(1u << bit));
    const uint8_t addr = static_cast<uint8_t>(0x80u + (port * 0x10u));
    const uint8_t old_val = mcu->sfr_shadow[addr];
    const uint8_t old_bit = static_cast<uint8_t>((old_val >> bit) & 1u);
    const uint8_t new_bit = level ? 1u : 0u;
    const uint8_t mask = static_cast<uint8_t>(1u << bit);
    const uint8_t new_val = new_bit ? static_cast<uint8_t>(old_val | mask)
                                    : static_cast<uint8_t>(old_val & ~mask);
    mcu->sfr_shadow[addr] = new_val;

    if (old_bit != new_bit) {
        // A-05: input direction / open-drain release produces no drive.
        // Latch still updates; internal write traps still fire.
        if (!gpio_may_drive(mcu, port, bit, new_bit)) {
            gpio_suppressed_policy();
        } else {
            const uint8_t strength = new_bit ? MCS51_DRIVE_WEAK : MCS51_DRIVE_SUPPLY;
            js_pal_gpio_write(static_cast<uint16_t>((port << 3) | bit),
                              new_bit != 0u, strength);
        }
        const mcs51_pin_trap_t& trap = mcu->pin_traps[port][bit];
        if (trap.on_write != nullptr) {
            trap.on_write(trap.write_ctx, new_bit);
        }
    }
}

void mcs51_gpio_sfr_write(uint8_t port, uint8_t new_val) {
    if (port >= 4u) {
        return;
    }
    Mcu51Context* mcu = mcs51_get_context();
    // GAP-24: whole-port write touches all 8 pins (see bit path above).
    mcs51_ext_bus_notify_gpio(mcu, port, 0xFFu);
    const uint8_t addr = static_cast<uint8_t>(0x80u + (port * 0x10u));
    const uint8_t old_val = mcu->sfr_shadow[addr];
    mcu->sfr_shadow[addr] = new_val;

    const uint8_t diff = static_cast<uint8_t>(old_val ^ new_val);
    if (diff != 0u) {
        for (uint8_t b = 0; b < 8u; ++b) {
            if ((diff & static_cast<uint8_t>(1u << b)) != 0u) {
                const uint8_t level = static_cast<uint8_t>((new_val >> b) & 1u);
                // A-05: same gating as the single-bit path.
                if (!gpio_may_drive(mcu, port, b, level)) {
                    gpio_suppressed_policy();
                } else {
                    const uint8_t strength = level ? MCS51_DRIVE_WEAK : MCS51_DRIVE_SUPPLY;
                    js_pal_gpio_write(static_cast<uint16_t>((port << 3) | b),
                                      level != 0u, strength);
                }
                const mcs51_pin_trap_t& trap = mcu->pin_traps[port][b];
                if (trap.on_write != nullptr) {
                    trap.on_write(trap.write_ctx, level);
                }
            }
        }
    }
}

uint32_t wink_mcs51_gpio_output_suppressed_count(void) {
    return s_gpio_suppressed;
}

uint32_t wink_mcs51_gpio_analog_read_count(void) {
    return s_gpio_analog_read;
}

uint32_t wink_mcs51_gpio_diag_total(void) {
    return s_gpio_suppressed + s_gpio_analog_read;
}

void wink_mcs51_gpio_diag_reset(void) {
    s_gpio_suppressed = 0u;
    s_gpio_analog_read = 0u;
#ifndef WINK_MCS51_STRICT
    s_gpio_suppressed_warned = false;
    s_gpio_analog_warned = false;
#endif
}

}  // extern "C"
