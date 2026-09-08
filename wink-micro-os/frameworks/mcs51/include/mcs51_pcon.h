// SPDX-License-Identifier: Apache-2.0
// MCS-51 PCON low-power modes (IDLE / Power-Down) dual-mode scheduling (Task R6).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

#ifndef WINK_MCS51_SCENARIO_HORIZON_MS
#define WINK_MCS51_SCENARIO_HORIZON_MS 1000u
#endif

// Calculate next internal scheduled event timestamp across peripherals and edge queue
uint64_t mcs51_calc_next_event_us(struct Mcu51Context* ctx, bool is_pd);

// SFR write hook for PCON (0x87)
void mcs51_on_pcon_write(struct Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val);

// Scenario termination hook when silent wait times out on horizon
void wink_mcs51_scenario_terminate_timeout(struct Mcu51Context* ctx);

#ifdef __cplusplus
}
#endif
