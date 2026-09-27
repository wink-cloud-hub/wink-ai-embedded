/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GAP_EVENT_CONNECT        0
#define BLE_GAP_EVENT_DISCONNECT     1
#define BLE_GAP_EVENT_DIR_ADV_EXP    2
#define BLE_GAP_EVENT_DISC           3
#define BLE_GAP_EVENT_ADV_COMPLETE   4
#define BLE_GAP_EVENT_SUBSCRIBE      5
#define BLE_GAP_EVENT_MTU            6
#define BLE_GAP_EVENT_NOTIFY_TX      7
#define BLE_GAP_EVENT_CONN_UPDATE    8

#define BLE_ADDR_PUBLIC              0
#define BLE_ADDR_RANDOM              1
#define BLE_ADDR_PUBLIC_ID           2
#define BLE_ADDR_RANDOM_ID           3

#define BLE_GAP_DISC_MODE_NON        0
#define BLE_GAP_DISC_MODE_LTD        1
#define BLE_GAP_DISC_MODE_GEN        2

#define BLE_GAP_CONN_MODE_NON        0
#define BLE_GAP_CONN_MODE_DIR        1
#define BLE_GAP_CONN_MODE_UND        2

#define BLE_GAP_ADV_FAST_INTERVAL1_MIN 0x0030
#define BLE_GAP_ADV_FAST_INTERVAL1_MAX 0x0060

typedef struct {
    uint8_t type;
    uint8_t val[6];
} ble_addr_t;

struct ble_gap_conn_desc {
    uint16_t conn_handle;
    uint16_t conn_itvl;
    uint16_t conn_latency;
    uint16_t supervision_timeout;
    uint8_t role;
    uint8_t master_id;
    ble_addr_t peer_id_addr;
    ble_addr_t peer_ota_addr;
    ble_addr_t our_id_addr;
    ble_addr_t our_ota_addr;
};

struct ble_gap_event {
    uint8_t type;
    union {
        struct {
            int status;
            struct ble_gap_conn_desc conn;
        } connect;
        struct {
            int reason;
            struct ble_gap_conn_desc conn;
        } disconnect;
        struct {
            int reason;
        } adv_complete;
        struct {
            uint16_t conn_handle;
            uint16_t attr_handle;
            uint8_t reason;
            uint8_t prev_notify:1;
            uint8_t cur_notify:1;
            uint8_t prev_indicate:1;
            uint8_t cur_indicate:1;
        } subscribe;
        struct {
            uint16_t conn_handle;
            uint16_t channel_id;
            uint16_t value;
        } mtu;
        struct {
            int status;
            uint16_t conn_handle;
            uint16_t attr_handle;
            uint8_t indication:1;
        } notify_tx;
        struct {
            int status;
            struct ble_gap_conn_desc conn;
        } conn_update;
    };
};

typedef int ble_gap_event_fn(struct ble_gap_event *event, void *arg);

struct ble_gap_adv_params {
    uint8_t conn_mode;
    uint8_t disc_mode;
    uint16_t itvl_min;
    uint16_t itvl_max;
    uint8_t channel_map;
    uint8_t filter_policy;
    uint8_t high_duty_cycle:1;
};

struct ble_hs_adv_fields {
    uint8_t flags;
    const uint8_t *name;
    uint8_t name_len;
    uint8_t name_is_complete:1;
    const ble_uuid16_t *uuids16;
    uint8_t num_uuids16;
    uint8_t uuids16_is_complete:1;
    const ble_uuid32_t *uuids32;
    uint8_t num_uuids32;
    uint8_t uuids32_is_complete:1;
    const ble_uuid128_t *uuids128;
    uint8_t num_uuids128;
    uint8_t uuids128_is_complete:1;
    int8_t tx_pwr_lvl;
    uint8_t tx_pwr_lvl_is_present:1;
    uint16_t appearance;
    uint8_t appearance_is_present:1;
};

#define BLE_HS_ADV_F_DISC_LTD 0x01
#define BLE_HS_ADV_F_DISC_GEN 0x02
#define BLE_HS_ADV_F_BREDR_UNSUP 0x04

int ble_gap_adv_start(uint8_t own_addr_type, const ble_addr_t *direct_addr,
                      int32_t duration_ms, const struct ble_gap_adv_params *adv_params,
                      ble_gap_event_fn *cb, void *cb_arg);
int ble_gap_adv_stop(void);
int ble_gap_adv_active(void);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *adv_fields);
int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields *rsp_fields);
int ble_gap_conn_find(uint16_t conn_handle, struct ble_gap_conn_desc *out_desc);
int ble_gap_terminate(uint16_t conn_handle, uint8_t hci_reason);

#ifdef __cplusplus
}
#endif
