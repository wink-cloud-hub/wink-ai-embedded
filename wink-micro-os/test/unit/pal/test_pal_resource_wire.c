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
#include "dal_rc_servo.h"
#include "dal_ultrasonic.h"
#include "dal_gps.h"
#include "dal_eeprom.h"


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

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
 * 4. dal_rc_servo：同 PWM channel 冲突
 * ===================================================================== */
void test_wire_dal_rc_servo_same_channel_conflict(void) {
    dal_rc_servo_t a = {0};
    dal_rc_servo_t b = {0};
    const dal_rc_servo_config_t cfg_a = {
        .owner = "servo_a", .pwm_channel = 3,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };
    const dal_rc_servo_config_t cfg_b = {
        .owner = "servo_b", .pwm_channel = 3,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_rc_servo_init(&b, &cfg_b));
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
 * 7. UART port 资源冲突语义（通过 pal_resource 原语验证）
 *
 *    背景（P0 E5-part1）：dal_gps 当前是 @experimental Stub，init 返
 *    WINK_ERR_UNSUPPORTED，不 claim 资源（避免"假成功"反模式）。因此
 *    此处不再通过 dal_gps_init 路径验证 UART 冲突——改为直接验证
 *    pal_resource_claim(PAL_RESOURCE_UART_PORT, ...) 的冲突语义，作为
 *    未来 dal_gps 真实实现 wire pal_resource 时必须遵守的契约红线。
 *    对应真实实现一旦到位，可在本测试末尾追加 dal_gps_init 的 BUSY 路径覆盖。
 * ===================================================================== */
void test_wire_uart_port_resource_conflict_via_pal(void) {
    /* 同 port 异 owner → BUSY */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_b"));
    /* 同 owner 重入 → OK（幂等） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    /* 不同 port → OK */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 2, "gps_b"));

    /* 清理：release 后可再 claim */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 2, "gps_b"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_c"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "gps_c"));
}

/* =====================================================================
 * 8. I2C (port, addr) 资源冲突语义（通过 pal_resource 原语验证）
 *
 *    同 uart 逻辑：dal_eeprom 是 stub 返 NOT_SUPPORTED，不 claim 资源；
 *    此处直接验证 pal_resource_i2c_id + PAL_RESOURCE_I2C_ADDR 的契约。
 * ===================================================================== */
void test_wire_i2c_addr_resource_conflict_via_pal(void) {
    uint32_t id_0_50 = pal_resource_i2c_id(0, 0x50);
    uint32_t id_0_51 = pal_resource_i2c_id(0, 0x51);
    uint32_t id_1_50 = pal_resource_i2c_id(1, 0x50);

    /* 同 (port, addr) 异 owner → BUSY */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_a"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_b"));

    /* 同 port 不同 addr → OK（共享总线） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_51, "oled"));

    /* 不同 port 同 addr → OK（地址空间按 port 独立） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_1_50, "eeprom_c"));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_a"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_0_51, "oled"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_1_50, "eeprom_c"));
}

/* =====================================================================
 * 9. dal_gps/dal_eeprom stub 契约诚实性：
 *    合法参数下 init 必须返 WINK_ERR_UNSUPPORTED（非 WINK_OK 假成功），
 *    且 dev->initialized 必须保持 false，read/write/poll/get_position 也返 NOT_SUPPORTED。
 * ===================================================================== */
void test_wire_gps_stub_returns_not_supported(void) {
    /* Fresh (uninitialized) dev exercises the honest stub path: memset +
     * WINK_ERR_UNSUPPORTED, initialized stays false. A pre-initialized dev
     * instead returns WINK_ERR_ALREADY_INITIALIZED (DAL-L-004), which is a
     * different contract and not what this test targets. */
    dal_gps_t dev = {0};
    const dal_gps_config_t cfg = {
        .owner = "test_gps", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);

    /* poll/get_position 在未 init 的 dev 上也返 NOT_SUPPORTED（而非 NOT_INITIALIZED，
     * 因为 stub 根本不支持该功能，比"未初始化"更准确） */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_poll(&dev));
    dal_gps_position_t pos;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_get_position(&dev, &pos));
}

void test_wire_eeprom_stub_returns_not_supported(void) {
    /* Fresh (uninitialized) dev exercises the honest stub path; a pre-init dev
     * would instead hit DAL-L-004 (WINK_ERR_ALREADY_INITIALIZED). */
    dal_eeprom_t dev = {0};
    const dal_eeprom_config_t cfg = {
        .owner = "test_eeprom", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);

    /* DAL-F-020: read returns UNSUPPORTED and MUST leave the caller buffer
     * untouched (no 0xFF fill); a real backend writes buf only on WINK_OK. */
    uint8_t buf[4] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_read_blocking(&dev, 0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x44, buf[3]);

    const uint8_t wbuf[4] = {1,2,3,4};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_write_blocking(&dev, 0, wbuf, sizeof(wbuf)));
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

    dal_rc_servo_t servo = {0};
    const dal_rc_servo_config_t scfg = {
        .owner = NULL, .pwm_channel = 0, .min_pulse_us = 500, .max_pulse_us = 2500
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&servo, &scfg));

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
    RUN_TEST(test_wire_dal_rc_servo_same_channel_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_trig_pin_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_echo_conflict_rolls_back_trig);
    RUN_TEST(test_wire_uart_port_resource_conflict_via_pal);
    RUN_TEST(test_wire_i2c_addr_resource_conflict_via_pal);
    RUN_TEST(test_wire_gps_stub_returns_not_supported);
    RUN_TEST(test_wire_eeprom_stub_returns_not_supported);
    RUN_TEST(test_wire_all_dal_reject_null_owner);
    RUN_TEST(test_wire_release_then_reclaim_across_dal);
    return UNITY_END();
}
