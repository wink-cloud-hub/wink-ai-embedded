/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

static const char *TAG = "esp_sleep";

static esp_sleep_source_t s_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
static uint64_t s_timer_wakeup_us = 0;
static uint64_t s_gpio_wakeup_mask = 0;
static esp_sleep_gpio_wake_up_mode_t s_gpio_wakeup_mode = ESP_GPIO_WAKEUP_GPIO_LOW;
static gpio_num_t s_ext0_wakeup_pin = GPIO_NUM_NC;
static int s_ext0_wakeup_level = 0;
static uint64_t s_ext1_wakeup_mask = 0;
static esp_sleep_ext1_wakeup_mode_t s_ext1_wakeup_mode = ESP_EXT1_WAKEUP_ALL_LOW;
static bool s_is_in_deep_sleep = false;

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
    s_timer_wakeup_us = time_in_us;
    ESP_LOGD(TAG, "Timer wakeup configured: %" PRIu64 " us", time_in_us);
    return ESP_OK;
}

esp_err_t esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(uint64_t gpio_pin_mask, esp_sleep_gpio_wake_up_mode_t mode) {
    s_gpio_wakeup_mask = gpio_pin_mask;
    s_gpio_wakeup_mode = mode;
    ESP_LOGD(TAG, "GPIO wakeup configured: mask=0x%" PRIx64 ", mode=%d", gpio_pin_mask, (int)mode);
    return ESP_OK;
}

esp_err_t esp_sleep_enable_gpio_wakeup(void) {
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level) {
    s_ext0_wakeup_pin = gpio_num;
    s_ext0_wakeup_level = level;
    ESP_LOGD(TAG, "EXT0 wakeup configured: pin=%d, level=%d", (int)gpio_num, level);
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode) {
    s_ext1_wakeup_mask = io_mask;
    s_ext1_wakeup_mode = level_mode;
    ESP_LOGD(TAG, "EXT1 wakeup configured: mask=0x%" PRIx64 ", mode=%d", io_mask, (int)level_mode);
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode) {
    return esp_sleep_enable_ext1_wakeup(io_mask, level_mode);
}

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) {
    return s_wakeup_cause;
}

uint32_t esp_sleep_get_wakeup_causes(void) {
    return (1U << (uint32_t)s_wakeup_cause);
}

uint64_t esp_sleep_get_ext1_wakeup_status(void) {
    return (s_wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) ? s_ext1_wakeup_mask : 0;
}

uint64_t esp_sleep_get_gpio_wakeup_status(void) {
    return (s_wakeup_cause == ESP_SLEEP_WAKEUP_GPIO) ? s_gpio_wakeup_mask : 0;
}

bool esp_sleep_is_valid_wakeup_gpio(gpio_num_t gpio_num) {
    return GPIO_IS_VALID_GPIO(gpio_num);
}

gpio_num_t esp_sleep_wakeup_io_bit2num(uint32_t bit) {
    if (bit < 40) {
        return (gpio_num_t)bit;
    }
    return GPIO_NUM_NC;
}

esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t domain, esp_sleep_pd_option_t option) {
    (void)domain;
    (void)option;
    return ESP_OK;
}

void esp_deep_sleep(uint64_t time_in_us) {
    s_is_in_deep_sleep = true;
    ESP_LOGI(TAG, "Entering deep sleep (%" PRIu64 " us)", time_in_us);
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void esp_deep_sleep_start(void) {
    esp_deep_sleep(s_timer_wakeup_us > 0 ? s_timer_wakeup_us : UINT64_MAX);
}

esp_err_t esp_light_sleep_start(void) {
    ESP_LOGI(TAG, "Entering light sleep");
    if (s_timer_wakeup_us > 0) {
        TickType_t delay_ticks = (TickType_t)(s_timer_wakeup_us / (portTICK_PERIOD_MS * 1000ULL));
        if (delay_ticks == 0) delay_ticks = 1;
        vTaskDelay(delay_ticks);
    } else {
        vTaskDelay(1);
    }
    ESP_LOGI(TAG, "Woke up from light sleep");
    return ESP_OK;
}

void esp_sleep_sim_reset(void) {
    s_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    s_timer_wakeup_us = 0;
    s_gpio_wakeup_mask = 0;
    s_gpio_wakeup_mode = ESP_GPIO_WAKEUP_GPIO_LOW;
    s_ext0_wakeup_pin = GPIO_NUM_NC;
    s_ext0_wakeup_level = 0;
    s_ext1_wakeup_mask = 0;
    s_ext1_wakeup_mode = ESP_EXT1_WAKEUP_ALL_LOW;
    s_is_in_deep_sleep = false;
}

void esp_sleep_sim_set_wakeup_cause(esp_sleep_source_t cause) {
    s_wakeup_cause = cause;
}

bool esp_sleep_sim_is_in_deep_sleep(void) {
    return s_is_in_deep_sleep;
}

gpio_num_t esp_sleep_sim_get_ext0_pin(void) {
    return s_ext0_wakeup_pin;
}

int esp_sleep_sim_get_ext0_level(void) {
    return s_ext0_wakeup_level;
}

esp_sleep_gpio_wake_up_mode_t esp_sleep_sim_get_gpio_mode(void) {
    return s_gpio_wakeup_mode;
}

esp_sleep_ext1_wakeup_mode_t esp_sleep_sim_get_ext1_mode(void) {
    return s_ext1_wakeup_mode;
}
