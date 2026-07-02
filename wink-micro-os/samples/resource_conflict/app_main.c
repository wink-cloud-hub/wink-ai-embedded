/**
 * @file app_main.c
 * @brief Track A M1 / Task A-3：资源占用冲突反例样本（负向验证）。
 *
 * 目的：让 AI codegen 生成的**设备树错误**（两个逻辑设备抢同一 GPIO/PWM/UART/I2C
 * 资源）在 dal_*_init 阶段就被 pal_resource_claim 拦截，返回 WINK_ERR_BUSY，而
 * 不是等到真机上电时才因电气冲突损坏硬件。
 *
 * 与 samples/devkitc_smoke 的对比：
 *   devkitc_smoke = 正例（正确配置，硬件全链路 smoke）
 *   resource_conflict = 反例（故意错配，验证冲突治理生效）
 *
 * 覆盖的冲突类型（一次跑完全部 4 种资源）：
 *   1. GPIO 引脚冲突（两个 dal_led 抢同 pin）
 *   2. PWM 通道冲突（两个 dal_servo 抢同 channel）
 *   3. UART 端口冲突（两个 dal_gps 抢同 port）
 *   4. I2C 地址冲突（两个 dal_eeprom 抢同 (port, addr)）
 *
 * 运行结果：
 *   全部 4 组的第二个 dal_*_init 应返 WINK_ERR_BUSY。
 *   若任一组返回 WINK_OK，则接线红线失效 → 反例断言失败 → puts("SAMPLE FAIL") + exit 1。
 */
#include "dal_led.h"
#include "dal_servo.h"
#include "dal_gps.h"
#include "dal_eeprom.h"
#include "wink_status.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(expected, actual, msg) do {                                       \
    wink_status_t _e = (expected);                                                  \
    wink_status_t _a = (actual);                                                    \
    if (_e != _a) {                                                                 \
        printf("SAMPLE FAIL: " msg " (expected=%d, got=%d)\n", (int)_e, (int)_a);   \
        exit(1);                                                                    \
    }                                                                               \
} while (0)

static void case_gpio_pin_conflict(void)
{
    dal_led_t led_a = {0};
    dal_led_t led_b = {0};
    const dal_led_config_t cfg_a = { .owner = "status_led",  .pin = 2, .active_high = true };
    const dal_led_config_t cfg_b = { .owner = "warning_led", .pin = 2, .active_high = true };

    ASSERT_EQ(WINK_OK,        dal_led_init(&led_a, &cfg_a), "GPIO: first LED init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_led_init(&led_b, &cfg_b), "GPIO: second LED same pin should BUSY");
    puts("[resource_conflict] GPIO pin 2 conflict correctly rejected");
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
    puts("[resource_conflict] PWM channel 0 conflict correctly rejected");
}

static void case_uart_port_conflict(void)
{
    dal_gps_t gps_a = {0};
    dal_gps_t gps_b = {0};
    const dal_gps_config_t cfg_a = {
        .owner = "primary_gps",   .uart_port = 1,
        .baudrate = 9600, .rx_buffer_size = 256
    };
    const dal_gps_config_t cfg_b = {
        .owner = "secondary_gps", .uart_port = 1,
        .baudrate = 9600, .rx_buffer_size = 256
    };

    ASSERT_EQ(WINK_OK,        dal_gps_init(&gps_a, &cfg_a), "UART: first GPS init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_gps_init(&gps_b, &cfg_b), "UART: second GPS same port should BUSY");
    puts("[resource_conflict] UART port 1 conflict correctly rejected");
}

static void case_i2c_addr_conflict(void)
{
    dal_eeprom_t ee_a = {0};
    dal_eeprom_t ee_b = {0};
    const dal_eeprom_config_t cfg_a = {
        .owner = "config_eeprom",  .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    const dal_eeprom_config_t cfg_b = {
        .owner = "logging_eeprom", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };

    ASSERT_EQ(WINK_OK,       dal_eeprom_init(&ee_a, &cfg_a), "I2C: first EEPROM init");
    ASSERT_EQ(WINK_ERR_BUSY, dal_eeprom_init(&ee_b, &cfg_b), "I2C: second EEPROM same (port,addr) should BUSY");
    puts("[resource_conflict] I2C (port=0, addr=0x50) conflict correctly rejected");
}

int main(void)
{
    puts("=== resource_conflict sample: verifying pal_resource conflict wiring ===");

    case_gpio_pin_conflict();
    case_pwm_channel_conflict();
    case_uart_port_conflict();
    case_i2c_addr_conflict();

    puts("SAMPLE PASS: all 4 resource conflicts correctly rejected at dal_*_init");
    return 0;
}
