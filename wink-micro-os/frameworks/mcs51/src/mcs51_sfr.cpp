// SPDX-License-Identifier: Apache-2.0
// MCS-51 Level-2 trap table operations (boundary ③).
// Operates on Mcu51Context (Task R2).
#include "mcs51_trap.h"
#include "mcs51_context.h"

#include <stddef.h>

extern "C" {

void mcs51_trap_register_write(uint8_t port, uint8_t bit,
                               mcs51_pin_write_fn_t fn, void *ctx) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    Mcu51Context* mcu = mcs51_get_context();
    mcu->pin_traps[port][bit].on_write = fn;
    mcu->pin_traps[port][bit].write_ctx = ctx;
}

void mcs51_trap_register_read(uint8_t port, uint8_t bit,
                              mcs51_pin_read_fn_t fn, void *ctx) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    Mcu51Context* mcu = mcs51_get_context();
    mcu->pin_traps[port][bit].on_read = fn;
    mcu->pin_traps[port][bit].read_ctx = ctx;
}

void mcs51_trap_clear_pin(uint8_t port, uint8_t bit) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    Mcu51Context* mcu = mcs51_get_context();
    mcu->pin_traps[port][bit] = mcs51_pin_trap_t{};
}

void mcs51_trap_register_sfr_write(uint8_t addr, mcs51_sfr_write_hook_t fn) {
    Mcu51Context* mcu = mcs51_get_context();
    mcu->sfr_write_hooks[addr] = fn;
}

void mcs51_trap_register_sfr_read(uint8_t addr, mcs51_sfr_read_hook_t fn) {
    Mcu51Context* mcu = mcs51_get_context();
    mcu->sfr_read_hooks[addr] = fn;
}

// Framework post-init hook (set by tests/boards; invoked by the bridge after
// trap_reset + internal hook registration on every runtime run).
static mcs51_framework_post_init_fn_t s_post_init_hook = nullptr;

void mcs51_framework_set_post_init_hook(mcs51_framework_post_init_fn_t hook) {
    s_post_init_hook = hook;
}

void mcs51_framework_run_post_init_hook(void) {
    if (s_post_init_hook != nullptr) {
        s_post_init_hook();
    }
}

void mcs51_trap_reset(void) {
    Mcu51Context* mcu = mcs51_get_context();
    for (uint8_t p = 0; p < 4u; ++p) {
        for (uint8_t b = 0; b < 8u; ++b) {
            mcu->pin_traps[p][b] = mcs51_pin_trap_t{};
        }
    }
    for (int a = 0; a < 256; ++a) {
        mcu->sfr_write_hooks[a] = nullptr;
        mcu->sfr_read_hooks[a] = nullptr;
    }
}

}  // extern "C"
