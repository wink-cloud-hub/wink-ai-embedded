// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_audio.h
 * @brief DAL Audio / I2S driver interface placeholder (Class 6 Isochronous Audio).
 *
 * Notice: This is an ADR-0012 honest contract placeholder. Full I2S DMA streaming
 * will be delivered in a dedicated Class 6 audio sub-plan.
 */

#ifndef DAL_AUDIO_H
#define DAL_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DAL_AUDIO_FORMAT_PCM_16BIT,
    DAL_AUDIO_FORMAT_PDM,
} dal_audio_format_t;

typedef struct {
    wink_pin_t         bclk_pin;
    wink_pin_t         ws_pin;
    wink_pin_t         dout_pin;
    uint32_t           sample_rate_hz;
    dal_audio_format_t format;
    uint8_t            channels;
} dal_audio_config_t;

typedef struct dal_audio_s {
    bool               is_initialized;
    dal_audio_config_t config;
} dal_audio_t;

WINK_WARN_UNUSED_RESULT
wink_status_t dal_audio_init(dal_audio_t *dev, const dal_audio_config_t *config);

wink_status_t dal_audio_deinit(dal_audio_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_audio_start_stream(dal_audio_t *dev);

wink_status_t dal_audio_stop_stream(dal_audio_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_audio_write(dal_audio_t *dev, const void *samples, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* DAL_AUDIO_H */
