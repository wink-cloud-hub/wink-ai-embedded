// SPDX-License-Identifier: LGPL-3.0-only
// Host-side fallback for the UniSim channel imports (ADR-0071 D3 / AD-8).
//
// Under emscripten these two symbols are JS imports (wink_sim_js.js in
// production, mcs51_wasm_node_stub.js in the bounded Node tests): the C side
// never defines them. On host (MSVC/GCC) there is no JS data plane, so the
// compat library supplies fallbacks: channel-1 pin notifications are recorded
// (tests assert the instant-notify path fired, with the exact linear pin id),
// and channel-3 analog pulls read 0.0 (tests inject via mcs51_adc_set_value,
// which overrides the pull).
#include <stdint.h>

#ifndef __EMSCRIPTEN__

extern "C" {

// Mirrors targets/wasm/wasm_bridge.h:
//   void js_pal_gpio_write(uint16_t, bool, uint8_t strength) (ADR-0077).
#define MCS51_HOST_NOTIFY_LOG_SIZE 128u

static uint32_t s_host_gpio_notifies;
static uint16_t s_notify_pin[MCS51_HOST_NOTIFY_LOG_SIZE];
static uint8_t  s_notify_level[MCS51_HOST_NOTIFY_LOG_SIZE];
static uint8_t  s_notify_strength[MCS51_HOST_NOTIFY_LOG_SIZE];

extern uint64_t wink_mcs51_virtual_us(void);
extern void wink_mcs51_pwm_meter_update(uint16_t pin, uint8_t level, uint64_t timestamp_us);

void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength) {
    uint32_t i = s_host_gpio_notifies;
    if (i < MCS51_HOST_NOTIFY_LOG_SIZE) {
        s_notify_pin[i] = pin;
        s_notify_level[i] = level ? 1u : 0u;
        // Default 0 -> SUPPLY so a caller that omits strength still reads
        // push-pull (mirrors the host `strength ?? SUPPLY` skew fallback).
        s_notify_strength[i] = strength != 0u ? strength : 3u;
    }
    ++s_host_gpio_notifies;
    wink_mcs51_pwm_meter_update(pin, level ? 1u : 0u, wink_mcs51_virtual_us());
}

// P3 (PLAN-20260912-MCS51-P3-TRIS): MCU-driver release records. Host has no
// arbiter; the chip model's TRIS 1->0 / open-drain / reset releases land here
// so tests can assert the exact pin was released.
#define MCS51_HOST_RELEASE_LOG_SIZE 128u
static uint32_t s_host_gpio_releases;
static uint16_t s_release_pin[MCS51_HOST_RELEASE_LOG_SIZE];

void js_pal_gpio_release_mcu(uint16_t pin) {
    uint32_t i = s_host_gpio_releases;
    if (i < MCS51_HOST_RELEASE_LOG_SIZE) {
        s_release_pin[i] = pin;
    }
    ++s_host_gpio_releases;
}

// Channel-3 analog pull override (A-02 host test seam): per rail-key norm
// override (Stage1 dual-space partition, see mcs51_adc.h: keys 0~31 are MCU
// physical pins, 32~63 board channels; this [64] table already covers both,
// it is the partitioned space itself). has-flag false = no override
// (returns 0.0, legacy).
static float s_host_analog_norm[64];
static bool s_host_analog_has[64];

float js_pal_adc_read_norm(uint16_t pin) {
    if (pin < 64u && s_host_analog_has[pin]) {
        return s_host_analog_norm[pin];
    }
    return 0.0f;
}

void wink_mcs51_host_set_analog_norm(uint16_t pin, float norm) {
    if (pin < 64u) {
        s_host_analog_norm[pin] = norm;
        s_host_analog_has[pin] = true;
    }
}

void wink_mcs51_host_analog_reset(void) {
    for (uint32_t i = 0; i < 64u; ++i) {
        s_host_analog_norm[i] = 0.0f;
        s_host_analog_has[i] = false;
    }
}

// Channel-2 UART TX (mirrors targets/wasm/wasm_bridge.h:
// void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len)).
// Host has no JS data plane: record the (port, byte) stream so tests can
// assert the live SBUF -> UARTBus route fired; production JS routes it to
// the UARTBus plugin via wink_sim_js.js.
#define MCS51_HOST_UART_LOG_SIZE 256u
static uint32_t s_host_uart_tx;
static uint8_t  s_uart_tx_port[MCS51_HOST_UART_LOG_SIZE];
static uint8_t  s_uart_tx_byte[MCS51_HOST_UART_LOG_SIZE];

void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t j = s_host_uart_tx;
        if (j < MCS51_HOST_UART_LOG_SIZE) {
            s_uart_tx_port[j] = port;
            s_uart_tx_byte[j] = buf ? buf[i] : 0u;
        }
        ++s_host_uart_tx;
    }
}

// Channel-2 SPI transfer (mirrors targets/wasm/wasm_bridge.h:
//   bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
//                            const uint8_t* tx_buf, uint32_t len,
//                            uint8_t* rx_buf, uint8_t mode, uint32_t sck_hz)).
// Phase 2 prerequisite: the on-chip SPI master model will route byte
// exchanges here once the UniSim SPI session contract (ADR-0087, T2.2a)
// lands. Host has no JS bus data plane, so the fallback is fail-closed —
// report "not executed" instead of faking device bytes (ADR-0012 honesty over
// silent degradation). Emscripten builds import the JS implementation instead
// of linking this definition.
bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
                         const uint8_t* tx_buf, uint32_t len,
                         uint8_t* rx_buf, uint8_t mode, uint32_t sck_hz) {
    (void)port;
    (void)device_id;
    (void)tx_buf;
    (void)len;
    (void)rx_buf;
    (void)mode;
    (void)sck_hz;
    return false;
}

// Channel-2 I2C transfer (mirrors targets/wasm/wasm_bridge.h:
//   bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
//                            const uint8_t* write_buf, uint32_t write_len,
//                            uint8_t* read_buf, uint32_t read_len)).
// Phase 2 prerequisite: the on-chip I2C master model will route whole
// transactions here (and per-byte sessions via the ADR-0086 ABI) once the
// UniSim engine wiring lands. Host has no JS bus data plane: fail closed
// (ADR-0012). Emscripten builds import the JS implementation instead.
bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                         const uint8_t* write_buf, uint32_t write_len,
                         uint8_t* read_buf, uint32_t read_len) {
    (void)port;
    (void)dev_addr;
    (void)write_buf;
    (void)write_len;
    (void)read_buf;
    (void)read_len;
    return false;
}

// Channel-1 read direction (external digital level driven by the JS
// PinArbiter / an input plugin). No JS data plane on host, so the compat
// library supplies a scriptable fallback. State codes mirror the platform
// JS_GPIO_STATE_* enum (0 low, 1 high, 2 HiZ, 3 conflict); the array lazily
// initialises to HiZ (2 = no external driver -> the proxy falls back to the
// latch), so tests that never inject still see latch semantics.
#define MCS51_HOST_EXT_HIZ 2u
static uint8_t s_host_ext_pin[32];
static bool s_host_ext_pin_ready = false;

static void host_ext_pin_ensure_init(void) {
    if (!s_host_ext_pin_ready) {
        for (uint32_t i = 0; i < 32u; ++i) {
            s_host_ext_pin[i] = MCS51_HOST_EXT_HIZ;
        }
        s_host_ext_pin_ready = true;
    }
}

uint8_t js_pal_gpio_read_state(uint16_t pin) {
    host_ext_pin_ensure_init();
    return (pin < 32u) ? s_host_ext_pin[pin] : MCS51_HOST_EXT_HIZ;
}

// Test injection for the channel-1 external Read-Pin path. state uses the
// JS_GPIO_STATE_* codes (0 low / 1 high / 2 HiZ).
void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state) {
    host_ext_pin_ensure_init();
    if (pin < 32u) {
        s_host_ext_pin[pin] = state;
    }
}
void wink_mcs51_host_ext_pins_reset(void) {
    host_ext_pin_ensure_init();
    for (uint32_t i = 0; i < 32u; ++i) {
        s_host_ext_pin[i] = MCS51_HOST_EXT_HIZ;
    }
}

// Test observability for the channel-2 UART TX route (SBUF write ->
// js_pal_uart_write). Mirrors the gpio notify log accessors.
uint32_t wink_mcs51_host_uart_tx_count(void) {
    return s_host_uart_tx;
}
uint8_t wink_mcs51_host_uart_tx_byte(uint32_t i) {
    return (i < MCS51_HOST_UART_LOG_SIZE) ? s_uart_tx_byte[i] : 0u;
}
uint8_t wink_mcs51_host_uart_tx_port(uint32_t i) {
    return (i < MCS51_HOST_UART_LOG_SIZE) ? s_uart_tx_port[i] : 0xFFu;
}
void wink_mcs51_host_uart_tx_reset(void) {
    s_host_uart_tx = 0;
}

// Test observability for the channel-1 instant-notification path.
uint32_t wink_mcs51_host_gpio_notify_count(void) {
    return s_host_gpio_notifies;
}
void wink_mcs51_host_gpio_notify_reset(void) {
    s_host_gpio_notifies = 0;
}
uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_pin[i] : 0xFFFFu;
}
uint8_t wink_mcs51_host_gpio_notify_level(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_level[i] : 0u;
}
uint8_t wink_mcs51_host_gpio_notify_strength(uint32_t i) {
    return (i < MCS51_HOST_NOTIFY_LOG_SIZE) ? s_notify_strength[i] : 0u;
}

// P3: release-side observability (TRIS 1->0 / open-drain / reset).
uint32_t wink_mcs51_host_gpio_release_count(void) {
    return s_host_gpio_releases;
}
uint16_t wink_mcs51_host_gpio_release_pin(uint32_t i) {
    return (i < MCS51_HOST_RELEASE_LOG_SIZE) ? s_release_pin[i] : 0xFFFFu;
}
void wink_mcs51_host_gpio_release_reset(void) {
    s_host_gpio_releases = 0u;
    for (uint32_t i = 0; i < MCS51_HOST_RELEASE_LOG_SIZE; ++i) {
        s_release_pin[i] = 0u;
    }
}

}  // extern "C"

#endif  // __EMSCRIPTEN__
