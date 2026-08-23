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

uint32_t pal_resource_max(pal_resource_type_t type) {
    switch (type) {
        case PAL_RESOURCE_SPI_BUS:
            return PAL_SPI_BUS_MAX;
        case PAL_RESOURCE_PCNT_UNIT:
            return PAL_PCNT_UNIT_MAX;
        case PAL_RESOURCE_PCNT_CHAN:
            return PAL_PCNT_CHAN_MAX;
        case PAL_RESOURCE_RMT_CHAN:
            return PAL_RMT_CHAN_MAX;
        case PAL_RESOURCE_HWTIMER:
            return PAL_HWTIMER_MAX;
        case PAL_RESOURCE_MCPWM_UNIT:
            return PAL_MCPWM_UNIT_MAX;
        case PAL_RESOURCE_MCPWM_TIMER:
            return PAL_MCPWM_TIMER_MAX;
        case PAL_RESOURCE_MCPWM_OPERATOR:
            return PAL_MCPWM_OPERATOR_MAX;
        case PAL_RESOURCE_MCPWM_COMPARATOR:
            return PAL_MCPWM_COMPARATOR_MAX;
        case PAL_RESOURCE_UART_PORT:
            return PAL_UART_PORT_MAX;
        case PAL_RESOURCE_ADC_CHANNEL:
            return PAL_ADC_CHANNEL_MAX;
        case PAL_RESOURCE_PWM_CHANNEL:
            return PAL_PWM_CHANNEL_MAX;
        case PAL_RESOURCE_GPIO_PIN:
            return PAL_GPIO_PIN_MAX;
        case PAL_RESOURCE_I2C_PORT:
            return 2u;
        case PAL_RESOURCE_I2C_ADDR:
        case PAL_RESOURCE_SPI_CS:
        case PAL_RESOURCE_MCPWM_SYNC_GPIO:
        case PAL_RESOURCE_GDMA_CHAN:
        default:
            return PAL_RESOURCE_UNLIMITED_MAX;
    }
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner) {
    if (owner == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    uint32_t max_id = pal_resource_max(type);
    if (max_id != PAL_RESOURCE_UNLIMITED_MAX && id >= max_id) {
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

    uint32_t max_id = pal_resource_max(type);
    if (max_id != PAL_RESOURCE_UNLIMITED_MAX && id >= max_id) {
        return WINK_ERR_INVALID_ARG;
    }

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
