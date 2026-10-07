/* SPDX-License-Identifier: LGPL-3.0-only */
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "rtc_io";

esp_err_t rtc_gpio_init(gpio_num_t gpio_num) {
    ESP_LOGD(TAG, "rtc_gpio_init: pin=%d", (int)gpio_num);
    return gpio_reset_pin(gpio_num);
}

esp_err_t rtc_gpio_deinit(gpio_num_t gpio_num) {
    (void)gpio_num;
    return ESP_OK;
}

esp_err_t rtc_gpio_isolate(gpio_num_t gpio_num) {
    ESP_LOGD(TAG, "rtc_gpio_isolate: pin=%d", (int)gpio_num);
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(gpio_num),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cfg);
}

esp_err_t rtc_gpio_pullup_en(gpio_num_t gpio_num) {
    return gpio_pullup_en(gpio_num);
}

esp_err_t rtc_gpio_pullup_dis(gpio_num_t gpio_num) {
    return gpio_pullup_dis(gpio_num);
}

esp_err_t rtc_gpio_pulldown_en(gpio_num_t gpio_num) {
    return gpio_pulldown_en(gpio_num);
}

esp_err_t rtc_gpio_pulldown_dis(gpio_num_t gpio_num) {
    return gpio_pulldown_dis(gpio_num);
}

esp_err_t rtc_gpio_set_direction(gpio_num_t gpio_num, rtc_gpio_mode_t mode) {
    return gpio_set_direction(gpio_num, (gpio_mode_t)mode);
}

esp_err_t rtc_gpio_set_direction_in_sleep(gpio_num_t gpio_num, rtc_gpio_mode_t mode) {
    (void)gpio_num;
    (void)mode;
    return ESP_OK;
}

esp_err_t rtc_gpio_set_level(gpio_num_t gpio_num, uint32_t level) {
    return gpio_set_level(gpio_num, level);
}

uint32_t rtc_gpio_get_level(gpio_num_t gpio_num) {
    return (uint32_t)gpio_get_level(gpio_num);
}

bool rtc_gpio_is_valid_gpio(gpio_num_t gpio_num) {
    return GPIO_IS_VALID_GPIO(gpio_num);
}

esp_err_t rtc_gpio_hold_en(gpio_num_t gpio_num) {
    (void)gpio_num;
    return ESP_OK;
}

esp_err_t rtc_gpio_hold_dis(gpio_num_t gpio_num) {
    (void)gpio_num;
    return ESP_OK;
}

esp_err_t rtc_gpio_force_hold_en_all(void) {
    return ESP_OK;
}

esp_err_t rtc_gpio_force_hold_dis_all(void) {
    return ESP_OK;
}

esp_err_t rtc_gpio_wakeup_enable(gpio_num_t gpio_num, gpio_int_type_t intr_type) {
    (void)gpio_num;
    (void)intr_type;
    return ESP_OK;
}

esp_err_t rtc_gpio_wakeup_disable(gpio_num_t gpio_num) {
    (void)gpio_num;
    return ESP_OK;
}

esp_err_t rtc_gpio_set_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t strength) {
    return gpio_set_drive_capability(gpio_num, strength);
}

esp_err_t rtc_gpio_get_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t *strength) {
    return gpio_get_drive_capability(gpio_num, strength);
}

esp_err_t rtc_gpio_iomux_func_sel(gpio_num_t gpio_num, int func) {
    (void)gpio_num;
    (void)func;
    return ESP_OK;
}

esp_err_t rtc_gpio_iomux_input(gpio_num_t gpio_num, int func, uint32_t signal_idx) {
    (void)gpio_num;
    (void)func;
    (void)signal_idx;
    return ESP_OK;
}

esp_err_t rtc_gpio_iomux_output(gpio_num_t gpio_num, int func) {
    (void)gpio_num;
    (void)func;
    return ESP_OK;
}

int rtc_io_number_get(gpio_num_t gpio_num) {
    if (GPIO_IS_VALID_GPIO(gpio_num)) {
        return (int)gpio_num;
    }
    return -1;
}
