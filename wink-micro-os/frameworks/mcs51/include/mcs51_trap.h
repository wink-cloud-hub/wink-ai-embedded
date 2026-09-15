// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 Level-2 instant-trap C ABI (boundary ③, ADR-0071 D1, AD-13).
//
// Peripheral models (board devices, on-chip converters, future bit-banged
// I2C/SPI) register
// POD function pointers here; the SFR proxy (mcs51_proxy.hpp) invokes them
// synchronously inside the very pin write/read statement, on the user fiber
// and in the same interception point. Static dispatch only (ADR-0004): no
// vtable, no container_of — tables are managed in Mcu51Context.
//
// Trap discipline (AD-13, four red lines): callbacks MUST (1) perform no
// delay/blocking call, (2) never yield the fiber, (3) be pure state machines
// touching only model state + the SFR shadow, and (4) never advance virtual
// time. Instant peripherals complete in 0 us (ADR-0072 D1).
//
// Shadow/hook write ordering (M6): the SFR proxy ALWAYS stores the new value
// into sfr_shadow first, then invokes the write hook with (old, new). A hook
// that needs non-trivial semantics (e.g. write-0-to-clear: shadow = old &
// new) OVERWRITES the shadow; a hook that only observes (timer re-arm) must
// leave it alone. Model-internal poll/step code writes the shadow DIRECTLY
// without going through the bridge (no microstep is charged there) — hooks
// must therefore never assume they observe every shadow mutation, only
// firmware-issued writes via the proxy.
//
// Diagnosis-by-counter contract (M4): this framework reports fallible
// conditions via monotonic counters + warn-once logs, NOT via wink_status_t.
// Rationale: interception points (SFR/XDATA access, ISR registration) cannot
// propagate errors into unmodified Keil firmware. Consumers (GAP-10 runner,
// headless scenario verdicts) poll the counters: xdata OOB, unmodeled-XSFR,
// UART-notready, unsupported-feature, duplicate-vector. STRICT builds abort
// at the offending site instead of counting. Diagnostic counters are
// PROCESS-level (file-static, shared across contexts); silicon state is
// PER-CONTEXT (Mcu51Context fields, cleared by mcs51_context_reset).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// ── GPIO pin traps: P0..P3, 8 pins each (AD-12/18) ─────────────────────────
typedef void (*mcs51_pin_write_fn_t)(void *ctx, uint8_t level);
typedef uint8_t (*mcs51_pin_read_fn_t)(void *ctx);

typedef struct {
    mcs51_pin_write_fn_t on_write;  // fired on a real edge (old level != new)
    void *write_ctx;
    mcs51_pin_read_fn_t  on_read;   // Read-Pin: external pin level reconstruction
    void *read_ctx;
} mcs51_pin_trap_t;

// ── Internal-peripheral SFR hooks (port_idx 0xFF: TCON/SCON/ADCON/PCON) ────
// Hook signatures explicitly carry struct Mcu51Context* ctx (Task R2 / R6).
// M3: hook DEFINITIONS must carry C language linkage (extern "C") to match
// these C-ABI typedefs — even when file-local. The enclosing anonymous
// namespace already gives internal linkage; a `static` specifier inside the
// linkage specification is legal (all three toolchains accept it) but
// redundant, so prefer the anonymous-namespace form. See a chip system
// model for the pattern.
typedef void (*mcs51_sfr_write_hook_t)(struct Mcu51Context* ctx, uint8_t addr,
                                        uint8_t old_val, uint8_t new_val);
typedef void (*mcs51_sfr_read_hook_t)(struct Mcu51Context* ctx, uint8_t addr);

// ── Pre-dispatch SFR-write notify (Stage3 S3-2: TA hook, GAP-07) ───────────
// Single per-context slot (stored in Mcu51Context::sfr_write_notify, memset
// zero = none). The bridge invokes it BEFORE the per-address hook dispatch,
// so a half-open TA window observes the intervening firmware write first and
// the pending protected write arrives locked and rolls back. Installed by
// the owning chip init with the explicit ctx pointer — no
// active-context dependence; cleared by context reset / trap reset like
// every hook. File-static globals are rejected here: two contexts of
// different families would cross-talk (total §3.1b-4).
typedef void (*mcs51_sfr_write_notify_fn_t)(struct Mcu51Context* ctx,
                                            uint8_t addr);

// ── XDATA/XSFR window validation hook (Stage5 CPL-08) ──────────────────────
// Per-context single slot (Mcu51Context::xsfr_validate, memset zero = none).
// When the ACTIVE family publishes an extended-SFR MOVX window
// (descriptor xsfr_base/xsfr_size), the generic xdata path asks the chip
// package whether an in-window address is a DECLARED chip register; an
// address the chip does not own feeds the GAP-23 unmodeled tripwire. The
// generic window test runs FIRST, so classic parts (no window) never reach
// this hook. File-static globals are rejected here (dual-context
// cross-talk, total §3.1b-4): the chip installs the slot on its context
// reset. Returns true when the address is chip-declared.
// The explicit ctx parameter follows the hook convention above (S5-H2):
// a static-declaration chip ignores it, but banked/paged XSFR windows can
// resolve per-context state without an ABI change.
typedef bool (*mcs51_xsfr_validate_fn_t)(struct Mcu51Context* ctx,
                                         uint64_t addr);

// ── GPIO Trait hooks (Stage2 S2-1 declares, stage4 mounts) ─────────────────
// Per-context function table (stored BY VALUE in Mcu51Context::gpio_hooks,
// memset zero = unhooked). Lets enhanced families override pin behavior
// without branching generic code: may_drive (pin can drive the given level;
// open-drain release suppresses high drive only, so the level travels with
// the query — S4-D1), is_analog (pin muxed to analog, digital reads HiZ),
// pullup (effective weak pull-up level for input reads: nonzero = pulls
// high). Standard parts take the caps_cache fast path and never pay an
// indirection (S2-1: pure declaration; S4-D1 widened may_drive with level).
typedef bool (*mcs51_gpio_may_drive_fn_t)(struct Mcu51Context* ctx,
                                           uint16_t pin, uint8_t level);
typedef bool (*mcs51_gpio_is_analog_fn_t)(struct Mcu51Context* ctx,
                                          uint16_t pin);
typedef uint8_t (*mcs51_gpio_pullup_fn_t)(struct Mcu51Context* ctx,
                                          uint16_t pin);

typedef struct {
    mcs51_gpio_may_drive_fn_t may_drive;
    mcs51_gpio_is_analog_fn_t is_analog;
    mcs51_gpio_pullup_fn_t    pullup;
} Mcs51GpioHooks;

// ── UART readiness/baud hooks (S4-D3) ─────────────────────────────────────
// Same static-dispatch discipline as the GPIO hooks: the core owns the TX
// engine (SBUF hook, capture, TI/IRQ, charge application) while the chip
// package owns source selection (clock-select register + pin-remap).
// Per-context table (stored BY VALUE in Mcu51Context::uart_hooks, memset
// zero = standard Timer1 path). Standard parts take the caps_cache fast
// path (UART_REMAP) and never pay an indirection.
typedef uint32_t (*mcs51_uart_notready_mask_fn_t)(struct Mcu51Context* ctx);
typedef uint32_t (*mcs51_uart_baud_hz_fn_t)(struct Mcu51Context* ctx);

typedef struct {
    mcs51_uart_notready_mask_fn_t notready_mask;
    mcs51_uart_baud_hz_fn_t       baud_hz;
} Mcs51UartHooks;

// Registration API (idempotent; operates on active Mcu51Context).
// port 0..3 = P0..P3; out-of-range port/bit is ignored.
void mcs51_trap_register_write(uint8_t port, uint8_t bit,
                               mcs51_pin_write_fn_t fn, void *ctx);
void mcs51_trap_register_read(uint8_t port, uint8_t bit,
                              mcs51_pin_read_fn_t fn, void *ctx);
void mcs51_trap_clear_pin(uint8_t port, uint8_t bit);

// Internal SFR hooks (any SFR address; e.g. 0x88 TCON, 0xDF ADCON0, 0x87 PCON).
void mcs51_trap_register_sfr_write(uint8_t addr, mcs51_sfr_write_hook_t fn);
void mcs51_trap_register_sfr_read(uint8_t addr, mcs51_sfr_read_hook_t fn);

// Test isolation: detach every pin trap and clear every SFR hook in active context.
void mcs51_trap_reset(void);

// Framework bridge extension (defined in mcs51_bridge.cpp). The framework init
// callback runs peripheral init + trap_reset; post-init test seam (Task R1)
// fires AFTER that, so test harnesses can dynamically bind pin traps.
typedef void (*mcs51_framework_post_init_fn_t)(void);
void mcs51_framework_set_post_init_hook(mcs51_framework_post_init_fn_t hook);
void mcs51_framework_run_post_init_hook(void);

// Task R1 test seam naming:
static inline void mcs51_test_bind_pin_traps(mcs51_framework_post_init_fn_t hook) {
    mcs51_framework_set_post_init_hook(hook);
}

#ifdef __cplusplus
}  // extern "C"
#endif
