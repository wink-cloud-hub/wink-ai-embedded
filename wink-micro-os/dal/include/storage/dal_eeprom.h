// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_EEPROM_H
#define DAL_EEPROM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM non-blocking state machine enum
 */
typedef enum {
    DAL_EEPROM_IDLE  = 0,   /**< Idle, ready for new request */
    DAL_EEPROM_BUSY  = 1,   /**< Operation in progress */
    DAL_EEPROM_READY = 2,   /**< Operation complete, result ready */
    DAL_EEPROM_ERROR = 3,   /**< Operation failed */
} dal_eeprom_state_t;

/**
 * @brief EEPROM configuration struct
 */
typedef struct {
    const char *owner;         /**< Instance owner static string */
    uint32_t capacity_bytes;   /**< Total EEPROM capacity in bytes */
    uint16_t i2c_addr;         /**< 7-bit I2C device address */
    uint16_t page_size;        /**< EEPROM page size in bytes */
    uint16_t write_time_ms;    /**< Page write cycle time in ms (default 5ms) */
    uint8_t  i2c_port;         /**< I2C bus port number */
} dal_eeprom_config_t;

/**
 * @brief EEPROM handle struct (POD)
 */
typedef struct {
    dal_eeprom_config_t config;       /**< Config copy */
    dal_eeprom_state_t  state;        /**< Non-blocking state machine state */
    wink_status_t       last_status;  /**< Last operation status code */
    uint32_t            req_addr;     /**< Current request start address */
    uint32_t            req_len;      /**< Current request length */
    uint8_t            *req_buf;      /**< Current request buffer pointer */
    bool                initialized;  /**< Set to true after successful init */
} dal_eeprom_t;

_Static_assert(offsetof(dal_eeprom_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_eeprom_config_t) == 16, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_eeprom_t, initialized) == 36, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_eeprom_t) == 40, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_eeprom_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_eeprom_t, initialized) == 48, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_eeprom_t) == 56, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize EEPROM driver instance (non-blocking)
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg);

/**
 * @brief Submit non-blocking read request
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[in] addr Start address byte offset.
 * @param[in] len Length in bytes to read.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len);

/**
 * @brief Submit non-blocking write request
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[in] addr Start address byte offset.
 * @param[in] buf Data buffer pointer.
 * @param[in] len Length in bytes to write.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len);

/**
 * @brief Advance EEPROM non-blocking state machine
 *
 * @param[in,out] dev EEPROM instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_eeprom_poll(dal_eeprom_t *dev);

/**
 * @brief Query current EEPROM operation state
 *
 * @param[in] dev EEPROM instance handle.
 * @param[out] out_state Output pointer for state enum.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_status(const dal_eeprom_t *dev, dal_eeprom_state_t *out_state);

/**
 * @brief Read completed read-operation result payload
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[out] buf Buffer to store read data.
 * @param[in] len Expected read length in bytes.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_read_result(dal_eeprom_t *dev, uint8_t *buf, uint32_t len);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Read data from EEPROM (blocking)
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[in] addr Start address byte offset.
 * @param[out] buf Output buffer pointer.
 * @param[in] len Length in bytes.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len);

/**
 * @brief Write data to EEPROM (blocking, auto page split)
 *
 * @param[in,out] dev EEPROM instance handle.
 * @param[in] addr Start address byte offset.
 * @param[in] buf Input data buffer.
 * @param[in] len Length in bytes.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief Deinitialize EEPROM driver
 *
 * @param[in,out] dev EEPROM instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_EEPROM) || !WINK_USE_EEPROM
#define WINK_EEPROM_DISABLED_MSG \
    "EEPROM driver not enabled; add an \"eeprom\" device to wink-app.json " \
    "(or set -DWINK_USE_EEPROM=ON)."
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG)
wink_status_t dal_eeprom_poll(dal_eeprom_t *dev);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_status(const dal_eeprom_t *dev, dal_eeprom_state_t *out_state);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_read_result(dal_eeprom_t *dev, uint8_t *buf, uint32_t len);
#ifndef WINK_STRICT_NONBLOCKING
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len);
#endif /* WINK_STRICT_NONBLOCKING */
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG)
wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev);
#endif /* !WINK_USE_EEPROM */

#endif /* DAL_EEPROM_H */
