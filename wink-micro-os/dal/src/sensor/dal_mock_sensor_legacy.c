#include "dal_mock_sensor_legacy.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h>

wink_status_t dal_mock_sensor_legacy_init(dal_mock_sensor_legacy_t *dev, const dal_mock_sensor_legacy_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    /* TODO: claim resources (pal_resource_claim) and init PAL handles.
     * On failure, release already-claimed resources via goto err_release. */

    /* Example claim for gpio_pin — adjust resource kind as needed:
     * wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->gpio_pin, cfg->owner);
     * if (wink_status_is_error(rs)) { return rs; }
     */


    memcpy(&dev->config, cfg, sizeof(dal_mock_sensor_legacy_config_t));
    dev->initialized = true;
    dev->last_status = WINK_OK;
    return WINK_OK;

    /* err_release:
     *   WINK_IGNORE_UNUSED(pal_resource_release(...));
     *   return status;
     */
}

wink_status_t dal_mock_sensor_legacy_deinit(dal_mock_sensor_legacy_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    /* TODO: reset hardware + release claims */
    memset(dev, 0, sizeof(dal_mock_sensor_legacy_t));
    return WINK_OK;
}


