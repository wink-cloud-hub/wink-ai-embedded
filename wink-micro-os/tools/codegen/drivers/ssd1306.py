"""SSD1306 OLED display driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase, DriverCategory

# ADR-0046: font TU + CACHE split for --mode=source vs --mode=defs.
_SSD1306_EXTRA_CMAKE_DEFS = """\
set(WINK_SSD1306_FONT "ascii_upper" CACHE STRING
    "SSD1306 5x7 font: minimal | ascii_upper")
if(WINK_SSD1306_FONT STREQUAL "minimal")
  target_compile_definitions(${WINK_DAL_TARGET} PUBLIC WINK_SSD1306_FONT_MINIMAL=1)
elseif(WINK_SSD1306_FONT STREQUAL "ascii_upper")
  target_compile_definitions(${WINK_DAL_TARGET} PUBLIC WINK_SSD1306_FONT_ASCII_UPPER=1)
else()
  message(FATAL_ERROR "WINK_SSD1306_FONT must be 'minimal' or 'ascii_upper'")
endif()
"""

_SSD1306_EXTRA_CMAKE_SOURCES = """\
if(WINK_SSD1306_FONT STREQUAL "minimal")
  target_sources(${WINK_DAL_TARGET} PRIVATE
      ${WINK_MICRO_OS_ROOT}/dal/src/display/dal_ssd1306_font_5x7_minimal.c)
elseif(WINK_SSD1306_FONT STREQUAL "ascii_upper")
  target_sources(${WINK_DAL_TARGET} PRIVATE
      ${WINK_MICRO_OS_ROOT}/dal/src/display/dal_ssd1306_font_5x7_ascii_upper.c)
endif()
"""


class Ssd1306Driver(DriverBase):
    type = "ssd1306"
    category = DriverCategory.DISPLAY
    is_actuator = False
    required_fields = ["i2c_bus"]
    extra_cmake_defs = _SSD1306_EXTRA_CMAKE_DEFS
    extra_cmake_sources = _SSD1306_EXTRA_CMAKE_SOURCES
    default_role = "text_display"
    role_verbs = {
        "text_display": ["clear", "draw_text", "flush"]
    }

    def get_headers(self) -> List[str]:
        return ["dal_ssd1306.h"]

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        if role == "text_display":
            if verb == "clear":
                return f"static inline void {dev_name}_clear(void) {{ WINK_IGNORE_RESULT(dal_ssd1306_clear(&{dev_name})); }}"
            elif verb == "draw_text":
                return f"static inline void {dev_name}_draw_text(uint16_t col, uint8_t page, const char *str) {{ WINK_IGNORE_RESULT(dal_ssd1306_draw_text(&{dev_name}, col, page, str)); }}"
            elif verb == "flush":
                return f"static inline void {dev_name}_flush(void) {{ WINK_IGNORE_RESULT(dal_ssd1306_flush(&{dev_name})); }}"
        return ""

    def get_device_type(self) -> str:
        return "dal_ssd1306_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        i2c_bus = spec["i2c_bus"]
        i2c_addr = spec.get("i2c_addr", 0x3C)
        width = spec.get("width", 128)
        height = spec.get("height", 64)
        return (
            f'    static const dal_ssd1306_config_t {dev_name}_cfg = {{\n'
            f'        .i2c_port = {i2c_bus},\n'
            f'        .i2c_addr = 0x{i2c_addr:X},\n'
            f'        .width = {width},\n'
            f'        .height = {height},\n'
            f'        .owner = "{dev_name}",\n'
            f'    }};\n'
            f'    WINK_TRY(dal_ssd1306_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_ssd1306_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        i2c_addr = spec.get("i2c_addr", 0x3C)
        return [f"#define {dev_name.upper()}_I2C_ADDR 0x{i2c_addr:X}"]
