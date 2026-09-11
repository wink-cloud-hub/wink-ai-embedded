// SPDX-License-Identifier: Apache-2.0
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

#include <cstdint>

// Stage4 CPL-10: family-selected chip registration (total §3.1b-3).
// Production glue (generated mcs51_family_select.h, stage6 codegen) defines
// MCS51_FAMILY_SELECT_REGISTER() as the family's register call; tests use
// mcs51_test_harness.h instead. Without the generated header (pre-stage6)
// no registration happens: core models run, chip models stay out.
#if defined(__has_include) && __has_include("mcs51_family_select.h")
#include "mcs51_family_select.h"
#define MCS51_HAVE_FAMILY_SELECT 1
#endif

extern "C" void wink_mcs51_user_main(void);

namespace {

void mcs51_framework_init(void) {
    Mcu51Context* ctx = mcs51_get_context();
#if defined(MCS51_HAVE_FAMILY_SELECT)
    MCS51_FAMILY_SELECT_REGISTER();
#endif
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
    wink_mcs51_clear_reti_suppress();
    wink_mcs51_charge_us(wink_mcs51_get_microstep_us());
    Mcu51Context* ctx = mcs51_get_context();
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
    wink_mcs51_user_main();
}
}  // namespace

extern "C" const wink_app_callbacks_t* wink_app_get_callbacks(void)
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
