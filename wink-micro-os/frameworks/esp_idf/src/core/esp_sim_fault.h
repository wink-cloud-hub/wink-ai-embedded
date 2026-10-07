/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_SIM_FAULT_H
#define ESP_SIM_FAULT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WINK_SIM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 故障注入领域定义 (Domains) */
typedef enum {
    ESP_FAULT_DOMAIN_NONE = 0,
    ESP_FAULT_DOMAIN_I2C,
    ESP_FAULT_DOMAIN_SPI,
    ESP_FAULT_DOMAIN_WIFI,
    ESP_FAULT_DOMAIN_NETIF,
    ESP_FAULT_DOMAIN_NVS,
    ESP_FAULT_DOMAIN_BLE,
    ESP_FAULT_DOMAIN_DEDIC_GPIO,
    ESP_FAULT_DOMAIN_MAX
} esp_fault_domain_t;

/* 故障类型定义 (Types) */
typedef enum {
    ESP_FAULT_TYPE_NONE = 0,
    ESP_FAULT_I2C_NACK,            /* return ESP_ERR_NOT_FOUND */
    ESP_FAULT_I2C_TIMEOUT,         /* return ESP_ERR_TIMEOUT */
    ESP_FAULT_SPI_TRANSFER_FAIL,   /* return ESP_ERR_INVALID_RESPONSE */
    ESP_FAULT_WIFI_AUTH_FAIL,      /* trigger WIFI_EVENT_STA_DISCONNECTED (param = reason or 204) */
    ESP_FAULT_WIFI_BEACON_TIMEOUT, /* beacon loss -> DISCONNECTED (reason 200) */
    ESP_FAULT_NETIF_DHCP_TIMEOUT,  /* DHCP fail -> no IP */
    ESP_FAULT_NVS_PARTITION_FULL,  /* return ESP_ERR_NVS_NOT_ENOUGH_SPACE */
    ESP_FAULT_NVS_READ_CORRUPT,    /* return ESP_ERR_NVS_CORRUPT_KEY_PART */
    ESP_FAULT_BLE_ADV_REJECT,      /* ble_gap_adv_start failure (return BLE_HS_EINVAL) */
    ESP_FAULT_DEDIC_GPIO_ALLOC_FAIL, /* return ESP_ERR_INVALID_STATE */
    ESP_FAULT_TYPE_MAX
} esp_fault_type_t;

/* 仿真受控故障注入 API 与 Wasm 导出 */
WINK_SIM_EXPORT int sim_esp_fault_inject(uint32_t domain, uint32_t fault_type, uint32_t param);
WINK_SIM_EXPORT int sim_esp_fault_clear(void);
WINK_SIM_EXPORT bool sim_esp_fault_is_active(uint32_t domain, uint32_t fault_type);
WINK_SIM_EXPORT uint32_t sim_esp_fault_get_param(uint32_t domain, uint32_t fault_type);

/* 软复位销毁钩子：清空所有注入故障，防止跨测试用例污染 */
void esp_fault_sim_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_SIM_FAULT_H */
