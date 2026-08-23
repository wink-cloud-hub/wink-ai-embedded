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
    PAL_RESOURCE_GPIO_PIN         = 1,
    PAL_RESOURCE_PWM_CHANNEL      = 2,
    PAL_RESOURCE_I2C_PORT         = 3,
    PAL_RESOURCE_I2C_ADDR         = 4,   /**< (port, 7-bit address) granularity */
    PAL_RESOURCE_UART_PORT        = 5,   /**< UART port exclusivity */
    PAL_RESOURCE_ADC_CHANNEL      = 6,   /**< PAL ADC logical channel */
    PAL_RESOURCE_SPI_BUS          = 7,   /**< SPI bus hosts (e.g. SPI2/SPI3) */
    PAL_RESOURCE_SPI_CS           = 8,   /**< SPI chip select pin / device */
    PAL_RESOURCE_PCNT_UNIT        = 9,   /**< Pulse Counter hardware unit */
    PAL_RESOURCE_PCNT_CHAN        = 10,  /**< PCNT channel per unit */
    PAL_RESOURCE_RMT_CHAN         = 11,  /**< RMT channel */
    PAL_RESOURCE_HWTIMER          = 12,  /**< Hardware timer (gptimer) */
    PAL_RESOURCE_MCPWM_UNIT       = 13,  /**< MCPWM hardware unit */
    PAL_RESOURCE_MCPWM_TIMER      = 14,  /**< MCPWM timer per unit */
    PAL_RESOURCE_MCPWM_OPERATOR   = 15,  /**< MCPWM operator per unit */
    PAL_RESOURCE_MCPWM_COMPARATOR = 16,  /**< MCPWM comparator */
    PAL_RESOURCE_MCPWM_SYNC_GPIO  = 17,  /**< MCPWM global GPIO sync input */
    PAL_RESOURCE_GDMA_CHAN        = 18,  /**< GDMA channel */
} pal_resource_type_t;

/** @brief Sentinel representing no simple contiguous ID upper bound */
#define PAL_RESOURCE_UNLIMITED_MAX    0xFFFFFFFFu

/* Hardware limits (ESP32 classic & simulation baselines) */
#define PAL_SPI_BUS_MAX               2u   /* SPI2 (HSPI), SPI3 (VSPI) */
#define PAL_PCNT_UNIT_MAX             8u   /* Unit 0..7 */
#define PAL_PCNT_CHAN_MAX             2u   /* 2 channels per unit */
#define PAL_RMT_CHAN_MAX              8u   /* Channel 0..7 */
#define PAL_HWTIMER_MAX               4u   /* Timer 0..3 */
#define PAL_MCPWM_UNIT_MAX            2u   /* Unit 0..1 */
#define PAL_MCPWM_TIMER_MAX           3u   /* 3 timers per unit */
#define PAL_MCPWM_OPERATOR_MAX        3u   /* 3 operators per unit */
#define PAL_MCPWM_COMPARATOR_MAX      2u   /* 2 comparators per operator */
#define PAL_UART_PORT_MAX             3u   /* UART 0..2 */
#define PAL_ADC_CHANNEL_MAX           10u  /* Logical ADC channel 0..9 */
#define PAL_PWM_CHANNEL_MAX           8u   /* LEDC channels 0..7 */
#define PAL_GPIO_PIN_MAX              40u  /* GPIO 0..39 */

#if defined(CONFIG_IDF_TARGET_ESP32)
_Static_assert(PAL_SPI_BUS_MAX == 2, "ESP32 classic has 2 DMA-capable SPI hosts");
_Static_assert(PAL_PCNT_UNIT_MAX == 8, "ESP32 classic has 8 PCNT units");
_Static_assert(PAL_MCPWM_UNIT_MAX == 2, "ESP32 classic has 2 MCPWM units");
_Static_assert(PAL_RMT_CHAN_MAX == 8, "ESP32 classic has 8 RMT channels");
_Static_assert(PAL_HWTIMER_MAX == 4, "ESP32 classic has 4 general purpose timers");
_Static_assert(PAL_UART_PORT_MAX == 3, "ESP32 classic has 3 UART controllers");
#endif

/**
 * @brief Encodes I2C (port, address) tuple into a PAL_RESOURCE_I2C_ADDR resource ID.
 * @param[in] port I2C hardware bus port
 * @param[in] addr I2C 7-bit or 10-bit target address
 * @return Encoded 32-bit resource ID
 */
static inline uint32_t pal_resource_i2c_id(uint8_t port, uint16_t addr) {
    return ((uint32_t)port << 16) | (uint32_t)addr;
}

/**
 * @brief Encodes MCPWM (unit, sub_id) tuple into a resource ID.
 * @param[in] unit MCPWM unit (0 or 1)
 * @param[in] sub_id Sub-module index (timer/operator/comparator)
 * @return Encoded 32-bit resource ID
 */
static inline uint32_t pal_resource_mcpwm_id(uint8_t unit, uint8_t sub_id) {
    return ((uint32_t)unit << 8) | (uint32_t)sub_id;
}

/**
 * @brief Query maximum valid ID count for a resource type on current target.
 * Valid IDs are in range [0, pal_resource_max(type) - 1].
 * Returns PAL_RESOURCE_UNLIMITED_MAX for unbounded types.
 *
 * @param[in] type Resource type
 * @return Maximum valid ID (exclusive upper bound)
 */
uint32_t pal_resource_max(pal_resource_type_t type);

/** @brief Static claim table capacity */
#ifndef PAL_RESOURCE_MAX_CLAIMS
#define PAL_RESOURCE_MAX_CLAIMS 64
#endif

/**
 * @brief Claim exclusive ownership of a hardware resource
 * @param[in] type Resource type
 * @param[in] id Resource identifier (GPIO pin / PWM channel / I2C port)
 * @param[in] owner Static string literal identifier of the claiming owner
 * @return WINK_OK on success, WINK_ERR_BUSY if claimed by another owner,
 *         WINK_ERR_INVALID_ARG if id >= pal_resource_max(type),
 *         WINK_ERR_RESOURCE_EXHAUSTED if table full
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
