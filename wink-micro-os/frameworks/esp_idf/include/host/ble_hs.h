/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "os/os_mbuf.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

#define BLE_HS_EAGAIN       1
#define BLE_HS_EALREADY     2
#define BLE_HS_EINVAL       3
#define BLE_HS_ENOMEM       4
#define BLE_HS_ENOTCONN     5
#define BLE_HS_ENOTSUP      6
#define BLE_HS_EAPP         7
#define BLE_HS_EDONE        8
#define BLE_HS_ETIMEOUT     9

typedef void ble_hs_sync_fn(void);
typedef void ble_hs_reset_fn(int reason);

struct ble_hs_cfg {
    ble_hs_sync_fn *sync_cb;
    ble_hs_reset_fn *reset_cb;
    ble_gatt_register_fn *gatts_register_cb;
    void *gatts_register_arg;
    void *store_status_cb;
};

extern struct ble_hs_cfg ble_hs_cfg;

int ble_hs_init(void);
int ble_hs_is_enabled(void);
int ble_hs_mbuf_to_flat(const struct os_mbuf *om, void *flat, uint16_t max_len, uint16_t *out_len);
struct os_mbuf *ble_hs_mbuf_from_flat(const void *buf, uint16_t len);
int ble_hs_id_infer_auto(int privacy, uint8_t *out_own_addr_type);
int ble_hs_util_ensure_addr(int prefer_random);

/* UniSim 仿真与前端交互导出类型 */
typedef struct {
    uint16_t handle;
    uint8_t  type;
    uint8_t  uuid_type;
    uint8_t  uuid_bytes[16];
} sim_ble_service_info_t;

typedef struct {
    uint16_t handle;
    uint16_t val_handle;
    uint16_t flags;
    uint8_t  uuid_type;
    uint8_t  uuid_bytes[16];
    bool     is_subscribed_notify;
    bool     is_subscribed_indicate;
} sim_ble_chr_info_t;

typedef void (*esp_nimble_sim_notify_hook_t)(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *data, uint16_t len);

void esp_nimble_sim_set_notify_hook(esp_nimble_sim_notify_hook_t hook);
WINK_SIM_EXPORT int esp_nimble_sim_get_service_count(void);
WINK_SIM_EXPORT int esp_nimble_sim_get_service_info(uint8_t index, sim_ble_service_info_t *out_info);
WINK_SIM_EXPORT int esp_nimble_sim_get_char_count(uint8_t svc_index);
WINK_SIM_EXPORT int esp_nimble_sim_get_char_info(uint8_t svc_index, uint8_t chr_index, sim_ble_chr_info_t *out_info);

/* UniSim 测试与操作注入 API */
WINK_SIM_EXPORT int esp_nimble_sim_is_advertising(void);
WINK_SIM_EXPORT int esp_nimble_sim_get_device_name(char *out_buf, size_t max_len);
WINK_SIM_EXPORT int esp_nimble_sim_connect(void);
WINK_SIM_EXPORT int esp_nimble_sim_disconnect(void);
WINK_SIM_EXPORT int esp_nimble_sim_read_chr(uint16_t conn_handle, uint16_t val_handle, void *out_buf, uint16_t max_len, uint16_t *out_len);
WINK_SIM_EXPORT int esp_nimble_sim_write_chr(uint16_t conn_handle, uint16_t val_handle, const void *data, uint16_t len);
WINK_SIM_EXPORT int esp_nimble_sim_subscribe(uint16_t conn_handle, uint16_t val_handle, bool notify, bool indicate);
WINK_SIM_EXPORT void esp_nimble_sim_reset(void);

#ifdef __cplusplus
}
#endif
