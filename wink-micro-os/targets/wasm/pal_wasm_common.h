// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_common.h
 * @brief Common definitions for Wasm target PAL subsystem.
 */
#ifndef PAL_WASM_COMMON_H
#define PAL_WASM_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "wink_sim_physical.h"
#include "pal_wasm_fault_types.h"

#ifndef PAL_WASM_INTERRUPT_QUEUE_SIZE
#define PAL_WASM_INTERRUPT_QUEUE_SIZE 16
#endif

#define WASM_SIM_MAX_PINS 128
#define WASM_FAULT_LOG_SIZE 256

int32_t pal_wasm_dispatch_pending_interrupts(void);
void    pal_wasm_dispatch_pending_irqs(void);

void     pal_wasm_advance_virtual_clock(uint64_t us);
bool     pal_wasm_is_clock_warning_fired(void);
uint64_t pal_wasm_get_virtual_clock_us(void);

uint32_t pal_wasm_get_bounce_us(void);
uint16_t pal_wasm_get_i2c_drop_permil(void);
uint32_t pal_wasm_get_prng_state(void);
void     pal_wasm_advance_prng_state(uint32_t new_state);
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin);

wink_sim_faults_t *pal_wasm_get_faults_ref(void);
void pal_wasm_clear_fault_latch(void);

struct wink_app_callbacks;
void pal_wasm_fault_set_callbacks(const struct wink_app_callbacks *cb);

void pal_wasm_invoke_fault(uint32_t code);
void pal_wasm_host_fault(uint32_t code, const char *msg_cstr);

void pal_wasm_reset_physical(void);

void     pal_wasm_log_fault(uint8_t fault_type, uint16_t pin_or_bus);
uint32_t pal_wasm_get_fault_log_count(void);
bool     pal_wasm_get_fault_event(uint32_t index, wasm_fault_event_t *out_event);
void     pal_wasm_reset_fault_log(void);

typedef struct wasm_pin_power_model_t {
    uint32_t active_current_ua;
    uint32_t leakage_current_ua;
    uint32_t transition_energy_nj;
} wasm_pin_power_model_t;

wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                           const wasm_pin_power_model_t *model);
uint64_t      pal_wasm_get_total_energy_mj(void);

typedef enum {
    WASM_FAULT_DOMAIN_GLOBAL = 0,
    WASM_FAULT_DOMAIN_GPIO   = 1,
    WASM_FAULT_DOMAIN_I2C0   = 2,
    WASM_FAULT_DOMAIN_I2C1   = 3,
    WASM_FAULT_DOMAIN_SPI0   = 4,
    WASM_FAULT_DOMAIN_CLOCK  = 5,
    WASM_FAULT_DOMAIN_COUNT
} wasm_fault_domain_id_t;

typedef struct {
    uint32_t domain_id;
    bool     armed;
    uint32_t trigger_count;
} wasm_fault_domain_t;

wink_sim_faults_t *pal_wasm_get_domain_config(uint32_t domain_id);
wink_status_t      pal_wasm_arm_fault_domain(uint32_t domain_id, bool armed);
uint32_t           pal_wasm_get_domain_trigger_count(uint32_t domain_id);

void pal_wasm_reset_fault_domains(void);

#endif /* PAL_WASM_COMMON_H */
