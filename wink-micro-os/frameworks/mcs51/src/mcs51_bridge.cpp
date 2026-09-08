// SPDX-License-Identifier: Apache-2.0
// MCS-51 simulation bridge (boundary ④): binds the cleaned Keil user program
// into the Wink cooperative runtime and wires the interception points to the
// virtual clock and peripheral models.
#include "pal_osal.h"
#include "wink_app.h"

#include "ADC0832.H"
#include "absacc.h"
#include "cms8s_adc.h"
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

#include <cstdint>

#if defined(__has_include)
#  if __has_include("mcs51_board_config.h")
#    include "mcs51_board_config.h"
#    define MCS51_BOARD_CONFIG_PRESENT 1
#  endif
#endif

extern "C" void wink_mcs51_user_main(void);

namespace {

void mcs51_framework_init(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);

    for (uint16_t pin = 0u; pin < 32u; ++pin) {
        js_pal_gpio_write(pin, true, MCS51_DRIVE_WEAK);
    }

    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (g_mcs51_peripherals[i].init != nullptr) {
            g_mcs51_peripherals[i].init(ctx);
        }
    }

#ifdef MCS51_HAS_ADC0832
    mcs51_adc0832_init(MCS51_PIN_ADC0832_CS_PORT,  MCS51_PIN_ADC0832_CS_BIT,
                       MCS51_PIN_ADC0832_CLK_PORT, MCS51_PIN_ADC0832_CLK_BIT,
                       MCS51_PIN_ADC0832_DI_PORT,  MCS51_PIN_ADC0832_DI_BIT,
                       MCS51_PIN_ADC0832_DO_PORT,  MCS51_PIN_ADC0832_DO_BIT);
#endif

    mcs51_framework_run_post_init_hook();

    ctx->sfr_write_hooks[0x87] = mcs51_on_pcon_write;

    wink_mcs51_set_catchup_hook(wink_mcs51_timers_step_to);
    wink_mcs51_isr_enable();
}

}  // namespace

extern "C" {

void wink_mcs51_microstep(void) {
    wink_mcs51_clear_reti_suppress();
    wink_mcs51_charge_us(WINK_MCS51_MICROSTEP_US);
    Mcu51Context* ctx = mcs51_get_context();
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (g_mcs51_peripherals[i].poll != nullptr) {
            g_mcs51_peripherals[i].poll(ctx);
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
