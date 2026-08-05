// SPDX-License-Identifier: Apache-2.0
#include "dal_gps.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "wink_pt_debug.h"
#include <string.h>

/* Non-blocking init (always compiled; STRICT-safe) */
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* @experimental Stub: UART backend + NMEA parser not yet implemented. */
    memset(dev, 0, sizeof(dal_gps_t));
    return WINK_ERR_UNSUPPORTED;
}

/* Blocking init variant (STRICT-guarded) */
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    WINK_ASSERT_NONBLOCKING();

    /* @experimental Stub */
    memset(dev, 0, sizeof(dal_gps_t));
    return WINK_ERR_UNSUPPORTED;
}
#endif /* WINK_STRICT_NONBLOCKING */

wink_status_t dal_gps_poll(dal_gps_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos) {
    if (dev == NULL || pos == NULL) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_gps_deinit(dal_gps_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    uint8_t uart_port = dev->config.uart_port;
    const char *owner = dev->config.owner;

    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_UART_PORT, uart_port, owner));

    memset(dev, 0, sizeof(dal_gps_t));

    return WINK_OK;
}
