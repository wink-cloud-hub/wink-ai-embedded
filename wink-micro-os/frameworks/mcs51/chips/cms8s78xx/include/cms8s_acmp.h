// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip Analog Comparator (ACMP0/ACMP1) peripheral model.
#pragma once

#include <stdint.h>

struct Mcu51Context;

#ifdef __cplusplus
extern "C" {
#endif

void cms8s_acmp_init(struct Mcu51Context* ctx);
void cms8s_acmp_reset(struct Mcu51Context* ctx);
void cms8s_acmp_poll(struct Mcu51Context* ctx);
uint64_t cms8s_acmp_next_event_us(struct Mcu51Context* ctx);

#ifdef __cplusplus
}
#endif
