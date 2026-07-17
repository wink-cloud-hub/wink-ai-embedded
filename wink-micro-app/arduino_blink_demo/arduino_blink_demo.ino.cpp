/*
 * Arduino Blink + Serial Demo
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 这是一份 100% 标准 Arduino 写法的 Sketch，验证 WinkMicroOS Arduino 兼容层的
 * GPIO / Timing / Serial / String 功能在 host E2E 环境下可正确编译和运行。
 *
 * 功能覆盖：
 *   - pinMode / digitalWrite      (GPIO 输出)
 *   - millis()                    (系统时钟)
 *   - delay()                     (延时)
 *   - Serial.begin / print / println  (串口格式化输出)
 *   - String 拼接                 (动态字符串)
 */
#include <Arduino.h>

#define LED_BUILTIN 2   // ESP32 DevKitC on-board LED pin

/*
 * WinkMicroOS host E2E: setup() / loop() are called from a C test harness
 * (test_arduino_blink_e2e.c), so we export them with C linkage.
 * On real Arduino hardware, the Arduino core's main.cpp would call them
 * directly (also via extern "C").
 */
extern "C" {

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("========================================");
    Serial.println("  WinkMicroOS Arduino Compat - Blink Demo");
    Serial.println("========================================");
    Serial.println();
}

void loop() {
    // LED ON
    digitalWrite(LED_BUILTIN, HIGH);
    String msg1 = String("LED ON   | uptime: ") + String(millis()) + String("ms");
    Serial.println(msg1);
    delay(500);

    // LED OFF
    digitalWrite(LED_BUILTIN, LOW);
    String msg2 = String("LED OFF  | uptime: ") + String(millis()) + String("ms");
    Serial.println(msg2);
    delay(500);
}

} // extern "C"
