// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_fault_types.h
 * @brief Wasm simulation fault types, event structures, and guard macros.
 */
#ifndef PAL_WASM_FAULT_TYPES_H
#define PAL_WASM_FAULT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FAULT_TYPE_GPIO_BOUNCE  = 1,
    FAULT_TYPE_I2C_DROP     = 2,
    FAULT_TYPE_I2C_NOISE    = 3,
    FAULT_TYPE_CLOCK_DRIFT  = 4,
    FAULT_TYPE_UART_OVERRUN = 5,
} wasm_fault_type_t;

typedef struct {
    uint64_t timestamp_us;
    uint8_t  fault_type;
    uint16_t pin_or_bus;
    uint32_t sequence;
} wasm_fault_event_t;

bool pal_wasm_is_faulted(void);

#define WASM_FAULT_GUARD_VOID()    do { if (pal_wasm_is_faulted()) return; } while (0)
#define WASM_FAULT_GUARD_WINKERR() do { if (pal_wasm_is_faulted()) return WINK_ERR_INVALID_STATE; } while (0)
#define WASM_FAULT_GUARD_BOOL()    do { if (pal_wasm_is_faulted()) return false; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* PAL_WASM_FAULT_TYPES_H */
