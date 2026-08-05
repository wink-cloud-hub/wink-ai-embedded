// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_GPS_H
#define DAL_GPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPS position data struct (NMEA parsing result)
 */
typedef struct {
    int32_t  lat_udeg;       /**< Latitude (micro-degrees, 1e-6°), positive=N, negative=S */
    int32_t  lon_udeg;       /**< Longitude (micro-degrees, 1e-6°), positive=E, negative=W */
    int32_t  alt_mm;         /**< Altitude in millimeters */
    float    speed_kmh;      /**< Ground speed in km/h */
    float    course_deg;     /**< Course heading in degrees (0-360°) */
    uint32_t timestamp_ms;   /**< Data update timestamp in ms */
    uint16_t hdop;           /**< HDOP x 100 (e.g., 120 = HDOP 1.20) */
    uint8_t  satellites;     /**< Visible satellites count */
    uint8_t  fix_quality;    /**< Fix quality: 0=invalid, 1=GPS, 2=DGPS, 6=estimated */
    bool     fix_valid;      /**< True if valid 3D fix */
    bool     time_valid;     /**< True if UTC time valid */
} dal_gps_position_t;

/**
 * @brief GPS configuration struct
 */
typedef struct {
    const char *owner;       /**< Instance owner static string */
    uint8_t  uart_port;      /**< UART port number */
    uint32_t baudrate;       /**< Baudrate (default 9600) */
    uint16_t rx_buffer_size; /**< NMEA RX buffer size in bytes (default 256) */
} dal_gps_config_t;

/**
 * @brief GPS handle struct (POD)
 */
typedef struct {
    dal_gps_config_t config;        /**< Config copy */
    dal_gps_position_t last_position; /**< Last valid position result */
    uint32_t last_fix_time_ms;      /**< Last valid fix timestamp */
    bool initialized;               /**< Set to true after successful init */
} dal_gps_t;

_Static_assert(offsetof(dal_gps_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_gps_config_t) == 16, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_gps_t, initialized) == 52, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_gps_t) == 56, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_gps_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_gps_t, initialized) == 60, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_gps_t) == 64, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize GPS driver instance (non-blocking)
 *
 * @param[in,out] dev GPS instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Initialize GPS driver instance (blocking)
 *
 * @param[in,out] dev GPS instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief Poll GPS receiver and parse NMEA sentences (non-blocking)
 *
 * @param[in,out] dev GPS instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_gps_poll(dal_gps_t *dev);

/**
 * @brief Read latest GPS position fix
 *
 * @param[in] dev GPS instance handle.
 * @param[out] pos Output pointer for position struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos);

/**
 * @brief Deinitialize GPS driver
 *
 * @param[in,out] dev GPS instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_gps_deinit(dal_gps_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_GPS) || !WINK_USE_GPS
#define WINK_GPS_DISABLED_MSG \
    "GPS driver not enabled; add a \"gps\" device to wink-app.json " \
    "(or set -DWINK_USE_GPS=ON)."
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg);
#ifndef WINK_STRICT_NONBLOCKING
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
#endif /* WINK_STRICT_NONBLOCKING */
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG)
wink_status_t dal_gps_poll(dal_gps_t *dev);
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos);
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG)
wink_status_t dal_gps_deinit(dal_gps_t *dev);
#endif /* !WINK_USE_GPS */

#endif /* DAL_GPS_H */
