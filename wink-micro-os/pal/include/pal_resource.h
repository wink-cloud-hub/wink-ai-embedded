// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_resource.h
 * @brief Resource claim governance (static table, zero dynamic memory allocation).
 *
 * Detects GPIO pin / PWM channel / I2C port/address duplicate claim conflicts.
 * Embedded on Host/debug and ESP32 targets (ESP32 uses FreeRTOS critical section to protect table);
 * Wasm sandbox claims degrade to no-op.
 *
 * Owner Lifetime Contract: The static table holds `owner` pointer (no string copy).
 * `owner` MUST point to static storage with lifetime >= resource claim duration (string literals or static names).
 */
#ifndef PAL_RESOURCE_H
#define PAL_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Governed resource types */
typedef enum {
    PAL_RESOURCE_GPIO_PIN    = 1,
    PAL_RESOURCE_PWM_CHANNEL = 2,
    PAL_RESOURCE_I2C_PORT    = 3,
    PAL_RESOURCE_I2C_ADDR    = 4,   /**< (port, 7-bit address) granularity */
    PAL_RESOURCE_UART_PORT   = 5,   /**< UART port exclusivity */
    PAL_RESOURCE_ADC_CHANNEL = 6,   /**< PAL ADC logical channel */
} pal_resource_type_t;

/**
 * @brief Encodes I2C (port, address) tuple into a PAL_RESOURCE_I2C_ADDR resource ID.
 * @param[in] port I2C hardware bus port
 * @param[in] addr I2C 7-bit or 10-bit target address
 * @return Encoded 32-bit resource ID
 */
static inline uint32_t pal_resource_i2c_id(uint8_t port, uint16_t addr) {
    return ((uint32_t)port << 16) | (uint32_t)addr;
}

/** @brief Static claim table capacity */
#ifndef PAL_RESOURCE_MAX_CLAIMS
#define PAL_RESOURCE_MAX_CLAIMS 32
#endif

/**
 * @brief Claim exclusive ownership of a hardware resource
 * @param[in] type Resource type
 * @param[in] id Resource identifier (GPIO pin / PWM channel / I2C port)
 * @param[in] owner Static string literal identifier of the claiming owner
 * @return WINK_OK on success, WINK_ERR_BUSY if claimed by another owner, WINK_ERR_RESOURCE_EXHAUSTED if table full
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner);

/**
 * @brief Release a claimed hardware resource
 * @param[in] type Resource type
 * @param[in] id Resource identifier
 * @param[in] owner Static string identifier of the claiming owner (must match claim call)
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG if owner mismatch or resource unclaimed
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner);

/**
 * @brief Query if a hardware resource is currently claimed
 * @param[in] type Resource type
 * @param[in] id Resource identifier
 * @return true if claimed, false otherwise
 */
bool pal_resource_is_claimed(pal_resource_type_t type, uint32_t id);

/** @brief Reset resource claim table (for test isolation / reboot initialization) */
void pal_resource_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RESOURCE_H */
