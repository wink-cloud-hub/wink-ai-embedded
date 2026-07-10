"""SSD1306 OLED display driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class Ssd1306Driver(DriverBase):
    type = "ssd1306"
    is_actuator = False
    required_fields = ["i2c_port"]

    def get_headers(self) -> List[str]:
        return ["dal_ssd1306.h"]

    def get_device_type(self) -> str:
        return "dal_ssd1306_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        i2c_port = spec["i2c_port"]
        i2c_addr = spec.get("i2c_addr", 0x3C)
        width = spec.get("width", 128)
        height = spec.get("height", 64)
        return (
            f'    static const dal_ssd1306_config_t {dev_name}_cfg = {{\n'
            f'        .i2c_port = {i2c_port},\n'
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
