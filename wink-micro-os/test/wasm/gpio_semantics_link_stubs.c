/**
 * @file gpio_semantics_link_stubs.c
 * @brief Minimal stubs so pal_hal_wasm.c links under a focused emcc test build.
 *        Only symbols required by GPIO read/write/init path + TU-referenced deps.
 */
#include "pal_pwm_router.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "wasm_bridge.h"
#include "wink_status.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --- OSAL / physical (bounce disabled) --- */
uint64_t pal_os_get_us(void) { return 0; }
uint32_t pal_wasm_get_bounce_us(void) { return 0; }
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin) {
    (void)pin;
    return NULL;
}
void pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus) {
    (void)fault_type;
    (void)pin_or_bus;
}
bool wink_phys_debounce_step(wink_phys_debounce_ctx_t *ctx, bool ideal,
                             uint64_t now_us, uint32_t bounce_us) {
    (void)ctx;
    (void)now_us;
    (void)bounce_us;
    return ideal;
}
uint16_t pal_wasm_get_i2c_drop_permil(void) { return 0; }
uint32_t pal_wasm_get_prng_state(void) { return 1; }
void pal_wasm_advance_prng_state(uint32_t new_state) { (void)new_state; }
bool wink_phys_bus_drop(uint16_t drop_permil, uint32_t *prng_state) {
    (void)drop_permil;
    (void)prng_state;
    return false;
}
void pal_wasm_advance_virtual_clock(uint64_t us) { (void)us; }
bool pal_wasm_is_faulted(void) { return false; }
float js_sim_get_plugin_channel(const char *instance_id, const char *channel_name) {
    (void)instance_id;
    (void)channel_name;
    return -1.0f;
}

/* --- PWM router (referenced by pal_hal_wasm PWM APIs) --- */
wink_status_t pal_pwm_router_acquire(uint8_t channel, const pal_pwm_timer_profile_t *prof,
                                     uint8_t *out_timer) {
    (void)channel;
    (void)prof;
    if (out_timer) *out_timer = 0;
    return WINK_OK;
}
bool pal_pwm_router_channel_ready(uint8_t channel) {
    (void)channel;
    return false;
}
void pal_pwm_router_release(uint8_t channel) { (void)channel; }

/* --- unused JS imports that may survive GC --- */
void js_pal_pwm_set_duty(uint8_t channel, float duty) {
    (void)channel;
    (void)duty;
}
bool js_pal_i2c_transfer(uint8_t port, uint16_t addr, const uint8_t *w, uint32_t wl,
                         uint8_t *r, uint32_t rl) {
    (void)port;
    (void)addr;
    (void)w;
    (void)wl;
    (void)r;
    (void)rl;
    return false;
}
