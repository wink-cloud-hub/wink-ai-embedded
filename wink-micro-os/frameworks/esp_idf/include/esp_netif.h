/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "esp_err.h"
#include "esp_netif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t    esp_netif_init(void);
esp_err_t    esp_netif_deinit(void);
esp_netif_t* esp_netif_create_default_wifi_sta(void);
esp_netif_t* esp_netif_create_default_wifi_ap(void);
esp_err_t    esp_netif_destroy_default_wifi(esp_netif_t *netif);
esp_err_t    esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip_info);

esp_err_t    esp_netif_get_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);
esp_err_t    esp_netif_set_dns_info(esp_netif_t *netif, esp_netif_dns_type_t type, const esp_netif_dns_info_t *dns);
esp_err_t    esp_netif_dhcps_stop(esp_netif_t *netif);
esp_err_t    esp_netif_dhcps_start(esp_netif_t *netif);
esp_err_t    esp_netif_dhcps_option(esp_netif_t *netif, esp_netif_dhcp_option_mode_t opt_op, esp_netif_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len);
esp_err_t    esp_netif_set_default_netif(esp_netif_t *netif);
esp_err_t    esp_netif_napt_enable(esp_netif_t *netif);
esp_err_t    esp_netif_napt_disable(esp_netif_t *netif);

#ifdef __cplusplus
}
#endif
