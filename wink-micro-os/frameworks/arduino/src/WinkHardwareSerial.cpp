#include "WinkHardwareSerial.h"
#include <stdio.h>

WinkHardwareSerial::WinkHardwareSerial() {}

void WinkHardwareSerial::begin(unsigned long baudrate) {
    (void)baudrate;
}

void WinkHardwareSerial::begin(unsigned long baudrate, uint16_t config) {
    (void)baudrate;
    (void)config;
}

void WinkHardwareSerial::end() {
    rx_buffer.clear();
}

int WinkHardwareSerial::available(void) {
    return rx_buffer.available();
}

int WinkHardwareSerial::peek(void) {
    return rx_buffer.peek();
}

int WinkHardwareSerial::read(void) {
    return rx_buffer.read_char();
}

void WinkHardwareSerial::flush(void) {
    fflush(stdout);
}

size_t WinkHardwareSerial::write(uint8_t c) {
    putchar(c);
    return 1;
}

WinkHardwareSerial::operator bool() {
    return true;
}

void WinkHardwareSerial::injectInput(const char* str) {
    if (str == NULL) return;
    while (*str) {
        rx_buffer.store_char((uint8_t)*str);
        str++;
    }
}

void WinkHardwareSerial::injectInputChar(uint8_t c) {
    rx_buffer.store_char(c);
}

// Define the global instance
WinkHardwareSerial Serial;
