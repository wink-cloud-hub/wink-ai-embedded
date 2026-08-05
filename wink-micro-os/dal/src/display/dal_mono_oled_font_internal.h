// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_MONO_OLED_FONT_INTERNAL_H
#define DAL_MONO_OLED_FONT_INTERNAL_H

#include <stdint.h>

/** @brief 5x7 glyph width in columns */
#define DAL_MONO_OLED_FONT_GLYPH_BYTES 5

/**
 * @brief Look up character glyph bitmap (returns space glyph for unsupported characters)
 * @param[in] c Character ASCII value.
 * @return Pointer to 5-byte read-only bitmap (never NULL).
 */
const uint8_t *dal_mono_oled_font_glyph(char c);

#endif /* DAL_MONO_OLED_FONT_INTERNAL_H */
