// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_rmt_stub.h
 * @brief Host target testing stub hooks for RMT symbol injection and TX verification.
 */
#ifndef PAL_RMT_STUB_H
#define PAL_RMT_STUB_H

#include <stdint.h>
#include <stddef.h>
#include "hal/pal_rmt.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inject RX symbols into an active RMT RX channel.
 * @param[in] ch Channel handle
 * @param[in] symbols Symbol array to inject
 * @param[in] count Number of symbols
 */
void stub_rmt_inject_rx(pal_rmt_channel_handle_t ch, const pal_rmt_symbol_t *symbols, size_t count);

/**
 * @brief Retrieve the last transmitted symbols on an RMT TX channel.
 * @param[in] ch Channel handle
 * @param[out] out_symbols Buffer to receive symbols
 * @param[in,out] out_count Pointer to max capacity, receives actual count
 */
void stub_rmt_get_last_tx(pal_rmt_channel_handle_t ch, pal_rmt_symbol_t *out_symbols, size_t *out_count);

/**
 * @brief Force the next TX or RX operation on a channel to fail.
 * @param[in] ch Channel handle
 * @param[in] err Error code
 */
void stub_rmt_force_failure(pal_rmt_channel_handle_t ch, wink_status_t err);

/**
 * @brief Reset the stub state for an RMT channel.
 * @param[in] ch Channel handle
 */
void stub_rmt_reset(pal_rmt_channel_handle_t ch);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RMT_STUB_H */
