// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_audio.c
 * @brief DAL Audio placeholder implementation (ADR-0012 honest contract).
 */
#include "output/dal_audio.h"

wink_status_t dal_audio_init(dal_audio_t *dev, const dal_audio_config_t *config) {
    (void)dev;
    (void)config;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_audio_deinit(dal_audio_t *dev) {
    (void)dev;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_audio_start_stream(dal_audio_t *dev) {
    (void)dev;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_audio_stop_stream(dal_audio_t *dev) {
    (void)dev;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t dal_audio_write(dal_audio_t *dev, const void *samples, size_t num_samples) {
    (void)dev;
    (void)samples;
    (void)num_samples;
    return WINK_ERR_UNSUPPORTED;
}
