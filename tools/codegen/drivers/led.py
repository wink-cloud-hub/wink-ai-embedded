"""LED driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class LedDriver(DriverBase):
    type = "led"
    is_actuator = True
    required_fields = ["pin"]

    def get_headers(self) -> List[str]:
        return ["dal_led.h"]

    def get_device_type(self) -> str:
        return "dal_led_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pin = spec["pin"]
        active_high = spec.get("active_high", True)
        active_high_c = "true" if active_high else "false"
        owner = dev_name
        return (
            f'    static const dal_led_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{owner}",\n'
            f'        .pin = {pin},\n'
            f'        .active_high = {active_high_c},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_led_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_led_deinit"
