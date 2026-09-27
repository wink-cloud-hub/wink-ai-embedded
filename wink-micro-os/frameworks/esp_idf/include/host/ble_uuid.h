/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BLE_UUID_TYPE_16 = 16,
    BLE_UUID_TYPE_32 = 32,
    BLE_UUID_TYPE_128 = 128,
};

typedef struct ble_uuid {
    uint8_t type;
} ble_uuid_t;

typedef struct ble_uuid16 {
    ble_uuid_t u;
    uint16_t value;
} ble_uuid16_t;

typedef struct ble_uuid32 {
    ble_uuid_t u;
    uint32_t value;
} ble_uuid32_t;

typedef struct ble_uuid128 {
    ble_uuid_t u;
    uint8_t value[16];
} ble_uuid128_t;

typedef union {
    ble_uuid_t u;
    ble_uuid16_t u16;
    ble_uuid32_t u32;
    ble_uuid128_t u128;
} ble_uuid_any_t;

#define BLE_UUID16_INIT(val) { .u = { .type = BLE_UUID_TYPE_16 }, .value = (val) }
#define BLE_UUID32_INIT(val) { .u = { .type = BLE_UUID_TYPE_32 }, .value = (val) }
#define BLE_UUID128_INIT(...) { .u = { .type = BLE_UUID_TYPE_128 }, .value = { __VA_ARGS__ } }

#define BLE_UUID16_DECLARE(val) \
    ((const ble_uuid_t *) (&(const ble_uuid16_t) BLE_UUID16_INIT(val)))

#define BLE_UUID32_DECLARE(val) \
    ((const ble_uuid_t *) (&(const ble_uuid32_t) BLE_UUID32_INIT(val)))

#define BLE_UUID128_DECLARE(...) \
    ((const ble_uuid_t *) (&(const ble_uuid128_t) BLE_UUID128_INIT(__VA_ARGS__)))

#define BLE_UUID16(u) ((const ble_uuid16_t *)(u))
#define BLE_UUID32(u) ((const ble_uuid32_t *)(u))
#define BLE_UUID128(u) ((const ble_uuid128_t *)(u))

#define BLE_UUID_STR_LEN 37

int ble_uuid_cmp(const ble_uuid_t *a, const ble_uuid_t *b);
char *ble_uuid_to_str(const ble_uuid_t *uuid, char *dst);
uint16_t ble_uuid_u16(const ble_uuid_t *uuid);

#ifdef __cplusplus
}
#endif
