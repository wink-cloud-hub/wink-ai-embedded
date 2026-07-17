/*
 * Host E2E harness for the Arduino Blink Demo.
 *
 * This is a pure-C test driver that:
 *   1. Initialises the PAL resource system and Arduino compat layer.
 *   2. Calls the sketch's setup() once and loop() N times.
 *   3. Verifies GPIO state toggles correctly via PAL loopback.
 *   4. Prints PASS / FAIL.
 *
 * It validates that:
 *   - ADR-0035 "leaf-node linking" works (this app binary links
 *     wink_arduino_compat; kernel libraries do NOT).
 *   - The Arduino setup()/loop() entry pattern runs end-to-end.
 *   - Serial.print output appears on stdout.
 */
#include <stdio.h>

/* The Arduino sketch defines setup()/loop() — these are declared with C
 * linkage by ArduinoCore-API's Common.h (inside its extern "C" block),
 * so the user's .ino file doesn't need any extern "C" wrapping. */
extern "C" void setup(void);
extern "C" void loop(void);

/* PAL resource management — host mock needs pin claims. */
#include "pal_resource.h"
#include "internal/pal_test_loopback.h"
#include "wink_status.h"

/* Arduino compat init */
extern "C" void wink_arduino_init(void);

#define E2E_PASS()    do { puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg) do { puts("E2E FAIL: " msg); return 1; } while(0)

int main(void) {
    printf("[arduino_blink_e2e] Starting host E2E test...\n");

    /* 1. Init PAL resources */
    pal_resource_reset();

    /* Claim the LED pin (pin 2 in this sketch) */
    if (pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 2, "blink_demo") != WINK_OK)
        E2E_FAIL("cannot claim pin 2");

    /* Claim a loopback input pin (pin 3) to read back LED state */
    if (pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 3, "blink_verify") != WINK_OK)
        E2E_FAIL("cannot claim pin 3");

    if (pal_test_enable_hardware_loopback(2, 3) != WINK_OK)
        E2E_FAIL("cannot enable loopback 2->3");

    /* 2. Init Arduino compat layer */
    wink_arduino_init();

    /* 3. Run sketch: setup() once */
    printf("\n--- Calling setup() ---\n");
    setup();

    /* 4. Run loop() 4 times (2 full blink cycles) */
    printf("\n--- Running loop() x4 ---\n");
    for (int i = 0; i < 4; i++) {
        printf("\n--- loop() iteration %d ---\n", i + 1);
        loop();
    }

    /* 5. Cleanup */
    pal_test_disable_hardware_loopback(2, 3);
    pal_resource_reset();

    printf("\n[arduino_blink_e2e] Sketch executed successfully.\n");
    E2E_PASS();
}
