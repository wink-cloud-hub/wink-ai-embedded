// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_host.c
 * @brief Host platform PAL HAL implementation.
 */
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_osal.h"
#define WINK_ALLOW_ADVANCED_IRQ_APIS
#include "pal_irq_advanced.h"
#include "pal_resource.h"
#include "pal_pwm_router.h"
#include "hal/pal_rmt.h"

#include "host_test_ctrl.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

extern uint64_t host_sim_time_us(void);
extern void host_sim_advance_to(uint64_t us);
extern uint64_t host_echo_rise_us(void);
extern uint64_t host_echo_high_us(void);
extern uint16_t host_echo_pin(void);
extern void host_record_pwm(uint8_t channel, float duty);

#define ECHO_POLL_WINDOW_US 30000u
#define HOST_MAX_GPIO_PIN  50

static pal_gpio_mode_t s_gpio_mode[HOST_MAX_GPIO_PIN];
static bool            s_gpio_mode_known[HOST_MAX_GPIO_PIN];

static bool s_gpio_out_level[HOST_MAX_GPIO_PIN];
static bool s_gpio_out_known[HOST_MAX_GPIO_PIN];

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

#define HOST_MAX_LOOPBACKS 8
static struct {
    wink_pin_t pin_out;
    wink_pin_t pin_in;
    bool active;
} s_host_loopbacks[HOST_MAX_LOOPBACKS] = {0};

static wink_pin_t s_host_pwm_pins[PAL_PWM_CHANNELS] = {
    -1, 4, 5, -1, -1, -1, -1, -1
};

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin >= 0 && pin < HOST_MAX_GPIO_PIN) {
        s_gpio_mode[pin] = mode;
        s_gpio_mode_known[pin] = true;
    }
    return WINK_OK;
}

void pal_gpio_reset_pin(wink_pin_t pin) {
    if (pin >= 0 && pin < HOST_MAX_GPIO_PIN) {
        s_gpio_mode_known[pin] = false;
        s_gpio_out_known[pin] = false;
    }
    (void)pin;
}

bool pal_host_get_gpio_level(wink_pin_t pin, bool *out_level) {
    if (out_level != NULL) {
        *out_level = false;
    }
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN || out_level == NULL) {
        return false;
    }
    if (!s_gpio_out_known[pin]) {
        return false;
    }
    *out_level = s_gpio_out_level[pin];
    return true;
}

void pal_host_reset_gpio_levels(void) {
    memset(s_gpio_out_level, 0, sizeof(s_gpio_out_level));
    memset(s_gpio_out_known, 0, sizeof(s_gpio_out_known));
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    (void)pin;
    (void)mode;
    return WINK_OK;
}

extern void sim_set_gpio_ideal(uint16_t pin, bool level);

static wink_pin_t s_host_rmt_pin         = -1;
static bool       s_host_rmt_armed       = false;
static uint64_t   s_host_rmt_last_rise_us  = 0;
static uint32_t   s_host_rmt_last_pulse_us = 0;

static void host_rmt_note_edge(bool level) {
    if (!s_host_rmt_armed) { return; }
    uint64_t now = host_sim_time_us();
    if (level) {
        s_host_rmt_last_rise_us = now;
    } else {
        if (s_host_rmt_last_rise_us > 0 && now > s_host_rmt_last_rise_us) {
            uint64_t diff = now - s_host_rmt_last_rise_us;
            s_host_rmt_last_pulse_us = (diff > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)diff;
        }
    }
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    s_gpio_out_level[pin] = level;
    s_gpio_out_known[pin] = true;
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin) {
            sim_set_gpio_ideal((uint16_t)s_host_loopbacks[i].pin_in, level);
            if (s_host_rmt_armed && s_host_loopbacks[i].pin_in == s_host_rmt_pin) {
                host_rmt_note_edge(level);
            }
        }
    }
    (void)level;
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_level = false;

    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    bool debounced;
    extern bool host_gpio_read_debounced(uint16_t pin, bool *out_level);
    if (host_gpio_read_debounced(pin, &debounced)) {
        *out_level = debounced;
        return WINK_OK;
    }

    if (pin != host_echo_pin()) {
        if (s_gpio_mode_known[pin]) {
            if (s_gpio_mode[pin] == PAL_GPIO_INPUT) {
                return WINK_ERR_DISCONNECTED;
            }
            *out_level = pal_gpio_mode_idle_level(s_gpio_mode[pin]);
        } else {
            *out_level = false;
        }
        return WINK_OK;
    }

    uint64_t t = host_sim_time_us();
    uint64_t rise = host_echo_rise_us();
    uint64_t high = host_echo_high_us();
    if (t < rise) {
        uint64_t target = rise;
        if (rise - t > ECHO_POLL_WINDOW_US) target = t + ECHO_POLL_WINDOW_US;
        host_sim_advance_to(target);
        *out_level = (target >= rise);
        return WINK_OK;
    }
    if (t < rise + high) {
        host_sim_advance_to(rise + high);
        *out_level = false;
        return WINK_OK;
    }
    *out_level = false;
    return WINK_OK;
}

#define HOST_MAX_PENDING   64

static pal_gpio_isr_t  s_gpio_isr[HOST_MAX_GPIO_PIN] = {NULL};
static void            *s_gpio_isr_arg[HOST_MAX_GPIO_PIN] = {NULL};
static uint32_t         s_isr_call_count[HOST_MAX_GPIO_PIN] = {0};

#if defined(_WIN32)
static CRITICAL_SECTION s_gpio_service_mux;
static LONG             s_gpio_service_mux_init = 0;
static void host_gpio_service_mux_init(void)
{
    if (InterlockedCompareExchange(&s_gpio_service_mux_init, 1, 0) == 0) {
        InitializeCriticalSection(&s_gpio_service_mux);
    }
}
#  define HOST_GPIO_SERVICE_LOCK()   do { host_gpio_service_mux_init(); EnterCriticalSection(&s_gpio_service_mux); } while (0)
#  define HOST_GPIO_SERVICE_UNLOCK() LeaveCriticalSection(&s_gpio_service_mux)
#else
static pthread_mutex_t  s_gpio_service_mux         = PTHREAD_MUTEX_INITIALIZER;
#  define HOST_GPIO_SERVICE_LOCK()   pthread_mutex_lock(&s_gpio_service_mux)
#  define HOST_GPIO_SERVICE_UNLOCK() pthread_mutex_unlock(&s_gpio_service_mux)
#endif
static bool             s_gpio_service_initialized = false;
static pal_irq_prio_t   s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;

static uint32_t s_pending_gpio[HOST_MAX_PENDING];
static uint32_t s_pending_count = 0;

static int s_irq_lock_depth = 0;

static void flush_pending_interrupts(void)
{
    while (s_pending_count > 0) {
        s_pending_count--;
        uint32_t pin = s_pending_gpio[s_pending_count];

        if (pin < HOST_MAX_GPIO_PIN && s_gpio_isr[pin] != NULL) {
            s_isr_call_count[pin]++;
            pal_os_set_sim_isr_context(true);
            s_gpio_isr[pin](s_gpio_isr_arg[pin]);
            pal_os_set_sim_isr_context(false);
        }
    }
}

wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin, pal_gpio_intr_t intr_type,
                                         pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg)
{
    (void)intr_type;

    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    if (callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
    }

    HOST_GPIO_SERVICE_LOCK();
    if (s_gpio_service_initialized) {
        if (prio != s_gpio_service_prio) {
            HOST_GPIO_SERVICE_UNLOCK();
            return WINK_ERR_INVALID_ARG;
        }
    } else {
        s_gpio_service_prio        = prio;
        s_gpio_service_initialized = true;
    }
    HOST_GPIO_SERVICE_UNLOCK();

    s_gpio_isr[pin] = callback;
    s_gpio_isr_arg[pin] = arg;
    s_isr_call_count[pin] = 0;

    return WINK_OK;
}

wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }

    s_gpio_isr[pin] = NULL;
    return WINK_OK;
}

wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    return WINK_OK;
}

void pal_host_trigger_gpio_interrupt(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) return;
    if (s_gpio_isr[pin] == NULL) return;

    if (s_irq_lock_depth > 0) {
        if (s_pending_count < HOST_MAX_PENDING) {
            s_pending_gpio[s_pending_count++] = (uint32_t)pin;
        }
    } else {
        s_isr_call_count[pin]++;
        pal_os_set_sim_isr_context(true);
        s_gpio_isr[pin](s_gpio_isr_arg[pin]);
        pal_os_set_sim_isr_context(false);
    }
}

uint32_t pal_host_get_isr_call_count(wink_pin_t pin)
{
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) return 0;
    return s_isr_call_count[pin];
}

uint32_t pal_host_get_pending_count(void)
{
    return s_pending_count;
}

int pal_host_get_irq_lock_depth(void)
{
    return s_irq_lock_depth;
}

#define HOST_MAX_IRQ  32
static pal_isr_t s_host_irq_table[HOST_MAX_IRQ] = {NULL};
static void *s_host_irq_arg[HOST_MAX_IRQ] = {NULL};
static uint32_t s_host_irq_call_count[HOST_MAX_IRQ] = {0};

wink_status_t pal_irq_enable(uint32_t irq_num, pal_irq_prio_t prio,
                              pal_isr_t handler, void *arg)
{
    if (irq_num >= HOST_MAX_IRQ || handler == NULL ||
        prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH) {
        return WINK_ERR_INVALID_ARG;
    }

    s_host_irq_table[irq_num] = handler;
    s_host_irq_arg[irq_num] = arg;
    s_host_irq_call_count[irq_num] = 0;
    return WINK_OK;
}

wink_status_t pal_irq_disable(uint32_t irq_num)
{
    if (irq_num >= HOST_MAX_IRQ) {
        return WINK_ERR_INVALID_ARG;
    }
    s_host_irq_table[irq_num] = NULL;
    s_host_irq_arg[irq_num] = NULL;
    return WINK_OK;
}

void pal_irq_set_pending(uint32_t irq_num)
{
    if (irq_num < HOST_MAX_IRQ && s_host_irq_table[irq_num] != NULL) {
        if (s_irq_lock_depth > 0) {
        } else {
            s_host_irq_call_count[irq_num]++;
            pal_os_set_sim_isr_context(true);
            s_host_irq_table[irq_num](s_host_irq_arg[irq_num]);
            pal_os_set_sim_isr_context(false);
        }
    }
}

void pal_irq_clear_pending(uint32_t irq_num)
{
    (void)irq_num;
}

void pal_irq_synchronize(uint32_t irq_num)
{
    (void)irq_num;
}

void pal_host_trigger_logical_interrupt(uint32_t irq_num)
{
    pal_irq_set_pending(irq_num);
}

uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num)
{
    if (irq_num >= HOST_MAX_IRQ) return 0;
    return s_host_irq_call_count[irq_num];
}

void pal_host_reset_isr_stats(void)
{
    memset(s_isr_call_count, 0, sizeof(s_isr_call_count));
    memset(s_host_irq_call_count, 0, sizeof(s_host_irq_call_count));
    s_pending_count = 0;
    s_irq_lock_depth = 0;

    HOST_GPIO_SERVICE_LOCK();
    s_gpio_service_initialized = false;
    s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;
    HOST_GPIO_SERVICE_UNLOCK();

    memset(s_gpio_isr, 0, sizeof(s_gpio_isr));
    memset(s_gpio_isr_arg, 0, sizeof(s_gpio_isr_arg));
}

uint32_t pal_irq_save(void)
{
    uint32_t old_depth = s_irq_lock_depth;
    s_irq_lock_depth++;
    return old_depth;
}

uint32_t pal_irq_save_rtos_safe(void)
{
    return pal_irq_save();
}

void pal_irq_restore(uint32_t mask)
{
    if (s_irq_lock_depth <= 0) {
        fprintf(stderr, "WARNING: pal_irq_restore() called without matching save()!\n");
        return;
    }

    s_irq_lock_depth--;

    if (s_irq_lock_depth == 0 && mask == 0) {
        flush_pending_interrupts();
    }
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq) {
    pal_pwm_config_t cfg = { .freq_hz = freq };
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
    wink_status_t rs = pal_pwm_router_acquire(channel, &prof, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    host_record_pwm(channel, duty);
    return WINK_OK;
}

wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz) {
    if (!pal_pwm_router_channel_ready(channel) || freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    return pal_pwm_router_set_freq(channel, freq_hz);
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }
    pal_pwm_router_release(channel);
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= HOST_MAX_GPIO_PIN || pin_in < 0 || pin_in >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    int first_empty = -1;
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin_out && s_host_loopbacks[i].pin_in == pin_in) {
            return WINK_OK;
        }
        if (!s_host_loopbacks[i].active && first_empty < 0) {
            first_empty = i;
        }
    }
    if (first_empty < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    s_host_loopbacks[first_empty].pin_out = pin_out;
    s_host_loopbacks[first_empty].pin_in = pin_in;
    s_host_loopbacks[first_empty].active = true;
    return WINK_OK;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    if (pin_out < 0 || pin_out >= HOST_MAX_GPIO_PIN || pin_in < 0 || pin_in >= HOST_MAX_GPIO_PIN) {
        return WINK_ERR_INVALID_ARG;
    }
    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_out == pin_out && s_host_loopbacks[i].pin_in == pin_in) {
            s_host_loopbacks[i].active = false;
            return WINK_OK;
        }
    }
    return WINK_OK;
}

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (s_host_pwm_pins[channel] < 0) {
        return WINK_ERR_UNSUPPORTED;
    }
    *out_pin = s_host_pwm_pins[channel];
    return WINK_OK;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

extern void host_record_i2c(uint8_t port, uint16_t addr, uint32_t write_len);

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

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t addr,
                      const uint8_t *w, uint32_t wl, uint8_t *r, uint32_t rl) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d transfer called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    (void)w;
    host_record_i2c(port, addr, wl);
    if (r != NULL && rl > 0u) {
        memset(r, 0, rl);
    }
    return WINK_OK;
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
    memset(out_found_bitmap, 0, 16);
    return WINK_OK;
}

static uint32_t (*s_sim_measure_fn)(uint16_t) = NULL;

void host_register_sim_ultrasonic(void (*trigger_fn)(uint16_t), uint32_t (*measure_fn)(uint16_t)) {
    (void)trigger_fn;
    s_sim_measure_fn = measure_fn;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    *pulse_us = 0;

    for (int i = 0; i < HOST_MAX_LOOPBACKS; i++) {
        if (s_host_loopbacks[i].active && s_host_loopbacks[i].pin_in == pin) {
            wink_pin_t pin_out = s_host_loopbacks[i].pin_out;
            for (int ch = 0; ch < PAL_PWM_CHANNELS; ch++) {
                if (s_host_pwm_pins[ch] == pin_out) {
                    float duty = sim_last_pwm_duty(ch);
                    uint32_t p = (uint32_t)((duty / 100.0f) * (1000000.0f / 50.0f));
                    if (p >= timeout_us) {
                        return WINK_ERR_TIMEOUT;
                    }
                    *pulse_us = p;
                    (void)level;
                    return WINK_OK;
                }
            }
        }
    }

    if (s_sim_measure_fn) {
        uint32_t p = s_sim_measure_fn((uint16_t)pin);
        if (p > 0) {
            if (p >= timeout_us) {
                return WINK_ERR_TIMEOUT;
            }
            *pulse_us = p;
            (void)level;
            return WINK_OK;
        }
    }

    if (pin != (wink_pin_t)host_echo_pin()) { return WINK_ERR_UNSUPPORTED; }
    uint64_t rise = host_echo_rise_us();
    if (rise > timeout_us) { return WINK_ERR_TIMEOUT; }
    *pulse_us = (uint32_t)host_echo_high_us();
    (void)level;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    if (pin < 0 || pin >= HOST_MAX_GPIO_PIN) { return WINK_ERR_INVALID_ARG; }
    s_host_rmt_pin = pin;
    s_host_rmt_armed = false;
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
    (void)start_edge;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_arm(void) {
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_ARG; }
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
    s_host_rmt_armed = true;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL) { return WINK_ERR_INVALID_ARG; }
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_STATE; }
    *pulse_us_out = 0;

    if (s_host_rmt_armed && s_host_rmt_last_pulse_us > 0) {
        uint32_t p = s_host_rmt_last_pulse_us;
        s_host_rmt_armed = false;
        s_host_rmt_last_pulse_us = 0;
        s_host_rmt_last_rise_us = 0;
        if (p >= timeout_us) {
            return WINK_ERR_TIMEOUT;
        }
        *pulse_us_out = p;
        return WINK_OK;
    }

    s_host_rmt_armed = false;
    wink_status_t st = pal_gpio_pulse_in(s_host_rmt_pin, true, timeout_us, pulse_us_out);
    if (st == WINK_ERR_UNSUPPORTED) {
        return WINK_ERR_TIMEOUT;
    }
    return st;
}

wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL) { return WINK_ERR_INVALID_ARG; }
    if (s_host_rmt_pin < 0) { return WINK_ERR_INVALID_STATE; }
    *pulse_us_out = 0;
    wink_status_t s = pal_rmt_pulse_capture_arm();
    if (wink_status_is_error(s)) { return s; }
    return pal_rmt_pulse_capture_wait_armed(timeout_us, pulse_us_out);
}

void pal_rmt_pulse_capture_deinit(void) {
    s_host_rmt_pin = -1;
    s_host_rmt_armed = false;
    s_host_rmt_last_rise_us = 0;
    s_host_rmt_last_pulse_us = 0;
}

bool pal_rmt_pulse_capture_is_active(void) {
    return s_host_rmt_pin >= 0;
}
