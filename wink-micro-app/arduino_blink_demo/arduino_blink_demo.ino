/*
 * Arduino Blink + Serial Demo
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Standard Arduino Sketch verifying the WinkMicroOS Arduino Compatibility Layer
 * (GPIO / Timing / Serial / String features under host E2E environment).
 */
#include <Arduino.h>

#define LED_BUILTIN 2   // ESP32 DevKitC on-board LED pin

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
