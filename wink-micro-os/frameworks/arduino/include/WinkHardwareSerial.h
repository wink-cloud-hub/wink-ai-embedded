#pragma once
#include "api/HardwareSerial.h"
#include "api/RingBuffer.h"

class WinkHardwareSerial : public arduino::HardwareSerial {
private:
    arduino::RingBuffer rx_buffer;
public:
    WinkHardwareSerial();
    void begin(unsigned long baudrate) override;
    void begin(unsigned long baudrate, uint16_t config) override;
    void end() override;
    int available(void) override;
    int peek(void) override;
    int read(void) override;
    void flush(void) override;
    size_t write(uint8_t c) override;
    operator bool() override;

    // Test helpers to inject simulated inputs
    void injectInput(const char* str);
    void injectInputChar(uint8_t c);
};

extern WinkHardwareSerial Serial;
