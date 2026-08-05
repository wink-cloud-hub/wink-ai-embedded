// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_pruning_neg.c
 * @brief Compile-negative test case for DAL driver pruning.
 */
#include "dal_rc_servo.h"

int main(void) {
    dal_rc_servo_t dev;
    dal_rc_servo_config_t cfg = {0};
    (void)dal_rc_servo_init(&dev, &cfg);
    return 0;
}
