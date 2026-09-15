// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 simulation bridge (boundary ④): binds the cleaned Keil user program
// into the Wink cooperative runtime and wires the interception points to the
// virtual clock and peripheral models.
#include "pal_osal.h"
#include "wink_app.h"

// S3-2 (CPL-09): zero chip/device includes — the bridge only speaks generic
// core headers. Chip notify (TA window) arrives via the per-context
// sfr_write_notify slot; board devices bind through the post-init hook.
#include "absacc.h"
#include "mcs51_adc.h"
#include "mcs51_proxy.hpp"
#include "mcs51_trap.h"
#include "mcs51_context.h"
#include "mcs51_peripheral.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_extint.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include "wink_mcs51_timer.h"
#include "wink_mcs51_uart.h"
#include "mcs51_pcon.h"
#include "wink_mcs51_edge_queue.h"

#include "wink_event.h"
#include "wink_mcs51_wdt.h"

#include <csetjmp>
#include <cstdint>

// Stage4 CPL-10 / Stage7 S7-1: chip package registration is LINK-TIME
// self-registration. Each chips/<family>/src/<family>_register.cpp carries a
// static initializer that appends its descriptors to the core-owned registry
// when the package's register OBJECT is linked; production root and tests
// link exactly one family through the manifest-resolved inject bundle plus
// the conventional register object. The bridge stays family-agnostic: no
// generated glue header, no chip symbols, no silent no-registration path.

extern "C" void wink_mcs51_user_main(void);

namespace {

std::jmp_buf s_reset_jmp_buf;
bool s_reentry_active = false;

void mcs51_perform_reset_sanitization(Mcu51Context* ctx) {
    ctx->reset_guard = 1u;
    ctx->reset_pending = 0u;

    // 1. Deinit event queue to flush old boot events (P13 / ADR-0082 D5)
    wink_event_queue_deinit();

    // 2. Clear edge queue
    ctx->edge_head = 0u;
    ctx->edge_tail = 0u;

    // 3. Clear interrupt nesting stack and suppress flag
    ctx->in_service_depth = 0u;
    ctx->reti_suppress_one = false;

    // 4. Full hardware context reset and seed recovery (including sticky PORF)
    mcs51_context_reset(ctx);

    // 5. Reinitialize fresh event queue
    (void)wink_event_queue_init(WINK_EVENT_QUEUE_DEFAULT_CAPACITY);

    // 6. Reset pins to weak pull-up high (0xFF)
    for (uint16_t pin = 0u; pin < 32u; ++pin) {
        js_pal_gpio_write(pin, true, MCS51_DRIVE_WEAK);
    }

    // 7. Re-run post-init hook, trap registrations, and ISR enable
    mcs51_framework_run_post_init_hook();
    mcs51_trap_register_sfr_write(0x87, mcs51_on_pcon_write);
    wink_mcs51_set_catchup_hook(wink_mcs51_timers_step_to);
    wink_mcs51_isr_enable();

    // 8. Bill any unbilled virtual time to master clock (ADR-0072 D1/D3)
    uint32_t unbilled = static_cast<uint32_t>(ctx->virtual_us - ctx->slice_start_us);
    if (unbilled > 0) {
        pal_os_busy_wait_us(unbilled);
        ctx->slice_start_us = ctx->virtual_us;
    }

    ctx->reset_guard = 0u;
}

void mcs51_framework_init(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);

    (void)wink_event_queue_init(WINK_EVENT_QUEUE_DEFAULT_CAPACITY);

    for (uint16_t pin = 0u; pin < 32u; ++pin) {
        js_pal_gpio_write(pin, true, MCS51_DRIVE_WEAK);
    }

    // S3-2: no device auto-bind here (CPL-21). Boards carrying an ADC0832
    // bind it in their own post-init hook via adc0832_device_attach(); the
    // hook below runs after framework init on every runtime run.
    mcs51_framework_run_post_init_hook();

    // M7: register through the trap API like every other model (direct
    // table assignment bypasses nothing today, but keeps one path).
    mcs51_trap_register_sfr_write(0x87, mcs51_on_pcon_write);

    wink_mcs51_set_catchup_hook(wink_mcs51_timers_step_to);
    wink_mcs51_isr_enable();
}

}  // namespace

extern "C" {

void wink_mcs51_microstep(void) {
    Mcu51Context* ctx = mcs51_get_context();
    // ADR-0082 D1 / P01: Safety interception point for reset
    if (ctx && ctx->reset_pending && !ctx->reset_guard) {
        mcs51_perform_reset_sanitization(ctx);
        if (s_reentry_active) {
            std::longjmp(s_reset_jmp_buf, 1);
        }
        return;
    }

    wink_mcs51_clear_reti_suppress();
    wink_mcs51_charge_us(wink_mcs51_get_microstep_us());
    mcs51_edge_queue_drain(ctx);
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
            continue;
        }
        if (g_mcs51_peripherals[i].poll != nullptr) {
            g_mcs51_peripherals[i].poll(ctx);
        }
    }
    // Stage4 CPL-10: chip registry polls after the core table (same filter;
    // phases order within each table, core phases before chip phases).
    for (uint8_t i = 0; i < mcs51_peripheral_registered_count(); ++i) {
        const mcs51_peripheral_desc_t* d = mcs51_peripheral_registered(i);
        if (!mcs51_peripheral_active_for(d, ctx->family)) {
            continue;
        }
        if (d->poll != nullptr) {
            d->poll(ctx);
        }
    }
    mcs51_irq_scan_and_dispatch();

    // Check again after peripheral poll in case WDT check latched reset
    if (ctx && ctx->reset_pending && !ctx->reset_guard) {
        mcs51_perform_reset_sanitization(ctx);
        if (s_reentry_active) {
            std::longjmp(s_reset_jmp_buf, 1);
        }
    }
}

void wink_mcs51_on_sfr_read(uint8_t addr) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_sfr_read_hook_t hook = ctx->sfr_read_hooks[addr];
    if (hook != nullptr) {
        hook(ctx, addr);
    }
    wink_mcs51_microstep();
}

void wink_mcs51_on_sfr_write(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    Mcu51Context* ctx = mcs51_get_context();
    // GAP-07: the registered pre-dispatch notify (chip TA window) runs
    // BEFORE the per-address hook, so an intervening firmware SFR write
    // aborts a half-open window first and the pending protected write
    // arrives locked and rolls back. S3-2: hookized, no chip hard call.
    mcs51_sfr_write_notify_fn_t notify = ctx->sfr_write_notify;
    if (notify != nullptr) {
        notify(ctx, addr);
    }
    mcs51_sfr_write_hook_t hook = ctx->sfr_write_hooks[addr];
    if (hook != nullptr) {
        hook(ctx, addr, old_val, new_val);
    }
    wink_mcs51_microstep();
}

}  // extern "C"

namespace {
void mcs51_app_loop(void) {
    s_reentry_active = true;
    while (true) {
        if (setjmp(s_reset_jmp_buf) != 0) {
            // Returned from longjmp (reset occurred).
            // A reset is a major hardware transition: yield cooperatively to the
            // simulator scheduler so that other fibers/sim events execute and time advances.
            wink_mcs51_cooperative_yield();
        }
        wink_mcs51_user_main();
        break;
    }
    s_reentry_active = false;
}
}  // namespace

extern "C" {

void wink_mcs51_test_run_reentry_loop(void (*fn)(void), uint32_t max_boots) {
    s_reentry_active = true;
    uint32_t boots = 0;
    while (boots < max_boots) {
        boots++;
        if (setjmp(s_reset_jmp_buf) == 0) {
            if (fn) fn();
            break;
        }
    }
    s_reentry_active = false;
}

const wink_app_callbacks_t* wink_app_get_callbacks(void)
{
    static const wink_app_callbacks_t s_mcs51_callbacks = {
        mcs51_framework_init,
        mcs51_app_loop,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    return &s_mcs51_callbacks;
}

// Overrides for targets/wasm/wasm_entry.c weak hooks without leaking wink_mcs51 naming into targets/
bool pal_wasm_target_has_pending_reset(void) {
    return wink_mcs51_has_pending_reset();
}

int pal_wasm_target_get_reset_reason(void) {
    return wink_mcs51_get_pending_reset_reason();
}

void pal_wasm_target_clear_pending_reset(void) {
    wink_mcs51_clear_pending_reset();
}

}  // extern "C"
