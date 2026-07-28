#pragma once
#include "actuator/dal_rc_servo.h"
#include "pal_log.h"
#include <assert.h>

/**
 * Arduino-facing facade for hobby/RC PWM servos (SG90-class).
 * Aligns with DAL `dal_rc_servo` (ADR-0050). Industrial closed-loop
 * servos will use a separate facade (e.g. WinkIndustrialServo) — do not
 * overload this class.
 */
class WinkRcServo {
public:
    explicit WinkRcServo(dal_rc_servo_t& dal_dev) : _dev(dal_dev) {}
    WinkRcServo(const WinkRcServo&) = delete;

    void write(float angle) {
        if (!_dev.initialized) {
            static bool warned = false;
            if (!warned) {
                pal_log_w("WinkRcServo",
                          "RC servo not initialized or not bound in JSON!");
                warned = true;
            }
#if defined(WINK_SIM_STRICT) && WINK_SIM_STRICT
            assert(false && "WinkRcServo::write on unbound device");
#endif
            return;
        }
        (void)dal_rc_servo_set_angle(&_dev, angle);
    }

private:
    dal_rc_servo_t& _dev;
};
