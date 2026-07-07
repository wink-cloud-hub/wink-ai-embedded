"""Button driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class ButtonDriver(DriverBase):
    type = "button"
    is_actuator = False
    required_fields = ["pin"]

    def get_headers(self) -> List[str]:
        return ["dal_button.h"]

    def get_device_type(self) -> str:
        return "dal_button_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pin = spec["pin"]
        active_low_c = "true" if spec.get("active_low", True) else "false"
        return (
            f'    static const dal_button_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .pin = {pin},\n'
            f'        .active_low = {active_low_c},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_button_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_post_init_calls(self, dev_name: str, spec: dict) -> List[str]:
        lines: List[str] = []
        ms = spec.get("long_press_ms")
        if ms is not None:
            lines.append(f"dal_button_set_long_press_ms(&{dev_name}, {ms})")
        if spec.get("isr_counter", False):
            lines.append(f"dal_button_enable_isr_counter(&{dev_name})")
        return lines

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        lines: List[str] = []
        ms_poll = spec.get("auto_poll_ms")
        if ms_poll is not None:
            lines.append(f"#define {dev_name.upper()}_AUTO_POLL_MS {ms_poll}u")
        ms_long = spec.get("long_press_ms")
        if ms_long is not None:
            lines.append(f"#define {dev_name.upper()}_LONG_PRESS_MS {ms_long}u")
        return lines

    def render_deinit(self, dev_name: str) -> str:
        return "dal_button_deinit"
