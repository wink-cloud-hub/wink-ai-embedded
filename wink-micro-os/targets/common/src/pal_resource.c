// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_resource.c
 * @brief Common resource claim management for host and wasm simulator targets.
 */
#include "pal_resource.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    pal_resource_type_t type;
    uint32_t            id;
    const char         *owner;
} pal_resource_claim_t;

static pal_resource_claim_t s_claims[PAL_RESOURCE_MAX_CLAIMS];
static uint32_t s_count = 0;

void pal_resource_reset(void) {
    s_count = 0;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) == 0) {
                return WINK_OK;
            }
            return WINK_ERR_BUSY;
        }
    }
    if (s_count >= PAL_RESOURCE_MAX_CLAIMS) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }
    s_claims[s_count].type  = type;
    s_claims[s_count].id    = id;
    s_claims[s_count].owner = owner;
    s_count++;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) { return WINK_ERR_INVALID_ARG; }

    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            if (strcmp(s_claims[i].owner, owner) != 0) {
                return WINK_ERR_INVALID_ARG;
            }
            s_count--;
            s_claims[i] = s_claims[s_count];
            return WINK_OK;
        }
    }
    return WINK_ERR_INVALID_ARG;
}

bool pal_resource_is_claimed(pal_resource_type_t type, uint32_t id) {
    for (uint32_t i = 0; i < s_count; i++) {
        if (s_claims[i].type == type && s_claims[i].id == id) {
            return true;
        }
    }
    return false;
}
