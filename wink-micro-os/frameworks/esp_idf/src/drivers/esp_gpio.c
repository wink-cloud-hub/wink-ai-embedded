/* SPDX-License-Identifier: LGPL-3.0-only */
#include "driver/gpio.h"
#include "hal/pal_gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

/* Output read-back cache (ADR-0012 降级条目 3, see docs/02-api-coverage-matrix.md):
 * Host/Wasm PAL `pal_gpio_read` on an output-configured pin reports the mode
 * idle level, not the last driven level, so the facade must remember what it
 * drove. Cleared on gpio_config-to-input / gpio_set_direction-to-input /
 * gpio_reset_pin. Single-threaded cooperative sim only; pins < 64. */
static uint64_t s_is_output = 0ULL;
static uint64_t s_output_levels = 0ULL;

static pal_gpio_mode_t convert_gpio_mode(gpio_mode_t mode, gpio_pullup_t pull_up, gpio_pulldown_t pull_down) {
    if (mode == GPIO_MODE_INPUT) {
        if (pull_up == GPIO_PULLUP_ENABLE) {
            return PAL_GPIO_INPUT_PULLUP;
        }
        if (pull_down == GPIO_PULLDOWN_ENABLE) {
            return PAL_GPIO_INPUT_PULLDOWN;
        }
        return PAL_GPIO_INPUT;
    }
    if (mode == GPIO_MODE_OUTPUT) {
        return PAL_GPIO_OUTPUT_PUSH_PULL;
    }
    if (mode == GPIO_MODE_OUTPUT_OD) {
        return PAL_GPIO_OUTPUT_OPEN_DRAIN;
    }
    if (mode == GPIO_MODE_INPUT_OUTPUT || mode == GPIO_MODE_INPUT_OUTPUT_OD) {
        return PAL_GPIO_INPUT_OUTPUT;
    }
    return PAL_GPIO_INPUT;
}

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig) {
    if (!pGPIOConfig || pGPIOConfig->pin_bit_mask == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    bool is_output = (pGPIOConfig->mode == GPIO_MODE_OUTPUT ||
                      pGPIOConfig->mode == GPIO_MODE_OUTPUT_OD ||
                      pGPIOConfig->mode == GPIO_MODE_INPUT_OUTPUT ||
                      pGPIOConfig->mode == GPIO_MODE_INPUT_OUTPUT_OD);

    /* Phase 1: Boundary & Mask Validation (reject any bit outside valid mask) */
    if ((pGPIOConfig->pin_bit_mask & ~SOC_GPIO_VALID_GPIO_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (is_output && (pGPIOConfig->pin_bit_mask & ~SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    pal_gpio_mode_t pal_mode = convert_gpio_mode(pGPIOConfig->mode,
                                                 pGPIOConfig->pull_up_en,
                                                 pGPIOConfig->pull_down_en);

    /* Phase 2: Lower to PAL GPIO init (0 claim, RAII in PAL) */
    for (int pin = 0; pin < SOC_GPIO_PIN_COUNT; pin++) {
        if (pGPIOConfig->pin_bit_mask & (1ULL << pin)) {
            wink_status_t status = pal_gpio_init((wink_pin_t)pin, pal_mode);
            if (status < 0) {
                return esp_err_from_wink(status);
            }
            if (is_output) {
                s_is_output |= (1ULL << pin);
            } else {
                s_is_output &= ~(1ULL << pin);
                s_output_levels &= ~(1ULL << pin);
            }
        }
    }

    return ESP_OK;
}

esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }

    bool is_output = (mode == GPIO_MODE_OUTPUT ||
                      mode == GPIO_MODE_OUTPUT_OD ||
                      mode == GPIO_MODE_INPUT_OUTPUT ||
                      mode == GPIO_MODE_INPUT_OUTPUT_OD);

    if (is_output && !GPIO_IS_VALID_OUTPUT_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* NOTE: lowered via pal_gpio_init (not pal_gpio_set_direction) on purpose:
     * the host PAL set_direction is a no-op that records no mode, while init is
     * idempotent and records the mode the read path depends on. Official
     * gpio_set_direction carries no pull args, so DISABLE pulls are correct. */
    pal_gpio_mode_t pal_mode = convert_gpio_mode(mode, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE);
    wink_status_t status = pal_gpio_init((wink_pin_t)gpio_num, pal_mode);
    if (status < 0) {
        return esp_err_from_wink(status);
    }

    if (is_output) {
        s_is_output |= (1ULL << gpio_num);
    } else {
        s_is_output &= ~(1ULL << gpio_num);
        s_output_levels &= ~(1ULL << gpio_num);
    }

    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }

    wink_status_t status = pal_gpio_write((wink_pin_t)gpio_num, level ? true : false);
    if (status >= 0) {
        if (level) {
            s_output_levels |= (1ULL << gpio_num);
        } else {
            s_output_levels &= ~(1ULL << gpio_num);
        }
    }
    return esp_err_from_wink(status);
}

int gpio_get_level(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        ESP_LOGE("GPIO", "gpio_get_level: invalid pin %d", (int)gpio_num);
        return 0;
    }

    /* If pin is tracked as output, read back the written level */
    if (s_is_output & (1ULL << gpio_num)) {
        return (s_output_levels & (1ULL << gpio_num)) ? 1 : 0;
    }

    bool val = false;
    wink_status_t status = pal_gpio_read((wink_pin_t)gpio_num, &val);
    if (status < 0) {
        ESP_LOGE("GPIO", "gpio_get_level: read failed pin %d rc %d", (int)gpio_num, (int)status);
        return 0;
    }

    return val ? 1 : 0;
}

esp_err_t gpio_reset_pin(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }

    s_is_output &= ~(1ULL << gpio_num);
    s_output_levels &= ~(1ULL << gpio_num);

    wink_status_t status = pal_gpio_deinit((wink_pin_t)gpio_num);
    return esp_err_from_wink(status);
}

/* Pull / interrupt / ISR family: NOT supported in M0 simulation (ADR-0012
 * 降级条目 4). Fail-loud with ESP_ERR_NOT_SUPPORTED, never silent ESP_OK. */

esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull) {
    (void)pull;
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_set_pull_mode: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_pullup_en(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_pullup_en: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_pullup_dis(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_pullup_dis: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_pulldown_en(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_pulldown_en: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_pulldown_dis(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_pulldown_dis: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type) {
    (void)intr_type;
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_set_intr_type: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_intr_enable(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_intr_enable: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_intr_disable(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGE("GPIO", "gpio_intr_disable: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_install_isr_service(int intr_alloc_flags) {
    (void)intr_alloc_flags;
    ESP_LOGE("GPIO", "gpio_install_isr_service: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

void gpio_uninstall_isr_service(void) {
    /* void C-ABI: cannot signal failure; warn loudly (降级条目 4). */
    ESP_LOGW("GPIO", "gpio_uninstall_isr_service: no-op, ISR not supported in simulation (M0)");
}

esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args) {
    (void)gpio_num;
    (void)isr_handler;
    (void)args;
    ESP_LOGE("GPIO", "gpio_isr_handler_add: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num) {
    (void)gpio_num;
    ESP_LOGE("GPIO", "gpio_isr_handler_remove: not supported in simulation (M0)");
    return ESP_ERR_NOT_SUPPORTED;
}
