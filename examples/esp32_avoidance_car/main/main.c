/**
 * @file main.c
 * @brief ESP-IDF 主入口 - 调用 Wink Micro OS 应用
 *
 * ESP32 avoidance_car 避障小车硬件接线：
 *
 * ┌─────────────────────────────────────────────────┐
 * │ HC-SR04 超声波                                   │
 * │   VCC  → 5V                                     │
 * │   GND  → GND                                    │
 * │   TRIG → GPIO 27                                │
 * │   ECHO → GPIO 26                                │
 * ├─────────────────────────────────────────────────┤
 * │ SG90 舵机 (9g)                                   │
 * │   VCC  → 5V                                     │
 * │   GND  → GND                                    │
 * │   PWM  → GPIO 2 (LEDC ch0)                      │
 * ├─────────────────────────────────────────────────┤
 * │ 按键 (可选)                                      │
 * │   一端 → GPIO 0 (BOOT 按键复用)                 │
 * │   另一端 → GND                                  │
 * └─────────────────────────────────────────────────┘
 *
 * 注：如果使用 L298N / TB6612 电机驱动：
 *   IN1 -> GPIO 16, IN2 -> GPIO 17
 *   IN3 -> GPIO 18, IN4 -> GPIO 19
 *   PWM_A -> GPIO 23, PWM_B -> GPIO 22
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pal.h"
#include "wink_runtime.h"

static const char *TAG = "wink_esp32";

/* ESP32 avoidance_car 引脚映射 */
#define PIN_ULTRASONIC_TRIG  27
#define PIN_ULTRASONIC_ECHO  26
#define PIN_SERVO_PWM         2
#define PIN_BUTTON            0

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Wink Micro OS - ESP32 avoidance_car");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Hardware pins:");
    ESP_LOGI(TAG, "  Ultrasonic TRIG: GPIO %d", PIN_ULTRASONIC_TRIG);
    ESP_LOGI(TAG, "  Ultrasonic ECHO: GPIO %d", PIN_ULTRASONIC_ECHO);
    ESP_LOGI(TAG, "  Servo PWM:       GPIO %d", PIN_SERVO_PWM);
    ESP_LOGI(TAG, "  Button:          GPIO %d", PIN_BUTTON);

    /* 初始化 PAL 层 */
    ESP_LOGI(TAG, "Initializing PAL ...");

    /* GPIO: 超声波 TRIG 输出 */
    wink_status_t rs = pal_gpio_init(PIN_ULTRASONIC_TRIG, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_gpio_init(TRIG) failed: %d", rs);
    }

    /* GPIO: 超声波 ECHO 输入 */
    rs = pal_gpio_init(PIN_ULTRASONIC_ECHO, PAL_GPIO_INPUT_PULLUP);
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_gpio_init(ECHO) failed: %d", rs);
    }

    /* PWM: 舵机 50Hz */
    rs = pal_pwm_init(0, 50);  /* channel 0, 50Hz for SG90 */
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_pwm_init(servo) failed: %d", rs);
    }

    /* 初始化看门狗 (Phase 5 Fail-Safe) */
    rs = pal_os_wdt_init(5000);  /* 5s timeout */
    if (wink_status_is_error(rs)) {
        ESP_LOGW(TAG, "pal_os_wdt_init not supported (skipping): %d", rs);
    }

    /* 打印复位原因 */
    pal_os_reset_reason_t rr = pal_os_get_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %d", rr);

    ESP_LOGI(TAG, "Starting application runtime ...");

    /* 启动 Wink 应用运行时 */
    extern void wink_app_main(void);
    wink_app_main();

    /* 正常情况下 wink_app_main 不会返回 */
    ESP_LOGE(TAG, "FATAL: wink_app_main returned unexpectedly");
    vTaskDelay(portMAX_DELAY);
}
