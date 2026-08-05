// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_ultrasonic"

#include "dal_ultrasonic.h"

#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "hal/pal_rmt.h"
#include "wink_pt_debug.h"
#include "pal_log.h"

#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define ULTRASONIC_TIMEOUT_US 60000u
#define ULTRASONIC_CM_PER_US  0.017f   /* Speed of sound (340m/s round trip) */

float dal_pulse_us_to_cm(uint32_t pulse_us) {
    return (float)pulse_us * ULTRASONIC_CM_PER_US;
}

wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len) {
    dal_ultrasonic_t *u = (dal_ultrasonic_t *)dev;
    if (u == NULL || params == NULL) { return WINK_ERR_INVALID_ARG; }

    uint16_t trig_pin;
    uint16_t echo_pin;
    if (len >= 5u && params[0] == 0x01u) {
        memcpy(&trig_pin, params + 1, 2);
        memcpy(&echo_pin, params + 3, 2);
    } else if (len >= 4u) {
        memcpy(&trig_pin, params + 0, 2);
        memcpy(&echo_pin, params + 2, 2);
    } else {
        return WINK_ERR_INVALID_ARG;
    }

    if (trig_pin == echo_pin) { return WINK_ERR_INVALID_ARG; }

    u->config.trig_pin = trig_pin;
    u->config.echo_pin = echo_pin;
    return WINK_OK;
}

wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    uint16_t trig_pin = dev->config.trig_pin;
    uint16_t echo_pin = dev->config.echo_pin;
    const char *owner = dev->config.owner;
    bool use_rmt = dev->config.use_rmt;

    wink_status_t first_err = WINK_OK;
#define LOGW_IF_RC(step, expr) do {                                            \
        if (wink_status_is_error((expr)) && !wink_status_is_error(first_err)) { \
            first_err = (expr);                                               \
            LOG_W("deinit step '%s' failed rc=%d (continuing best-effort)",  \
                  (step), (int)(expr));                                       \
        }                                                                      \
    } while (0)
#define LOGW_IF_VOID(step, call) do {                                          \
        LOG_W("deinit step '%s' returned void (no rc to record; check PAL)",  \
              (step));                                                         \
        (void)(call);                                                          \
    } while (0)

    LOGW_IF_RC("pal_gpio_write(trig LOW)", pal_gpio_write(trig_pin, false));

    if (use_rmt) {
        LOGW_IF_VOID("pal_rmt_pulse_capture_deinit", pal_rmt_pulse_capture_deinit());
    }

    LOGW_IF_VOID("pal_gpio_reset_pin(trig)", pal_gpio_reset_pin(trig_pin));
    LOGW_IF_VOID("pal_gpio_reset_pin(echo)", pal_gpio_reset_pin(echo_pin));

    LOGW_IF_RC("pal_resource_release(trig)",
               pal_resource_release(PAL_RESOURCE_GPIO_PIN, trig_pin, owner));
    LOGW_IF_RC("pal_resource_release(echo)",
               pal_resource_release(PAL_RESOURCE_GPIO_PIN, echo_pin, owner));

#undef LOGW_IF_RC
#undef LOGW_IF_VOID

    memset(dev, 0, sizeof(dal_ultrasonic_t));

    return first_err;
}

wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->trig_pin == cfg->echo_pin) {
        return WINK_ERR_INVALID_ARG;
    }

    dev->initialized = false;
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    bool          trig_claimed = false;
    bool          echo_claimed = false;
    bool          trig_inited  = false;
    bool          echo_inited  = false;
    wink_status_t rc;

    rc = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->trig_pin, cfg->owner);
    if (wink_status_is_error(rc)) { return rc; }
    trig_claimed = true;

    rc = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->echo_pin, cfg->owner);
    if (wink_status_is_error(rc)) { goto cleanup; }
    echo_claimed = true;

    memcpy(&dev->config, cfg, sizeof(dal_ultrasonic_config_t));
    dev->last_distance = 0.0f;
    dev->state = DAL_ULTRASONIC_IDLE;
    dev->last_status = WINK_OK;
    dev->last_pulse_us = 0u;

    rc = pal_gpio_init(cfg->trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(rc)) { goto cleanup; }
    trig_inited = true;

    rc = pal_gpio_init(cfg->echo_pin, PAL_GPIO_INPUT);
    if (wink_status_is_error(rc)) { goto cleanup; }
    echo_inited = true;

    if (cfg->use_rmt) {
        WINK_IGNORE_UNUSED(pal_rmt_pulse_capture_init(cfg->echo_pin,
                                                       PAL_RMT_EDGE_RISING));
    }

    dev->initialized = true;
    LOG_I("init OK: owner=%s trig=%u echo=%u rmt=%d",
          (cfg->owner == NULL ? "?" : cfg->owner), (unsigned)cfg->trig_pin,
          (unsigned)cfg->echo_pin, (int)cfg->use_rmt);
    return WINK_OK;

cleanup:
    LOG_E("init FAILED rc=%d: owner=%s trig=%u echo=%u rmt=%d (rolling back)",
          (int)rc, (cfg->owner == NULL ? "?" : cfg->owner),
          (unsigned)cfg->trig_pin, (unsigned)cfg->echo_pin, (int)cfg->use_rmt);
    if (echo_inited)  { (void)pal_gpio_reset_pin(cfg->echo_pin); }
    if (trig_inited)  { (void)pal_gpio_reset_pin(cfg->trig_pin); }
    if (echo_claimed) { WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->echo_pin, cfg->owner)); }
    if (trig_claimed) { WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->trig_pin, cfg->owner)); }
    return rc;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    if (dev->state == DAL_ULTRASONIC_MEASURING) {
        return WINK_ERR_BUSY;
    }

    wink_status_t write_status = pal_gpio_write(dev->config.trig_pin, true);
    if (wink_status_is_error(write_status)) {
        dev->last_status = write_status;
        dev->state = DAL_ULTRASONIC_ERROR;
        return WINK_OK;
    }
    pal_os_busy_wait_us(10);
    write_status = pal_gpio_write(dev->config.trig_pin, false);
    if (wink_status_is_error(write_status)) {
        dev->last_status = write_status;
        dev->state = DAL_ULTRASONIC_ERROR;
        return WINK_OK;
    }
    dev->state = DAL_ULTRASONIC_MEASURING;

    uint32_t pulse_us = 0;
    wink_status_t cap = pal_gpio_pulse_in(
        dev->config.echo_pin,
        true,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );

    if (wink_status_is_error(cap)) {
        dev->last_status = cap;
        dev->state = DAL_ULTRASONIC_ERROR;
    } else {
        dev->last_pulse_us = pulse_us;
        dev->last_distance = dal_pulse_us_to_cm(pulse_us);
        dev->last_status = WINK_OK;
#if defined(ESP_PLATFORM)
        __asm__ __volatile__("memw" ::: "memory");
#endif
        dev->state = DAL_ULTRASONIC_READY;
    }
    return WINK_OK;
}
#endif /* WINK_STRICT_NONBLOCKING (request_measurement) */

wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *out_distance_cm) {
    if (dev == NULL || out_distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    float              dist  = dev->last_distance;
    wink_status_t      lstat = dev->last_status;
    dal_ultrasonic_state_t st = dev->state;

    switch (st) {
        case DAL_ULTRASONIC_READY:
            *out_distance_cm = dist;
            return WINK_OK;
        case DAL_ULTRASONIC_MEASURING:
            if (lstat == WINK_OK) {
                *out_distance_cm = dist;
                return WINK_OK;
            }
            return WINK_ERR_BUSY;
        case DAL_ULTRASONIC_ERROR:
            return lstat;
        case DAL_ULTRASONIC_IDLE:
        default:
            return WINK_ERR_EMPTY;
    }
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out_distance_cm) {
    WINK_ASSERT_NONBLOCKING();
    if (dev == NULL || out_distance_cm == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    wink_status_t write_status = pal_gpio_write(dev->config.trig_pin, true);
    if (wink_status_is_error(write_status)) {
        return write_status;
    }
    pal_os_busy_wait_us(10);
    write_status = pal_gpio_write(dev->config.trig_pin, false);
    if (wink_status_is_error(write_status)) {
        return write_status;
    }

    uint32_t pulse_us = 0;
    wink_status_t status = pal_gpio_pulse_in(
        dev->config.echo_pin,
        true,
        ULTRASONIC_TIMEOUT_US,
        &pulse_us
    );
    if (wink_status_is_error(status)) {
        return status;
    }

    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *out_distance_cm = dev->last_distance;
    return WINK_OK;
}
#endif  /* WINK_STRICT_NONBLOCKING */
