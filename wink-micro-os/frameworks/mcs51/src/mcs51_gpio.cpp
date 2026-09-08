// SPDX-License-Identifier: Apache-2.0
// MCS-51 GPIO dual-read path and pin arbitration implementation (Task R0).
#include "wink_mcs51_gpio.h"

#include "absacc.h"
#include "mcs51_trap.h"
#include "mcs51_context.h"

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
        const mcs51_pin_trap_t& trap = mcu->pin_traps[port][b];
        uint8_t pin_level;
        if (trap.on_read != nullptr) {
            pin_level = trap.on_read(trap.read_ctx) ? 1u : 0u;
        } else {
            const int ext = mcs51_ext_pin_level(port, b);
            if (ext < 0) {
                continue;  // HiZ/conflict: keep the latch bit
            }
            pin_level = static_cast<uint8_t>(ext);
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
    const uint8_t addr = static_cast<uint8_t>(0x80u + (port * 0x10u));
    const uint8_t old_val = mcu->sfr_shadow[addr];
    const uint8_t old_bit = static_cast<uint8_t>((old_val >> bit) & 1u);
    const uint8_t new_bit = level ? 1u : 0u;
    const uint8_t mask = static_cast<uint8_t>(1u << bit);
    const uint8_t new_val = new_bit ? static_cast<uint8_t>(old_val | mask)
                                    : static_cast<uint8_t>(old_val & ~mask);
    mcu->sfr_shadow[addr] = new_val;

    if (old_bit != new_bit) {
        const uint8_t strength = new_bit ? MCS51_DRIVE_WEAK : MCS51_DRIVE_SUPPLY;
        js_pal_gpio_write(static_cast<uint16_t>((port << 3) | bit),
                          new_bit != 0u, strength);
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
    const uint8_t addr = static_cast<uint8_t>(0x80u + (port * 0x10u));
    const uint8_t old_val = mcu->sfr_shadow[addr];
    mcu->sfr_shadow[addr] = new_val;

    const uint8_t diff = static_cast<uint8_t>(old_val ^ new_val);
    if (diff != 0u) {
        for (uint8_t b = 0; b < 8u; ++b) {
            if ((diff & static_cast<uint8_t>(1u << b)) != 0u) {
                const uint8_t level = static_cast<uint8_t>((new_val >> b) & 1u);
                const uint8_t strength = level ? MCS51_DRIVE_WEAK : MCS51_DRIVE_SUPPLY;
                js_pal_gpio_write(static_cast<uint16_t>((port << 3) | b),
                                  level != 0u, strength);
                const mcs51_pin_trap_t& trap = mcu->pin_traps[port][b];
                if (trap.on_write != nullptr) {
                    trap.on_write(trap.write_ctx, level);
                }
            }
        }
    }
}

}  // extern "C"
