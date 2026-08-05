// SPDX-License-Identifier: Apache-2.0
#include "dal_button.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include "pal_irq.h"
#include "pal_log.h"
#include <string.h>

#define LOG_TAG "dal_button"

static volatile dal_button_irq_notify_hook_t s_irq_hook = NULL;
static void *s_irq_hook_ctx = NULL;

static bool button_raw_pressed(bool raw_level, bool active_low) {
    return raw_level != active_low;
}

static bool button_pull_valid(dal_button_pull_t pull)
{
    return pull <= DAL_BUTTON_PULL_NONE;
}

static pal_gpio_mode_t button_gpio_mode(const dal_button_config_t *cfg)
{
    if (cfg->pull == DAL_BUTTON_PULL_AUTO) {
        return cfg->active_low ? PAL_GPIO_INPUT_PULLUP : PAL_GPIO_INPUT_PULLDOWN;
    }
    if (cfg->pull == DAL_BUTTON_PULL_UP) {
        return PAL_GPIO_INPUT_PULLUP;
    }
    if (cfg->pull == DAL_BUTTON_PULL_DOWN) {
        return PAL_GPIO_INPUT_PULLDOWN;
    }
    return PAL_GPIO_INPUT;
}

PAL_DEFINE_ISR(dal_button_gpio_isr, dal_button_t, dev) {
    if (dev->isr_counter_enabled) {
        dev->edge_count++;
    }
    if (dev->event_backend == DAL_BUTTON_BACKEND_IRQ) {
        dev->irq_pending = true;
        dal_button_irq_notify_hook_t hook = s_irq_hook;
        if (hook != NULL) {
            hook(s_irq_hook_ctx);
        }
    }
}

wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }
    if (!button_pull_valid(cfg->pull)) { return WINK_ERR_INVALID_ARG; }

    dev->initialized = false;

    bool          pin_claimed = false;
    bool          pin_inited  = false;
    wink_status_t rc;

    rc = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rc)) { return rc; }
    pin_claimed = true;

    pal_gpio_mode_t mode = button_gpio_mode(cfg);
    rc = pal_gpio_init(cfg->pin, mode);
    if (wink_status_is_error(rc)) { goto cleanup; }
    pin_inited = true;

    memcpy(&dev->config, cfg, sizeof(dal_button_config_t));
    dev->stable_pressed   = false;
    dev->last_reported    = false;
    dev->initialized      = true;
    dev->debounce_counter = 0;
    dev->debounce_threshold = DAL_BUTTON_DEBOUNCE_THRESHOLD;

    dev->event_cb            = NULL;
    dev->event_cb_ctx        = NULL;
    dev->long_press_fired    = false;
    dev->prev_pressed_for_event = false;
    dev->long_press_ms       = DAL_BUTTON_DEFAULT_LONG_PRESS_MS;
    dev->press_start_ms      = 0;
    dev->last_status         = WINK_OK;
    dev->isr_counter_enabled = false;
    dev->edge_count          = 0;

    dev->event_backend       = DAL_BUTTON_BACKEND_NONE;
    dev->gpio_isr_registered = false;
    dev->irq_pending         = false;
    return WINK_OK;

cleanup:
    if (pin_inited)  { (void)pal_gpio_reset_pin(cfg->pin); }
    if (pin_claimed) { WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner)); }
    return rc;
}

wink_status_t dal_button_poll(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool raw = false;
    wink_status_t s = pal_gpio_read(dev->config.pin, &raw);
    if (wink_status_is_error(s)) {
        dev->last_status = s;
        return s;
    }
    dev->last_status = WINK_OK;

    bool now_pressed = button_raw_pressed(raw, dev->config.active_low);

    if (now_pressed == dev->stable_pressed) {
        dev->debounce_counter = 0;
    } else {
        dev->debounce_counter++;
        if (dev->debounce_counter >= dev->debounce_threshold) {
            dev->stable_pressed = now_pressed;
            dev->debounce_counter = 0;
        }
    }

    if (dev->event_cb != NULL && dev->stable_pressed != dev->prev_pressed_for_event) {
        dev->prev_pressed_for_event = dev->stable_pressed;
        if (dev->stable_pressed) {
            dev->press_start_ms   = pal_os_get_ms();
            dev->long_press_fired = false;
            dev->event_cb(DAL_BUTTON_EVT_PRESS, dev->event_cb_ctx);
        } else {
            dev->long_press_fired = false;
            dev->event_cb(DAL_BUTTON_EVT_RELEASE, dev->event_cb_ctx);
        }
    }

    if (dev->event_cb != NULL && dev->stable_pressed && !dev->long_press_fired) {
        uint64_t held_ms = pal_os_get_ms() - dev->press_start_ms;
        if (held_ms >= dev->long_press_ms) {
            dev->long_press_fired = true;
            dev->event_cb(DAL_BUTTON_EVT_LONG_PRESS, dev->event_cb_ctx);
        }
    }

    return WINK_OK;
}

wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed) {
    if (dev == NULL || out_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_pressed = dev->stable_pressed;
    return WINK_OK;
}

wink_status_t dal_button_get_status(const dal_button_t *dev, wink_status_t *out_status) {
    if (dev == NULL || out_status == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_status = dev->last_status;
    return WINK_OK;
}

wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed) {
    if (dev == NULL || out_was_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool event = false;
    PAL_CRITICAL_SECTION({
        event = (dev->stable_pressed && !dev->last_reported);
        dev->last_reported = dev->stable_pressed;
        *out_was_pressed = event;
    });
    return WINK_OK;
}

wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    dev->event_cb     = cb;
    dev->event_cb_ctx = ctx;
    dev->prev_pressed_for_event = dev->stable_pressed;
    dev->long_press_fired       = false;
    return WINK_OK;
}

wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (ms == 0) { return WINK_ERR_INVALID_ARG; }
    dev->long_press_ms = ms;
    return WINK_OK;
}

wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (ms == 0u) { return WINK_ERR_INVALID_ARG; }

    uint32_t samples = ms / 10u;
    if (samples < 1u) { samples = 1u; }
    if (samples > 255u) { samples = 255u; }
    dev->debounce_threshold = (uint8_t)samples;
    dev->debounce_counter = 0;
    return WINK_OK;
}

wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (dev->gpio_isr_registered) { return WINK_OK; }

    wink_status_t st = pal_gpio_enable_interrupt(
        dev->config.pin,
        PAL_GPIO_INTR_ANY_EDGE,
        dal_button_gpio_isr,
        dev);
    if (wink_status_is_error(st)) { return st; }
    dev->gpio_isr_registered = true;
    return WINK_OK;
}

void dal_button_disable_gpio_isr(dal_button_t *dev) {
    if (dev == NULL || !dev->initialized) { return; }
    if (!dev->gpio_isr_registered) { return; }
    if (dev->isr_counter_enabled) { return; }
    if (dev->event_backend == DAL_BUTTON_BACKEND_IRQ) { return; }

    WINK_IGNORE_UNUSED(pal_gpio_disable_interrupt(dev->config.pin));
    WINK_IGNORE_UNUSED(pal_gpio_synchronize_interrupt(dev->config.pin));
    dev->gpio_isr_registered = false;
}

wink_status_t dal_button_enable_isr_counter(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (dev->isr_counter_enabled) { return WINK_OK; }

    dev->edge_count          = 0;
    dev->isr_counter_enabled = true;

    wink_status_t st = dal_button_enable_gpio_isr(dev);
    if (wink_status_is_error(st)) {
        dev->isr_counter_enabled = false;
        return st;
    }
    return WINK_OK;
}

wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count) {
    if (dev == NULL || out_count == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_count = dev->edge_count;
    return WINK_OK;
}

wink_status_t dal_button_reset_edge_count(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    PAL_CRITICAL_SECTION({
        dev->edge_count = 0;
    });
    return WINK_OK;
}

void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend) {
    if (dev == NULL) { return; }
    PAL_CRITICAL_SECTION({
        dev->event_backend = backend;
    });
}

wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending) {
    if (dev == NULL || out_was_pending == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    bool was_pending = false;
    PAL_CRITICAL_SECTION({
        was_pending = dev->irq_pending;
        dev->irq_pending = false;
    });
    *out_was_pending = was_pending;
    return WINK_OK;
}

void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx) {
    s_irq_hook_ctx = ctx;
    s_irq_hook     = fn;
}

wink_status_t dal_button_deinit(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    uint16_t pin = dev->config.pin;
    const char *owner = dev->config.owner;

    wink_status_t first_err = WINK_OK;
#define LOGW_IF_RC(step, expr) do {                                              \
        if (wink_status_is_error((expr)) && !wink_status_is_error(first_err)) { \
            first_err = (expr);                                                  \
            LOG_W("deinit step '%s' failed rc=%d (continuing best-effort)",      \
                  (step), (int)(expr));                                          \
        }                                                                        \
    } while (0)
#define LOGW_IF_VOID(step, call) do {                                            \
        LOG_W("deinit step '%s' returned void (no rc to record; check PAL)",     \
              (step));                                                           \
        (void)(call);                                                            \
    } while (0)

    dev->event_backend       = DAL_BUTTON_BACKEND_NONE;
    dev->isr_counter_enabled = false;
    if (dev->gpio_isr_registered) {
        LOGW_IF_RC("pal_gpio_disable_interrupt", pal_gpio_disable_interrupt(pin));
        LOGW_IF_RC("pal_gpio_synchronize_interrupt",
                   pal_gpio_synchronize_interrupt(pin));
        dev->gpio_isr_registered = false;
    }

    LOGW_IF_VOID("pal_gpio_reset_pin", pal_gpio_reset_pin(pin));

    LOGW_IF_RC("pal_resource_release",
               pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, owner));

#undef LOGW_IF_RC
#undef LOGW_IF_VOID

    memset(dev, 0, sizeof(dal_button_t));

    return first_err;
}
