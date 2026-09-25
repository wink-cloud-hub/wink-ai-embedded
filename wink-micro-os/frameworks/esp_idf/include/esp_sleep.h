/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_SLEEP_H
#define WINK_H_GUARD_ESP_SLEEP_H
#ifndef __WINK_HARVESTED_ESP_SLEEP_H__
#define __WINK_HARVESTED_ESP_SLEEP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_PD_DOMAIN_RTC8M
#define ESP_PD_DOMAIN_RTC8M _Pragma("GCC warning \"'ESP_PD_DOMAIN_RTC8M' enum is deprecated\"") ESP_PD_DOMAIN_RC_FAST
#endif
#ifndef WAKEUP_MODE_2_INT_TYPE
#define WAKEUP_MODE_2_INT_TYPE(mode) ((gpio_int_type_t)((0x32154 >> ((mode) * 4)) & 0xF))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_EXT1_WAKEUP_ALL_LOW = 0,
    ESP_EXT1_WAKEUP_ANY_HIGH = 1,
} esp_sleep_ext1_wakeup_mode_t;
typedef enum {
    ESP_GPIO_WAKEUP_GPIO_LOW = 0,
    ESP_GPIO_WAKEUP_GPIO_HIGH = 1,
} esp_sleep_gpio_wake_up_mode_t;
typedef enum {
    ESP_PD_DOMAIN_RTC_PERIPH = 0,
    ESP_PD_DOMAIN_RTC_SLOW_MEM = 1,
    ESP_PD_DOMAIN_RTC_FAST_MEM = 2,
    ESP_PD_DOMAIN_XTAL = 3,
    ESP_PD_DOMAIN_RC_FAST = 4,
    ESP_PD_DOMAIN_VDDSDIO = 5,
    ESP_PD_DOMAIN_MODEM = 6,
    ESP_PD_DOMAIN_MAX = 7,
} esp_sleep_pd_domain_t;
typedef enum {
    ESP_PD_OPTION_OFF = 0,
    ESP_PD_OPTION_ON = 1,
    ESP_PD_OPTION_AUTO = 2,
} esp_sleep_pd_option_t;
typedef enum {
    ESP_SLEEP_WAKEUP_UNDEFINED = 0,
    ESP_SLEEP_WAKEUP_ALL = 1,
    ESP_SLEEP_WAKEUP_EXT0 = 2,
    ESP_SLEEP_WAKEUP_EXT1 = 3,
    ESP_SLEEP_WAKEUP_TIMER = 4,
    ESP_SLEEP_WAKEUP_TOUCHPAD = 5,
    ESP_SLEEP_WAKEUP_ULP = 6,
    ESP_SLEEP_WAKEUP_GPIO = 7,
    ESP_SLEEP_WAKEUP_UART0 = 8,
    ESP_SLEEP_WAKEUP_UART = 8,
    ESP_SLEEP_WAKEUP_UART1 = 9,
    ESP_SLEEP_WAKEUP_WIFI = 10,
    ESP_SLEEP_WAKEUP_COCPU = 11,
    ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG = 12,
    ESP_SLEEP_WAKEUP_BT = 13,
    ESP_SLEEP_WAKEUP_VAD = 14,
    ESP_SLEEP_WAKEUP_VBAT_UNDER_VOLT = 15,
    ESP_SLEEP_WAKEUP_USB = 16,
} esp_sleep_source_t;
typedef enum {
    ESP_SLEEP_MODE_LIGHT_SLEEP = 0,
    ESP_SLEEP_MODE_DEEP_SLEEP = 1,
} esp_sleep_mode_t;
typedef enum {
    ESP_SLEEP_AUTO_FLUSH_SUSPEND_UART = 0,
    ESP_SLEEP_ALWAYS_FLUSH_UART = 1,
    ESP_SLEEP_ALWAYS_SUSPEND_UART = 2,
    ESP_SLEEP_ALWAYS_DISCARD_UART = 3,
    ESP_SLEEP_NO_HANDLING = 4,
} esp_sleep_uart_handling_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*esp_deep_sleep_cb_t)(void);
typedef esp_sleep_source_t esp_sleep_wakeup_cause_t;
typedef void (*esp_deep_sleep_wake_stub_fn_t)(void);



#if defined(__WINK_SIM__)
void esp_deep_sleep(uint64_t time_in_us) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep out of Core 8 scope.");
#else
void esp_deep_sleep(uint64_t time_in_us);
#endif

#if defined(__WINK_SIM__)
void esp_deep_sleep_deregister_hook(esp_deep_sleep_cb_t old_dslp_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_deregister_hook out of Core 8 scope.");
#else
void esp_deep_sleep_deregister_hook(esp_deep_sleep_cb_t old_dslp_cb);
#endif

#if defined(__WINK_SIM__)
void esp_deep_sleep_disable_rom_logging(void) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_disable_rom_logging out of Core 8 scope.");
#else
void esp_deep_sleep_disable_rom_logging(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_deep_sleep_register_hook(esp_deep_sleep_cb_t new_dslp_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_register_hook out of Core 8 scope.");
#else
esp_err_t esp_deep_sleep_register_hook(esp_deep_sleep_cb_t new_dslp_cb);
#endif

#if defined(__WINK_SIM__)
void esp_deep_sleep_start(void) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_start out of Core 8 scope.");
#else
void esp_deep_sleep_start(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_deep_sleep_try(uint64_t time_in_us) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_try out of Core 8 scope.");
#else
esp_err_t esp_deep_sleep_try(uint64_t time_in_us);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_deep_sleep_try_to_start(void) WINK_SLA_ERROR("Wink SLA Violation: esp_deep_sleep_try_to_start out of Core 8 scope.");
#else
esp_err_t esp_deep_sleep_try_to_start(void);
#endif

#if defined(__WINK_SIM__)
void esp_default_wake_deep_sleep(void) WINK_SLA_ERROR("Wink SLA Violation: esp_default_wake_deep_sleep out of Core 8 scope.");
#else
void esp_default_wake_deep_sleep(void);
#endif

#if defined(__WINK_SIM__)
esp_deep_sleep_wake_stub_fn_t esp_get_deep_sleep_wake_stub(void) WINK_SLA_ERROR("Wink SLA Violation: esp_get_deep_sleep_wake_stub out of Core 8 scope.");
#else
esp_deep_sleep_wake_stub_fn_t esp_get_deep_sleep_wake_stub(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_light_sleep_start(void) WINK_SLA_ERROR("Wink SLA Violation: esp_light_sleep_start out of Core 8 scope.");
#else
esp_err_t esp_light_sleep_start(void);
#endif

#if defined(__WINK_SIM__)
void esp_set_deep_sleep_wake_stub(esp_deep_sleep_wake_stub_fn_t new_stub) WINK_SLA_ERROR("Wink SLA Violation: esp_set_deep_sleep_wake_stub out of Core 8 scope.");
#else
void esp_set_deep_sleep_wake_stub(esp_deep_sleep_wake_stub_fn_t new_stub);
#endif

#if defined(__WINK_SIM__)
void esp_set_deep_sleep_wake_stub_default_entry(void) WINK_SLA_ERROR("Wink SLA Violation: esp_set_deep_sleep_wake_stub_default_entry out of Core 8 scope.");
#else
void esp_set_deep_sleep_wake_stub_default_entry(void);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_config_gpio_isolate(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_config_gpio_isolate out of Core 8 scope.");
#else
void esp_sleep_config_gpio_isolate(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_bt_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_bt_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_bt_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_ext1_wakeup_io(uint64_t io_mask) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_ext1_wakeup_io out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_ext1_wakeup_io(uint64_t io_mask);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_usb_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_usb_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_usb_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_wakeup_source out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_wifi_beacon_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_wifi_beacon_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_wifi_beacon_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_disable_wifi_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_disable_wifi_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_disable_wifi_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_bt_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_bt_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_bt_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_ext0_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_ext1_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_ext1_wakeup_io out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_enable_gpio_switch(bool enable) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_gpio_switch out of Core 8 scope.");
#else
void esp_sleep_enable_gpio_switch(bool enable);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_gpio_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_gpio_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_gpio_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(uint64_t gpio_pin_mask, esp_sleep_gpio_wake_up_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(uint64_t gpio_pin_mask, esp_sleep_gpio_wake_up_mode_t mode);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_enable_lowpower_analog_mode(bool enable) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_lowpower_analog_mode out of Core 8 scope.");
#else
void esp_sleep_enable_lowpower_analog_mode(bool enable);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_timer_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_touchpad_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_touchpad_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_touchpad_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_uart_wakeup(int uart_num) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_uart_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_uart_wakeup(int uart_num);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_ulp_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_ulp_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_ulp_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_usb_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_usb_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_usb_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_wifi_beacon_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_wifi_beacon_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_wifi_beacon_wakeup(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_enable_wifi_wakeup(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_enable_wifi_wakeup out of Core 8 scope.");
#else
esp_err_t esp_sleep_enable_wifi_wakeup(void);
#endif

#if defined(__WINK_SIM__)
uint64_t esp_sleep_get_ext1_wakeup_status(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_get_ext1_wakeup_status out of Core 8 scope.");
#else
uint64_t esp_sleep_get_ext1_wakeup_status(void);
#endif

#if defined(__WINK_SIM__)
uint64_t esp_sleep_get_gpio_wakeup_status(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_get_gpio_wakeup_status out of Core 8 scope.");
#else
uint64_t esp_sleep_get_gpio_wakeup_status(void);
#endif

#if defined(__WINK_SIM__)
int esp_sleep_get_touchpad_wakeup_status(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_get_touchpad_wakeup_status out of Core 8 scope.");
#else
int esp_sleep_get_touchpad_wakeup_status(void);
#endif

#if defined(__WINK_SIM__)
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_get_wakeup_cause out of Core 8 scope.");
#else
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_sleep_get_wakeup_causes(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_get_wakeup_causes out of Core 8 scope.");
#else
uint32_t esp_sleep_get_wakeup_causes(void);
#endif

#if defined(__WINK_SIM__)
bool esp_sleep_is_valid_wakeup_gpio(gpio_num_t gpio_num) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_is_valid_wakeup_gpio out of Core 8 scope.");
#else
bool esp_sleep_is_valid_wakeup_gpio(gpio_num_t gpio_num);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t domain,
                              esp_sleep_pd_option_t option) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_pd_config out of Core 8 scope.");
#else
esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t domain,
                              esp_sleep_pd_option_t option);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_set_console_uart_handling_mode(esp_sleep_uart_handling_mode_t handling_mode) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_set_console_uart_handling_mode out of Core 8 scope.");
#else
esp_err_t esp_sleep_set_console_uart_handling_mode(esp_sleep_uart_handling_mode_t handling_mode);
#endif

#if defined(__WINK_SIM__)
gpio_num_t esp_sleep_wakeup_io_bit2num(uint32_t bit) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_wakeup_io_bit2num out of Core 8 scope.");
#else
gpio_num_t esp_sleep_wakeup_io_bit2num(uint32_t bit);
#endif

#if defined(__WINK_SIM__)
void esp_wake_deep_sleep(void) WINK_SLA_ERROR("Wink SLA Violation: esp_wake_deep_sleep out of Core 8 scope.");
#else
void esp_wake_deep_sleep(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_SLEEP_H__ */
#endif /* WINK_H_GUARD_ESP_SLEEP_H */
