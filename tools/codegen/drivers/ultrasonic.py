"""Ultrasonic driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class UltrasonicDriver(DriverBase):
    type = "ultrasonic"
    is_actuator = False
    required_fields = ["trig_pin", "echo_pin"]

    def get_headers(self) -> List[str]:
        return ["dal_ultrasonic.h"]

    def get_device_type(self) -> str:
        return "dal_ultrasonic_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        trig = spec["trig_pin"]
        echo = spec["echo_pin"]
        use_rmt = spec.get("use_rmt", True)
        use_rmt_c = "true" if use_rmt else "false"
        owner = dev_name
        return (
            f'    static const dal_ultrasonic_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{owner}",\n'
            f'        .trig_pin = {trig},\n'
            f'        .echo_pin = {echo},\n'
            f'        .use_rmt = {use_rmt_c},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_ultrasonic_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_ultrasonic_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        use_rmt = spec.get("use_rmt", True)
        use_rmt_c = "true" if use_rmt else "false"
        return [f"#define {dev_name.upper()}_USE_RMT {use_rmt_c}"]
