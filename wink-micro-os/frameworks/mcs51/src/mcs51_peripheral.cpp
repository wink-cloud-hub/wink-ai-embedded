// SPDX-License-Identifier: Apache-2.0
// Task R1: MCS-51 strong peripheral descriptor table definition (ADR-0004).
#include "mcs51_peripheral.h"
#include "mcs51_context.h"

extern "C" {

void mcs51_timer_init(struct Mcu51Context* ctx);
void mcs51_timer_reset(struct Mcu51Context* ctx);
void mcs51_timer_poll(struct Mcu51Context* ctx);
uint64_t mcs51_timer_next_event_us(struct Mcu51Context* ctx);

void mcs51_uart_init(struct Mcu51Context* ctx);
void mcs51_uart_reset(struct Mcu51Context* ctx);
void mcs51_uart_poll(struct Mcu51Context* ctx);
uint64_t mcs51_uart_next_event_us(struct Mcu51Context* ctx);

void mcs51_extint_init(struct Mcu51Context* ctx);
void mcs51_extint_reset(struct Mcu51Context* ctx);
void mcs51_extint_poll(struct Mcu51Context* ctx);
uint64_t mcs51_extint_next_event_us(struct Mcu51Context* ctx);

void cms8s_adc_init(struct Mcu51Context* ctx);
void cms8s_adc_model_reset(struct Mcu51Context* ctx);
void cms8s_adc_poll(struct Mcu51Context* ctx);
uint64_t cms8s_adc_next_event_us(struct Mcu51Context* ctx);

void cms8s_buzzer_init(struct Mcu51Context* ctx);
void cms8s_buzzer_reset(struct Mcu51Context* ctx);
void cms8s_buzzer_poll(struct Mcu51Context* ctx);
uint64_t cms8s_buzzer_next_event_us(struct Mcu51Context* ctx);

void cms8s_sys_init(struct Mcu51Context* ctx);
void cms8s_sys_reset(struct Mcu51Context* ctx);

// M1: core 8051 models run on every family; cms8s_* models only where the
// silicon exists. Loops (init/reset/poll/next_event) filter on family_mask.
const mcs51_peripheral_desc_t g_mcs51_peripherals[] = {
    {
        "timer",
        mcs51_timer_init,
        mcs51_timer_reset,
        mcs51_timer_poll,
        mcs51_timer_next_event_us,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_ALL
    },
    {
        "uart",
        mcs51_uart_init,
        mcs51_uart_reset,
        mcs51_uart_poll,
        mcs51_uart_next_event_us,
        MCS51_PHASE_RX_DRAIN,
        MCS51_FAMILY_MASK_ALL
    },
    {
        "extint",
        mcs51_extint_init,
        mcs51_extint_reset,
        mcs51_extint_poll,
        mcs51_extint_next_event_us,
        MCS51_PHASE_EXTINT,
        MCS51_FAMILY_MASK_ALL
    },
    {
        "cms8s_adc",
        cms8s_adc_init,
        cms8s_adc_model_reset,
        cms8s_adc_poll,
        cms8s_adc_next_event_us,
        MCS51_PHASE_ADC,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        "cms8s_buzzer",
        cms8s_buzzer_init,
        cms8s_buzzer_reset,
        cms8s_buzzer_poll,
        cms8s_buzzer_next_event_us,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_CMS8S78XX
    },
    {
        "cms8s_sys",
        cms8s_sys_init,
        cms8s_sys_reset,
        nullptr,
        nullptr,
        MCS51_PHASE_CLOCK,
        MCS51_FAMILY_MASK_CMS8S78XX
    }
};

const uint8_t g_mcs51_num_peripherals =
    static_cast<uint8_t>(sizeof(g_mcs51_peripherals) / sizeof(g_mcs51_peripherals[0]));

} // extern "C"
