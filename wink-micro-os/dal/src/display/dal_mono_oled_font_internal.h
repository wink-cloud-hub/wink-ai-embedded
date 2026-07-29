#ifndef DAL_MONO_OLED_FONT_INTERNAL_H
#define DAL_MONO_OLED_FONT_INTERNAL_H

#include <stdint.h>

/** @brief 5×7 字形宽度（列数）；与 dal_mono_oled.h MONO_OLED_FONT_GLYPH_W 一致。 */
#define DAL_MONO_OLED_FONT_GLYPH_BYTES 5

/**
 * @brief 查找字符点阵；不支持的字符返回空格字形。
 * @return 指向 5 字节只读点阵的指针（永不为 NULL）。
 */
const uint8_t *dal_mono_oled_font_glyph(char c);

#endif /* DAL_MONO_OLED_FONT_INTERNAL_H */
