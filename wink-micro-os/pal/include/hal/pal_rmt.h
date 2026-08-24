// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_rmt.h
 * @brief PAL RMT (Remote Control) multi-channel pulse-train transceiver subsystem.
 *
 * Provides thread-safe, static-allocated multi-channel TX and RX symbol generation
 * and capture for LED strips (WS2812), IR protocols (NEC/RC5), stepper pulsing,
 * and high-precision ultrasonic pulse capture.
 *
 * Rules:
 * - Symbol format: pal_rmt_symbol_t contains standard uint16_t/uint8_t fields without C bitfields (ADR-0002).
 * - Allocation: Channels are acquired from static pool; zero runtime dynamic memory allocation.
 * - Callbacks: Triggered in ISR context (ESP-IDF) or task context (Host). Strictly NO blocking/malloc/logging in callbacks.
 */

#ifndef PAL_RMT_H
#define PAL_RMT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RMT channel direction.
 */
typedef enum {
    PAL_RMT_DIR_TX = 0, /**< Pulse generator (transmitter) */
    PAL_RMT_DIR_RX = 1, /**< Pulse capture (receiver) */
} pal_rmt_dir_t;

/**
 * @brief RMT symbol representation (ADR-0002 compliant, no bitfields).
 *
 * Each symbol describes two consecutive pulse phases:
 * Phase 0: duration0_ticks at level0 (0 or 1)
 * Phase 1: duration1_ticks at level1 (0 or 1)
 */
typedef struct {
    uint16_t duration0_ticks; /**< Duration of phase 0 in timer ticks (0..32767) */
    uint16_t duration1_ticks; /**< Duration of phase 1 in timer ticks (0..32767) */
    uint8_t  level0;          /**< Output level for phase 0 (0 or 1) */
    uint8_t  level1;          /**< Output level for phase 1 (0 or 1) */
    uint8_t  _pad[2];         /**< Explicit padding for 32-bit alignment */
} pal_rmt_symbol_t;

/**
 * @brief RMT channel configuration.
 */
typedef struct {
    wink_pin_t    pin;               /**< GPIO pin */
    pal_rmt_dir_t direction;         /**< TX or RX */
    uint32_t      resolution_hz;     /**< Clock resolution (e.g. 10000000 = 10 MHz -> 100ns/tick) */
    size_t        mem_block_symbols; /**< Hardware memory block size in symbols (default 64) */
    bool          dma_enabled;       /**< Enable GDMA for direct large-buffer streaming (S3+ only) */
    uint32_t      max_symbols;       /**< Maximum symbols buffer capacity (0 for driver default) */
} pal_rmt_channel_config_t;

typedef struct pal_rmt_channel_s *pal_rmt_channel_handle_t;

/**
 * @brief TX transmission completion callback.
 * @note Invoked in ISR context on ESP-IDF.
 */
typedef void (*pal_rmt_tx_callback_t)(void *arg, wink_status_t result);

/**
 * @brief RX symbol received callback.
 * @note Invoked in ISR context on ESP-IDF.
 */
typedef void (*pal_rmt_rx_callback_t)(void *arg, const pal_rmt_symbol_t *symbols, size_t count);

/**
 * @brief Acquire an RMT channel for TX or RX.
 * @param[in] cfg Channel configuration
 * @param[out] out_ch Output channel handle
 * @return WINK_OK on success, WINK_ERR_RESOURCE_EXHAUSTED if all channels busy, error status on invalid arg
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_acquire_channel(const pal_rmt_channel_config_t *cfg,
                                      pal_rmt_channel_handle_t *out_ch);

/**
 * @brief Release an acquired RMT channel.
 * @param[in] ch Channel handle
 * @return WINK_OK on success
 */
wink_status_t pal_rmt_release_channel(pal_rmt_channel_handle_t ch);

/**
 * @brief Transmit an array of RMT symbols asynchronously.
 * @param[in] ch Channel handle (must be PAL_RMT_DIR_TX)
 * @param[in] symbols Array of symbols to transmit
 * @param[in] count Number of symbols
 * @param[in] cb Optional completion callback (NULL if non-blocking fire-and-forget)
 * @param[in] arg User argument passed to callback
 * @return WINK_OK if queued, WINK_ERR_BUSY if channel transmitting, error status otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_tx_send(pal_rmt_channel_handle_t ch,
                              const pal_rmt_symbol_t *symbols,
                              size_t count,
                              pal_rmt_tx_callback_t cb,
                              void *arg);

/**
 * @brief Set the RX callback for an RMT receiver channel.
 * @param[in] ch Channel handle (must be PAL_RMT_DIR_RX)
 * @param[in] cb Callback invoked when symbols are received
 * @param[in] arg User argument passed to callback
 * @return WINK_OK on success
 */
wink_status_t pal_rmt_rx_set_callback(pal_rmt_channel_handle_t ch,
                                      pal_rmt_rx_callback_t cb,
                                      void *arg);

/**
 * @brief Start listening on an RMT receiver channel.
 * @param[in] ch Channel handle
 * @return WINK_OK on success
 */
wink_status_t pal_rmt_rx_start(pal_rmt_channel_handle_t ch);

/**
 * @brief Stop listening on an RMT receiver channel.
 * @param[in] ch Channel handle
 * @return WINK_OK on success
 */
wink_status_t pal_rmt_rx_stop(pal_rmt_channel_handle_t ch);

/**
 * @brief Helper to generate a low-level reset / latch symbol (e.g. WS2812 >= 50us LOW).
 * @param[in] resolution_hz Channel resolution frequency
 * @param[in] hold_low_us Duration in microseconds to hold low
 * @return Formatted pal_rmt_symbol_t
 */
static inline pal_rmt_symbol_t pal_rmt_make_reset_symbol(uint32_t resolution_hz, uint32_t hold_low_us) {
    pal_rmt_symbol_t s;
    uint32_t ticks = (uint32_t)((uint64_t)hold_low_us * resolution_hz / 1000000ULL);
    if (ticks > 32767) {
        ticks = 32767;
    }
    s.duration0_ticks = (uint16_t)ticks;
    s.level0 = 0;
    s.duration1_ticks = 0;
    s.level1 = 0;
    s._pad[0] = 0;
    s._pad[1] = 0;
    return s;
}

/* ========================================================================= */
/* Legacy Pulse-Capture Singleton API (Preserved for Backward Compatibility)  */
/* ========================================================================= */

/**
 * @brief Pulse capture start edge direction.
 */
typedef enum {
    PAL_RMT_EDGE_RISING  = 0,   /**< Start at rising edge, measure to falling edge (high pulse width). */
    PAL_RMT_EDGE_FALLING = 1,   /**< Start at falling edge, measure to rising edge (low pulse width). */
} pal_rmt_edge_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_arm(void);

#ifndef WINK_STRICT_NONBLOCKING
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out);

WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out);
#endif /* WINK_STRICT_NONBLOCKING */

void pal_rmt_pulse_capture_deinit(void);
bool pal_rmt_pulse_capture_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RMT_H */
