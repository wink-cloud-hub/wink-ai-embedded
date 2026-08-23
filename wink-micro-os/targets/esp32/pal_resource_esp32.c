// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_resource_esp32.c
 * @brief ESP32 target hardware resource claim management implementation.
 */
#include "pal_resource.h"
#include "pal_spinlock.h"
#include <stddef.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

typedef struct {
    pal_resource_type_t type;
    uint32_t            id;
    const char         *owner;
} pal_resource_claim_t;

static pal_resource_claim_t s_claims[PAL_RESOURCE_MAX_CLAIMS];
static uint32_t s_count = 0;

static pal_spinlock_t s_resource_mux = PAL_SPINLOCK_INITIALIZER;

void pal_resource_reset(void) {
    pal_spinlock_lock(&s_resource_mux);
    s_count = 0;
    pal_spinlock_unlock(&s_resource_mux);
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_resource_mux);

    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                pal_spinlock_unlock(&s_resource_mux);
                return WINK_OK;
            }
            pal_spinlock_unlock(&s_resource_mux);
            return WINK_ERR_BUSY;
        }
    }

    if (s_count >= PAL_RESOURCE_MAX_CLAIMS) {
        pal_spinlock_unlock(&s_resource_mux);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    s_claims[s_count].type  = type;
    s_claims[s_count].id    = id;
    s_claims[s_count].owner = owner;
    s_count++;

    pal_spinlock_unlock(&s_resource_mux);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) { return WINK_ERR_INVALID_ARG; }

    pal_spinlock_lock(&s_resource_mux);

    wink_status_t result = WINK_ERR_INVALID_ARG;
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                s_count--;
                s_claims[i] = s_claims[s_count];
                result = WINK_OK;
            }
            break;
        }
    }

    pal_spinlock_unlock(&s_resource_mux);
    return result;
}

bool pal_resource_is_claimed(pal_resource_type_t type, uint32_t id) {
    pal_spinlock_lock(&s_resource_mux);
    bool claimed = false;
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            claimed = true;
            break;
        }
    }
    pal_spinlock_unlock(&s_resource_mux);
    return claimed;
}
