"""LED driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class LedDriver(DriverBase):
    type = "led"
    is_actuator = True
    required_fields = ["gpio_pin"]
    default_role = "binary_indicator"
    role_verbs = {
        "binary_indicator": ["activate", "deactivate", "toggle"]
    }

    def get_headers(self) -> List[str]:
        return ["dal_led.h"]

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        if role == "binary_indicator":
            if verb == "activate":
                return f"static inline void {dev_name}_activate(void) {{ WINK_IGNORE_RESULT(dal_led_on(&{dev_name})); }}"
            elif verb == "deactivate":
                return f"static inline void {dev_name}_deactivate(void) {{ WINK_IGNORE_RESULT(dal_led_off(&{dev_name})); }}"
            elif verb == "toggle":
                return f"static inline void {dev_name}_toggle(void) {{ WINK_IGNORE_RESULT(dal_led_toggle(&{dev_name})); }}"
        return ""

    def get_device_type(self) -> str:
        return "dal_led_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pin = spec["gpio_pin"]
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

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        active_high = spec.get("active_high", True)
        active_high_c = "true" if active_high else "false"
        return [f"#define {dev_name.upper()}_ACTIVE_HIGH {active_high_c}"]
