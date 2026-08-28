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
