// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx Low-Voltage Detect (LVD) peripheral model.
#pragma once

#include <stdint.h>

struct Mcu51Context;

#ifdef __cplusplus
extern "C" {
#endif

void cms8s_lvd_init(struct Mcu51Context* ctx);
void cms8s_lvd_reset(struct Mcu51Context* ctx);
void cms8s_lvd_poll(struct Mcu51Context* ctx);
uint64_t cms8s_lvd_next_event_us(struct Mcu51Context* ctx);

// Host-only test seam: inject a virtual VDD level in millivolts (0..5000).
// Writes the rail key backing LVD_VDD_SENSE_PIN; never touches the ADC
// reference rail (mcs51_adc_set_vrail_mv). Headless scenarios inject the
// same key through INPUT_ANALOG (adcChannel 62).
void cms8s_lvd_set_vdd_mv(struct Mcu51Context* ctx, uint16_t mv);

#ifdef __cplusplus
}
#endif
