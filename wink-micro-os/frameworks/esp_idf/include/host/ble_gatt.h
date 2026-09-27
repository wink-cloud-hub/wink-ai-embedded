/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GATT_SVC_TYPE_END         0
#define BLE_GATT_SVC_TYPE_PRIMARY     1
#define BLE_GATT_SVC_TYPE_SECONDARY   2

#define BLE_GATT_CHR_F_BROADCAST      0x0001
#define BLE_GATT_CHR_F_READ           0x0002
#define BLE_GATT_CHR_F_WRITE_NO_RSP   0x0004
#define BLE_GATT_CHR_F_WRITE          0x0008
#define BLE_GATT_CHR_F_NOTIFY         0x0010
#define BLE_GATT_CHR_F_INDICATE       0x0020
#define BLE_GATT_CHR_F_AUTHEN         0x0040
#define BLE_GATT_CHR_F_AUTHOR         0x0080
#define BLE_GATT_CHR_F_ENC            0x0100

#define BLE_GATT_ACCESS_OP_READ_CHR   0
#define BLE_GATT_ACCESS_OP_WRITE_CHR  1
#define BLE_GATT_ACCESS_OP_READ_DSC   2
#define BLE_GATT_ACCESS_OP_WRITE_DSC  3

#define BLE_GATT_REGISTER_OP_SVC      1
#define BLE_GATT_REGISTER_OP_CHR      2
#define BLE_GATT_REGISTER_OP_DSC      3

struct ble_gatt_access_ctxt;
struct ble_gatt_chr_def;
struct ble_gatt_dsc_def;
struct ble_gatt_svc_def;

typedef int ble_gatt_access_fn(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

struct ble_gatt_access_ctxt {
    uint8_t op;
    union {
        struct {
            const struct ble_gatt_chr_def *chr;
        } chr;
        struct {
            const struct ble_gatt_dsc_def *dsc;
        } dsc;
    };
    struct os_mbuf *om;
};

struct ble_gatt_register_ctxt {
    uint8_t op;
    union {
        struct {
            const struct ble_gatt_svc_def *svc;
            uint16_t handle;
        } svc;
        struct {
            const struct ble_gatt_chr_def *chr;
            uint16_t handle;
            uint16_t val_handle;
        } chr;
        struct {
            const struct ble_gatt_dsc_def *dsc;
            uint16_t handle;
        } dsc;
    };
};

typedef void ble_gatt_register_fn(struct ble_gatt_register_ctxt *ctxt, void *arg);

struct ble_gatt_dsc_def {
    const ble_uuid_t *uuid;
    uint8_t att_flags;
    uint8_t min_key_size;
    ble_gatt_access_fn *access_cb;
    void *arg;
};

struct ble_gatt_chr_def {
    const ble_uuid_t *uuid;
    ble_gatt_access_fn *access_cb;
    void *arg;
    const struct ble_gatt_dsc_def *descriptors;
    uint16_t flags;
    uint8_t min_key_size;
    uint16_t *val_handle;
};

struct ble_gatt_svc_def {
    uint8_t type;
    const ble_uuid_t *uuid;
    const struct ble_gatt_svc_def **includes;
    const struct ble_gatt_chr_def *characteristics;
};

int ble_gatts_count_cfg(const struct ble_gatt_svc_def *defs);
int ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs);
int ble_gatts_start(void);
int ble_gatts_chr_updated(uint16_t chr_def_handle);
int ble_gatts_notify(uint16_t conn_handle, uint16_t chr_val_handle);
int ble_gatts_notify_custom(uint16_t conn_handle, uint16_t val_handle, struct os_mbuf *om);
int ble_gatts_find_chr(const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid,
                       uint16_t *out_def_handle, uint16_t *out_val_handle);

#ifdef __cplusplus
}
#endif
