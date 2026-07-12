"""GPS driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class GpsDriver(DriverBase):
    type = "gps"
    is_actuator = False
    required_fields = ["uart_port"]

    def get_headers(self) -> List[str]:
        return ["dal_gps.h"]

    def get_device_type(self) -> str:
        return "dal_gps_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        uart_port = spec["uart_port"]
        baudrate = spec.get("baudrate", 9600)
        rx_buf_size = spec.get("rx_buffer_size", 256)
        return (
            f'    static const dal_gps_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .uart_port = {uart_port},\n'
            f'        .baudrate = {baudrate},\n'
            f'        .rx_buffer_size = {rx_buf_size},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_gps_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_gps_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        baudrate = spec.get("baudrate", 9600)
        return [f"#define {dev_name.upper()}_BAUDRATE {baudrate}u"]
