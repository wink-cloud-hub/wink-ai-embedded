// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_pcnt_host.c
 * @brief Host first-class target PAL PCNT pulse counter implementation with injection stub.
 */
#include "hal/pal_pcnt.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_pcnt_stub.h"
#include <string.h>

struct pal_pcnt_unit_s {
    bool              in_use;
    uint8_t           id;
    pal_pcnt_config_t cfg;
    int64_t           count;
    uint32_t          filter_ns;
    wink_status_t     forced_err;
};

static struct pal_pcnt_unit_s s_pcnt_units[PAL_PCNT_UNIT_MAX];
static pal_spinlock_t s_pcnt_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Control Hooks --- */

void stub_pcnt_set_count(pal_pcnt_unit_handle_t handle, int64_t count) {
    if (handle == NULL) {
        return;
    }
    pal_spinlock_lock(&s_pcnt_lock);
    handle->count = count;
    pal_spinlock_unlock(&s_pcnt_lock);
}

void stub_pcnt_step(pal_pcnt_unit_handle_t handle, int64_t delta) {
    if (handle == NULL) {
        return;
    }
    pal_spinlock_lock(&s_pcnt_lock);
    handle->count += delta;
    pal_spinlock_unlock(&s_pcnt_lock);
}

void stub_pcnt_force_failure(pal_pcnt_unit_handle_t handle, wink_status_t err) {
    if (handle == NULL) {
        return;
    }
    pal_spinlock_lock(&s_pcnt_lock);
    handle->forced_err = err;
    pal_spinlock_unlock(&s_pcnt_lock);
}

/* --- PAL PCNT Public API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pcnt_init(const pal_pcnt_config_t *cfg,
                            pal_pcnt_unit_handle_t *out_handle) {
    if (cfg == NULL || out_handle == NULL || cfg->pin_a < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);

    struct pal_pcnt_unit_s *slot = NULL;
    for (uint8_t i = 0; i < PAL_PCNT_UNIT_MAX; i++) {
        if (!s_pcnt_units[i].in_use) {
            slot = &s_pcnt_units[i];
            slot->id = i;
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Claim PCNT hardware unit */
    wink_status_t st = pal_resource_claim(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return st;
    }

    /* Claim pin A */
    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_host");
    if (st != WINK_OK) {
        pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_host");
        pal_spinlock_unlock(&s_pcnt_lock);
        return st;
    }

    /* Claim pin B if configured */
    if (cfg->pin_b >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_b, "pal_pcnt_host");
        if (st != WINK_OK) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_a, "pal_pcnt_host");
            pal_resource_release(PAL_RESOURCE_PCNT_UNIT, slot->id, "pal_pcnt_host");
            pal_spinlock_unlock(&s_pcnt_lock);
            return st;
        }
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->count = 0;
    slot->filter_ns = cfg->filter_ns;
    slot->forced_err = WINK_OK;

    *out_handle = slot;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_deinit(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use || handle->id >= PAL_PCNT_UNIT_MAX) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_ARG;
    }

    if (handle->cfg.pin_b >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)handle->cfg.pin_b, "pal_pcnt_host");
    }
    pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)handle->cfg.pin_a, "pal_pcnt_host");
    pal_resource_release(PAL_RESOURCE_PCNT_UNIT, handle->id, "pal_pcnt_host");

    handle->in_use = false;
    handle->count = 0;

    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t handle, int64_t *count_out) {
    if (handle == NULL || count_out == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (handle->forced_err != WINK_OK) {
        wink_status_t err = handle->forced_err;
        handle->forced_err = WINK_OK;
        pal_spinlock_unlock(&s_pcnt_lock);
        return err;
    }

    *count_out = handle->count;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_clear(pal_pcnt_unit_handle_t handle) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    handle->count = 0;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}

wink_status_t pal_pcnt_set_glitch_filter(pal_pcnt_unit_handle_t handle, uint32_t filter_ns) {
    if (handle == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_pcnt_lock);
    if (!handle->in_use) {
        pal_spinlock_unlock(&s_pcnt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    handle->filter_ns = filter_ns;
    pal_spinlock_unlock(&s_pcnt_lock);
    return WINK_OK;
}
