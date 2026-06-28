/**
 * @file device_tree.c
 * @brief 设备实例静态分配（零动态分配，§6.1 约束1）。
 *        真实 codegen 会据画布连线生成；此处手动演示。
 *
 * ADR-0008：静态 POD 实例 + Flash 配置动态覆写。device_tree_apply_flash_config()
 * 在 app_init 顶部从 pal_storage 读 blob 覆写字段；失败静默降级到下方编译期默认。
 */
#include "device_tree.h"
#include "wink_dev_config.h"
#include "pal_storage.h"

dal_ultrasonic_t front_radar = {
    .trig_pin = 4,
    .echo_pin = 5,
    .last_distance = 0.0f,
};

dal_servo_t neck_servo = {
    .pwm_channel = 0,
    .current_angle = 90.0f,
    .min_pulse_ms = 0.5f,
    .max_pulse_ms = 2.5f,
};

/* ADR-0008 覆写注册表：(device_id → 类型化 dev → apply_fn) 类型正确三元组。
 * 固件侧类型安全：apply_fn 与 dev 类型由本表绑定，codegen 须成对正确。 */
static const wink_dev_override_entry_t g_overrides[] = {
    { DEV_ID_NECK_SERVO,  &neck_servo,  dal_servo_apply_override      },
    { DEV_ID_FRONT_RADAR, &front_radar, dal_ultrasonic_apply_override },
};

wink_status_t device_tree_apply_flash_config(void) {
    uint8_t  buf[WINK_DEV_CONFIG_MAX_BYTES];
    uint16_t len = 0;

    wink_status_t r = pal_storage_read(WINK_DEV_CONFIG_KEY, buf, sizeof buf, &len);
    if (wink_status_is_error(r)) {
        return r;   /* EMPTY/UNSUPPORTED/IO → 静默降级到编译期默认 */
    }
    return wink_dev_config_apply(buf, len, g_overrides,
                                 (uint16_t)(sizeof(g_overrides) / sizeof(g_overrides[0])));
}
