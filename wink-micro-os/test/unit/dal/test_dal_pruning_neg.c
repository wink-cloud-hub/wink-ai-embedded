/*
 * test_dal_pruning_neg.c — compile-negative test case.
 *
 * This TU is expected to FAIL compilation with -DWINK_USE_SERVO=0.
 * It deliberately calls dal_servo_init() even though the driver is
 * disabled; the WINK_UNAVAILABLE_MSG stub in dal_servo.h should emit a
 * compile error whose message points the user at remediation
 * ("add a \"servo\" device to wink-app.json").
 *
 * It is compiled directly by test_dal_pruning_neg.cmake outside the
 * normal build (never linked into libdal or any host test binary).
 */
#include "dal_servo.h"

int main(void) {
    dal_servo_t dev;
    dal_servo_config_t cfg = {0};
    (void)dal_servo_init(&dev, &cfg);  /* expect: unavailable error here */
    return 0;
}
