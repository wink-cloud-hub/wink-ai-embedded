#pragma once
#include "actuator/dal_servo.h"
#include "pal_log.h"
#include <assert.h>

class WinkServo {
public:
    explicit WinkServo(dal_servo_t& dal_dev) : _dev(dal_dev) {}
    WinkServo(const WinkServo&) = delete;

    void write(float angle) {
        if (!_dev.initialized) {
            static bool warned = false;
            if (!warned) {
                pal_log_w("WinkServo", "Servo device not initialized or not bound in JSON!");
                warned = true;
            }
            #if defined(WINK_SIM_STRICT) && WINK_SIM_STRICT
            assert(false && "Servo write on unbound device");
            #endif
            return;
        }
        (void)dal_servo_set_angle(&_dev, angle);
    }
private:
    dal_servo_t& _dev;
};
