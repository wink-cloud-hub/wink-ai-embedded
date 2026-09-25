/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ETM_H
#define WINK_H_GUARD_ESP_ETM_H
#ifndef __WINK_HARVESTED_ESP_ETM_H__
#define __WINK_HARVESTED_ESP_ETM_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "hal/etm_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct esp_etm_channel_t * esp_etm_channel_handle_t;
typedef struct esp_etm_event_t * esp_etm_event_handle_t;
typedef struct esp_etm_task_t * esp_etm_task_handle_t;
typedef struct {
etm_clock_source_t clk_src; 
    
    struct etm_chan_flags {
        uint32_t allow_pd : 1; 

    } flags;
} esp_etm_channel_config_t;



#if defined(__WINK_SIM__)
esp_err_t esp_etm_channel_connect(esp_etm_channel_handle_t chan, esp_etm_event_handle_t event, esp_etm_task_handle_t task) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_channel_connect out of Core 8 scope.");
#else
esp_err_t esp_etm_channel_connect(esp_etm_channel_handle_t chan, esp_etm_event_handle_t event, esp_etm_task_handle_t task);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_channel_disable(esp_etm_channel_handle_t chan) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_channel_disable out of Core 8 scope.");
#else
esp_err_t esp_etm_channel_disable(esp_etm_channel_handle_t chan);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_channel_enable(esp_etm_channel_handle_t chan) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_channel_enable out of Core 8 scope.");
#else
esp_err_t esp_etm_channel_enable(esp_etm_channel_handle_t chan);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_del_channel(esp_etm_channel_handle_t chan) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_del_channel out of Core 8 scope.");
#else
esp_err_t esp_etm_del_channel(esp_etm_channel_handle_t chan);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_del_event(esp_etm_event_handle_t event) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_del_event out of Core 8 scope.");
#else
esp_err_t esp_etm_del_event(esp_etm_event_handle_t event);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_del_task(esp_etm_task_handle_t task) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_del_task out of Core 8 scope.");
#else
esp_err_t esp_etm_del_task(esp_etm_task_handle_t task);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_dump(FILE *out_stream) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_dump out of Core 8 scope.");
#else
esp_err_t esp_etm_dump(FILE *out_stream);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_etm_new_channel(const esp_etm_channel_config_t *config, esp_etm_channel_handle_t *ret_chan) WINK_SLA_ERROR("Wink SLA Violation: esp_etm_new_channel out of Core 8 scope.");
#else
esp_err_t esp_etm_new_channel(const esp_etm_channel_config_t *config, esp_etm_channel_handle_t *ret_chan);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ETM_H__ */
#endif /* WINK_H_GUARD_ESP_ETM_H */
