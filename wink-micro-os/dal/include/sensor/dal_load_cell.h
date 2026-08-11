/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Wink AI Project
 */

#ifndef DAL_LOAD_CELL_SENSOR_H
#define DAL_LOAD_CELL_SENSOR_H

#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(WINK_USE_LOAD_CELL) || !WINK_USE_LOAD_CELL

/* --- 编译期裁剪 fallback 桩段 --- */
#define WINK_LOAD_CELL_UNAVAILABLE WINK_UNAVAILABLE_MSG("DAL load_cell driver is disabled in build config")

static inline wink_status_t dal_load_cell_init(void *dev, const void *config) { (void)dev; (void)config; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_deinit(void *dev) { (void)dev; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_is_data_ready(const void *dev, bool *out_ready) { (void)dev; (void)out_ready; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_request_read(void *dev) { (void)dev; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_get_cached_raw(const void *dev, int32_t *out_raw) { (void)dev; (void)out_raw; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_get_cached_weight_g(const void *dev, float *out_g) { (void)dev; (void)out_g; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_read_weight_g(void *dev, float *out_g) { (void)dev; (void)out_g; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_tare(void *dev) { (void)dev; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_set_calibration_factor(void *dev, float factor) { (void)dev; (void)factor; return WINK_LOAD_CELL_UNAVAILABLE; }
static inline wink_status_t dal_load_cell_apply_override(void *dev, const uint8_t *params, uint16_t len) { (void)dev; (void)params; (void)len; return WINK_LOAD_CELL_UNAVAILABLE; }

#else

/**
 * @brief Load cell AFE hardware interface variant (affects_pins: true)
 */
typedef enum {
    DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE  = 0, /**< Default: 4Pin 2-wire pulse bit-bang (HX711/TM7711/NA770) */
    DAL_LOAD_CELL_VARIANT_CS1237_TWO_WIRE = 1, /**< 4Pin 2-wire half-duplex register read/write (CS1237/CS1238) */
    DAL_LOAD_CELL_VARIANT_ADS1232_SPI     = 2, /**< 6Pin 2-wire shift with hardware gain control (ADS1232/ADS1234) */
    DAL_LOAD_CELL_VARIANT_MODBUS_RTU      = 3, /**< 4Pin UART RS485 Modbus RTU weighing transmitter */
    DAL_LOAD_CELL_VARIANT_COUNT            = 4, /**< Total variant count for static assertion */
} dal_load_cell_variant_t;

/**
 * @brief Programmable Gain Amplifier (PGA) setting
 * @note GAIN_32 represents Channel B Gain 32 on HX711; ADS1232 uses hardware pins; Modbus ignores this field.
 */
typedef enum {
    DAL_LOAD_CELL_GAIN_128 = 0, /**< Channel A gain 128 (Default for HX711) */
    DAL_LOAD_CELL_GAIN_64  = 1, /**< Channel A gain 64 (HX711) */
    DAL_LOAD_CELL_GAIN_32  = 2, /**< Channel B gain 32 (HX711) */
} dal_load_cell_gain_t;

/**
 * @brief Load cell configuration struct (Flat POD layout with sentinel trimming & ABI padding)
 */
typedef struct {
    const char              *owner;              /**< Instance owner static tag string */
    float                    calibration_factor; /**< Scale factor (counts per gram, default 1.0f) */
    int32_t                  zero_offset;        /**< Tare offset count (raw zero reading) */
    uint32_t                 timeout_us;         /**< Measurement DRDY timeout in µs (default 150000us) */
    uint32_t                 baud_rate;          /**< UART baud rate (modbus_rtu only, default 9600) */
    dal_load_cell_variant_t  variant;            /**< AFE interface variant */
    dal_load_cell_gain_t     gain;               /**< PGA gain selection */
    wink_pin_t               dt_pin;             /**< HX711 DT / DOUT pin (-1 if unused) */
    wink_pin_t               sck_pin;            /**< HX711 SCK / PD_SCK pin (-1 if unused) */
    wink_pin_t               out_in_pin;         /**< CS1237 bi-directional OUT_IN pin (-1 if unused) */
    wink_pin_t               sclk_pin;           /**< CS1237/ADS1232 SCLK pin (-1 if unused) */
    wink_pin_t               dout_pin;           /**< ADS1232 DOUT pin (-1 if unused) */
    wink_pin_t               gain0_pin;          /**< ADS1232 GAIN0 pin (-1 if unused) */
    wink_pin_t               gain1_pin;          /**< ADS1232 GAIN1 pin (-1 if unused) */
    uint8_t                  modbus_addr;        /**< Modbus slave address (modbus_rtu only, default 1) */
    uint8_t                  uart_port;          /**< Logical UART port index (modbus_rtu only) */
    uint8_t                  _reserved[4];       /**< ABI alignment padding & future expansion reservation */
} dal_load_cell_config_t;

/**
 * @brief Load cell device handle
 */
typedef struct {
    dal_load_cell_config_t   config;             /**< Device configuration POD (Offset 0) */
    int32_t                  last_raw;           /**< Last raw 24-bit ADC reading */
    float                    last_weight_g;      /**< Last calculated weight in grams */
    dal_load_cell_gain_t     pending_gain;       /**< Next-frame pending gain setting */
    volatile bool            initialized;        /**< Initialization flag */
} dal_load_cell_t;

/* --- SSOT §5.1 Mandatory Double Static Assertions & ABI Freeze Checks --- */
_Static_assert(offsetof(dal_load_cell_t, config) == 0, "ABI break: config struct must be at offset 0 of handle");
_Static_assert(DAL_LOAD_CELL_VARIANT_COUNT == 4, "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_LOAD_CELL_VARIANT_MODBUS_RTU + 1 == DAL_LOAD_CELL_VARIANT_COUNT, "Sequential variant ordering error");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_load_cell_config_t) == 48, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_load_cell_t, initialized) == 60, "ABI break: initialized offset changed on 32-bit target");
_Static_assert(sizeof(dal_load_cell_t) == 64, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_load_cell_config_t) == 56, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_load_cell_t, initialized) == 68, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_load_cell_t) == 72, "ABI break: handle size changed on 64-bit host");
#endif

/* --- API 函数声明 --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_init(dal_load_cell_t *dev, const dal_load_cell_config_t *config);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_deinit(dal_load_cell_t *dev);

/* --- ADR-0017 非阻塞解耦 API 契约 --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_is_data_ready(const dal_load_cell_t *dev, bool *out_ready);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_request_read(dal_load_cell_t *dev);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_get_cached_raw(const dal_load_cell_t *dev, int32_t *out_raw);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_get_cached_weight_g(const dal_load_cell_t *dev, float *out_g);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Synchronous blocking read (Waits for DRDY up to timeout_us)
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_read_weight_g(dal_load_cell_t *dev, float *out_g);
#endif

/* --- 业务辅助 API --- */
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_tare(dal_load_cell_t *dev);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_set_calibration_factor(dal_load_cell_t *dev, float factor);
WINK_WARN_UNUSED_RESULT wink_status_t dal_load_cell_apply_override(void *dev, const uint8_t *params, uint16_t len);

#endif /* WINK_USE_LOAD_CELL */

#ifdef __cplusplus
}
#endif

#endif /* DAL_LOAD_CELL_SENSOR_H */
