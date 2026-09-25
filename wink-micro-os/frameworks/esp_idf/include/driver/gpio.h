/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_GPIO_H
#define WINK_H_GUARD_DRIVER_GPIO_H
#ifndef __WINK_HARVESTED_DRIVER_GPIO_H__
#define __WINK_HARVESTED_DRIVER_GPIO_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio_etm.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef GPIO_IS_HP_PERIPH_PD_WAKEUP_VALID_IO
#define GPIO_IS_HP_PERIPH_PD_WAKEUP_VALID_IO(gpio_num) ((gpio_num >= 0) &&  (((1ULL << (gpio_num)) & SOC_GPIO_HP_PERIPH_PD_SLEEP_WAKEABLE_MASK) != 0))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef intr_handle_t gpio_isr_handle_t;
typedef void (*gpio_isr_t)(void *arg);
typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
void gpio_deep_sleep_hold_dis(void);
void gpio_deep_sleep_hold_en(void);
esp_err_t gpio_dump_io_configuration(FILE *out_stream, uint64_t io_bit_mask);
esp_err_t gpio_get_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t *strength);
esp_err_t gpio_get_io_config(gpio_num_t gpio_num, gpio_io_config_t *out_io_config);
int gpio_get_level(gpio_num_t gpio_num);
esp_err_t gpio_hold_dis(gpio_num_t gpio_num);
esp_err_t gpio_hold_en(gpio_num_t gpio_num);
esp_err_t gpio_input_enable(gpio_num_t gpio_num);
esp_err_t gpio_install_isr_service(int intr_alloc_flags);
esp_err_t gpio_intr_disable(gpio_num_t gpio_num);
esp_err_t gpio_intr_enable(gpio_num_t gpio_num);
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args);
esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num);
esp_err_t gpio_isr_register(void (*fn)(void *), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle);
esp_err_t gpio_od_disable(gpio_num_t gpio_num);
esp_err_t gpio_od_enable(gpio_num_t gpio_num);
esp_err_t gpio_output_disable(gpio_num_t gpio_num);
esp_err_t gpio_output_enable(gpio_num_t gpio_num);
esp_err_t gpio_pulldown_dis(gpio_num_t gpio_num);
esp_err_t gpio_pulldown_en(gpio_num_t gpio_num);
esp_err_t gpio_pullup_dis(gpio_num_t gpio_num);
esp_err_t gpio_pullup_en(gpio_num_t gpio_num);
esp_err_t gpio_reset_pin(gpio_num_t gpio_num);
esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode);
esp_err_t gpio_set_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t strength);
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull);
esp_err_t gpio_sleep_sel_dis(gpio_num_t gpio_num);
esp_err_t gpio_sleep_sel_en(gpio_num_t gpio_num);
esp_err_t gpio_sleep_set_direction(gpio_num_t gpio_num, gpio_mode_t mode);
esp_err_t gpio_sleep_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull);
esp_err_t gpio_uninstall_isr_service(void);
esp_err_t gpio_wakeup_disable(gpio_num_t gpio_num);
esp_err_t gpio_wakeup_disable_on_hp_periph_powerdown_sleep(gpio_num_t gpio_num);
esp_err_t gpio_wakeup_enable(gpio_num_t gpio_num, gpio_int_type_t intr_type);
esp_err_t gpio_wakeup_enable_on_hp_periph_powerdown_sleep(gpio_num_t gpio_num, gpio_int_type_t intr_type);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_GPIO_H__ */
#endif /* WINK_H_GUARD_DRIVER_GPIO_H */
