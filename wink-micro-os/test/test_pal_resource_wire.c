/**
 * @file test_pal_resource_wire.c
 * @brief Track A M1 / Task A-4：DAL↔PAL 资源接线 e2e 单测。
 *
 * 与 test_pal_resource.c 的关系：
 *   test_pal_resource.c  测 pal_resource 静态表**本身**的语义（幂等 / 冲突 / 表满 /
 *                        release-then-reclaim / wrong-owner reject），是 PAL 层单元测试。
 *   test_pal_resource_wire.c（本文件） 测 6 个 DAL init **正确 wire** 了 pal_resource_claim
 *                        的接线契约（Track A R-1 红线），是 DAL↔PAL 集成测试。
 *
 * 覆盖：
 *   - 每个 DAL 类型至少 1 组冲突用例（BUSY 触发路径）
 *   - dal_ultrasonic 多资源 claim 的 rollback 正确性（trig 后 echo 失败 → trig 应被回滚）
 *   - 跨 DAL 冲突（dal_button vs dal_led 同 pin）
 *   - release-then-reclaim 跨 DAL 场景
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_resource.h"

#include "dal_button.h"
#include "dal_led.h"
#include "dal_servo.h"
#include "dal_ultrasonic.h"
#include "dal_gps.h"
#include "dal_eeprom.h"

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

/* =====================================================================
 * 1. dal_button：同 pin 冲突
 * ===================================================================== */
void test_wire_dal_button_same_pin_conflict(void) {
    dal_button_t a = {0};
    dal_button_t b = {0};
    const dal_button_config_t cfg_a = { .owner = "button_a", .pin = 12, .active_low = true };
    const dal_button_config_t cfg_b = { .owner = "button_b", .pin = 12, .active_low = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_button_init(&b, &cfg_b));
    /* b 未成功 → initialized 未置 true */
    TEST_ASSERT_FALSE(b.initialized);
}

/* =====================================================================
 * 2. dal_led：同 pin 冲突
 * ===================================================================== */
void test_wire_dal_led_same_pin_conflict(void) {
    dal_led_t a = {0};
    dal_led_t b = {0};
    const dal_led_config_t cfg_a = { .owner = "led_a", .pin = 5, .active_high = true };
    const dal_led_config_t cfg_b = { .owner = "led_b", .pin = 5, .active_high = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_led_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

/* =====================================================================
 * 3. 跨 DAL 冲突：dal_button 占 pin 后，dal_led 抢同 pin 应 BUSY
 *    （现实场景：codegen 误把 button 和 status LED 映射到同一引脚）
 * ===================================================================== */
void test_wire_cross_dal_button_vs_led_same_pin(void) {
    dal_button_t btn = {0};
    dal_led_t    led = {0};
    const dal_button_config_t cfg_btn = { .owner = "user_button",   .pin = 8, .active_low  = true };
    const dal_led_config_t    cfg_led = { .owner = "status_led",    .pin = 8, .active_high = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_led_init(&led, &cfg_led));
    TEST_ASSERT_FALSE(led.initialized);
}

/* =====================================================================
 * 4. dal_servo：同 PWM channel 冲突
 * ===================================================================== */
void test_wire_dal_servo_same_channel_conflict(void) {
    dal_servo_t a = {0};
    dal_servo_t b = {0};
    const dal_servo_config_t cfg_a = {
        .owner = "servo_a", .pwm_channel = 3,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };
    const dal_servo_config_t cfg_b = {
        .owner = "servo_b", .pwm_channel = 3,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_servo_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

/* =====================================================================
 * 5. dal_ultrasonic：trig 冲突（第一个 claim 就撞车，无 rollback 分支覆盖）
 * ===================================================================== */
void test_wire_dal_ultrasonic_trig_pin_conflict(void) {
    /* 先让 dal_button 占住 pin 20 */
    dal_button_t btn = {0};
    const dal_button_config_t cfg_btn = { .owner = "btn_squatter", .pin = 20, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));

    /* dal_ultrasonic 的 trig_pin 撞上 pin 20 → 第一次 claim (trig) 直接 BUSY，无 rollback */
    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t cfg_us = {
        .owner = "us_a", .trig_pin = 20, .echo_pin = 21, .use_rmt = false
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_ultrasonic_init(&us, &cfg_us));
    TEST_ASSERT_FALSE(us.initialized);
}

/* =====================================================================
 * 6. dal_ultrasonic：echo 冲突 → 必须触发 trig 的 rollback
 *
 *    验证策略（可观测 rollback）：
 *      步骤 A：让 dal_button 占住 pin 31（作为 echo 冲突源）
 *      步骤 B：dal_ultrasonic 配 trig=30, echo=31 → init 必失败
 *      步骤 C：如果 rollback 正确，pin 30 应仍空闲 —— 用另一 DAL 尝试占 30 应成功
 *              如果 rollback 漏了，pin 30 已被 us_a 占住 → 该次 claim 会 BUSY，测试断言失败
 * ===================================================================== */
void test_wire_dal_ultrasonic_echo_conflict_rolls_back_trig(void) {
    /* A：占 echo 冲突源 pin 31 */
    dal_button_t squatter = {0};
    const dal_button_config_t cfg_sq = { .owner = "echo_squatter", .pin = 31, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&squatter, &cfg_sq));

    /* B：ultrasonic trig=30 (空闲) + echo=31 (被占) → 应失败于第二次 claim，trig 被 rollback */
    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t cfg_us = {
        .owner = "us_rollback", .trig_pin = 30, .echo_pin = 31, .use_rmt = false
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_ultrasonic_init(&us, &cfg_us));
    TEST_ASSERT_FALSE(us.initialized);

    /* C：如果 trig(30) 被正确 rollback，则 dal_led 应能占 30 */
    dal_led_t probe_led = {0};
    const dal_led_config_t cfg_led = { .owner = "probe_led", .pin = 30, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&probe_led, &cfg_led));
    TEST_ASSERT_TRUE(probe_led.initialized);
}

/* =====================================================================
 * 7. dal_gps：同 uart_port 冲突
 * ===================================================================== */
void test_wire_dal_gps_same_uart_port_conflict(void) {
    dal_gps_t a = {0};
    dal_gps_t b = {0};
    const dal_gps_config_t cfg_a = {
        .owner = "gps_a", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };
    const dal_gps_config_t cfg_b = {
        .owner = "gps_b", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_gps_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

/* =====================================================================
 * 8. dal_eeprom：同 (port, addr) 冲突（异 owner）
 * ===================================================================== */
void test_wire_dal_eeprom_same_port_addr_conflict(void) {
    dal_eeprom_t a = {0};
    dal_eeprom_t b = {0};
    const dal_eeprom_config_t cfg_a = {
        .owner = "eeprom_a", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    const dal_eeprom_config_t cfg_b = {
        .owner = "eeprom_b", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_eeprom_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

/* =====================================================================
 * 9. dal_eeprom：同 port 不同 addr 应共享总线（不冲突，与 pal_resource.h §pal_resource_i2c_id 契约一致）
 * ===================================================================== */
void test_wire_dal_eeprom_same_port_different_addr_ok(void) {
    dal_eeprom_t a = {0};
    dal_eeprom_t b = {0};
    const dal_eeprom_config_t cfg_a = {
        .owner = "eeprom_a", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    const dal_eeprom_config_t cfg_b = {
        .owner = "eeprom_b", .i2c_port = 0, .i2c_addr = 0x51,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_init(&b, &cfg_b));
    TEST_ASSERT_TRUE(a.initialized);
    TEST_ASSERT_TRUE(b.initialized);
}

/* =====================================================================
 * 10. Owner NULL 契约：所有 DAL 都必须校验 cfg->owner != NULL
 * ===================================================================== */
void test_wire_all_dal_reject_null_owner(void) {
    dal_button_t btn = {0};
    const dal_button_config_t bcfg = { .owner = NULL, .pin = 4, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(&btn, &bcfg));

    dal_led_t led = {0};
    const dal_led_config_t lcfg = { .owner = NULL, .pin = 4, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_init(&led, &lcfg));

    dal_servo_t servo = {0};
    const dal_servo_config_t scfg = {
        .owner = NULL, .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_init(&servo, &scfg));

    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t ucfg = {
        .owner = NULL, .trig_pin = 4, .echo_pin = 5, .use_rmt = false
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(&us, &ucfg));

    dal_gps_t gps = {0};
    const dal_gps_config_t gcfg = {
        .owner = NULL, .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_init(&gps, &gcfg));

    dal_eeprom_t ee = {0};
    const dal_eeprom_config_t ecfg = {
        .owner = NULL, .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_eeprom_init(&ee, &ecfg));
}

/* =====================================================================
 * 11. Release-after-conflict-resolved：不同 owner 用 pal_resource_release 释放后可重 claim
 *     （模拟 codegen 修正设备树后重试）
 * ===================================================================== */
void test_wire_release_then_reclaim_across_dal(void) {
    dal_led_t led_a = {0};
    const dal_led_config_t cfg_a = { .owner = "led_a", .pin = 15, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&led_a, &cfg_a));

    /* 手动释放 led_a 的 claim（模拟未来 dal_led_deinit） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          pal_resource_release(PAL_RESOURCE_GPIO_PIN, 15, "led_a"));

    /* 释放后，另一 DAL 应能占用同一 pin */
    dal_button_t btn = {0};
    const dal_button_config_t cfg_btn = { .owner = "btn_after_release", .pin = 15, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));
    TEST_ASSERT_TRUE(btn.initialized);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wire_dal_button_same_pin_conflict);
    RUN_TEST(test_wire_dal_led_same_pin_conflict);
    RUN_TEST(test_wire_cross_dal_button_vs_led_same_pin);
    RUN_TEST(test_wire_dal_servo_same_channel_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_trig_pin_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_echo_conflict_rolls_back_trig);
    RUN_TEST(test_wire_dal_gps_same_uart_port_conflict);
    RUN_TEST(test_wire_dal_eeprom_same_port_addr_conflict);
    RUN_TEST(test_wire_dal_eeprom_same_port_different_addr_ok);
    RUN_TEST(test_wire_all_dal_reject_null_owner);
    RUN_TEST(test_wire_release_then_reclaim_across_dal);
    return UNITY_END();
}
