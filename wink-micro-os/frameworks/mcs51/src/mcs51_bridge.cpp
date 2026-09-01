// SPDX-License-Identifier: Apache-2.0
// MCS-51 simulation bridge (boundary ④): binds the cleaned Keil user program
// into the Wink cooperative runtime and wires the interception points to the
// virtual clock and peripheral models.
//
// The user `main` (remapped to wink_mcs51_user_main by REGX52.H) is a bare-metal
// init+`while(1)` super-loop that never returns. We expose it as the runtime
// `loop` callback; the runtime wraps that in its app_main fiber. Every SFR
// access and every `_nop_()` funnels through the virtual clock, which charges
// functional microseconds and force-yields the fiber on quota (ADR-0072 D1~D3),
// while timer overflows are evaluated at catch-up/resume points.
//
// M4 data plane: the proxy (mcs51_proxy.hpp) owns latch updates, GPIO diff
// edge dispatch and pin traps; this TU owns the non-GPIO SFR hook table —
// timer lazy evaluation and UART TX are registered as SFR read/write hooks at
// framework init, and the two proxy interception entries below dispatch that
// table plus the microstep charge.
#include "pal_osal.h"
#include "wink_app.h"

#include "ADC0832.H"
#include "absacc.h"
#include "cms8s_adc.h"
#include "mcs51_adc.h"
#include "mcs51_proxy.hpp"  // js_pal_gpio_write + MCS51_DRIVE_WEAK (ADR-0077)
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_extint.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include "wink_mcs51_timer.h"
#include "wink_mcs51_uart.h"

#include <cstdint>

// Optional firmware-time board description, emitted by the wink-tools codegen
// from wink-app.json (mcs51_board_config.h.j2). Absent in unit tests — they
// bind peripherals directly. When present it defines MCS51_HAS_ADC0832 plus
// the CS/CLK/DI/DO port+bit pin constants.
#if defined(__has_include)
#  if __has_include("mcs51_board_config.h")
#    include "mcs51_board_config.h"
#    define MCS51_BOARD_CONFIG_PRESENT 1
#  endif
#endif

// The user entry point, emitted by the cleaned TU as C linkage (REGX52.H
// boundary ①). Defined by the linked user program.
extern "C" void wink_mcs51_user_main(void);

namespace {

// ── SFR hook adapters: model TUs keep their (addr)-style entry points ───────
void sfr_write_hook_timer(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)old_val;
    (void)new_val;
    wink_mcs51_timer_on_write(addr);
}
void sfr_write_hook_uart(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)old_val;
    (void)new_val;
    wink_mcs51_uart_on_write(addr);
}
void sfr_read_hook_timer(uint8_t addr) {
    wink_mcs51_timer_on_read(addr);
}

// SFR addresses the existing models care about (see mcs51_timer.cpp /
// mcs51_uart.cpp): TCON 0x88 (read: lazy TF eval; write: TR start/stop),
// TMOD 0x89 + TH/TL 0x8A..0x8D (write: reload/config), SBUF 0x99 (write: TX).
constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_TMOD = 0x89;
constexpr uint8_t SFR_TL0  = 0x8A;
constexpr uint8_t SFR_TL1  = 0x8B;
constexpr uint8_t SFR_TH0  = 0x8C;
constexpr uint8_t SFR_TH1  = 0x8D;
constexpr uint8_t SFR_SBUF = 0x99;

// One-time framework bring-up (runtime init callback, runs on the host/main
// context before the fiber is registered). Static C++ registration has already
// populated the ISR vector table; here we reset model state, rebuild the
// Level-2 trap/hook tables (M4), wire the catch-up hook, and open the
// execution-phase interrupt gate (铁律 3, ADR-0072 D5).
void mcs51_framework_init(void) {
    wink_mcs51_clock_reset();
    wink_mcs51_timers_reset();
    wink_mcs51_uart_reset();
    wink_mcs51_extint_reset();
    wink_mcs51_xdata_reset();
    wink_mcs51_unsupported_reset();

    // M4: clear all Level-2 pin traps + SFR hooks, then re-register the
    // internal-peripheral hooks. (Board peripherals are bound afterwards, so
    // codegen/static pin traps survive — anything registered before this init
    // point is intentionally wiped for run-to-run test isolation.)
    mcs51_trap_reset();
    mcs51_adc_reset();

    // Power-on port state (ADR-0077): a real 8051 leaves P0..P3 latched at
    // 0xFF after reset — every pin is a quasi-bidirectional input held high by
    // the weak internal pull-up. Seed BOTH halves of that state:
    //   1. the latch shadow = 0xFF, so firmware's first `Pn = 0xFF` input-init
    //      write computes diff==0 (no edge, mirroring silicon that never edges);
    //   2. an explicit WEAK-HIGH driver registration on all 32 pins, so the
    //      host PinArbiter knows the MCU weakly drives high even before the
    //      firmware's first pin edge (an input button then arbitrates WEAK vs
    //      SUPPLY-low instead of reading HiZ). Shadow alone is insufficient:
    //      with no registered driver the host side sees no MCU drive at all.
    wink_mcs51_sfr_shadow[0x80] = 0xFFu;  // P0
    wink_mcs51_sfr_shadow[0x90] = 0xFFu;  // P1
    wink_mcs51_sfr_shadow[0xA0] = 0xFFu;  // P2
    wink_mcs51_sfr_shadow[0xB0] = 0xFFu;  // P3
    for (uint16_t pin = 0u; pin < 32u; ++pin) {
        js_pal_gpio_write(pin, true, MCS51_DRIVE_WEAK);
    }

    mcs51_trap_register_sfr_read(SFR_TCON, &sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TCON, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TMOD, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL0, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL1, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH0, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH1, &sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_SBUF, &sfr_write_hook_uart);

    // CMS8S78xx on-chip ADC: ADCON0 write hook (0-cycle instant conversion).
    cms8s_adc_init();

#ifdef MCS51_HAS_ADC0832
    // Codegen-provided board: external ADC0832 on fixed pins (runtime app_init
    // static binding, zero JSON dependency — umbrella SSOT §3.3).
    mcs51_adc0832_init(MCS51_PIN_ADC0832_CS_PORT,  MCS51_PIN_ADC0832_CS_BIT,
                       MCS51_PIN_ADC0832_CLK_PORT, MCS51_PIN_ADC0832_CLK_BIT,
                       MCS51_PIN_ADC0832_DI_PORT,  MCS51_PIN_ADC0832_DI_BIT,
                       MCS51_PIN_ADC0832_DO_PORT,  MCS51_PIN_ADC0832_DO_BIT);
#endif

    // Test/board extension hook: runs AFTER trap_reset + internal hook
    // registration, so harnesses (and non-codegen boards) can bind pin traps
    // that survive the framework-init reset. Registered before runtime run.
    mcs51_framework_run_post_init_hook();

    wink_mcs51_set_catchup_hook(wink_mcs51_timers_step_to);
    wink_mcs51_isr_enable();
}

}  // namespace

extern "C" {

// Interception point for <intrins.h> _nop_(): charge one functional microstep.
// Also the fiber-context rendezvous for events pushed from outside the fiber:
// queued UART RX bytes drain here (RI latch + vector 4 on the firmware's own
// context, never re-entered from the JS/host pusher).
void wink_mcs51_microstep(void) {
    wink_mcs51_charge_us(WINK_MCS51_MICROSTEP_US);
    wink_mcs51_uart_rx_drain();
    wink_mcs51_extint_poll();
}

// SFR proxy interception entries (boundary ③ crosses into this TU). The proxy
// has already updated the latch shadow before calling these.
//
// Read: run the registered SFR read-hook (timer lazy TF evaluation must close
// a `while(!TF0)` poll on the read) then charge one microstep.
void wink_mcs51_on_sfr_read(uint8_t addr) {
    mcs51_sfr_read_hook_t hook = wink_mcs51_sfr_read_hooks[addr];
    if (hook != nullptr) {
        hook(addr);
    }
    wink_mcs51_microstep();
}

// Write: run the registered SFR write-hook (SBUF TX emits + sets TI + vectors
// synchronously; timer TR/reload latches) then charge one microstep — so a
// quota-triggered catch-up on the same interception point steps the freshly
// configured timer.
void wink_mcs51_on_sfr_write(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    mcs51_sfr_write_hook_t hook = wink_mcs51_sfr_write_hooks[addr];
    if (hook != nullptr) {
        hook(addr, old_val, new_val);
    }
    wink_mcs51_microstep();
}

}  // extern "C"

// ── Runtime callback binding (boundary ④) ───────────────────────────────────
// Provided with C linkage: the Keil user program never defines wink callbacks
// (it only defines main -> wink_mcs51_user_main), so the interception layer
// supplies them. C linkage lets the C host test, the C wasm entry
// (wasm_entry.c), and the C runtime all resolve the same symbol.
namespace {
void mcs51_app_loop(void) {
    wink_mcs51_user_main();  // bare-metal super-loop; does not return
}
}  // namespace

// Strong, single definition (NOT weak): the Keil user program never supplies
// wink callbacks, so the interception layer is the sole provider. On MinGW/PE a
// weak function definition with no strong fallback resolves to null (unlike
// ELF), which broke the host link; a plain strong extern "C" definition is
// correct for both host and wasm.
extern "C" const wink_app_callbacks_t* wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t s_mcs51_callbacks = {
        mcs51_framework_init,  // init  (framework bring-up; user does its own in main)
        mcs51_app_loop,        // loop  (the bare-metal super-loop; never returns)
        nullptr,               // on_fault
        nullptr,               // on_boot
        nullptr,               // init_status
        nullptr,               // on_fault_status
        nullptr,               // on_event
    };
    return &s_mcs51_callbacks;
}
