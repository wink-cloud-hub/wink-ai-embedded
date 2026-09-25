/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "hal/spi_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef BUS_LOCK_DEBUG
#define BUS_LOCK_DEBUG 0
#endif
#ifndef BUS_LOCK_DEBUG_EXECUTE_CHECK
#define BUS_LOCK_DEBUG_EXECUTE_CHECK(x) assert(x)
#endif
#ifndef CHECK_IOMUX_PIN
#define CHECK_IOMUX_PIN(HOST, PIN_NAME) if (GPIO.func_in_sel_cfg[spi_periph_signal[(HOST)].PIN_NAME##_in].sig_in_sel) return false
#endif
#ifndef DEV_NUM_MAX
#define DEV_NUM_MAX 6
#endif
#ifndef SPI_BUS_LOCK_DEV_FLAG_CS_REQUIRED
#define SPI_BUS_LOCK_DEV_FLAG_CS_REQUIRED BIT(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct spi_bus_lock_t * spi_bus_lock_handle_t;
typedef struct spi_bus_lock_dev_t * spi_bus_lock_dev_handle_t;
typedef void (*bg_ctrl_func_t)(void*);
typedef struct {
    int host_id;
    int cs_num;
} spi_bus_lock_config_t;
typedef struct {
    uint32_t flags;
} spi_bus_lock_dev_config_t;



#if defined(__WINK_SIM__)
void spi_bus_deinit_lock(spi_bus_lock_handle_t lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_deinit_lock out of Core 8 scope.");
#else
void spi_bus_deinit_lock(spi_bus_lock_handle_t lock);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_init_lock(spi_bus_lock_handle_t *out_lock, const spi_bus_lock_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_init_lock out of Core 8 scope.");
#else
esp_err_t spi_bus_init_lock(spi_bus_lock_handle_t *out_lock, const spi_bus_lock_config_t *config);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_acquire_end(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_acquire_end out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_acquire_end(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_acquire_start(spi_bus_lock_dev_handle_t dev_handle, uint32_t wait) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_acquire_start out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_acquire_start(spi_bus_lock_dev_handle_t dev_handle, uint32_t wait);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_check_dev_acq(spi_bus_lock_handle_t lock, spi_bus_lock_dev_handle_t *out_dev_lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_check_dev_acq out of Core 8 scope.");
#else
bool spi_bus_lock_bg_check_dev_acq(spi_bus_lock_handle_t lock, spi_bus_lock_dev_handle_t *out_dev_lock);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_check_dev_req(spi_bus_lock_dev_handle_t dev_lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_check_dev_req out of Core 8 scope.");
#else
bool spi_bus_lock_bg_check_dev_req(spi_bus_lock_dev_handle_t dev_lock);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_clear_req(spi_bus_lock_dev_handle_t dev_lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_clear_req out of Core 8 scope.");
#else
bool spi_bus_lock_bg_clear_req(spi_bus_lock_dev_handle_t dev_lock);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_entry(spi_bus_lock_handle_t lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_entry out of Core 8 scope.");
#else
bool spi_bus_lock_bg_entry(spi_bus_lock_handle_t lock);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_exit(spi_bus_lock_handle_t lock, bool wip, int* do_yield) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_exit out of Core 8 scope.");
#else
bool spi_bus_lock_bg_exit(spi_bus_lock_handle_t lock, bool wip, int* do_yield);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_bg_req_exist(spi_bus_lock_handle_t lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_req_exist out of Core 8 scope.");
#else
bool spi_bus_lock_bg_req_exist(spi_bus_lock_handle_t lock);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_bg_request(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_bg_request out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_bg_request(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
spi_bus_lock_dev_handle_t spi_bus_lock_get_acquiring_dev(spi_bus_lock_handle_t lock) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_get_acquiring_dev out of Core 8 scope.");
#else
spi_bus_lock_dev_handle_t spi_bus_lock_get_acquiring_dev(spi_bus_lock_handle_t lock);
#endif

#if defined(__WINK_SIM__)
spi_bus_lock_handle_t spi_bus_lock_get_by_id(spi_host_device_t host_id) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_get_by_id out of Core 8 scope.");
#else
spi_bus_lock_handle_t spi_bus_lock_get_by_id(spi_host_device_t host_id);
#endif

#if defined(__WINK_SIM__)
int spi_bus_lock_get_dev_id(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_get_dev_id out of Core 8 scope.");
#else
int spi_bus_lock_get_dev_id(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
spi_bus_lock_handle_t spi_bus_lock_get_parent(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_get_parent out of Core 8 scope.");
#else
spi_bus_lock_handle_t spi_bus_lock_get_parent(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_init_main_dev(void) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_init_main_dev out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_init_main_dev(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_register_dev(spi_bus_lock_handle_t lock,
                                    spi_bus_lock_dev_config_t *config,
                                    spi_bus_lock_dev_handle_t *out_dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_register_dev out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_register_dev(spi_bus_lock_handle_t lock,
                                    spi_bus_lock_dev_config_t *config,
                                    spi_bus_lock_dev_handle_t *out_dev_handle);
#endif

#if defined(__WINK_SIM__)
void spi_bus_lock_set_bg_control(spi_bus_lock_handle_t lock, bg_ctrl_func_t bg_enable,
                                 bg_ctrl_func_t bg_disable, void *arg) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_set_bg_control out of Core 8 scope.");
#else
void spi_bus_lock_set_bg_control(spi_bus_lock_handle_t lock, bg_ctrl_func_t bg_enable,
                                 bg_ctrl_func_t bg_disable, void *arg);
#endif

#if defined(__WINK_SIM__)
bool spi_bus_lock_touch(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_touch out of Core 8 scope.");
#else
bool spi_bus_lock_touch(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
void spi_bus_lock_unregister_dev(spi_bus_lock_dev_handle_t dev_handle) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_unregister_dev out of Core 8 scope.");
#else
void spi_bus_lock_unregister_dev(spi_bus_lock_dev_handle_t dev_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t spi_bus_lock_wait_bg_done(spi_bus_lock_dev_handle_t dev_handle, uint32_t wait) WINK_SLA_ERROR("Wink SLA Violation: spi_bus_lock_wait_bg_done out of Core 8 scope.");
#else
esp_err_t spi_bus_lock_wait_bg_done(spi_bus_lock_dev_handle_t dev_handle, uint32_t wait);
#endif

#if defined(__WINK_SIM__)
bool spicommon_periph_claim(spi_host_device_t host, const char* source) WINK_SLA_ERROR("Wink SLA Violation: spicommon_periph_claim out of Core 8 scope.");
#else
bool spicommon_periph_claim(spi_host_device_t host, const char* source);
#endif

#if defined(__WINK_SIM__)
bool spicommon_periph_free(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spicommon_periph_free out of Core 8 scope.");
#else
bool spicommon_periph_free(spi_host_device_t host);
#endif

#if defined(__WINK_SIM__)
bool spicommon_periph_in_use(spi_host_device_t host) WINK_SLA_ERROR("Wink SLA Violation: spicommon_periph_in_use out of Core 8 scope.");
#else
bool spicommon_periph_in_use(spi_host_device_t host);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SPI_SHARE_HW_CTRL_H */
