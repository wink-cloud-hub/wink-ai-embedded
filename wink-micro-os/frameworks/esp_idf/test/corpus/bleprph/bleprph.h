/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <stdint.h>
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Heart Rate Service UUIDs */
#define GATT_SVR_SVC_HEART_RATE_UUID 0x180D
#define GATT_SVR_CHR_HEART_RATE_MEASUREMENT_UUID 0x2A37
#define GATT_SVR_CHR_BODY_SENSOR_LOCATION_UUID 0x2A38
#define GATT_SVR_CHR_HEART_RATE_CONTROL_POINT_UUID 0x2A39

/* Device Information Service UUIDs */
#define GATT_SVR_SVC_DEVICE_INFO_UUID 0x180A
#define GATT_SVR_CHR_MANUFACTURER_NAME_UUID 0x2A29
#define GATT_SVR_CHR_MODEL_NUMBER_UUID 0x2A24

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_svr_init(void);

#ifdef __cplusplus
}
#endif
