/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_DEDIC_GPIO_H
#define WINK_H_GUARD_DRIVER_DEDIC_GPIO_H
#ifndef __WINK_HARVESTED_DRIVER_DEDIC_GPIO_H__
#define __WINK_HARVESTED_DRIVER_DEDIC_GPIO_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct dedic_gpio_bundle_t * dedic_gpio_bundle_handle_t;
typedef struct {
const int *gpio_array; 
    size_t array_size;     
    struct {
        unsigned int in_en: 1;      
        unsigned int in_invert: 1;  
        unsigned int out_en: 1;     
        unsigned int out_invert: 1; 
    } flags;
} dedic_gpio_bundle_config_t;

esp_err_t dedic_gpio_del_bundle(dedic_gpio_bundle_handle_t bundle);
esp_err_t dedic_gpio_get_in_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask);
esp_err_t dedic_gpio_get_in_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset);
esp_err_t dedic_gpio_get_out_mask(dedic_gpio_bundle_handle_t bundle, uint32_t *mask);
esp_err_t dedic_gpio_get_out_offset(dedic_gpio_bundle_handle_t bundle, uint32_t *offset);
esp_err_t dedic_gpio_new_bundle(const dedic_gpio_bundle_config_t *config, dedic_gpio_bundle_handle_t *ret_bundle);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_DEDIC_GPIO_H__ */
#endif /* WINK_H_GUARD_DRIVER_DEDIC_GPIO_H */
