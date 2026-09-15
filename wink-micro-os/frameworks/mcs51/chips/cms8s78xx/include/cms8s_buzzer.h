// SPDX-License-Identifier: GPL-3.0-only
// CMS8S78xx on-chip Buzzer functional model.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

void cms8s_buzzer_init(struct Mcu51Context* ctx);
void cms8s_buzzer_reset(struct Mcu51Context* ctx);
void cms8s_buzzer_poll(struct Mcu51Context* ctx);
uint64_t cms8s_buzzer_next_event_us(struct Mcu51Context* ctx);

// Test observability
bool     cms8s_buzzer_is_running(void);
uint32_t cms8s_buzzer_toggle_count(void);
uint32_t cms8s_buzzer_half_period_us(void);

#ifdef __cplusplus
}
#endif
