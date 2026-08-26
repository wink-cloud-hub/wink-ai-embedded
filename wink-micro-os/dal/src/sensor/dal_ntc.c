// SPDX-License-Identifier: Apache-2.0
#include "sensor/dal_ntc.h"
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include <string.h>

/* NTC ADC is locked to 12-bit (0..4095) by dal_ntc_init; LUT nodes are spaced
 * 128 LSB apart (33 entries cover 32 intervals). Safety math is in the mV
 * domain so it is resolution-agnostic. */
#define NTC_RAW_MAX          4095u
#define NTC_LUT_NODES        33u
#define NTC_LUT_STEP         128u
#define NTC_LUT_INDEX_SHIFT  7u   /* log2(128) */
#define NTC_LUT_FRAC_MASK    0x7Fu

wink_status_t dal_ntc_init(dal_ntc_t *dev, const dal_ntc_config_t *cfg) {
    if (dev == NULL || cfg == NULL || cfg->owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    /* Phase 1: differential bridge needs PAL PGA/differential ADC support. */
    if (cfg->variant == DAL_NTC_VARIANT_DIFFERENTIAL_BRIDGE) {
        return WINK_ERR_UNSUPPORTED;
    }

    pal_adc_config_t pal_cfg = {
        .pin = (wink_pin_t)cfg->ao_pin,
        .full_scale_mv = cfg->vref_mv,
        .resolution_bits = 12,
    };

    pal_adc_channel_t ch = 0;
    wink_status_t st = pal_adc_acquire((wink_pin_t)cfg->ao_pin, &pal_cfg, &ch);
    if (wink_status_is_error(st)) {
        return st;
    }

    /* Dual resource lock: ADC logical channel + GPIO physical pin. */
    st = pal_resource_claim(PAL_RESOURCE_ADC_CHANNEL, (uint32_t)ch, cfg->owner);
    if (wink_status_is_error(st)) {
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        return st;
    }

    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->ao_pin, cfg->owner);
    if (wink_status_is_error(st)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_ADC_CHANNEL, (uint32_t)ch, cfg->owner));
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        return st;
    }

    memcpy(&dev->config, cfg, sizeof(dal_ntc_config_t));
    dev->adc_channel = (uint8_t)ch;
    dev->last_raw = 0;
    dev->last_mv = 0;
    dev->last_ddegc = 0;
#if !defined(WINK_PROFILE_MICRO) && !defined(WINK_NO_FLOAT)
    dev->last_degc = 0.0f;
#endif
    dev->fault_open = false;
    dev->fault_short = false;
    dev->fault_debounce = 0;
    dev->last_status = WINK_OK;
    dev->initialized = true;

    return WINK_OK;
}

wink_status_t dal_ntc_deinit(dal_ntc_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) {
        return WINK_OK;
    }

    pal_adc_channel_t ch = 0;
    if (pal_adc_pin_channel((wink_pin_t)dev->config.ao_pin, &ch) == WINK_OK) {
        WINK_IGNORE_UNUSED(pal_adc_release(ch));
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_ADC_CHANNEL, (uint32_t)ch, dev->config.owner));
    }
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)dev->config.ao_pin, dev->config.owner));

    dev->initialized = false;
    dev->last_status = WINK_OK;
    return WINK_OK;
}

/* mV-domain safety gate. A latched open/short fault sticks until clear_faults().
 * Transient out-of-range samples are debounced by debounce_count consecutive
 * readings before latching; during the debounce window WINK_ERR_BUSY is returned.
 * PULL_DOWN polarity is inverted relative to PULL_UP. */
static wink_status_t dal_ntc_check_safety(dal_ntc_t *dev, uint16_t mv, uint16_t vref_mv) {
    if (dev->fault_open || dev->fault_short) {
        dev->last_status = WINK_ERR_HARDWARE;
        return WINK_ERR_HARDWARE;
    }

    /* ~1% of full scale with a 20mV floor: avoids false trips from ADC noise
     * while staying well inside the valid divider range of standard NTCs. */
    uint16_t deadband_mv = (uint16_t)(vref_mv / 100u);
    if (deadband_mv < 20u) {
        deadband_mv = 20u;
    }

    bool is_short = false;
    bool is_open = false;

    if (dev->config.divider_type == DAL_NTC_DIVIDER_PULL_UP) {
        if (mv <= deadband_mv) {
            is_short = true;       /* NTC ~ 0: midpoint pulled to GND */
        } else if (mv >= (uint16_t)(vref_mv - deadband_mv)) {
            is_open = true;        /* NTC ~ inf: midpoint pulled to VCC */
        }
    } else {
        if (mv >= (uint16_t)(vref_mv - deadband_mv)) {
            is_short = true;       /* NTC ~ 0: midpoint pulled to VCC */
        } else if (mv <= deadband_mv) {
            is_open = true;        /* NTC ~ inf: midpoint pulled to GND */
        }
    }

    if (is_short || is_open) {
        uint8_t threshold = dev->config.debounce_count;
        if (threshold == 0 || ++dev->fault_debounce >= threshold) {
            dev->fault_debounce = threshold;
            if (is_short) {
                dev->fault_short = true;
            }
            if (is_open) {
                dev->fault_open = true;
            }
            dev->last_status = WINK_ERR_HARDWARE;
            return WINK_ERR_HARDWARE;
        }
        /* Within the debounce window: signal that data is not yet stable. */
        dev->last_status = WINK_ERR_BUSY;
        return WINK_ERR_BUSY;
    }

    dev->fault_debounce = 0;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING

static int16_t ntc_lut_interpolate_int16(const int16_t *table, uint16_t raw) {
    if (raw > NTC_RAW_MAX) {
        raw = NTC_RAW_MAX;
    }
    uint8_t idx = (uint8_t)(raw >> NTC_LUT_INDEX_SHIFT);     /* 0..31 */
    uint8_t frac = (uint8_t)(raw & NTC_LUT_FRAC_MASK);      /* 0..127 */

    int16_t y0 = table[idx];
    int16_t y1 = table[idx + 1];                            /* table has 33 entries */

    return y0 + (int16_t)(((int32_t)(y1 - y0) * (int32_t)frac) >> NTC_LUT_INDEX_SHIFT);
}

/* Take one raw ADC snapshot and derive mV from the channel's real Vref.
 * Returns WINK_OK on success and fills *out_raw / *out_mv / *out_vref. */
static wink_status_t dal_ntc_sample(dal_ntc_t *dev, uint16_t *out_raw,
                                    uint16_t *out_mv, uint16_t *out_vref) {
    uint16_t raw = 0;
    wink_status_t st = pal_adc_read_raw((pal_adc_channel_t)dev->adc_channel, &raw);
    if (st != WINK_OK) {
        dev->last_status = st;
        return st;
    }
    dev->last_raw = raw;

    uint16_t vref = dev->config.vref_mv;
    if (vref == 0) {
        (void)pal_adc_full_scale_mv((pal_adc_channel_t)dev->adc_channel, &vref);
        if (vref == 0) {
            vref = 3300u;  /* defensive fallback; PAL normally supplies a default */
        }
    }
    uint16_t mv = (uint16_t)(((uint32_t)raw * vref) / NTC_RAW_MAX);
    dev->last_mv = mv;

    *out_raw = raw;
    *out_mv = mv;
    *out_vref = vref;
    return WINK_OK;
}

static void dal_ntc_range_check(dal_ntc_t *dev, int32_t ddegc, wink_status_t *out_st) {
    int32_t max_ddegc = (int32_t)dev->config.max_valid_temp_c * 10;
    int32_t min_ddegc = (int32_t)dev->config.min_valid_temp_c * 10;
    if (ddegc > max_ddegc) {
        dev->last_status = WINK_ERR_OVERTEMPERATURE;
        *out_st = WINK_ERR_OVERTEMPERATURE;
    } else if (ddegc < min_ddegc) {
        dev->last_status = WINK_ERR_OUT_OF_RANGE;
        *out_st = WINK_ERR_OUT_OF_RANGE;
    } else {
        dev->last_status = WINK_OK;
        *out_st = WINK_OK;
    }
}

wink_status_t dal_ntc_read_ddegc(dal_ntc_t *dev, int16_t *out_ddegc) {
    if (dev == NULL || !dev->initialized || out_ddegc == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dev->config.lut_table == NULL) {
        return WINK_ERR_INVALID_STATE;
    }

    uint16_t raw = 0;
    uint16_t mv = 0;
    uint16_t vref = 0;
    wink_status_t st = dal_ntc_sample(dev, &raw, &mv, &vref);
    if (st != WINK_OK) {
        return st;
    }

    st = dal_ntc_check_safety(dev, mv, vref);
    if (st != WINK_OK) {
        return st;
    }

    int16_t ddegc = ntc_lut_interpolate_int16(dev->config.lut_table, raw);
    dev->last_ddegc = ddegc;
    *out_ddegc = ddegc;

    dal_ntc_range_check(dev, (int32_t)ddegc, &st);
    return st;
}

#if !defined(WINK_PROFILE_MICRO) && !defined(WINK_NO_FLOAT)
#include <math.h>

wink_status_t dal_ntc_read_degc(dal_ntc_t *dev, float *out_degc) {
    if (dev == NULL || !dev->initialized || out_degc == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    uint16_t raw = 0;
    uint16_t mv = 0;
    uint16_t vref = 0;
    wink_status_t st = dal_ntc_sample(dev, &raw, &mv, &vref);
    if (st != WINK_OK) {
        return st;
    }
    (void)raw;

    st = dal_ntc_check_safety(dev, mv, vref);
    if (st != WINK_OK) {
        return st;
    }

    float v_sig = (float)mv;
    float v_ref = (float)vref;
    float r_pull = (float)dev->config.r_pull_ohm;
    float r_ntc = (dev->config.divider_type == DAL_NTC_DIVIDER_PULL_UP)
                  ? (v_sig * r_pull) / (v_ref - v_sig)
                  : (r_pull * (v_ref - v_sig)) / v_sig;

    float inv_t = (1.0f / 298.15f) +
                  (1.0f / (float)dev->config.b_value) * logf(r_ntc / (float)dev->config.r25_ohm);
    if (inv_t <= 0.0f) {
        dev->last_status = WINK_ERR_HARDWARE;
        return WINK_ERR_HARDWARE;
    }
    float degc = (1.0f / inv_t) - 273.15f;

    if (isnan(degc) || isinf(degc)) {
        dev->last_status = WINK_ERR_HARDWARE;
        return WINK_ERR_HARDWARE;
    }

    dev->last_degc = degc;
    dev->last_ddegc = (degc > 3276.0f) ? 32760
                     : (degc < -3276.0f ? -32760
                                        : (int16_t)(degc * 10.0f));
    *out_degc = degc;

    if (degc > (float)dev->config.max_valid_temp_c) {
        dev->last_status = WINK_ERR_OVERTEMPERATURE;
        return WINK_ERR_OVERTEMPERATURE;
    }
    if (degc < (float)dev->config.min_valid_temp_c) {
        dev->last_status = WINK_ERR_OUT_OF_RANGE;
        return WINK_ERR_OUT_OF_RANGE;
    }
    dev->last_status = WINK_OK;
    return WINK_OK;
}
#endif /* !WINK_PROFILE_MICRO && !WINK_NO_FLOAT */

wink_status_t dal_ntc_read_raw(dal_ntc_t *dev, uint16_t *out_raw) {
    if (dev == NULL || !dev->initialized || out_raw == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    wink_status_t st = pal_adc_read_raw((pal_adc_channel_t)dev->adc_channel, out_raw);
    if (st == WINK_OK) {
        dev->last_raw = *out_raw;
    }
    dev->last_status = st;
    return st;
}

wink_status_t dal_ntc_read_mv(dal_ntc_t *dev, uint16_t *out_mv) {
    if (dev == NULL || !dev->initialized || out_mv == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    wink_status_t st = pal_adc_read_mv((pal_adc_channel_t)dev->adc_channel, out_mv);
    if (st == WINK_OK) {
        dev->last_mv = *out_mv;
    }
    dev->last_status = st;
    return st;
}

#endif /* !WINK_STRICT_NONBLOCKING */

wink_status_t dal_ntc_clear_faults(dal_ntc_t *dev) {
    if (dev == NULL || !dev->initialized) {
        return WINK_ERR_INVALID_ARG;
    }
    dev->fault_open = false;
    dev->fault_short = false;
    dev->fault_debounce = 0;
    dev->last_status = WINK_OK;
    return WINK_OK;
}
