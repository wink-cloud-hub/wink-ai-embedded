#ifndef DAL_MOCK_SENSOR_H
#define DAL_MOCK_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MockSensor config (codegen-friendly POD; ADR-0004).
 * Fill pin/bus fields as needed for this peripheral.
 */
typedef struct {
    const char *owner;

    uint16_t gpio_pin;

} dal_mock_sensor_config_t;

typedef struct {
    dal_mock_sensor_config_t config;
    bool initialized;
    volatile wink_status_t last_status;
} dal_mock_sensor_t;

/* DAL-S-014: Static assertion for config offset at 0 */
_Static_assert(offsetof(dal_mock_sensor_t, config) == 0,
               "config must be the first member");

WINK_WARN_UNUSED_RESULT
wink_status_t dal_mock_sensor_init(dal_mock_sensor_t *dev, const dal_mock_sensor_config_t *cfg);

wink_status_t dal_mock_sensor_deinit(dal_mock_sensor_t *dev);



#ifdef __cplusplus
}
#endif

#if !defined(WINK_USE_MOCK_SENSOR) || !WINK_USE_MOCK_SENSOR
#define WINK_MOCK_SENSOR_DISABLED_MSG \
    "MockSensor driver not enabled; add a \"mock_sensor\" device to wink-app.json " \
    "(or set -DWINK_USE_MOCK_SENSOR=ON)."
WINK_UNAVAILABLE_MSG(WINK_MOCK_SENSOR_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_mock_sensor_init(dal_mock_sensor_t *dev, const dal_mock_sensor_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_MOCK_SENSOR_DISABLED_MSG)
wink_status_t dal_mock_sensor_deinit(dal_mock_sensor_t *dev);

#endif /* !WINK_USE_MOCK_SENSOR */

#endif /* DAL_MOCK_SENSOR_H */
