// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx on-chip 12-bit ADC — instant-conversion model (M5, ADR-0073).
#include "cms8s_adc.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "mcs51_adc.h"
#include "mcs51_trap.h"
#include "mcs51_context.h"
#include "mcs51_sfr_map.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

extern "C" {
uint8_t js_pal_gpio_read_state(uint16_t pin);
}

namespace {

constexpr uint8_t SFR_ADCON0 = 0xDF;
constexpr uint8_t SFR_ADCON1 = 0xDE;
constexpr uint8_t SFR_ADCON2 = 0xE9;
constexpr uint8_t SFR_ADCCHS = 0xD9;
constexpr uint8_t SFR_ADRESH = 0xDD;
constexpr uint8_t SFR_ADRESL = 0xDC;
// M5: shared addresses alias the single source (mcs51_sfr_map.h).
constexpr uint8_t SFR_EIE2   = MCS51_SFR_EIE2;
constexpr uint8_t SFR_EIF2   = MCS51_SFR_EIF2;
constexpr uint16_t XSFR_PS_ADET = MCS51_XSFR_PS_ADET;

constexpr uint8_t ADCON0_ADGO = 0x02u;  // bit1
constexpr uint8_t ADCON0_ADFM = 0x40u;  // bit6
constexpr uint8_t ADCON1_ADEN = 0x80u;  // bit7
constexpr uint8_t ADCON2_ADCEX = 0x80u; // bit7

constexpr uint8_t ADCON2_ADTGS_Pos = 4u;
constexpr uint8_t ADCON2_ADTGS_Msk = 0x30u;
constexpr uint8_t ADCON2_ADEGS_Pos = 2u;
constexpr uint8_t ADCON2_ADEGS_Msk = 0x0Cu;

constexpr uint8_t ADC_TG_ADET = 0x03u;
constexpr uint8_t ADC_TG_FALLING = 0x00u;
constexpr uint8_t ADC_TG_RISING  = 0x01u;

constexpr uint8_t EIE2_ADCIE  = 0x10u;  // bit4
constexpr uint8_t EIF2_ADCIF  = 0x10u;  // bit4

constexpr uint8_t ADC_CH_MAX_EXTERNAL = 25u;
constexpr uint8_t ADC_CH_INTERNAL     = 0x3Fu;

constexpr uint16_t XSFR_ADCLDO = 0xF692u;
constexpr uint8_t ADCLDO_LDOEN = 0x80u;
constexpr uint8_t ADCLDO_VSEL_Msk = 0x60u;
constexpr uint8_t ADCLDO_VSEL_Pos = 5u;

// ADCON1 ADCKS (bits6:4) -> divider (vendor ADC_CLK_DIV_2..256).
constexpr uint32_t ADC_DIV_TABLE[8] = {2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u};
// VSEL -> Vref in mV (vendor ADC_VREF_1P2V/2V/2P4V/3V).
constexpr uint16_t ADC_VREF_MV[4] = {1200u, 2000u, 2400u, 3000u};

// A-02 readiness counters (diagnostic, file-static like uart_notready).
// STRICT builds abort before counting, so counters stay 0 there by design.
uint32_t s_adc_notready[2] = {};
#ifndef WINK_MCS51_STRICT
bool s_adc_notready_warned[2] = {};
#endif

constexpr const char* kAdcNotreadyNames[2] = {
    "LDO not enabled (ADCLDO.LDOEN=0)",
    "analog mux not selected (PxxCFG != AN)",
};

inline uint8_t adc_reason_index(uint32_t reason_bit) {
    switch (reason_bit) {
        case WINK_MCS51_ADC_NOTREADY_LDO: return 0u;
        case WINK_MCS51_ADC_NOTREADY_MUX: return 1u;
        default: return 0xFFu;
    }
}

void adc_notready_policy(uint32_t mask) {
#ifdef WINK_MCS51_STRICT
    (void)mask;
    assert(0 && "ADC conversion not ready (WINK_MCS51_STRICT)");
    std::abort();
#else
    for (uint8_t i = 0; i < 2; ++i) {
        const uint32_t bit = (1u << i);
        if ((mask & bit) == 0) {
            continue;
        }
        if (s_adc_notready[i] < 0xFFFFFFFFu) {
            ++s_adc_notready[i];
        }
        if (!s_adc_notready_warned[i]) {
            s_adc_notready_warned[i] = true;
            pal_log_w("MCS51", "ADC conversion not ready: %s", kAdcNotreadyNames[i]);
        }
    }
#endif
}

// AN channel -> pin CFG XSFR address. Assumed mapping (ref manual ADC chapter
// + pin counts 8+8+6+4=26): AN0-7=P0.0-7, AN8-15=P1.0-7, AN16-21=P2.0-5,
// AN22-25=P3.0-3. Returns 0xFFFF for internal/unmapped.
inline uint16_t adc_channel_cfg_addr(uint8_t ch) {
    if (ch <= 7u) {
        return static_cast<uint16_t>(0xF000u + ch);
    }
    if (ch <= 15u) {
        return static_cast<uint16_t>(0xF010u + (ch - 8u));
    }
    if (ch <= 21u) {
        return static_cast<uint16_t>(0xF020u + (ch - 16u));
    }
    if (ch <= 25u) {
        return static_cast<uint16_t>(0xF030u + (ch - 22u));
    }
    return 0xFFFFu;
}

constexpr uint8_t PORT_PINS[4] = {8u, 8u, 8u, 8u};
constexpr uint8_t EXT_LOW  = 0u;
constexpr uint8_t EXT_HIGH = 1u;

// M2: Cms8sAdcState/AdetPinState/Cms8sAdcPriv now live in mcs51_context.h
// (Mcs51Cms8sAdcState/Mcs51AdetPinState/Mcs51Cms8sAdcPriv) as
// Mcu51Context::cms8sAdc. The old file-static default object aliased ONE
// priv into EVERY context via soc_priv — two contexts shared conversions.

inline Mcs51Cms8sAdcPriv* get_adc_priv(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    return &ctx->cms8sAdc;
}

// Performs one 12-bit ADC conversion synchronously: readiness gates
// (LDO/mux), DIV-sensitive charge_us, analog rail pull, packs ADRESH/ADRESL
// per ADFM, latches ADCIF, and dispatches vector 19 if enabled.
void do_adc_conversion(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    Mcs51Cms8sAdcPriv* priv = get_adc_priv(ctx);
    const uint8_t ch = static_cast<uint8_t>(ctx->sfr_shadow[SFR_ADCCHS] & 0x3Fu);

    // Gate 0: LDO must be enabled; Vref follows VSEL (A-02, GAP-05).
    const uint8_t adcldo = ctx->xdata_shadow[XSFR_ADCLDO];
    const uint16_t vref_mv =
        ADC_VREF_MV[(adcldo & ADCLDO_VSEL_Msk) >> ADCLDO_VSEL_Pos];
    ctx->adc_vref_mv = vref_mv;
    if ((adcldo & ADCLDO_LDOEN) == 0u) {
        adc_notready_policy(WINK_MCS51_ADC_NOTREADY_LDO);
        ctx->sfr_shadow[SFR_ADCON0] =
            static_cast<uint8_t>(ctx->sfr_shadow[SFR_ADCON0] & ~ADCON0_ADGO);
        return;
    }

    // Gate 1: channel pin must select analog mux (PxxCFG == 0x01).
    if (ch <= ADC_CH_MAX_EXTERNAL) {
        const uint16_t cfg = adc_channel_cfg_addr(ch);
        if (cfg == 0xFFFFu || ctx->xdata_shadow[cfg] != 0x01u) {
            adc_notready_policy(WINK_MCS51_ADC_NOTREADY_MUX);
            ctx->sfr_shadow[SFR_ADCON0] =
                static_cast<uint8_t>(ctx->sfr_shadow[SFR_ADCON0] & ~ADCON0_ADGO);
            return;
        }
    }

    // DIV-sensitive synchronous accounting (A-02): 16 SAR clocks at
    // Fsys/DIV, charged before the synchronous completion (no async timer,
    // no §3.1 deadlock). health_pot DIV_256 @24MHz ≈ 170µs.
    const uint8_t adcks =
        static_cast<uint8_t>((ctx->sfr_shadow[SFR_ADCON1] >> 4u) & 0x07u);
    const uint32_t div = ADC_DIV_TABLE[adcks];
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    if (fsys != 0u) {
        const uint32_t conv_us =
            static_cast<uint32_t>((16ull * div * 1000000ull) / fsys);
        if (conv_us != 0u) {
            wink_mcs51_charge_us(conv_us);
        }
    }

    uint16_t raw;
    if (ch <= ADC_CH_MAX_EXTERNAL) {
        raw = static_cast<uint16_t>(mcs51_adc_get_value(ch) & 0x0FFFu);
    } else {
        raw = 0u;  // AN63 (BGR/temp/VDD) not modeled in v1.
        (void)ADC_CH_INTERNAL;
    }

    const uint8_t adcon0 = ctx->sfr_shadow[SFR_ADCON0];
    if ((adcon0 & ADCON0_ADFM) != 0u) {
        ctx->sfr_shadow[SFR_ADRESH] = static_cast<uint8_t>((raw >> 8) & 0x0Fu);
        ctx->sfr_shadow[SFR_ADRESL] = static_cast<uint8_t>(raw & 0xFFu);
    } else {
        ctx->sfr_shadow[SFR_ADRESH] = static_cast<uint8_t>((raw >> 4) & 0xFFu);
        ctx->sfr_shadow[SFR_ADRESL] = static_cast<uint8_t>((raw & 0x0Fu) << 4);
    }

    // Clear ADGO in case this was triggered by software poll
    ctx->sfr_shadow[SFR_ADCON0] = static_cast<uint8_t>(adcon0 & ~ADCON0_ADGO);

    ++priv->adc.conversion_count;
    priv->adc.last_channel = ch;

    // End-of-conversion interrupt: latch ADCIF when ADCIE is set,
    // and raise semantic ADC IRQ (ADR-0078).
    if ((ctx->sfr_shadow[SFR_EIE2] & EIE2_ADCIE) != 0u) {
        ctx->sfr_shadow[SFR_EIF2] =
            static_cast<uint8_t>(ctx->sfr_shadow[SFR_EIF2] | EIF2_ADCIF);
        mcs51_raise_irq(IRQ_SOURCE_ADC);
    }
}

// ADCON0 write hook. `new_val` is the value the WinkSfr proxy has ALREADY
// stored into the shadow; hook mutations to the shadow persist.
// M3: C language linkage for the C-ABI hook table (see cms8s_sys.cpp).
extern "C" void on_adcon0_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;

    if (!ctx) ctx = mcs51_get_context();

    // Conversion starts only on an ADGO write with the module enabled.
    if ((new_val & ADCON0_ADGO) == 0u) {
        return;
    }
    if ((ctx->sfr_shadow[SFR_ADCON1] & ADCON1_ADEN) == 0u) {
        return;
    }

    do_adc_conversion(ctx);
}

}  // namespace

extern "C" {

void cms8s_adc_model_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    Mcs51Cms8sAdcPriv* priv = get_adc_priv(ctx);
    priv->in_poll = false;
    priv->adet.last_pin = 0xFFFFu;
    priv->adet.last_level = 0xFFu;
    priv->adet.have_sample = false;
    // PS_ADET selector resets to 0x7F ("no pin connected", ref manual §7.2.3)
    ctx->xdata_shadow[XSFR_PS_ADET] = 0x7Fu;
    for (uint8_t i = 0; i < 2; ++i) {
        s_adc_notready[i] = 0;
#ifndef WINK_MCS51_STRICT
        s_adc_notready_warned[i] = false;
#endif
    }
    ctx->adc_vref_mv = 3000u;
    ctx->adc_vrail_mv = 3000u;
}

void cms8s_adc_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    Mcs51Cms8sAdcPriv* priv = get_adc_priv(ctx);
    priv->adc.conversion_count = 0u;
    priv->adc.last_channel = 0xFFu;
    cms8s_adc_model_reset(ctx);
    mcs51_trap_register_sfr_write(SFR_ADCON0, on_adcon0_write);
}

void cms8s_adc_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    Mcs51Cms8sAdcPriv* priv = get_adc_priv(ctx);
    if (priv->in_poll) {
        return;
    }

    // Gate 1: Module must be enabled
    if ((ctx->sfr_shadow[SFR_ADCON1] & ADCON1_ADEN) == 0u) {
        return;
    }
    // Gate 2: Hardware trigger must be enabled (ADCON2.ADCEX=1)
    const uint8_t adcon2 = ctx->sfr_shadow[SFR_ADCON2];
    if ((adcon2 & ADCON2_ADCEX) == 0u) {
        return;
    }

    // Gate 3: Trigger source must be ADET
    const uint8_t tg_src = static_cast<uint8_t>((adcon2 & ADCON2_ADTGS_Msk) >> ADCON2_ADTGS_Pos);
    if (tg_src != ADC_TG_ADET) {
        return;
    }

    // Resolve pin from XSFR PS_ADET (0xF0CC, format 0xPN)
    const uint8_t sel = ctx->xdata_shadow[XSFR_PS_ADET];
    const uint8_t port = (sel >> 4) & 0x07u;
    const uint8_t bit  = sel & 0x0Fu;
    if (port >= 4u || bit >= PORT_PINS[port]) {
        return;  // unmapped or out of range
    }
    const uint16_t pin = static_cast<uint16_t>((port << 3) | bit);

    bool pin_changed = (pin != priv->adet.last_pin);
    if (pin_changed) {
        priv->adet.last_pin = pin;
        priv->adet.last_level = 0xFFu;
        priv->adet.have_sample = false;
    }

    uint8_t st = js_pal_gpio_read_state(pin);
    uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;

    if (!priv->adet.have_sample) {
        priv->adet.last_level = level;
        priv->adet.have_sample = true;
        return;
    }

    bool was_high = (priv->adet.last_level == EXT_HIGH);
    bool now_low  = (level == EXT_LOW);
    bool was_low  = (priv->adet.last_level == EXT_LOW);
    bool now_high = (level == EXT_HIGH);
    priv->adet.last_level = level;

    const uint8_t tg_mode = static_cast<uint8_t>((adcon2 & ADCON2_ADEGS_Msk) >> ADCON2_ADEGS_Pos);
    bool triggered = false;
    if (tg_mode == ADC_TG_FALLING && was_high && now_low) {
        triggered = true;
    } else if (tg_mode == ADC_TG_RISING && was_low && now_high) {
        triggered = true;
    }

    if (triggered) {
        priv->in_poll = true;
        do_adc_conversion(ctx);
        priv->in_poll = false;
        mcs51_irq_scan_and_dispatch();
    }
}

uint64_t cms8s_adc_next_event_us(struct Mcu51Context* ctx) {
    (void)ctx;
    return UINT64_MAX; // 0-cycle instant in Native mode
}

uint32_t cms8s_adc_conversion_count(void) {
    return get_adc_priv(nullptr)->adc.conversion_count;
}

uint8_t cms8s_adc_last_channel(void) {
    return get_adc_priv(nullptr)->adc.last_channel;
}

uint32_t cms8s_adc_notready_mask(void) {
    Mcu51Context* ctx = mcs51_get_context();
    uint32_t mask = 0u;
    if ((ctx->xdata_shadow[XSFR_ADCLDO] & ADCLDO_LDOEN) == 0u) {
        mask |= WINK_MCS51_ADC_NOTREADY_LDO;
    }
    const uint8_t ch = static_cast<uint8_t>(ctx->sfr_shadow[SFR_ADCCHS] & 0x3Fu);
    if (ch <= ADC_CH_MAX_EXTERNAL) {
        const uint16_t cfg = adc_channel_cfg_addr(ch);
        if (cfg == 0xFFFFu || ctx->xdata_shadow[cfg] != 0x01u) {
            mask |= WINK_MCS51_ADC_NOTREADY_MUX;
        }
    }
    return mask;
}

uint32_t cms8s_adc_notready_count(uint32_t reason_bit) {
    const uint8_t idx = adc_reason_index(reason_bit);
    return (idx < 2u) ? s_adc_notready[idx] : 0u;
}

uint32_t cms8s_adc_notready_total(void) {
    return s_adc_notready[0] + s_adc_notready[1];
}

}  // extern "C"
