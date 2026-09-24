// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx system-clock output (CLO) functional model.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

void cms8s_clo_init(struct Mcu51Context* ctx);
void cms8s_clo_reset(struct Mcu51Context* ctx);
void cms8s_clo_poll(struct Mcu51Context* ctx);
uint64_t cms8s_clo_next_event_us(struct Mcu51Context* ctx);

// Test observability
bool     cms8s_clo_is_running(void);
uint32_t cms8s_clo_toggle_count(void);
uint32_t cms8s_clo_half_period_us(void);

#ifdef __cplusplus
}
#endif
