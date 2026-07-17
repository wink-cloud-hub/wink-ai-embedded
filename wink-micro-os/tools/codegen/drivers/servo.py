"""Servo driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


class ServoDriver(DriverBase):
    type = "servo"
    is_actuator = True
    required_fields = ["pwm_channel"]
    default_role = "angular_actuator"
    role_verbs = {
        "angular_actuator": ["set_angle"],
    }

    def get_headers(self) -> List[str]:
        return ["dal_servo.h"]

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        del spec  # Role wrappers ignore per-device knobs for set_angle.
        if role == "angular_actuator" and verb == "set_angle":
            return (
                f"static inline void {dev_name}_set_angle(float angle) {{ "
                f"WINK_IGNORE_RESULT(dal_servo_set_angle(&{dev_name}, angle)); }}"
            )
        return ""

    def get_device_type(self) -> str:
        return "dal_servo_t"

    def get_safe_off_fn(self) -> str:
        return "dal_servo_safe_off"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pwm_channel = spec["pwm_channel"]
        min_pulse = spec.get("min_pulse_ms", 0.5)
        max_pulse = spec.get("max_pulse_ms", 2.5)
        return (
            f'    static const dal_servo_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .pwm_channel = {pwm_channel},\n'
            f'        .min_pulse_ms = {min_pulse}f,\n'
            f'        .max_pulse_ms = {max_pulse}f,\n'
            f'    }};\n'
            f'    WINK_TRY(dal_servo_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_servo_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        macros: List[str] = []
        min_pulse = spec.get("min_pulse_ms")
        if min_pulse is not None:
            macros.append(f"#define {dev_name.upper()}_MIN_PULSE_MS {min_pulse}f")
        max_pulse = spec.get("max_pulse_ms")
        if max_pulse is not None:
            macros.append(f"#define {dev_name.upper()}_MAX_PULSE_MS {max_pulse}f")
        return macros
