/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "esp_err.h"
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t   esp_netif_init(void);
esp_err_t   esp_netif_deinit(void);
esp_netif_t esp_netif_create_default_wifi_sta(void);
esp_err_t   esp_netif_destroy_default_wifi(esp_netif_t netif);
esp_err_t   esp_netif_get_ip_info(esp_netif_t netif, esp_netif_ip_info_t *ip_info);

#ifdef __cplusplus
}
#endif
