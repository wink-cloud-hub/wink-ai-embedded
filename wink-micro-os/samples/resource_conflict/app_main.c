/**
 * @file app_main.c
 * @brief Track A M1 / Task A-3：资源占用冲突反例样本（负向验证）。
 *
 * 目的：让 AI codegen 生成的**设备树错误**（两个逻辑设备抢同一 GPIO/PWM/UART/I2C
 * 资源）在 init 阶段就被 pal_resource_claim 拦截，返回 WINK_ERR_BUSY，而
 * 不是等到真机上电时才因电气冲突损坏硬件。
 *
 * 与 samples/devkitc_smoke 的对比：
 *   devkitc_smoke = 正例（正确配置，硬件全链路 smoke）
 *   resource_conflict = 反例（故意错配，验证冲突治理生效 + stub 契约诚实）
 *
 * 覆盖的冲突类型：
 *   1. GPIO 引脚冲突（两个 dal_led 抢同 pin）
 *   2. PWM 通道冲突（两个 dal_servo 抢同 channel）
 *   3. UART 端口冲突（通过 pal_resource 原语演示 — dal_gps 是 @experimental stub
 *      真实后端未实现，init 返 NOT_SUPPORTED 不 claim 资源，避免"假成功"反模式）
 *   4. I2C 地址冲突（通过 pal_resource 原语演示 — dal_eeprom 同理）
 *   5. 未实现 DAL 的契约诚实（dal_gps/dal_eeprom 合法参数下必须返 NOT_SUPPORTED，
 *      而不是 WINK_OK 假成功）
 *
 * 运行结果：
 *   全部 case 通过 → SAMPLE PASS。任一组断言失败 → LOG_E("SAMPLE FAIL") + exit 1。
 */
#define LOG_TAG "resource_conflict"

#include "dal_led.h"
#include "dal_servo.h"
#include "dal_gps.h"
#include "dal_eeprom.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "wink_status.h"
#include "wink_blocking_region.h"

/* stdlib.h: exit(). This sample is host-only by design (skipped on wasm and
 * ESP32 in CMakeLists.txt), so a direct stdlib dep is acceptable — no
 * cross-target portability concern. All formatted output routes through
 * LOG_*() to match the project-standard log channel. */
#include <stdlib.h>


#define ASSERT_EQ(expected, actual, msg) do {                                                  \
    wink_status_t _e = (expected);                                                             \
    wink_status_t _a = (actual);                                                               \
    if (_e != _a) {                                                                            \
        LOG_E("SAMPLE FAIL: " msg " (expected=%d, got=%d)", (int)_e, (int)_a);                 \
        exit(1);                                                                               \
    }                                                                                          \
} while (0)

static void case_gpio_pin_conflict(void)
{
    dal_led_t led_a = {0};
    dal_led_t led_b = {0};
    const dal_led_config_t cfg_a = { .owner = "status_led",  .pin = 2, .active_high = true };
    const dal_led_config_t cfg_b = { .owner = "warning_led", .pin = 2, .active_high = true };

    ASSERT_EQ(WINK_OK,        dal_led_init(&led_a, &cfg_a), "GPIO: first LED init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_led_init(&led_b, &cfg_b), "GPIO: second LED same pin should BUSY");
    LOG_I("GPIO pin 2 conflict correctly rejected");
}

static void case_pwm_channel_conflict(void)
{
    dal_servo_t servo_a = {0};
    dal_servo_t servo_b = {0};
    const dal_servo_config_t cfg_a = {
        .owner = "steering", .pwm_channel = 0,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };
    const dal_servo_config_t cfg_b = {
        .owner = "gripper",  .pwm_channel = 0,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f
    };

    ASSERT_EQ(WINK_OK,        dal_servo_init(&servo_a, &cfg_a), "PWM: first servo init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_servo_init(&servo_b, &cfg_b), "PWM: second servo same channel should BUSY");
    LOG_I("PWM channel 0 conflict correctly rejected");
}

/* UART/I2C 冲突通过 pal_resource 原语直接验证（dal_gps/dal_eeprom 当前是 stub
 * 不 claim 资源；真实后端到达后可改回 DAL 层的端到端验证）。 */
static void case_uart_port_conflict(void)
{
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "primary_gps"),
        "UART: first claim port 1");
    ASSERT_EQ(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "secondary_gps"),
        "UART: second claim same port should BUSY");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "primary_gps"),
        "UART: release port 1");
    LOG_I("UART port 1 conflict correctly rejected via pal_resource");
}

static void case_i2c_addr_conflict(void)
{
    uint32_t id = pal_resource_i2c_id(0, 0x50);
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "config_eeprom"),
        "I2C: first claim (port=0,addr=0x50)");
    ASSERT_EQ(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "logging_eeprom"),
        "I2C: second claim same (port,addr) should BUSY");
    /* 同 port 不同 addr 应 OK（共享总线） */
    uint32_t id2 = pal_resource_i2c_id(0, 0x51);
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id2, "oled_display"),
        "I2C: same port different addr should OK");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id, "config_eeprom"),
        "I2C: release eeprom");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id2, "oled_display"),
        "I2C: release oled");
    LOG_I("I2C (port=0, addr=0x50) conflict correctly rejected via pal_resource");
}

/* 契约诚实验证：未实现的 DAL 在合法参数下必须返 NOT_SUPPORTED，不得 WINK_OK 假成功。
 * 这是 ADR-0012 "契约诚实优先于静默降级" 的活文档样本。 */
static void case_stub_honesty(void)
{
    /* ADR-0017 init-phase exception: stub-honesty test calls WINK_BLOCKING
     * APIs (dal_gps_init, dal_eeprom_init) to verify they return
     * NOT_SUPPORTED, not fake WINK_OK. */
    WINK_INIT_BLOCKING_REGION_BEGIN
    dal_gps_t gps = {0};
    const dal_gps_config_t gps_cfg = {
        .owner = "test_gps", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };
    ASSERT_EQ(WINK_ERR_UNSUPPORTED, dal_gps_init(&gps, &gps_cfg),
              "Stub honesty: dal_gps_init must return NOT_SUPPORTED (no fake success)");

    dal_eeprom_t ee = {0};
    const dal_eeprom_config_t ee_cfg = {
        .owner = "test_eeprom", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    ASSERT_EQ(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&ee, &ee_cfg),
              "Stub honesty: dal_eeprom_init must return NOT_SUPPORTED (no fake success)");
    WINK_INIT_BLOCKING_REGION_END
    LOG_I("stub honesty (ADR-0012): unimplemented DALs return NOT_SUPPORTED, not fake WINK_OK");
}

int main(void)
{
    LOG_I("=== resource_conflict sample: verifying pal_resource conflict wiring + stub honesty ===");

    case_gpio_pin_conflict();
    case_pwm_channel_conflict();
    case_uart_port_conflict();
    case_i2c_addr_conflict();
    case_stub_honesty();

    LOG_I("SAMPLE PASS: all resource conflict checks and stub-honesty checks passed");
    return 0;
}
