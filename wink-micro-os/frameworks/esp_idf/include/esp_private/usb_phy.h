/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_USB_PHY_H
#define WINK_H_GUARD_ESP_PRIVATE_USB_PHY_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_USB_PHY_H__
#define __WINK_HARVESTED_ESP_PRIVATE_USB_PHY_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef USB_PHY_SELF_POWERED_DEVICE
#define USB_PHY_SELF_POWERED_DEVICE(vbus_monitor_io) {                                                                                                                   .iddig_io_num = -1,                                                                                             .avalid_io_num = -1,                                                                                            .vbusvalid_io_num = -1,                                                                                         .idpullup_io_num = -1,                                                                                          .dppulldown_io_num = -1,                                                                                        .dmpulldown_io_num = -1,                                                                                        .drvvbus_io_num = -1,                                                                                           .bvalid_io_num = vbus_monitor_io,                                                                               .sessend_io_num = -1,                                                                                           .chrgvbus_io_num = -1,                                                                                          .dischrgvbus_io_num = -1,                                                                                       };
#endif
#ifndef USB_PHY_SUPPORTS_P4_OTG11
#define USB_PHY_SUPPORTS_P4_OTG11 1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    USB_PHY_TARGET_INT = 0,
    USB_PHY_TARGET_UTMI = 1,
    USB_PHY_TARGET_EXT = 2,
    USB_PHY_TARGET_MAX = 3,
} usb_phy_target_t;
typedef enum {
    USB_PHY_CTRL_OTG = 0,
    USB_PHY_CTRL_MAX = 1,
} usb_phy_controller_t;
typedef enum {
    USB_PHY_MODE_DEFAULT = 0,
    USB_OTG_MODE_HOST = 1,
    USB_OTG_MODE_DEVICE = 2,
    USB_OTG_MODE_MAX = 3,
} usb_otg_mode_t;
typedef enum {
    USB_PHY_SPEED_UNDEFINED = 0,
    USB_PHY_SPEED_LOW = 1,
    USB_PHY_SPEED_FULL = 2,
    USB_PHY_SPEED_HIGH = 3,
    USB_PHY_SPEED_MAX = 4,
} usb_phy_speed_t;
typedef enum {
    USB_PHY_STATUS_FREE = 0,
    USB_PHY_STATUS_IN_USE = 1,
} usb_phy_status_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    int vp_io_num;
    int vm_io_num;
    int rcv_io_num;
    int suspend_n_io_num;
    int oen_io_num;
    int vpo_io_num;
    int vmo_io_num;
    int fs_edge_sel_io_num;
} usb_phy_ext_io_conf_t;
typedef struct {
    int iddig_io_num;
    int avalid_io_num;
    int vbusvalid_io_num;
    int idpullup_io_num;
    int dppulldown_io_num;
    int dmpulldown_io_num;
    int drvvbus_io_num;
    int bvalid_io_num;
    int sessend_io_num;
    int chrgvbus_io_num;
    int dischrgvbus_io_num;
} usb_phy_otg_io_conf_t;
typedef struct {
    usb_phy_controller_t controller;
    usb_phy_target_t target;
    usb_otg_mode_t otg_mode;
    usb_phy_speed_t otg_speed;
    const usb_phy_ext_io_conf_t * ext_io_conf;
    const usb_phy_otg_io_conf_t * otg_io_conf;
} usb_phy_config_t;
typedef struct phy_context_t * usb_phy_handle_t;



#if defined(__WINK_SIM__)
esp_err_t usb_del_phy(usb_phy_handle_t handle) WINK_SLA_ERROR("Wink SLA Violation: usb_del_phy out of Core 8 scope.");
#else
esp_err_t usb_del_phy(usb_phy_handle_t handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t usb_new_phy(const usb_phy_config_t *config, usb_phy_handle_t *handle_ret) WINK_SLA_ERROR("Wink SLA Violation: usb_new_phy out of Core 8 scope.");
#else
esp_err_t usb_new_phy(const usb_phy_config_t *config, usb_phy_handle_t *handle_ret);
#endif

#if defined(__WINK_SIM__)
void usb_phy_clear_otg_wakeup_status(void) WINK_SLA_ERROR("Wink SLA Violation: usb_phy_clear_otg_wakeup_status out of Core 8 scope.");
#else
void usb_phy_clear_otg_wakeup_status(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t usb_phy_get_phy_status(usb_phy_target_t target, usb_phy_status_t *status) WINK_SLA_ERROR("Wink SLA Violation: usb_phy_get_phy_status out of Core 8 scope.");
#else
esp_err_t usb_phy_get_phy_status(usb_phy_target_t target, usb_phy_status_t *status);
#endif

#if defined(__WINK_SIM__)
esp_err_t usb_phy_otg_set_mode(usb_phy_handle_t handle, usb_otg_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: usb_phy_otg_set_mode out of Core 8 scope.");
#else
esp_err_t usb_phy_otg_set_mode(usb_phy_handle_t handle, usb_otg_mode_t mode);
#endif

#if defined(__WINK_SIM__)
void usb_phy_set_otg_suspend_state(bool in_suspend) WINK_SLA_ERROR("Wink SLA Violation: usb_phy_set_otg_suspend_state out of Core 8 scope.");
#else
void usb_phy_set_otg_suspend_state(bool in_suspend);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_USB_PHY_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_USB_PHY_H */
