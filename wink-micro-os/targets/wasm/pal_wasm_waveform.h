// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_waveform.h
 * @brief Sub-step high-fidelity waveform event queue & SSOT C-driven notification.
 */

#ifndef PAL_WASM_WAVEFORM_H
#define PAL_WASM_WAVEFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAVEFORM_RING_SIZE 512

typedef struct {
    uint64_t t_us;
    uint16_t pin;
    uint8_t  level;
    uint32_t generation;
    bool     valid;
} waveform_edge_t;

void     pal_wasm_waveform_reset(void);
void     pal_wasm_push_waveform_edge(uint16_t pin, uint64_t t_us, uint8_t level, uint32_t generation);
void     pal_wasm_cancel_waveform_generation(uint16_t pin, uint32_t generation);
uint32_t pal_wasm_drain_due_waveform_edges(uint64_t until_us);
uint32_t pal_wasm_get_waveform_overflow_count(void);

int32_t  pal_wasm_export_waveform_state(uint8_t *out_buf, uint32_t max_len);
int32_t  pal_wasm_restore_waveform_state(const uint8_t *in_buf, uint32_t len);

void     pal_wasm_set_pulse_measurement_active(bool active);
bool     pal_wasm_is_pulse_measurement_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_WASM_WAVEFORM_H */
