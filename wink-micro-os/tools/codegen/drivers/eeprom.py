"""EEPROM driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class EepromDriver(DriverBase):
    type = "eeprom"
    is_actuator = False
    required_fields = ["i2c_bus"]

    def get_headers(self) -> List[str]:
        return ["dal_eeprom.h"]

    def get_device_type(self) -> str:
        return "dal_eeprom_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        i2c_bus = spec["i2c_bus"]
        i2c_addr = spec.get("i2c_addr", 0x50)
        capacity = spec.get("capacity_bytes", 256)
        page_size = spec.get("page_size_bytes", 8)
        write_time = spec.get("write_time_ms", 5)
        return (
            f'    static const dal_eeprom_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .i2c_port = {i2c_bus},\n'
            f'        .i2c_addr = 0x{i2c_addr:X},\n'
            f'        .capacity_bytes = {capacity},\n'
            f'        .page_size = {page_size},\n'
            f'        .write_time_ms = {write_time},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_eeprom_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_eeprom_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        capacity = spec.get("capacity_bytes", 256)
        return [f"#define {dev_name.upper()}_CAPACITY_BYTES {capacity}u"]
