"""Motor driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase, DriverCategory

_DEFAULT_PWM_FREQ_HZ = 20000
_DEFAULT_DIR_PIN_B = -1


class MotorDriver(DriverBase):
    type = "motor"
    category = DriverCategory.ACTUATOR
    is_actuator = True
    required_fields = ["pwm_channel", "dir_pin_a"]

    def get_headers(self) -> List[str]:
        return ["dal_motor.h"]

    def get_device_type(self) -> str:
        return "dal_motor_t"

    def get_safe_off_fn(self) -> str:
        return "dal_motor_safe_off"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pwm_channel = spec["pwm_channel"]
        dir_pin_a = spec["dir_pin_a"]
        dir_pin_b = spec.get("dir_pin_b", _DEFAULT_DIR_PIN_B)
        pwm_freq_hz = spec.get("pwm_freq_hz", _DEFAULT_PWM_FREQ_HZ)
        return (
            f'    static const dal_motor_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .pwm_channel = {pwm_channel},\n'
            f'        .dir_pin_a = {dir_pin_a},\n'
            f'        .dir_pin_b = {dir_pin_b},\n'
            f'        .pwm_freq_hz = {pwm_freq_hz}u,\n'
            f'    }};\n'
            f'    WINK_TRY(dal_motor_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_motor_deinit"
