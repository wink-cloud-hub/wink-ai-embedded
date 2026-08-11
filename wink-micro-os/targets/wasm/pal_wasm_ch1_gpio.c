// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch1_gpio.c
 * @brief Wasm target Axis A (CH1) GPIO digital read/write, edge events, & mode management.
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"
#include "pal_wasm_waveform.h"
#include "wink_sim_physical.h"
#include "sensor/wink_ultrasonic_distance_events.h"

static pal_gpio_mode_t s_gpio_mode[WASM_SIM_MAX_PINS];
static bool            s_gpio_mode_known[WASM_SIM_MAX_PINS];
static bool            s_gpio_output_state[WASM_SIM_MAX_PINS];

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

void pal_wasm_ch1_gpio_reset(void) {
    memset(s_gpio_mode, 0, sizeof(s_gpio_mode));
    memset(s_gpio_mode_known, 0, sizeof(s_gpio_mode_known));
    memset(s_gpio_output_state, 0, sizeof(s_gpio_output_state));
    wasm_sim_pin_events_reset();
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
    s_gpio_output_state[(uint8_t)pin] = level;
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
                    pal_wasm_set_pulse_measurement_active(true);
                    pal_wasm_advance_virtual_clock(t_end - current_time);
                    pal_wasm_set_pulse_measurement_active(false);
                }
                *pulse_us = (uint32_t)duration;
                s_pin_event_count[(uint8_t)pin] = 0;
                return WINK_OK;
            }
        }
    }

    s_pin_event_count[(uint8_t)pin] = 0;
    if (timeout_us > 0) {
        pal_wasm_set_pulse_measurement_active(true);
        pal_wasm_advance_virtual_clock((uint64_t)timeout_us);
        pal_wasm_set_pulse_measurement_active(false);
    }
    return WINK_ERR_TIMEOUT;
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us, uint8_t level) {
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

    pal_wasm_push_waveform_edge((uint16_t)pin, pal_os_get_us() + delay_us, level, 0);
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_trigger_ultrasonic_measurement(uint8_t trig_pin) {
    WASM_FAULT_GUARD_VOID();
    (void)wink_ultrasonic_distance_events_trigger_now_by_trig_pin(trig_pin);
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_gpio_read(uint16_t pin) {
    WASM_FAULT_GUARD_BOOL();
    bool level = false;
    wink_status_t st = pal_gpio_read((wink_pin_t)pin, &level);
    return wink_status_is_error(st) ? false : level;
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_gpio_input(uint8_t pin, bool level) {
    js_pal_gpio_drive_ideal((uint16_t)pin, level);
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_get_gpio_output(uint8_t pin) {
    return (pin < WASM_SIM_MAX_PINS) ? s_gpio_output_state[pin] : false;
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
