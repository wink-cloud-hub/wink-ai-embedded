// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_wasm.c
 * @brief Wasm simulation target PAL HAL implementation (GPIO / PWM / I2C / pulse_in / debug_printf).
 */
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_pwm_router.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "devices/wasm_sim_registry.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI contract #5 "
    "and review every (uint32_t)(uintptr_t) cast in pal_hal_wasm.c / createUnisimImports.ts");

static pal_gpio_mode_t s_gpio_mode[WASM_SIM_MAX_PINS];
static bool            s_gpio_mode_known[WASM_SIM_MAX_PINS];

#define PIN_EVENT_QUEUE_SIZE 8

typedef struct {
    uint64_t virtual_time_us;
    uint8_t level;
} wasm_pin_event_t;

static wasm_pin_event_t s_pin_events[WASM_SIM_MAX_PINS][PIN_EVENT_QUEUE_SIZE];
static uint8_t s_pin_event_count[WASM_SIM_MAX_PINS] = {0};

void wasm_sim_pin_events_reset(void) {
    memset(s_pin_event_count, 0, sizeof(s_pin_event_count));
    memset(s_pin_events, 0, sizeof(s_pin_events));
}

static bool pal_gpio_mode_idle_level(pal_gpio_mode_t mode)
{
    switch (mode) {
        case PAL_GPIO_INPUT_PULLUP:
            return true;
        case PAL_GPIO_INPUT_PULLDOWN:
            return false;
        default:
            return false;
    }
}

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin >= 0 && pin < WASM_SIM_MAX_PINS) {
        s_gpio_mode[(uint8_t)pin] = mode;
        s_gpio_mode_known[(uint8_t)pin] = true;
        if (mode == PAL_GPIO_INPUT
            || mode == PAL_GPIO_INPUT_PULLUP
            || mode == PAL_GPIO_INPUT_PULLDOWN) {
            js_pal_gpio_release_mcu((uint16_t)pin);
        }
    }
    return WINK_OK;
}

void pal_gpio_reset_pin(wink_pin_t pin) {
    if (pin >= 0 && pin < WASM_SIM_MAX_PINS) {
        s_gpio_mode_known[(uint8_t)pin] = false;
    }
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    (void)pin; (void)mode;
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    wasm_sim_gpio_write((uint8_t)pin, level);
    js_pal_gpio_write((uint32_t)pin, level);
    js_pal_gpio_on_write((uint8_t)pin, level ? 1 : 0);
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_level = false;

    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }

    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    bool ideal;
    uint8_t st = js_pal_gpio_read_state((uint16_t)pin);
    if (st == JS_GPIO_STATE_HIGH || st == JS_GPIO_STATE_CONFLICT) {
        ideal = true;
    } else if (st == JS_GPIO_STATE_LOW) {
        ideal = false;
    } else {
        if (!s_gpio_mode_known[(uint8_t)pin]) {
            ideal = false;
        } else if (s_gpio_mode[(uint8_t)pin] == PAL_GPIO_INPUT) {
            return WINK_ERR_DISCONNECTED;
        } else {
            ideal = pal_gpio_mode_idle_level(s_gpio_mode[(uint8_t)pin]);
        }
    }

    uint32_t bounce_us = pal_wasm_get_bounce_us();
    if (bounce_us > 0u) {
        wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(pin);
        if (ctx != NULL) {
            bool was_in_bounce = ctx->in_bounce;
            bool result = wink_phys_debounce_step(ctx, ideal, pal_os_get_us(), bounce_us);
            if (!was_in_bounce && ctx->in_bounce) {
                pal_wasm_log_fault(FAULT_TYPE_GPIO_BOUNCE, pin);
            }
            *out_level = result;
            return WINK_OK;
        }
    }

    *out_level = ideal;
    return WINK_OK;
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz) {
    pal_pwm_config_t cfg = { .freq_hz = frequency_hz };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg) {
    if (cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        return WINK_ERR_UNSUPPORTED;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    if (bits == 0u || bits > 20u) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO,
    };
    uint8_t timer_num = 0;
    return pal_pwm_router_acquire(channel, &prof, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (wasm_sim_pwm_channel_exists(channel)) {
        wasm_sim_pwm_set_duty(channel, duty_cycle_percent);
    }
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz) {
    if (!pal_pwm_router_channel_ready(channel) || freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    return pal_pwm_router_set_freq(channel, freq_hz);
}

void pal_pwm_deinit(uint8_t channel) {
    pal_pwm_router_release(channel);
}

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

static bool s_i2c_bus_inited[PAL_I2C_PORTS] = {false};

wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz) {
    (void)sda; (void)scl; (void)hz;
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    s_i2c_bus_inited[port] = true;
    return WINK_OK;
}

void pal_i2c_bus_deinit(uint8_t port) {
    if (port < PAL_I2C_PORTS) {
        s_i2c_bus_inited[port] = false;
    }
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d transfer called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }

    uint16_t drop_permil = pal_wasm_get_i2c_drop_permil();
    if (drop_permil > 0u) {
        uint32_t prng_state = pal_wasm_get_prng_state();
        bool should_drop = wink_phys_bus_drop(drop_permil, &prng_state);
        pal_wasm_advance_prng_state(prng_state);
        if (should_drop) {
            pal_wasm_log_fault(FAULT_TYPE_I2C_DROP, port);
            return WINK_ERR_IO;
        }
    }

    if (wasm_sim_i2c_dev_exists(dev_addr)) {
        return wasm_sim_i2c_dev_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len);
    }

    return js_pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)
           ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes) {
    if (out_found_bitmap == NULL || bitmap_bytes < 16) {
        return WINK_ERR_INVALID_ARG;
    }
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d scan called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    if (start_addr > end_addr || end_addr > 0x7F) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t lo = start_addr < 0x03 ? 0x03 : start_addr;
    uint8_t hi = end_addr   > 0x77 ? 0x77 : end_addr;

    memset(out_found_bitmap, 0, 16);
    for (uint16_t addr = lo; addr <= hi; addr++) {
        if (wasm_sim_i2c_dev_exists((uint8_t)addr)) {
            uint8_t byte_idx = (uint8_t)(addr >> 3);
            uint8_t bit_idx  = (uint8_t)(addr & 0x7);
            out_found_bitmap[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
    }
    return WINK_OK;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    *pulse_us = 0;

    uint8_t count = s_pin_event_count[(uint8_t)pin];
    if (count > 0) {
        int start_idx = -1;
        int end_idx = -1;
        for (int i = 0; i < count; i++) {
            if (s_pin_events[(uint8_t)pin][i].level == (level ? 1 : 0)) {
                start_idx = i;
                for (int j = i + 1; j < count; j++) {
                    if (s_pin_events[(uint8_t)pin][j].level != (level ? 1 : 0)) {
                        end_idx = j;
                        break;
                    }
                }
                break;
            }
        }
        if (start_idx != -1 && end_idx != -1) {
            uint64_t t_start = s_pin_events[(uint8_t)pin][start_idx].virtual_time_us;
            uint64_t t_end = s_pin_events[(uint8_t)pin][end_idx].virtual_time_us;
            if (t_end > t_start) {
                uint64_t duration = t_end - t_start;
                uint64_t current_time = pal_os_get_us();
                if (t_end > current_time) {
                    pal_wasm_advance_virtual_clock(t_end - current_time);
                }
                *pulse_us = (uint32_t)duration;
                s_pin_event_count[(uint8_t)pin] = 0;
                return WINK_OK;
            }
        }
    }

    if (timeout_us > 0) {
        pal_wasm_advance_virtual_clock((uint64_t)timeout_us);
    }
    return WINK_ERR_TIMEOUT;
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us, uint8_t level) {
    WASM_FAULT_GUARD_VOID();
    if (pin >= WASM_SIM_MAX_PINS) {
        return;
    }
    uint8_t count = s_pin_event_count[pin];
    if (count >= PIN_EVENT_QUEUE_SIZE) {
        for (int i = 1; i < PIN_EVENT_QUEUE_SIZE; i++) {
            s_pin_events[pin][i - 1] = s_pin_events[pin][i];
        }
        count = PIN_EVENT_QUEUE_SIZE - 1;
    }
    s_pin_events[pin][count].virtual_time_us = pal_os_get_us() + delay_us;
    s_pin_events[pin][count].level = level;
    s_pin_event_count[pin] = count + 1;
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    (void)pin_out; (void)pin_in;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    (void)pin_out; (void)pin_in;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, uint8_t start_edge) {
    (void)pin;
    (void)start_edge;
    return WINK_ERR_UNSUPPORTED;
}
