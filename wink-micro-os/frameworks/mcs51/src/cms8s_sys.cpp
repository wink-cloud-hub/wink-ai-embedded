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

// Power-on Fosc comes from the family descriptor (single source of truth
// with mcs51_context.cpp seeds); the 24 MHz ±1% datasheet value lives in
// the CMS8S78xx descriptor row.

// M2: TA phase lives in Mcu51Context::sysProt (was file-static s_sys).
// Hooks already carry ctx; reset_state takes it explicitly.
void reset_state(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    ctx->sysProt.ta_phase = 0;
}

// Returns true exactly once after a well-formed TA unlock sequence.
bool consume_unlock(Mcu51Context* ctx) {
    const bool ok = (ctx->sysProt.ta_phase == 2u);
    ctx->sysProt.ta_phase = 0u;
    return ok;
}

void on_ta_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!ctx) ctx = mcs51_get_context();
    (void)addr;
    (void)old_val;
    if (ctx->sysProt.ta_phase == 0u && new_val == TA_KEY1) {
        ctx->sysProt.ta_phase = 1u;
    } else if (ctx->sysProt.ta_phase == 1u && new_val == TA_KEY2) {
        ctx->sysProt.ta_phase = 2u;
    } else {
        // Any wrong/extra TA write aborts the sequence.
        ctx->sysProt.ta_phase = 0u;
    }
}

void on_clkdiv_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!consume_unlock(ctx)) {
        // Locked write is ignored by silicon: restore the previous value.
        ctx->sfr_shadow[addr] = old_val;
        return;
    }
    // Ref manual §4.2.1: div=0 -> Fsys = Fosc; otherwise Fsys = Fosc/(2*div).
    const uint32_t fosc = mcs51_family_desc(ctx->family)->fosc_hz;
    const uint32_t fsys = (new_val == 0u)
        ? fosc
        : fosc / (2u * static_cast<uint32_t>(new_val));
    wink_mcs51_set_hardware_clock_hz(fsys != 0u ? fsys : fosc);
    // Timers already pending at the old rate must be re-based immediately.
    wink_mcs51_timers_step_to(ctx->virtual_us);
}

void on_wdcon_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)new_val;
    if (!consume_unlock(ctx)) {
        ctx->sfr_shadow[addr] = old_val;
    }
    // WDT reset timing is not modelled: accepted (unlocked) writes simply
    // persist in the shadow register.
}

}  // namespace

extern "C" {

void cms8s_sys_reset(struct Mcu51Context* ctx) {
    reset_state(ctx);
}

void cms8s_sys_init(struct Mcu51Context* ctx) {
    reset_state(ctx);
    mcs51_trap_register_sfr_write(SFR_TA, on_ta_write);
    mcs51_trap_register_sfr_write(SFR_CLKDIV, on_clkdiv_write);
    mcs51_trap_register_sfr_write(SFR_WDCON, on_wdcon_write);
}

}  // extern "C"
