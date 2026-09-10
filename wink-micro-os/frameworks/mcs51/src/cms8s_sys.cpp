// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx system-protection model:
//   - TA (Time Access) protection window for CLKDIV / WDCON writes. The
//     silicon ignores writes to protected SFRs unless immediately preceded
//     by TA = 0xAA; TA = 0x55; (ref manual §4.2); firmware must unlock every
//     write. The model reverts shadow updates that arrive locked so that
//     silicon-incorrect code fails in simulation instead of silently
//     "working" against a permissive register file.
//   - CLKDIV write hook: derives the simulated system clock from the
//     CMS8S78xx 24 MHz internal RC (Fsys = Fosc for div=0, else Fosc/(2*div)).
//   - WDCON register exists (TA-protected) so WDT enable/feed sequences are
//     exercisable; watchdog *reset* timing is not modelled yet.
#include "mcs51_context.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_timer.h"

#include <cstdint>


namespace {

constexpr uint8_t SFR_TA     = 0x96;
constexpr uint8_t SFR_CLKDIV = 0x8F;
constexpr uint8_t SFR_WDCON  = 0x97;

constexpr uint8_t TA_KEY1 = 0xAAu;
constexpr uint8_t TA_KEY2 = 0x55u;

// CMS8S78xx power-on internal RC oscillator (datasheet: 24 MHz ±1%).
constexpr uint32_t CMS8S_FOSC_HZ = 24000000u;

struct SysProtState {
    // 0 = waiting 0xAA, 1 = got 0xAA waiting 0x55, 2 = unlocked (next
    // protected write passes and consumes the window).
    uint8_t ta_phase;
};

SysProtState s_sys = {};

void reset_state() {
    s_sys.ta_phase = 0;
}

// Returns true exactly once after a well-formed TA unlock sequence.
bool consume_unlock() {
    const bool ok = (s_sys.ta_phase == 2u);
    s_sys.ta_phase = 0u;
    return ok;
}

void on_ta_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)ctx;
    (void)addr;
    (void)old_val;
    if (s_sys.ta_phase == 0u && new_val == TA_KEY1) {
        s_sys.ta_phase = 1u;
    } else if (s_sys.ta_phase == 1u && new_val == TA_KEY2) {
        s_sys.ta_phase = 2u;
    } else {
        // Any wrong/extra TA write aborts the sequence.
        s_sys.ta_phase = 0u;
    }
}

void on_clkdiv_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!consume_unlock()) {
        // Locked write is ignored by silicon: restore the previous value.
        ctx->sfr_shadow[addr] = old_val;
        return;
    }
    // Ref manual §4.2.1: div=0 -> Fsys = Fosc; otherwise Fsys = Fosc/(2*div).
    const uint32_t fsys = (new_val == 0u)
        ? CMS8S_FOSC_HZ
        : CMS8S_FOSC_HZ / (2u * static_cast<uint32_t>(new_val));
    wink_mcs51_set_hardware_clock_hz(fsys != 0u ? fsys : CMS8S_FOSC_HZ);
    // Timers already pending at the old rate must be re-based immediately.
    wink_mcs51_timers_step_to(ctx->virtual_us);
}

void on_wdcon_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)new_val;
    if (!consume_unlock()) {
        ctx->sfr_shadow[addr] = old_val;
    }
    // WDT reset timing is not modelled: accepted (unlocked) writes simply
    // persist in the shadow register.
}

}  // namespace

extern "C" {

void cms8s_sys_reset(struct Mcu51Context* ctx) {
    (void)ctx;
    reset_state();
}

void cms8s_sys_init(struct Mcu51Context* ctx) {
    (void)ctx;
    reset_state();
    mcs51_trap_register_sfr_write(SFR_TA, on_ta_write);
    mcs51_trap_register_sfr_write(SFR_CLKDIV, on_clkdiv_write);
    mcs51_trap_register_sfr_write(SFR_WDCON, on_wdcon_write);
}

}  // extern "C"
