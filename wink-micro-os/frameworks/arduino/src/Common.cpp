#include "Arduino.h"
#include <stdlib.h>
#include "pal_resource.h"
#include "pal_log.h"
#include <assert.h>

// Default weak definition of the board pin mapping.
// Individual boards (BSPs) can override these symbols to define custom pin mappings.
#if defined(_MSC_VER)
extern const wink_pin_t arduino_pin_map[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
};
extern const size_t arduino_pin_map_size = 64;
#else
__attribute__((weak)) extern const wink_pin_t arduino_pin_map[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
};
__attribute__((weak)) extern const size_t arduino_pin_map_size = 64;
#endif

static inline wink_pin_t resolve_pin(pin_size_t pinNumber) {
    if (pinNumber >= arduino_pin_map_size) {
        return -1;
    }
    return arduino_pin_map[pinNumber];
}

extern "C" {

static char* utoa_helper(unsigned long value, char *string, int radix) {
    if (radix < 2 || radix > 36) {
        *string = '\0';
        return string;
    }
    char tmp[65];
    int pos = 0;
    do {
        unsigned long rem = value % radix;
        tmp[pos++] = (rem < 10) ? (char)('0' + rem) : (char)('a' + (rem - 10));
        value /= radix;
    } while (value > 0);

    char *p = string;
    while (pos > 0) {
        *p++ = tmp[--pos];
    }
    *p = '\0';
    return string;
}

char* utoa(unsigned value, char *string, int radix) {
    return utoa_helper(value, string, radix);
}

char* ultoa(unsigned long value, char *string, int radix) {
    return utoa_helper(value, string, radix);
}

char* itoa(int value, char *string, int radix) {
    if (radix == 10 && value < 0) {
        *string = '-';
        utoa_helper((unsigned long)(-value), string + 1, radix);
        return string;
    }
    return utoa_helper((unsigned long)value, string, radix);
}

char* ltoa(long value, char *string, int radix) {
    if (radix == 10 && value < 0) {
        *string = '-';
        utoa_helper((unsigned long)(-value), string + 1, radix);
        return string;
    }
    return utoa_helper((unsigned long)value, string, radix);
}

char* dtostrf(double val, signed char width, unsigned char prec, char *s) {
    char format[16];
    sprintf(format, "%%%d.%df", width, prec);
    sprintf(s, format, val);
    return s;
}

void pinMode(pin_size_t pinNumber, PinMode mode) {
    wink_pin_t p = resolve_pin(pinNumber);
    if (p < 0) return;

    // R2 / R3 Conflict Prevention & Governance (ADR-0040)
    wink_status_t claim_rc = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, p, "arduino_compat");
    if (claim_rc == WINK_ERR_BUSY) {
        pal_log_w("arduino_compat", "pinMode: pin %d is owned by another device!", p);
        #if defined(WINK_SIM_STRICT) && WINK_SIM_STRICT
        assert(false && "pinMode late hijack conflict!");
        #endif
        return; // Fail-loud: block pinMode configure
    }

    pal_gpio_mode_t pal_mode;
    switch (mode) {
        case INPUT:            pal_mode = PAL_GPIO_INPUT; break;
        case OUTPUT:           pal_mode = PAL_GPIO_OUTPUT_PUSH_PULL; break;
        case INPUT_PULLUP:     pal_mode = PAL_GPIO_INPUT_PULLUP; break;
        case INPUT_PULLDOWN:   pal_mode = PAL_GPIO_INPUT_PULLDOWN; break;
        case OUTPUT_OPENDRAIN: pal_mode = PAL_GPIO_OUTPUT_OPEN_DRAIN; break;
        default: return;
    }
    wink_status_t rc = pal_gpio_init(p, pal_mode);
    (void)rc; // Silence warn_unused_result
}

void digitalWrite(pin_size_t pinNumber, PinStatus status) {
    wink_pin_t p = resolve_pin(pinNumber);
    if (p < 0) return;
    wink_status_t rc = pal_gpio_write(p, status == HIGH);
    (void)rc; // Silence warn_unused_result
}

PinStatus digitalRead(pin_size_t pinNumber) {
    wink_pin_t p = resolve_pin(pinNumber);
    if (p < 0) return LOW;
    bool level = false;
    if (pal_gpio_read(p, &level) == WINK_OK) {
        return level ? HIGH : LOW;
    }
    return LOW;
}

unsigned long millis(void) {
    return (unsigned long)pal_os_get_ms();
}

unsigned long micros(void) {
    return (unsigned long)pal_os_get_us();
}

// Temporarily ignore deprecated-declarations for pal_os_sleep_ms within the adapter layer
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifndef WINK_STRICT_NONBLOCKING
void delay(unsigned long ms) {
    pal_os_sleep_ms(ms);
}

void yield(void) {
    pal_os_sleep_ms(0);
}
#else
void delay(unsigned long ms) {
    pal_os_busy_wait_us(ms * 1000);
}

void yield(void) {
    // No-op
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

void delayMicroseconds(unsigned int us) {
    pal_os_busy_wait_us(us);
}

#include "wink_runtime.h"

extern void setup(void);
extern void loop(void);

static void arduino_app_init(void) {
    wink_arduino_init();
    setup();
}

static void arduino_app_loop(void) {
    loop();
}

#if defined(_MSC_VER)
const wink_app_callbacks_t * __declspec(selectany) wink_app_get_callbacks(void)
#else
__attribute__((weak)) const wink_app_callbacks_t *wink_app_get_callbacks(void)
#endif
{
    static const wink_app_callbacks_t s_arduino_callbacks = {
        arduino_app_init,
        arduino_app_loop,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    };
    return &s_arduino_callbacks;
}

void wink_arduino_init(void) {
    // Compatibility layer initialization if needed
}

unsigned long pulseIn(pin_size_t pin, uint8_t state, unsigned long timeout) {
#if defined(SIMULATION) && SIMULATION
    static bool warned = false;
    if (!warned) {
        pal_log_w("arduino_compat", "pulseIn is unsupported in simulation! Please use WinkUltrasonic semantic facade.");
        warned = true;
    }
    #if defined(WINK_SIM_STRICT) && WINK_SIM_STRICT
    assert(false && "pulseIn called under STRICT simulation mode!");
    #endif
    return 0;
#else
    wink_pin_t p = resolve_pin(pin);
    if (p < 0) return 0;
    
    uint32_t start_us = pal_os_get_us();
    uint32_t timeout_us = timeout;
    
    bool val = (state == HIGH);
    bool current = false;
    
    // 1. Wait for pin to be in opposite state
    while (1) {
        if (pal_gpio_read(p, &current) != WINK_OK) return 0;
        if (current != val) break;
        if ((pal_os_get_us() - start_us) > timeout_us) return 0;
    }
    
    // 2. Wait for transition to state
    uint32_t transition_start_us = pal_os_get_us();
    while (1) {
        if (pal_gpio_read(p, &current) != WINK_OK) return 0;
        if (current == val) break;
        if ((pal_os_get_us() - transition_start_us) > timeout_us) return 0;
    }
    
    // 3. Measure duration in state
    uint32_t pulse_start_us = pal_os_get_us();
    while (1) {
        if (pal_gpio_read(p, &current) != WINK_OK) return 0;
        if (current != val) {
            return pal_os_get_us() - pulse_start_us;
        }
        if ((pal_os_get_us() - pulse_start_us) > timeout_us) return 0;
    }
#endif
}

unsigned long pulseInLong(pin_size_t pin, uint8_t state, unsigned long timeout) {
    return pulseIn(pin, state, timeout);
}

} // extern "C"
