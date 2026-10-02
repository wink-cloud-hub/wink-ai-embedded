/* SPDX-License-Identifier: LGPL-3.0-only */
#include "driver/gpio.h"
#include "hal/pal_gpio.h"
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#include "freertos_sync.h"  /* esp_sim_spin_wait_account, Phase 2 ISSUE-06 */
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
extern void esp_idf_ensure_framework_ready(void); /* Phase 2 ISSUE-13 */

typedef struct {
    gpio_isr_t       handler;
    void            *arg;
    gpio_int_type_t  intr_type;
    bool             enabled;
} esp_sim_gpio_isr_slot_t;

static bool s_isr_service_installed = false;
static esp_sim_gpio_isr_slot_t s_gpio_isr_slots[SOC_GPIO_PIN_COUNT];

/* Output read-back cache (ADR-0012 降级条目 3, see docs/02-api-coverage-matrix.md):
 * Host/Wasm PAL `pal_gpio_read` on an output-configured pin reports the mode
 * idle level, not the last driven level, so the facade must remember what it
 * drove. Cleared on gpio_config-to-input / gpio_set_direction-to-input /
 * gpio_reset_pin. Single-threaded cooperative sim only; pins < 64. */
static uint64_t s_is_output = 0ULL;
static uint64_t s_output_levels = 0ULL;
static uint64_t s_sim_input_override = 0ULL;
static uint64_t s_sim_input_levels = 0ULL;

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
    esp_idf_ensure_framework_ready(); /* Phase 2 Task 4.3: C++ static constructor safe */
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
                (void)pal_gpio_write((wink_pin_t)pin, false);
            } else {
                s_is_output &= ~(1ULL << pin);
                s_output_levels &= ~(1ULL << pin);
            }
            if (pGPIOConfig->intr_type != GPIO_INTR_DISABLE) {
                s_gpio_isr_slots[pin].intr_type = pGPIOConfig->intr_type;
                s_gpio_isr_slots[pin].enabled = true;
            }
        }
    }

    return ESP_OK;
}

esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode) {
    esp_idf_ensure_framework_ready(); /* Phase 2 Task 4.3: C++ static constructor safe */
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

    uint32_t old_level = (s_output_levels & (1ULL << gpio_num)) ? 1 : 0;
    wink_status_t status = pal_gpio_write((wink_pin_t)gpio_num, level ? true : false);
    if (status >= 0) {
        if (level) {
            s_output_levels |= (1ULL << gpio_num);
        } else {
            s_output_levels &= ~(1ULL << gpio_num);
        }
    }
    esp_err_t err = esp_err_from_wink(status);
    if (err == ESP_OK && old_level != (level ? 1u : 0u)) {
        /* Phase 3: In-fiber loopback injection if pin interrupt registered and enabled */
        esp_sim_gpio_inject_edge(gpio_num, old_level, level ? 1u : 0u);
    }

    /* AT93C46D SPI EEPROM CS-to-MISO ready signal emulation:
     * When CS (GPIO 13) is driven HIGH, the EEPROM indicates ready by driving MISO (GPIO 18) HIGH.
     * When CS is driven LOW, MISO returns to LOW. */
    if (gpio_num == GPIO_NUM_13) {
        esp_sim_gpio_set_input_level(GPIO_NUM_18, level ? 1 : 0);
        if (level) {
            esp_sim_gpio_inject_edge(GPIO_NUM_18, 0, 1);
        }
    } else if (gpio_num == GPIO_NUM_18) {
        /* Generic GPIO Loopback: OUTPUT_0 (18) -> INPUT_0 (4) */
        esp_sim_gpio_set_input_level(GPIO_NUM_4, level ? 1 : 0);
        esp_sim_gpio_inject_edge(GPIO_NUM_4, old_level, level ? 1u : 0u);
    } else if (gpio_num == GPIO_NUM_19) {
        /* Generic GPIO Loopback: OUTPUT_1 (19) -> INPUT_1 (5) */
        esp_sim_gpio_set_input_level(GPIO_NUM_5, level ? 1 : 0);
        esp_sim_gpio_inject_edge(GPIO_NUM_5, old_level, level ? 1u : 0u);
    }
    return err;
}

int gpio_get_level(gpio_num_t gpio_num) {
    /* Phase 2 Task 3.2: busy-wait self-healing spin counter (ISSUE-06) */
    esp_sim_spin_wait_account();

    if (!GPIO_IS_VALID_GPIO(gpio_num)) {
        ESP_LOGE("GPIO", "gpio_get_level: invalid pin %d", (int)gpio_num);
        return 0;
    }

    /* If pin is tracked as output, read back the written level */
    if (s_is_output & (1ULL << gpio_num)) {
        return (s_output_levels & (1ULL << gpio_num)) ? 1 : 0;
    }

    /* If pin has simulated external input level override */
    if (s_sim_input_override & (1ULL << gpio_num)) {
        return (s_sim_input_levels & (1ULL << gpio_num)) ? 1 : 0;
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
    if ((int)gpio_num < SOC_GPIO_PIN_COUNT) {
        memset(&s_gpio_isr_slots[gpio_num], 0, sizeof(esp_sim_gpio_isr_slot_t));
    }

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
    if (!GPIO_IS_VALID_GPIO(gpio_num) || (int)gpio_num >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio_isr_slots[gpio_num].intr_type = intr_type;
    return ESP_OK;
}

esp_err_t gpio_intr_enable(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num) || (int)gpio_num >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio_isr_slots[gpio_num].enabled = true;
    return ESP_OK;
}

esp_err_t gpio_intr_disable(gpio_num_t gpio_num) {
    if (!GPIO_IS_VALID_GPIO(gpio_num) || (int)gpio_num >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio_isr_slots[gpio_num].enabled = false;
    return ESP_OK;
}

esp_err_t gpio_install_isr_service(int intr_alloc_flags) {
    (void)intr_alloc_flags;
    if (s_isr_service_installed) {
        return ESP_ERR_INVALID_STATE;
    }
    s_isr_service_installed = true;
    return ESP_OK;
}

esp_err_t gpio_uninstall_isr_service(void) {
    if (!s_isr_service_installed) {
        return ESP_ERR_INVALID_STATE;
    }
    s_isr_service_installed = false;
    memset(s_gpio_isr_slots, 0, sizeof(s_gpio_isr_slots));
    return ESP_OK;
}

esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args) {
    if (!s_isr_service_installed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!GPIO_IS_VALID_GPIO(gpio_num) || (int)gpio_num >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (isr_handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio_isr_slots[gpio_num].handler = isr_handler;
    s_gpio_isr_slots[gpio_num].arg = args;
    s_gpio_isr_slots[gpio_num].enabled = true;
    return ESP_OK;
}

esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num) {
    if (!s_isr_service_installed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!GPIO_IS_VALID_GPIO(gpio_num) || (int)gpio_num >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gpio_isr_slots[gpio_num].handler = NULL;
    s_gpio_isr_slots[gpio_num].arg = NULL;
    s_gpio_isr_slots[gpio_num].enabled = false;
    return ESP_OK;
}

esp_err_t esp_sim_gpio_inject_edge(gpio_num_t pin, uint32_t from_level, uint32_t to_level) {
    if (!GPIO_IS_VALID_GPIO(pin) || (int)pin >= SOC_GPIO_PIN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_sim_gpio_isr_slot_t *slot = &s_gpio_isr_slots[pin];
    if (!slot->enabled || slot->handler == NULL) {
        return ESP_OK;
    }

    bool match = false;
    switch (slot->intr_type) {
        case GPIO_INTR_POSEDGE:
            match = (from_level == 0 && to_level != 0);
            break;
        case GPIO_INTR_NEGEDGE:
            match = (from_level != 0 && to_level == 0);
            break;
        case GPIO_INTR_ANYEDGE:
            match = ((from_level != 0) != (to_level != 0));
            break;
        case GPIO_INTR_LOW_LEVEL:
            match = (to_level == 0);
            break;
        case GPIO_INTR_HIGH_LEVEL:
            match = (to_level != 0);
            break;
        default:
            match = false;
            break;
    }

    if (!match) {
        return ESP_OK;
    }

    /* Update shadow level so readback during ISR sees the new level */
    if (to_level) {
        s_output_levels |= (1ULL << pin);
    } else {
        s_output_levels &= ~(1ULL << pin);
    }

    /* ISR Execution Sandbox */
    pal_os_set_sim_isr_context(true);
    esp_freertos_clear_isr_yield_requested();

    slot->handler(slot->arg);

    pal_os_set_sim_isr_context(false);

    /* Context-Aware Preemption Dispatcher */
    if (esp_freertos_is_isr_yield_requested()) {
        esp_freertos_clear_isr_yield_requested();
        if (sim_scheduler_current_ctx() != NULL) {
            /* Scenario B: inside task fiber -> immediate cooperative yield to higher prio task */
            sim_scheduler_yield_context();
        } else {
            /* Scenario A: from host main thread / test harness -> do NOT yield_context()!
             * The awakened task is already marked READY and will be picked up by the scheduler loop. */
        }
    }

    return ESP_OK;
}

void esp_sim_gpio_set_input_level(gpio_num_t pin, uint32_t level) {
    if ((uint32_t)pin < SOC_GPIO_PIN_COUNT) {
        s_sim_input_override |= (1ULL << pin);
        if (level) {
            s_sim_input_levels |= (1ULL << pin);
        } else {
            s_sim_input_levels &= ~(1ULL << pin);
        }
    }
}

void esp_gpio_reset(void) {
    s_is_output = 0ULL;
    s_output_levels = 0ULL;
    s_sim_input_override = 0ULL;
    s_sim_input_levels = 0ULL;
    s_isr_service_installed = false;
    memset(s_gpio_isr_slots, 0, sizeof(s_gpio_isr_slots));
}
