// SPDX-License-Identifier: Apache-2.0
/**
 * @file main.c
 * @brief ESP-IDF Main Entry Point — invokes Wink Micro OS application.
 *
 * ESP32 avoidance_car hardware wiring:
 *
 * +-------------------------------------------------+
 * | HC-SR04 Ultrasonic                              |
 * |   VCC  -> 5V                                    |
 * |   GND  -> GND                                   |
 * |   TRIG -> GPIO 27                               |
 * |   ECHO -> GPIO 26                               |
 * +-------------------------------------------------+
 * | SG90 Servo (9g)                                 |
 * |   VCC  -> 5V                                    |
 * |   GND  -> GND                                   |
 * |   PWM  -> GPIO 2 (LEDC ch0)                     |
 * +-------------------------------------------------+
 * | Button (Optional)                               |
 * |   Pin  -> GPIO 0 (BOOT button reuse)            |
 * |   GND  -> GND                                   |
 * +-------------------------------------------------+
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pal.h"
#include "wink_runtime.h"

static const char *TAG = "wink_esp32";

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

    ESP_LOGI(TAG, "Initializing PAL ...");

    /* GPIO: Ultrasonic TRIG output */
    wink_status_t rs = pal_gpio_init(PIN_ULTRASONIC_TRIG, PAL_GPIO_OUTPUT_PUSH_PULL);
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_gpio_init(TRIG) failed: %d", rs);
    }

    /* GPIO: Ultrasonic ECHO input */
    rs = pal_gpio_init(PIN_ULTRASONIC_ECHO, PAL_GPIO_INPUT_PULLUP);
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_gpio_init(ECHO) failed: %d", rs);
    }

    /* PWM: Servo 50Hz */
    rs = pal_pwm_init(0, 50);
    if (wink_status_is_error(rs)) {
        ESP_LOGE(TAG, "pal_pwm_init(servo) failed: %d", rs);
    }

    /* Watchdog initialization (Phase 5 Fail-Safe) */
    rs = pal_os_wdt_init(5000);
    if (wink_status_is_error(rs)) {
        ESP_LOGW(TAG, "pal_os_wdt_init not supported (skipping): %d", rs);
    }

    pal_os_reset_reason_t rr = pal_os_get_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %d", rr);

    ESP_LOGI(TAG, "Starting application runtime ...");

    extern void wink_app_main(void);
    wink_app_main();

    ESP_LOGE(TAG, "FATAL: wink_app_main returned unexpectedly");
    vTaskDelay(portMAX_DELAY);
}
