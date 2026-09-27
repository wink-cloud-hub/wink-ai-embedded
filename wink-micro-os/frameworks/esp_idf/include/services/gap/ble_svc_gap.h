/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_svc_gap_init(void);
int ble_svc_gap_device_name_set(const char *name);
const char *ble_svc_gap_device_name(void);
int ble_svc_gap_device_appearance_set(uint16_t appearance);
uint16_t ble_svc_gap_device_appearance(void);

#ifdef __cplusplus
}
#endif
