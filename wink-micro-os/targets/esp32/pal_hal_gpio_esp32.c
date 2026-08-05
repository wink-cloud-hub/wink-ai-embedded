// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_gpio_esp32.c
 * @brief ESP32 target PAL HAL GPIO subsystem implementation.
 */
#include "pal_hal.h"
#include "pal_irq.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_atomic_esp32.h"
#include "pal_hal_internal_esp32.h"
#include "hal/pal_rmt.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/gpio_struct.h"
#include "esp_rom_gpio.h"
#include "esp_idf_version.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"

#ifndef SIG_GPIO_OUT_IDX
#define SIG_GPIO_OUT_IDX 256
#endif

_Static_assert((gpio_num_t)GPIO_NUM_NC == -1,
    "GPIO_NUM_NC must be -1 for wink_pin_t sign-compatibility");

static inline void gpio_clear_intr_status(gpio_num_t gpio_num) {
    if (gpio_num < 32) {
        GPIO.status_w1tc = (1UL << gpio_num);
    } else {
        GPIO.status1_w1tc.val = (1UL << (gpio_num - 32));
    }
}

static portMUX_TYPE s_gpio_table_mux = portMUX_INITIALIZER_UNLOCKED;

static pal_gpio_isr_t s_gpio_isr[GPIO_NUM_MAX] = {NULL};
static void *s_gpio_isr_arg[GPIO_NUM_MAX] = {NULL};

static bool           s_gpio_service_initialized = false;
static pal_irq_prio_t s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

static volatile uint32_t s_gpio_irq_in_flight[GPIO_NUM_MAX] = {0};

static pal_gpio_mode_t s_gpio_mode[GPIO_NUM_MAX];
static bool            s_gpio_mode_known[GPIO_NUM_MAX];

void pal_esp32_gpio_synchronize_all(uint64_t timeout_us)
{
    for (uint32_t i = 0; i < GPIO_NUM_MAX; i++) {
        uint64_t start = pal_os_get_us();
        while (Atomic_Load_u32(&s_gpio_irq_in_flight[i]) > 0) {
            if (pal_os_get_us() - start > timeout_us) {
                ESP_LOGE("pal_irq", "synchronize timeout on gpio=%lu",
                         (unsigned long)i);
                break;
            }
        }
    }
}

static void PAL_ISR gpio_isr_wrapper(void *arg)
{
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    if (pin >= GPIO_NUM_MAX) {
        return;
    }

    Atomic_Increment_u32(&s_gpio_irq_in_flight[pin]);

    gpio_intr_disable((gpio_num_t)pin);
    gpio_clear_intr_status((gpio_num_t)pin);

    pal_gpio_isr_t isr = NULL;
    void *isr_arg = NULL;
    bool need_reenable = false;

    portENTER_CRITICAL_ISR(&s_gpio_table_mux);
    isr = s_gpio_isr[pin];
    isr_arg = s_gpio_isr_arg[pin];
    need_reenable = (s_gpio_isr[pin] != NULL);
    portEXIT_CRITICAL_ISR(&s_gpio_table_mux);

    if (isr != NULL) {
        isr(isr_arg);
    }

    if (need_reenable) {
        portENTER_CRITICAL_ISR(&s_gpio_table_mux);
        need_reenable = (s_gpio_isr[pin] != NULL);
        portEXIT_CRITICAL_ISR(&s_gpio_table_mux);

        if (need_reenable) {
            gpio_intr_enable((gpio_num_t)pin);
        }
    }

    Atomic_Decrement_u32(&s_gpio_irq_in_flight[pin]);
}

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    switch (mode) {
        case PAL_GPIO_INPUT:
            cfg.mode = GPIO_MODE_INPUT;
            break;
        case PAL_GPIO_INPUT_PULLUP:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case PAL_GPIO_INPUT_PULLDOWN:
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        case PAL_GPIO_OUTPUT_PUSH_PULL:
            cfg.mode = GPIO_MODE_OUTPUT;
            break;
        case PAL_GPIO_OUTPUT_OPEN_DRAIN:
            cfg.mode = GPIO_MODE_OUTPUT_OD;
            break;
        case PAL_GPIO_INPUT_OUTPUT:
            cfg.mode = GPIO_MODE_INPUT_OUTPUT;
            break;
        default:
            return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    s_gpio_mode[pin] = mode;
    s_gpio_mode_known[pin] = true;
    return WINK_OK;
}

void pal_gpio_reset_pin(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO((gpio_num_t)pin)) {
        return;
    }
    gpio_reset_pin((gpio_num_t)pin);
    s_gpio_mode_known[pin] = false;

    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = NULL;
    s_gpio_isr_arg[pin] = NULL;
    portEXIT_CRITICAL(&s_gpio_table_mux);
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_gpio_mode_known[pin]) {
        return WINK_ERR_INVALID_STATE;
    }

    gpio_mode_t idf_mode;
    switch (mode) {
        case PAL_GPIO_INPUT:             idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_INPUT_PULLUP:      idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_INPUT_PULLDOWN:    idf_mode = GPIO_MODE_INPUT;    break;
        case PAL_GPIO_OUTPUT_PUSH_PULL:  idf_mode = GPIO_MODE_OUTPUT;   break;
        case PAL_GPIO_OUTPUT_OPEN_DRAIN: idf_mode = GPIO_MODE_OUTPUT_OD;break;
        case PAL_GPIO_INPUT_OUTPUT:      idf_mode = GPIO_MODE_INPUT_OUTPUT; break;
        default: return WINK_ERR_INVALID_ARG;
    }

    esp_err_t err = gpio_set_direction((gpio_num_t)pin, idf_mode);
    if (err == ESP_ERR_INVALID_ARG) { return WINK_ERR_INVALID_ARG; }
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    if (mode == PAL_GPIO_INPUT_PULLUP) {
        (void)gpio_pullup_en((gpio_num_t)pin);
        (void)gpio_pulldown_dis((gpio_num_t)pin);
    } else if (mode == PAL_GPIO_INPUT_PULLDOWN) {
        (void)gpio_pullup_dis((gpio_num_t)pin);
        (void)gpio_pulldown_en((gpio_num_t)pin);
    }

    s_gpio_mode[pin] = mode;
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    esp_err_t err = gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
    if (err == ESP_ERR_INVALID_ARG) { return WINK_ERR_INVALID_ARG; }
    if (err != ESP_OK) { return WINK_ERR_IO; }
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) { return WINK_ERR_INVALID_ARG; }
    *out_level = false;
    if (pin < 0 || pin >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin)) {
        return WINK_ERR_INVALID_ARG;
    }
    int val = gpio_get_level((gpio_num_t)pin);
    if (val < 0) { return WINK_ERR_IO; }
    *out_level = (val != 0);
    return WINK_OK;
}

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }
    if (callback == NULL) { return WINK_ERR_INVALID_ARG; }
    if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) { return WINK_ERR_INVALID_ARG; }

    portENTER_CRITICAL(&s_gpio_table_mux);
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            portEXIT_CRITICAL(&s_gpio_table_mux);
            return WINK_ERR_INVALID_ARG;
        }
    }
    portEXIT_CRITICAL(&s_gpio_table_mux);

    gpio_int_type_t esp_intr_type;
    switch (intr_type) {
        case PAL_GPIO_INTR_RISING_EDGE:
            esp_intr_type = GPIO_INTR_POSEDGE;
            break;
        case PAL_GPIO_INTR_FALLING_EDGE:
            esp_intr_type = GPIO_INTR_NEGEDGE;
            break;
        case PAL_GPIO_INTR_ANY_EDGE:
            esp_intr_type = GPIO_INTR_ANYEDGE;
            break;
        case PAL_GPIO_INTR_LOW_LEVEL:
            esp_intr_type = GPIO_INTR_LOW_LEVEL;
            break;
        case PAL_GPIO_INTR_HIGH_LEVEL:
            esp_intr_type = GPIO_INTR_HIGH_LEVEL;
            break;
        default:
            return WINK_ERR_INVALID_ARG;
    }

    static const int s_gpio_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
        [PAL_IRQ_PRIO_LOW]     = ESP_INTR_FLAG_LEVEL1,
        [PAL_IRQ_PRIO_NORMAL]  = ESP_INTR_FLAG_LEVEL2,
        [PAL_IRQ_PRIO_HIGH]    = ESP_INTR_FLAG_LEVEL3,
    };

    if (!s_gpio_service_initialized) {
        int intr_flags = s_gpio_prio_flag_map[prio] | ESP_INTR_FLAG_IRAM;
        esp_err_t err = gpio_install_isr_service(intr_flags);
        if (err == ESP_OK) {
            portENTER_CRITICAL(&s_gpio_table_mux);
            s_gpio_service_prio        = prio;
            s_gpio_service_initialized = true;
            portEXIT_CRITICAL(&s_gpio_table_mux);
        } else if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGI("pal_hal", "GPIO ISR service already installed externally; "
                                "locking pal tracker to prio=%d", (int)prio);
            portENTER_CRITICAL(&s_gpio_table_mux);
            s_gpio_service_prio        = prio;
            s_gpio_service_initialized = true;
            portEXIT_CRITICAL(&s_gpio_table_mux);
        } else {
            return WINK_ERR_HARDWARE;
        }
    }

    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    portEXIT_CRITICAL(&s_gpio_table_mux);

    esp_err_t err = gpio_isr_handler_add((gpio_num_t)pin,
                                          gpio_isr_wrapper,
                                          (void *)(uintptr_t)pin);
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_gpio_table_mux);
        s_gpio_isr[pin] = NULL;
        portEXIT_CRITICAL(&s_gpio_table_mux);
        return WINK_ERR_HARDWARE;
    }

    if (s_gpio_mode_known[pin]) {
        if (s_gpio_mode[pin] == PAL_GPIO_OUTPUT_PUSH_PULL) {
            (void)gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
            s_gpio_mode[pin] = PAL_GPIO_INPUT_OUTPUT;
        } else if (s_gpio_mode[pin] == PAL_GPIO_OUTPUT_OPEN_DRAIN) {
            (void)gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT_OD);
            s_gpio_mode[pin] = PAL_GPIO_INPUT_OUTPUT;
        }
    }

    err = gpio_set_intr_type((gpio_num_t)pin, esp_intr_type);
    if (err != ESP_OK) {
        (void)gpio_isr_handler_remove((gpio_num_t)pin);
        portENTER_CRITICAL(&s_gpio_table_mux);
        s_gpio_isr[pin] = NULL;
        portEXIT_CRITICAL(&s_gpio_table_mux);
        return WINK_ERR_HARDWARE;
    }
    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    esp_err_t err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

    (void)gpio_isr_handler_remove((gpio_num_t)pin);

    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_isr[pin] = NULL;
    s_gpio_isr_arg[pin] = NULL;
    portEXIT_CRITICAL(&s_gpio_table_mux);
    return WINK_OK;
}

#define PAL_GPIO_SYNC_TIMEOUT_US 100000ULL

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return WINK_ERR_INVALID_ARG; }

    uint64_t start = pal_os_get_us();
    while (Atomic_Load_u32(&s_gpio_irq_in_flight[pin]) > 0) {
        if (pal_os_get_us() - start > PAL_GPIO_SYNC_TIMEOUT_US) {
            ESP_LOGE("pal_hal", "synchronize_interrupt timeout on gpio=%d",
                     (int)pin);
            break;
        }
    }
    esp_memory_barrier();
    return WINK_OK;
}

static wink_pin_t s_rmt_echo_pin_cache = -1;

static void pal_restore_loopback_direction_if_needed(wink_pin_t pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) { return; }
    if (!s_gpio_mode_known[pin]) { return; }
    if (s_gpio_mode[pin] != PAL_GPIO_INPUT_OUTPUT) { return; }
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&cfg);
    esp_rom_gpio_connect_out_signal((gpio_num_t)pin, SIG_GPIO_OUT_IDX, false, false);
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]);
    static uint8_t s_restore_log_count[GPIO_NUM_MAX];
    if (s_restore_log_count[pin] < 3) {
        s_restore_log_count[pin]++;
        int lvl_before = gpio_get_level((gpio_num_t)pin);
        (void)gpio_set_level((gpio_num_t)pin, 1);
        int lvl_high = gpio_get_level((gpio_num_t)pin);
        (void)gpio_set_level((gpio_num_t)pin, 0);
        int lvl_after = gpio_get_level((gpio_num_t)pin);
        esp_rom_printf("[pal_gpio] restore loopback pin=%d (#%u) levels: before=%d hi=%d after=%d\n",
                       (int)pin, (unsigned)s_restore_log_count[pin],
                       lvl_before, lvl_high, lvl_after);
    }
}

static wink_status_t pal_gpio_pulse_in_busy_wait(wink_pin_t pin, bool level,
                                                 uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us = 0;

    uint64_t start = pal_os_get_us();
    bool current_val = false;

    while (1) {
        wink_status_t st = pal_gpio_read(pin, &current_val);
        if (wink_status_is_error(st)) {
            return WINK_ERR_IO;
        }
        if (current_val == level) {
            break;
        }
        if (pal_os_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    uint64_t pulse_start = pal_os_get_us();
    while (1) {
        wink_status_t st = pal_gpio_read(pin, &current_val);
        if (wink_status_is_error(st)) {
            return WINK_ERR_IO;
        }
        if (current_val != level) {
            break;
        }
        if (pal_os_get_us() - start > timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
    }

    *pulse_us = (uint32_t)(pal_os_get_us() - pulse_start);
    return WINK_OK;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL || pin < 0 || pin >= GPIO_NUM_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us = 0;

    bool rmt_ready = pal_rmt_pulse_capture_is_active();
    if (!rmt_ready || s_rmt_echo_pin_cache != pin) {
        if (pal_rmt_pulse_capture_init(pin, PAL_RMT_EDGE_RISING) == WINK_OK) {
            rmt_ready = true;
            s_rmt_echo_pin_cache = pin;
            pal_restore_loopback_direction_if_needed(pin);
        } else {
            rmt_ready = false;
            s_rmt_echo_pin_cache = -1;
        }
    }

    if (rmt_ready && s_rmt_echo_pin_cache == pin) {
        return pal_rmt_pulse_capture_wait(timeout_us, pulse_us);
    }

    return pal_gpio_pulse_in_busy_wait(pin, level, timeout_us, pulse_us);
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_out) ||
        pin_in < 0 || pin_in >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_in)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pin_out == pin_in) {
        gpio_config_t cfg_self = {
            .pin_bit_mask = (1ULL << pin_out),
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        esp_err_t err = gpio_config(&cfg_self);
        if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
        esp_rom_gpio_connect_out_signal((gpio_num_t)pin_out, SIG_GPIO_OUT_IDX, false, false);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin_out]);
        s_gpio_mode[pin_out] = PAL_GPIO_INPUT_OUTPUT;
        s_gpio_mode_known[pin_out] = true;
        return WINK_OK;
    }

    gpio_config_t config_in = {
        .pin_bit_mask = (1ULL << pin_in),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&config_in);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uint32_t sig = GPIO.func_out_sel_cfg[pin_out].func_sel;
#else
    uint32_t sig = GPIO.func_out[pin_out].func;
#endif
    esp_rom_gpio_connect_out_signal((gpio_num_t)pin_in, sig, false, false);
    return WINK_OK;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_out) ||
        pin_in < 0 || pin_in >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(pin_in)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (pin_out == pin_in) {
        PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[pin_out]);
        gpio_config_t cfg_self = {
            .pin_bit_mask = (1ULL << pin_out),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        (void)gpio_config(&cfg_self);
        esp_rom_gpio_connect_out_signal((gpio_num_t)pin_out, SIG_GPIO_OUT_IDX, false, false);
        return WINK_OK;
    }

    esp_rom_gpio_connect_out_signal((gpio_num_t)pin_in, SIG_GPIO_OUT_IDX, false, false);
    gpio_config_t config_in = {
        .pin_bit_mask = (1ULL << pin_in),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    (void)gpio_config(&config_in);
    return WINK_OK;
}

#else

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode)
{ (void)pin; (void)mode; return WINK_ERR_UNSUPPORTED; }

void pal_gpio_reset_pin(wink_pin_t pin) { (void)pin; }

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode)
{ (void)pin; (void)mode; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) { (void)pin; (void)level; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level != NULL) { *out_level = false; }
    (void)pin;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg)
{ (void)pin; (void)intr_type; (void)prio; (void)callback; (void)arg;
  return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{ (void)pin; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{ (void)pin; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level,
                                  uint32_t timeout_us, uint32_t *pulse_us)
{ (void)pin; (void)level; (void)timeout_us; (void)pulse_us;
  return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in)
{ (void)pin_out; (void)pin_in; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in)
{ (void)pin_out; (void)pin_in; return WINK_ERR_UNSUPPORTED; }

#endif
