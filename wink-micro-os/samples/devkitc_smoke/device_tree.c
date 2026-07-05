/**
 * @file device_tree.c
 * @brief DevKitC 冒烟测试设备树实现（静态实例 + init/deinit 序列）。
 *
 * 零动态分配。所有 config 字面量为 static const（存 Flash，不占 RAM）。
 * Actuator safe-off 注册在全部 init 成功后统一完成，避免半初始化设备泄漏进 registry。
 */
#define LOG_TAG "device_tree"

#include "device_tree.h"
#include "wink_fault.h"
#include "wink_actuator_registry.h"
#include "pal_log.h"

/* ── 引脚定义（私有；只有 sim 用的 SMOKE_TRIG/ECHO 暴露在头里，P1 codegen 后清理） ── */
#define BOARD_LED_PIN     2u
#define BOARD_BUTTON_PIN  0u

/* ── 1. 静态实例（零 malloc） ── */
dal_led_t         board_led   = {0};
dal_button_t      boot_button = {0};
dal_ultrasonic_t  smoke_sonar = {0};

/* ── 2. Actuator safe-off thunk ── */
WINK_DEFINE_ACTUATOR_THUNK(board_led_safe_off, dal_led_off, dal_led_t)

/* ── 3. Init 序列 ── */
wink_status_t wink_device_tree_init(void)
{
    /* board_led (actuator: 基础设施，先 init) */
    static const dal_led_config_t led_cfg = {
        .owner = "board_led",
        .pin = BOARD_LED_PIN,
        .active_high = true,
    };
    WINK_TRY(dal_led_init(&board_led, &led_cfg));

    /* boot_button (sensor) */
    static const dal_button_config_t btn_cfg = {
        .owner = "boot_button",
        .pin = BOARD_BUTTON_PIN,
        .active_low = true,
    };
    WINK_TRY(dal_button_init(&boot_button, &btn_cfg));
    WINK_TRY(dal_button_set_long_press_ms(&boot_button, 3000));
    WINK_TRY(dal_button_enable_isr_counter(&boot_button));

    /* smoke_sonar (sensor) */
    static const dal_ultrasonic_config_t sonar_cfg = {
        .owner = "smoke_sonar",
        .trig_pin = SMOKE_TRIG_PIN,
        .echo_pin = SMOKE_ECHO_PIN,
        .use_rmt = true,
    };
    WINK_TRY(dal_ultrasonic_init(&smoke_sonar, &sonar_cfg));

    /* ── 所有设备 init 成功后，注册 actuator safe-off ── */
    WINK_TRY(wink_actuator_register(board_led_safe_off, &board_led));

    return WINK_OK;
}

/* ── 4. Deinit 序列（init 反序） ── */
void wink_device_tree_deinit(void)
{
    /* 先反注册 actuator，再 deinit 设备（未注册返 NOT_FOUND，best-effort 忽略） */
    WINK_IGNORE_RESULT(wink_actuator_unregister(board_led_safe_off, &board_led));

    WINK_IGNORE_RESULT(dal_ultrasonic_deinit(&smoke_sonar));
    WINK_IGNORE_RESULT(dal_button_deinit(&boot_button));
    WINK_IGNORE_RESULT(dal_led_deinit(&board_led));
}
