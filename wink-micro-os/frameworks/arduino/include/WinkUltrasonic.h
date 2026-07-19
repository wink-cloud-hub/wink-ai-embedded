#pragma once
#include "dal_ultrasonic.h"
#include "pal_log.h"
#include <assert.h>

class WinkUltrasonic {
public:
    explicit WinkUltrasonic(dal_ultrasonic_t& dal_dev) : _dev(dal_dev) {}
    WinkUltrasonic(const WinkUltrasonic&) = delete;

    float read() {
        if (!_dev.initialized) {
            static bool warned = false;
            if (!warned) {
                pal_log_w("WinkUltrasonic", "Ultrasonic device not initialized or not bound in JSON!");
                warned = true;
            }
            #if defined(WINK_SIM_STRICT) && WINK_SIM_STRICT
            assert(false && "Ultrasonic read on unbound device");
            #endif
            return -1.0f;
        }
        float cm = -1.0f;
        // Fetch the last cached value
        (void)dal_ultrasonic_get_cached_distance(&_dev, &cm);
        // Request a new measurement for the next tick
        (void)dal_ultrasonic_request_measurement(&_dev);
        return cm;
    }
private:
    dal_ultrasonic_t& _dev;
};
