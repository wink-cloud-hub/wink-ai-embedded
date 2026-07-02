#include "device_tree.h"
#include "wink_dev_config.h"
#include "pal_storage.h"

dal_ultrasonic_t front_radar = {
    .config.owner    = "front_radar",
    .config.trig_pin = 4,
    .config.echo_pin = 5,
    .config.use_rmt = false,
    .last_distance = 0.0f,
};

dal_servo_t neck_servo = {
    .pwm_channel = 0,
    .current_angle = 90.0f,
    .min_pulse_ms = 0.5f,
    .max_pulse_ms = 2.5f,
};

static const wink_dev_override_entry_t g_overrides[] = {
    { DEV_ID_NECK_SERVO,  &neck_servo,  dal_servo_apply_override      },
    { DEV_ID_FRONT_RADAR, &front_radar, dal_ultrasonic_apply_override },
};

wink_status_t device_tree_apply_flash_config(void) {
    uint8_t  buf[WINK_DEV_CONFIG_MAX_BYTES];
    uint16_t len = 0;

    wink_status_t r = pal_storage_read(WINK_DEV_CONFIG_KEY, buf, sizeof buf, &len);
    if (wink_status_is_error(r)) {
        return r;
    }
    return wink_dev_config_apply(buf, len, g_overrides,
                                 (uint16_t)(sizeof(g_overrides) / sizeof(g_overrides[0])));
}
