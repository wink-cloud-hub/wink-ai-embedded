// SPDX-License-Identifier: Apache-2.0
// MCS-51 Level-2 instant-trap C ABI (boundary ③, ADR-0071 D1, AD-13).
//
// Peripheral models (ADC0832, CMS8S ADC, future bit-banged I2C/SPI) register
// POD function pointers here; the SFR proxy (mcs51_proxy.hpp) invokes them
// synchronously inside the very pin write/read statement, on the user fiber
// and in the same interception point. Static dispatch only (ADR-0004): no
// vtable, no container_of — the tables are plain zero-initialised C arrays in
// BSS (static-init 铁律 1), so registration during C++ static construction is
// always safe.
//
// Trap discipline (AD-13, four red lines): callbacks MUST (1) perform no
// delay/blocking call, (2) never yield the fiber, (3) be pure state machines
// touching only model state + the SFR shadow, and (4) never advance virtual
// time. Instant peripherals complete in 0 us (ADR-0072 D1).
//
// This header is C-safe: the 8051 SFR shadow is declared here too so models
// written in C or C++ share one ABI.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 256-byte SFR address space. Index = SFR address (P0=0x80 … P3=0xB0).
// Defined in mcs51_sfr.cpp (C linkage, BSS).
extern uint8_t wink_mcs51_sfr_shadow[256];

// ── GPIO pin traps: P0..P3, 8 pins each (AD-12/18) ─────────────────────────
typedef void (*mcs51_pin_write_fn_t)(void *ctx, uint8_t level);
typedef uint8_t (*mcs51_pin_read_fn_t)(void *ctx);

typedef struct {
    mcs51_pin_write_fn_t on_write;  // fired on a real edge (old level != new)
    void *write_ctx;
    mcs51_pin_read_fn_t  on_read;   // Read-Pin: external pin level reconstruction
    void *read_ctx;
} mcs51_pin_trap_t;

// [port 0..3][bit 0..7]. Zero-initialised BSS; a NULL callback means "no trap".
extern mcs51_pin_trap_t wink_mcs51_pin_traps[4][8];

// ── Internal-peripheral SFR hooks (port_idx 0xFF: TCON/SCON/ADCON/…) ────────
// Write hook sees the latch already updated (old/new values for edge detect);
// read hook runs lazily BEFORE the value is observed (timer TF evaluation).
typedef void (*mcs51_sfr_write_hook_t)(uint8_t addr, uint8_t old_val,
                                       uint8_t new_val);
typedef void (*mcs51_sfr_read_hook_t)(uint8_t addr);

extern mcs51_sfr_write_hook_t wink_mcs51_sfr_write_hooks[256];
extern mcs51_sfr_read_hook_t  wink_mcs51_sfr_read_hooks[256];

// Registration API (idempotent; used by peripheral init / tests).
// port 0..3 = P0..P3; out-of-range port/bit is ignored.
void mcs51_trap_register_write(uint8_t port, uint8_t bit,
                               mcs51_pin_write_fn_t fn, void *ctx);
void mcs51_trap_register_read(uint8_t port, uint8_t bit,
                              mcs51_pin_read_fn_t fn, void *ctx);
void mcs51_trap_clear_pin(uint8_t port, uint8_t bit);

// Internal SFR hooks (any SFR address; e.g. 0x88 TCON, 0xE1 CMS8S ADCON).
void mcs51_trap_register_sfr_write(uint8_t addr, mcs51_sfr_write_hook_t fn);
void mcs51_trap_register_sfr_read(uint8_t addr, mcs51_sfr_read_hook_t fn);

// Test isolation: detach every pin trap and clear every SFR hook.
void mcs51_trap_reset(void);

// Framework bridge extension (defined in mcs51_bridge.cpp). The framework init
// callback runs mcs51_trap_reset() + internal SFR-hook registration on every
// runtime run; a post-init hook fires AFTER that, so test harnesses and
// non-codegen boards can (re)bind pin traps that survive init. Register it
// before wink_runtime_run(); NULL (default) means none.
typedef void (*mcs51_framework_post_init_fn_t)(void);
void mcs51_framework_set_post_init_hook(mcs51_framework_post_init_fn_t hook);
void mcs51_framework_run_post_init_hook(void);  // bridge calls this

#ifdef __cplusplus
}  // extern "C"
#endif
