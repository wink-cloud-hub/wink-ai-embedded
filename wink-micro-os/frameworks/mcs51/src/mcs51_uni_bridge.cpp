// SPDX-License-Identifier: Apache-2.0
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

// Mirrors targets/wasm/wasm_bridge.h: void js_pal_gpio_write(uint16_t, bool).
#define MCS51_HOST_NOTIFY_LOG_SIZE 128u

static uint32_t s_host_gpio_notifies;
static uint16_t s_notify_pin[MCS51_HOST_NOTIFY_LOG_SIZE];
static uint8_t  s_notify_level[MCS51_HOST_NOTIFY_LOG_SIZE];

void js_pal_gpio_write(uint16_t pin, bool level) {
    uint32_t i = s_host_gpio_notifies;
    if (i < MCS51_HOST_NOTIFY_LOG_SIZE) {
        s_notify_pin[i] = pin;
        s_notify_level[i] = level ? 1u : 0u;
    }
    ++s_host_gpio_notifies;
}

float js_pal_adc_read_norm(uint16_t pin) {
    (void)pin;
    return 0.0f;
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

}  // extern "C"

#endif  // __EMSCRIPTEN__
