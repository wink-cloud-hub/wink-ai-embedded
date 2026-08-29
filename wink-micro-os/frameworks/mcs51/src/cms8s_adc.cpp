// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip 12-bit ADC — instant-conversion model (M5, ADR-0073).
//
// See cms8s_adc.h. The model is a single SFR write hook on ADCON0 (0xDF),
// fired by WinkSfr::operator= AFTER the shadow store. A write with ADGO=1
// while ADEN=1 completes the conversion synchronously (0-cycle passthrough,
// same interception-point discipline as the ADC0832 model): pull the 12-bit
// code, pack ADRESH/ADRESL per ADFM, self-clear ADGO in the shadow, latch
// ADCIF, and vector ISR 19 when EA+ADCIE are set.
#include "cms8s_adc.h"

#include <stdint.h>

#include "mcs51_adc.h"
#include "mcs51_trap.h"
#include "wink_mcs51_isr.h"

namespace {

// ── CMS8S78xx ADC SFR addresses (vendor cms8s78xx.h) ────────────────────────
constexpr uint8_t SFR_ADCON0 = 0xDF;
constexpr uint8_t SFR_ADCON1 = 0xDE;
constexpr uint8_t SFR_ADCCHS = 0xD9;
constexpr uint8_t SFR_ADRESL = 0xDC;
constexpr uint8_t SFR_ADRESH = 0xDD;
constexpr uint8_t SFR_EIE2   = 0xAA;
constexpr uint8_t SFR_EIF2   = 0xB2;
constexpr uint8_t SFR_IE     = 0xA8;

constexpr uint8_t ADCON0_ADFM = 0x40;  // bit6: 1 = right-justify
constexpr uint8_t ADCON0_ADGO = 0x02;  // bit1: start / busy (self-clears)
constexpr uint8_t ADCON1_ADEN = 0x80;  // bit7: module enable
constexpr uint8_t EIE2_ADCIE  = 0x10;  // EIE2 bit4
constexpr uint8_t EIF2_ADCIF  = 0x10;  // EIF2 bit4
constexpr uint8_t IE_EA       = 0x80;  // IE bit7

constexpr uint8_t  ADC_CH_MAX_EXTERNAL = 25u;   // AN0..AN25
constexpr uint8_t  ADC_CH_INTERNAL     = 0x3Fu;  // AN63
constexpr uint8_t  VECTOR_ADC          = 19u;   // Keil interrupt 19 (0x9B)

struct Cms8sAdcState {
    uint32_t conversion_count;
    uint8_t  last_channel;
};

Cms8sAdcState s_adc;

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

    // Channel select. AN0..AN25 pull the 12-bit analog rail; the AN63
    // internal mux (and unimplemented channel codes) read 0 in v1.
    const uint8_t ch = (uint8_t)(wink_mcs51_sfr_shadow[SFR_ADCCHS] & 0x3Fu);
    uint16_t raw;
    if (ch <= ADC_CH_MAX_EXTERNAL) {
        raw = (uint16_t)(mcs51_adc_get_value(ch) & 0x0FFFu);
    } else {
        raw = 0u;  // AN63 (BGR/temp/VDD) not modeled in v1.
        (void)ADC_CH_INTERNAL;
    }

    // Pack the result per ADFM (vendor ADC_GetADCResult formulas, reversed):
    //   right: result = 0xFFF & ((ADRESH<<8) | ADRESL)
    //   left : result = 0xFFF & ((ADRESH<<4) | (ADRESL>>4))
    if ((new_val & ADCON0_ADFM) != 0u) {
        wink_mcs51_sfr_shadow[SFR_ADRESH] = (uint8_t)((raw >> 8) & 0x0Fu);
        wink_mcs51_sfr_shadow[SFR_ADRESL] = (uint8_t)(raw & 0xFFu);
    } else {
        wink_mcs51_sfr_shadow[SFR_ADRESH] = (uint8_t)((raw >> 4) & 0xFFu);
        wink_mcs51_sfr_shadow[SFR_ADRESL] = (uint8_t)((raw & 0x0Fu) << 4);
    }

    // Hardware self-clears ADGO when the conversion completes — this is the
    // 0-cycle passthrough: the poll `while(ADCON0 & 0x02)` exits first read.
    wink_mcs51_sfr_shadow[SFR_ADCON0] = (uint8_t)(new_val & ~ADCON0_ADGO);

    ++s_adc.conversion_count;
    s_adc.last_channel = ch;

    // End-of-conversion interrupt: latch ADCIF when ADCIE is set (UART TI
    // precedent — the flag is software-cleared, hardware does not auto-clear
    // on vectoring), and dispatch vector 19 when EA is also set.
    if ((wink_mcs51_sfr_shadow[SFR_EIE2] & EIE2_ADCIE) != 0u) {
        wink_mcs51_sfr_shadow[SFR_EIF2] =
            (uint8_t)(wink_mcs51_sfr_shadow[SFR_EIF2] | EIF2_ADCIF);
        if ((wink_mcs51_sfr_shadow[SFR_IE] & IE_EA) != 0u) {
            (void)wink_mcs51_dispatch_vector(VECTOR_ADC);
        }
    }
}

}  // namespace

extern "C" {

void cms8s_adc_init(void) {
    s_adc.conversion_count = 0u;
    s_adc.last_channel = 0xFFu;
    mcs51_trap_register_sfr_write(SFR_ADCON0, &on_adcon0_write);
}

uint32_t cms8s_adc_conversion_count(void) {
    return s_adc.conversion_count;
}

uint8_t cms8s_adc_last_channel(void) {
    return s_adc.last_channel;
}

}  // extern "C"
