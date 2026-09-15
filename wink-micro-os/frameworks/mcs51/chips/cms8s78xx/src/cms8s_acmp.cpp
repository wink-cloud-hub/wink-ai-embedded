// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip Analog Comparator (ACMP0/ACMP1) peripheral model.
#include "cms8s_acmp.h"

#include "cms8s_priv.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "wink_mcs51_gpio.h"
#include "wink_mcs51_isr.h"

#include <cstdint>

extern "C" {
float js_pal_adc_read_norm(uint16_t pin);
}

namespace {

constexpr uint16_t XSFR_C0CON0  = 0xF500u;
constexpr uint16_t XSFR_C0CON1  = 0xF501u;
constexpr uint16_t XSFR_C0CON2  = 0xF502u;
constexpr uint16_t XSFR_C1CON0  = 0xF503u;
constexpr uint16_t XSFR_C1CON1  = 0xF504u;
constexpr uint16_t XSFR_C1CON2  = 0xF505u;
constexpr uint16_t XSFR_CNVRCON = 0xF506u;
constexpr uint16_t XSFR_CNFBCON = 0xF507u;
constexpr uint16_t XSFR_CNIE    = 0xF508u;
constexpr uint16_t XSFR_CNIF    = 0xF509u;
constexpr uint16_t XSFR_P02CFG  = 0xF002u;
constexpr uint16_t XSFR_P10CFG  = 0xF010u;
constexpr uint16_t XSFR_P20CFG  = 0xF020u;
constexpr uint16_t XSFR_P24CFG  = 0xF024u;

static float get_pin_norm(Mcu51Context* ctx, uint8_t pin) {
    if (ctx && ctx->adc_inject_flag[pin] != 0u) {
        return static_cast<float>(ctx->adc_injected[pin]) / 4095.0f;
    }
    return js_pal_adc_read_norm(pin);
}

static float compute_internal_vneg(Mcu51Context* ctx) {
    const uint8_t cnvrcon = ctx->xdata_shadow[XSFR_CNVRCON];
    if ((cnvrcon & 0x40u) == 0u) {
        // CNSVR == 0: Direct 1.2V Bandgap (1.2V / 5.0V = 0.24f)
        return 0.24f;
    }
    // CNSVR == 1: Divider enabled
    // CNDIVS: 1 = BG (1.2V / 5.0V = 0.24f), 0 = VDD (5.0V / 5.0V = 1.00f)
    const float vsource = ((cnvrcon & 0x80u) != 0u) ? 0.24f : 1.00f;
    const uint8_t k = cnvrcon & 0x3Fu;
    float ratio = 0.20f;
    if (k <= 0x0Fu) {
        ratio = static_cast<float>(k + 1u) / 60.0f;
    } else if (k == 0x10u) {
        ratio = 0.50f;
    } else if (k == 0x20u) {
        ratio = 0.75f;
    } else if (k == 0x30u) {
        ratio = 1.00f;
    }
    return vsource * ratio;
}

}  // namespace

extern "C" {

void cms8s_acmp_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);
    Cms8sPriv* priv = cms8s_priv(ctx);
    if (priv) {
        priv->acmp.last_c0out = 0u;
        priv->acmp.last_c1out = 0u;
        priv->acmp.cnif = 0u;
        priv->acmp.c0_initialized = false;
        priv->acmp.c1_initialized = false;
        priv->acmp.last_poll_us = 0u;
    }
}

void cms8s_acmp_reset(struct Mcu51Context* ctx) {
    cms8s_acmp_init(ctx);
}

void cms8s_acmp_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx || ctx->family != MCS51_FAMILY_CMS8S78XX) return;

    Cms8sPriv* priv = cms8s_priv(ctx);
    if (!priv) return;

    // CNIF is write-0-to-clear (W0C): firmware stores ~mask to clear a bit
    // and 1-bits preserve. Settle any pending firmware write first (zeroes
    // clear, ones preserve), run edge detection, then publish the effective
    // flag byte back to the shadow.
    priv->acmp.cnif &= ctx->xdata_shadow[XSFR_CNIF];

    // ── ACMP0 ──
    const uint8_t c0con0 = ctx->xdata_shadow[XSFR_C0CON0];
    if ((c0con0 & 0x80u) != 0u) {  // C0EN
        uint8_t pos_pin = 9u;      // P1.1 (C0P0)
        float vpos = get_pin_norm(ctx, pos_pin);

        float vneg = 0.24f;        // 1.2V / 5.0V (Bandgap)
        const uint8_t c0ns = (c0con0 >> 3) & 0x03u;
        if (c0ns == 0u) {
            vneg = get_pin_norm(ctx, 10u);  // P1.2 (C0N)
        } else {
            vneg = compute_internal_vneg(ctx);
        }

        uint8_t out = (vpos > vneg) ? 1u : 0u;
        const uint8_t c0con2 = ctx->xdata_shadow[XSFR_C0CON2];
        if ((c0con2 & 0x20u) != 0u) {       // C0POS: reverse polarity
            out ^= 1u;
        }

        // Reflect output in C0CON1 bit 7 (C0OUT)
        if (out) {
            ctx->xdata_shadow[XSFR_C0CON1] |= 0x80u;
        } else {
            ctx->xdata_shadow[XSFR_C0CON1] &= ~0x80u;
        }

        // Output to pin P1.0 if multiplexed as C0O (0x05)
        if (ctx->xdata_shadow[XSFR_P10CFG] == 0x05u) {
            js_pal_gpio_write(8u, out != 0, MCS51_DRIVE_SUPPLY);
        }

        // Edge detection for interrupt
        if (!priv->acmp.c0_initialized) {
            priv->acmp.last_c0out = out;
            priv->acmp.c0_initialized = true;
        } else if (priv->acmp.last_c0out == 0u && out == 1u) {
            priv->acmp.last_c0out = out;
            const uint8_t cnie = ctx->xdata_shadow[XSFR_CNIE];
            if ((cnie & 0x01u) != 0u) {
                priv->acmp.cnif |= 0x01u;
                mcs51_raise_irq(IRQ_SOURCE_ACMP);
            }
        } else {
            priv->acmp.last_c0out = out;
        }
    } else {
        priv->acmp.c0_initialized = false;
        ctx->xdata_shadow[XSFR_C0CON1] &= ~0x80u;
    }

    // ── ACMP1 ──
    const uint8_t c1con0 = ctx->xdata_shadow[XSFR_C1CON0];
    if ((c1con0 & 0x80u) != 0u) {  // C1EN
        uint8_t pos_pin = 17u;     // P2.1 (C1P0)
        const uint8_t c1ps = c1con0 & 0x07u;
        if (c1ps == 1u) pos_pin = 19u;      // P2.3 (C1P1)
        else if (c1ps == 2u) pos_pin = 0u;  // P0.0 (C1P2)
        else if (c1ps == 3u) pos_pin = 6u;  // P0.6 (C1P3)

        float vpos = get_pin_norm(ctx, pos_pin);
        float vneg = 0.24f;
        const uint8_t c1ns = (c1con0 >> 3) & 0x03u;
        if (c1ns == 0u) {
            vneg = get_pin_norm(ctx, 18u);  // P2.2 (C1N)
        } else {
            vneg = compute_internal_vneg(ctx);
        }

        uint8_t out = (vpos > vneg) ? 1u : 0u;
        const uint8_t c1con2 = ctx->xdata_shadow[XSFR_C1CON2];
        if ((c1con2 & 0x20u) != 0u) {
            out ^= 1u;
        }

        if (out) {
            ctx->xdata_shadow[XSFR_C1CON1] |= 0x80u;
        } else {
            ctx->xdata_shadow[XSFR_C1CON1] &= ~0x80u;
        }

        // Output to pin P2.4 (20u), P2.0 (16u), or P0.2 (2u) if multiplexed as C1O (0x05)
        if (ctx->xdata_shadow[XSFR_P24CFG] == 0x05u) {
            js_pal_gpio_write(20u, out != 0, MCS51_DRIVE_SUPPLY);
        } else if (ctx->xdata_shadow[XSFR_P20CFG] == 0x05u) {
            js_pal_gpio_write(16u, out != 0, MCS51_DRIVE_SUPPLY);
        } else if (ctx->xdata_shadow[XSFR_P02CFG] == 0x05u) {
            js_pal_gpio_write(2u, out != 0, MCS51_DRIVE_SUPPLY);
        }

        if (!priv->acmp.c1_initialized) {
            priv->acmp.last_c1out = out;
            priv->acmp.c1_initialized = true;
        } else if (priv->acmp.last_c1out == 0u && out == 1u) {
            priv->acmp.last_c1out = out;
            const uint8_t cnie = ctx->xdata_shadow[XSFR_CNIE];
            if ((cnie & 0x02u) != 0u) {
                priv->acmp.cnif |= 0x02u;
                mcs51_raise_irq(IRQ_SOURCE_ACMP);
            }
        } else {
            priv->acmp.last_c1out = out;
        }
    } else {
        priv->acmp.c1_initialized = false;
        ctx->xdata_shadow[XSFR_C1CON1] &= ~0x80u;
    }

    // Publish the settled W0C flag byte (pending clears + new edges).
    ctx->xdata_shadow[XSFR_CNIF] = priv->acmp.cnif;
}

uint64_t cms8s_acmp_next_event_us(struct Mcu51Context* ctx) {
    (void)ctx;
    return UINT64_MAX;
}

}  // extern "C"
