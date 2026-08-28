// SPDX-License-Identifier: Apache-2.0
// MCS-51 SFR shadow + Level-2 trap table storage (C linkage, boundary ③).
// All tables are plain zero-initialised POD in BSS (static-init 铁律 1,
// ADR-0072 D5): safe to populate from C++ static constructors in any TU,
// before framework init runs.
#include "mcs51_trap.h"

#include <stddef.h>

extern "C" {

// 256-byte SFR address space (BSS; NOT reset by framework init — pin input
// injection between runtime runs relies on persistence, see test_mcs51_gpio).
uint8_t wink_mcs51_sfr_shadow[256] = {0};

// P0..P3 × 8 pins Level-2 instant traps.
mcs51_pin_trap_t wink_mcs51_pin_traps[4][8] = {};

// Internal-peripheral SFR hooks (TCON/SCON/ADCON/…).
mcs51_sfr_write_hook_t wink_mcs51_sfr_write_hooks[256] = {nullptr};
mcs51_sfr_read_hook_t  wink_mcs51_sfr_read_hooks[256]  = {nullptr};

void mcs51_trap_register_write(uint8_t port, uint8_t bit,
                               mcs51_pin_write_fn_t fn, void *ctx) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    wink_mcs51_pin_traps[port][bit].on_write = fn;
    wink_mcs51_pin_traps[port][bit].write_ctx = ctx;
}

void mcs51_trap_register_read(uint8_t port, uint8_t bit,
                              mcs51_pin_read_fn_t fn, void *ctx) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    wink_mcs51_pin_traps[port][bit].on_read = fn;
    wink_mcs51_pin_traps[port][bit].read_ctx = ctx;
}

void mcs51_trap_clear_pin(uint8_t port, uint8_t bit) {
    if (port >= 4u || bit >= 8u) {
        return;
    }
    wink_mcs51_pin_traps[port][bit] = mcs51_pin_trap_t{};
}

void mcs51_trap_register_sfr_write(uint8_t addr, mcs51_sfr_write_hook_t fn) {
    wink_mcs51_sfr_write_hooks[addr] = fn;
}

void mcs51_trap_register_sfr_read(uint8_t addr, mcs51_sfr_read_hook_t fn) {
    wink_mcs51_sfr_read_hooks[addr] = fn;
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
    for (uint8_t p = 0; p < 4u; ++p) {
        for (uint8_t b = 0; b < 8u; ++b) {
            wink_mcs51_pin_traps[p][b] = mcs51_pin_trap_t{};
        }
    }
    for (int a = 0; a < 256; ++a) {
        wink_mcs51_sfr_write_hooks[a] = nullptr;
        wink_mcs51_sfr_read_hooks[a] = nullptr;
    }
}

}  // extern "C"
