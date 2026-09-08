// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip 12-bit ADC — instant-conversion model (M5, ADR-0073).
//
// See cms8s_adc.h. The model supports both:
//  1) Software trigger: SFR write hook on ADCON0 (0xDF) with ADGO=1;
//  2) Hardware trigger: ADCON2 (0xE9) ADCEX=1 with ADET edge detection
//     routed via XSFR PS_ADET (0xF0CC), polled on microsteps.
#include "cms8s_adc.h"

#include <stdint.h>

#include "absacc.h"
#include "mcs51_adc.h"
#include "mcs51_trap.h"
#include "wink_mcs51_isr.h"

extern "C" uint8_t js_pal_gpio_read_state(uint16_t pin);

namespace {

// ── CMS8S78xx ADC SFR / XSFR addresses (vendor cms8s78xx.h) ─────────────────
constexpr uint8_t  SFR_ADCON0   = 0xDF;
constexpr uint8_t  SFR_ADCON1   = 0xDE;
constexpr uint8_t  SFR_ADCON2   = 0xE9;
constexpr uint8_t  SFR_ADCCHS   = 0xD9;
constexpr uint8_t  SFR_ADRESL   = 0xDC;
constexpr uint8_t  SFR_ADRESH   = 0xDD;
constexpr uint8_t  SFR_EIE2     = 0xAA;
constexpr uint8_t  SFR_EIF2     = 0xB2;
constexpr uint8_t  SFR_IE       = 0xA8;
constexpr uint16_t XSFR_PS_ADET = 0xF0CCu;

constexpr uint8_t ADCON0_ADFM       = 0x40;  // bit6: 1 = right-justify
constexpr uint8_t ADCON0_ADGO       = 0x02;  // bit1: start / busy (self-clears)
constexpr uint8_t ADCON1_ADEN       = 0x80;  // bit7: module enable
constexpr uint8_t ADCON2_ADCEX      = 0x80;  // bit7: hardware trigger enable
constexpr uint8_t ADCON2_ADTGS_Msk  = 0x30;  // bits5:4: trigger source
constexpr uint8_t ADCON2_ADTGS_Pos  = 4u;
constexpr uint8_t ADCON2_ADEGS_Msk  = 0x0C;  // bits3:2: trigger mode/edge
constexpr uint8_t ADCON2_ADEGS_Pos  = 2u;

constexpr uint8_t ADC_TG_ADET       = 0x03;
constexpr uint8_t ADC_TG_FALLING    = 0x00;
constexpr uint8_t ADC_TG_RISING     = 0x01;

constexpr uint8_t EIE2_ADCIE        = 0x10;  // EIE2 bit4
constexpr uint8_t EIF2_ADCIF        = 0x10;  // EIF2 bit4
constexpr uint8_t IE_EA             = 0x80;  // IE bit7

constexpr uint8_t ADC_CH_MAX_EXTERNAL = 25u;   // AN0..AN25
constexpr uint8_t ADC_CH_INTERNAL     = 0x3Fu;  // AN63

constexpr uint8_t PORT_PINS[4]        = {8u, 8u, 6u, 4u};
constexpr uint8_t EXT_LOW             = 0u;
constexpr uint8_t EXT_HIGH            = 1u;

struct Cms8sAdcState {
    uint32_t conversion_count;
    uint8_t  last_channel;
};

struct AdetPinState {
    uint16_t last_pin;
    uint8_t  last_level;
    bool     have_sample;
};

Cms8sAdcState s_adc;
AdetPinState  s_adet;
bool          s_in_poll = false;

// Performs one 12-bit ADC conversion synchronously: pulls analog rail,
// packs ADRESH/ADRESL per ADFM, latches ADCIF, and dispatches vector 19 if enabled.
void do_adc_conversion(void) {
    const uint8_t ch = (uint8_t)(wink_mcs51_sfr_shadow[SFR_ADCCHS] & 0x3Fu);
    uint16_t raw;
    if (ch <= ADC_CH_MAX_EXTERNAL) {
        raw = (uint16_t)(mcs51_adc_get_value(ch) & 0x0FFFu);
    } else {
        raw = 0u;  // AN63 (BGR/temp/VDD) not modeled in v1.
        (void)ADC_CH_INTERNAL;
    }

    const uint8_t adcon0 = wink_mcs51_sfr_shadow[SFR_ADCON0];
    if ((adcon0 & ADCON0_ADFM) != 0u) {
        wink_mcs51_sfr_shadow[SFR_ADRESH] = (uint8_t)((raw >> 8) & 0x0Fu);
        wink_mcs51_sfr_shadow[SFR_ADRESL] = (uint8_t)(raw & 0xFFu);
    } else {
        wink_mcs51_sfr_shadow[SFR_ADRESH] = (uint8_t)((raw >> 4) & 0xFFu);
        wink_mcs51_sfr_shadow[SFR_ADRESL] = (uint8_t)((raw & 0x0Fu) << 4);
    }

    // Clear ADGO in case this was triggered by software poll
    wink_mcs51_sfr_shadow[SFR_ADCON0] = (uint8_t)(adcon0 & ~ADCON0_ADGO);

    ++s_adc.conversion_count;
    s_adc.last_channel = ch;

    // End-of-conversion interrupt: latch ADCIF when ADCIE is set,
    // and raise semantic ADC IRQ (ADR-0078).
    if ((wink_mcs51_sfr_shadow[SFR_EIE2] & EIE2_ADCIE) != 0u) {
        wink_mcs51_sfr_shadow[SFR_EIF2] =
            (uint8_t)(wink_mcs51_sfr_shadow[SFR_EIF2] | EIF2_ADCIF);
        mcs51_raise_irq(IRQ_SOURCE_ADC);
    }
}

// ADCON0 write hook. `new_val` is the value the WinkSfr proxy has ALREADY
// stored into the shadow; hook mutations to the shadow persist.
void on_adcon0_write(uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;

    // Conversion starts only on an ADGO write with the module enabled.
    if ((new_val & ADCON0_ADGO) == 0u) {
        return;
    }
    if ((wink_mcs51_sfr_shadow[SFR_ADCON1] & ADCON1_ADEN) == 0u) {
        return;
    }

    do_adc_conversion();
}

}  // namespace

extern "C" {

void cms8s_adc_reset(void) {
    s_in_poll = false;
    s_adet.last_pin = 0xFFFFu;
    s_adet.last_level = 0xFFu;
    s_adet.have_sample = false;
    // PS_ADET selector resets to 0x7F ("no pin connected", ref manual §7.2.3)
    wink_mcs51_xdata_shadow[XSFR_PS_ADET] = 0x7Fu;
}

void cms8s_adc_init(void) {
    s_adc.conversion_count = 0u;
    s_adc.last_channel = 0xFFu;
    cms8s_adc_reset();
    mcs51_trap_register_sfr_write(SFR_ADCON0, &on_adcon0_write);
}

void cms8s_adc_poll(void) {
    if (s_in_poll) {
        return;
    }

    // Gate 1: Module must be enabled
    if ((wink_mcs51_sfr_shadow[SFR_ADCON1] & ADCON1_ADEN) == 0u) {
        return;
    }
    // Gate 2: Hardware trigger must be enabled (ADCON2.ADCEX=1)
    const uint8_t adcon2 = wink_mcs51_sfr_shadow[SFR_ADCON2];
    if ((adcon2 & ADCON2_ADCEX) == 0u) {
        return;
    }

    // Gate 3: Trigger source must be ADET
    const uint8_t tg_src = (uint8_t)((adcon2 & ADCON2_ADTGS_Msk) >> ADCON2_ADTGS_Pos);
    if (tg_src != ADC_TG_ADET) {
        // TODO(EPWM): Support ADC_TG_PWM0 / ADC_TG_PWM2 hardware triggers when EPWM model is introduced.
        return;
    }

    // Resolve pin from XSFR PS_ADET (0xF0CC, format 0xPN)
    const uint8_t sel = wink_mcs51_xdata_shadow[XSFR_PS_ADET];
    const uint8_t port = (sel >> 4) & 0x07u;
    const uint8_t bit  = sel & 0x0Fu;
    if (port >= 4u || bit >= PORT_PINS[port]) {
        return;  // unmapped or out of range
    }
    const uint16_t pin = static_cast<uint16_t>((port << 3) | bit);

    bool pin_changed = (pin != s_adet.last_pin);
    if (pin_changed) {
        s_adet.last_pin = pin;
        s_adet.last_level = 0xFFu;
        s_adet.have_sample = false;
    }

    uint8_t st = js_pal_gpio_read_state(pin);
    uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;

    if (!s_adet.have_sample) {
        s_adet.last_level = level;
        s_adet.have_sample = true;
        return;
    }

    bool was_high = (s_adet.last_level == EXT_HIGH);
    bool now_low  = (level == EXT_LOW);
    bool was_low  = (s_adet.last_level == EXT_LOW);
    bool now_high = (level == EXT_HIGH);
    s_adet.last_level = level;

    const uint8_t tg_mode = (uint8_t)((adcon2 & ADCON2_ADEGS_Msk) >> ADCON2_ADEGS_Pos);
    bool triggered = false;
    if (tg_mode == ADC_TG_FALLING && was_high && now_low) {
        triggered = true;
    } else if (tg_mode == ADC_TG_RISING && was_low && now_high) {
        triggered = true;
    }

    if (triggered) {
        s_in_poll = true;
        do_adc_conversion();
        s_in_poll = false;
        mcs51_irq_scan_and_dispatch();
    }
}

uint32_t cms8s_adc_conversion_count(void) {
    return s_adc.conversion_count;
}

uint8_t cms8s_adc_last_channel(void) {
    return s_adc.last_channel;
}

}  // extern "C"
