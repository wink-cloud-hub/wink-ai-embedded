"""Servo driver plugin for app_codegen."""
from __future__ import annotations

from typing import List, Optional, Tuple

from .advanced import parse_advanced, require_int, require_string_enum
from .base import DriverBase, DriverCategory


_VALID_CLOCK = frozenset({"auto", "stable_required"})
_CLOCK_TO_C = {
    "auto": "DAL_SERVO_CLOCK_AUTO",
    "stable_required": "DAL_SERVO_CLOCK_STABLE_REQUIRED",
}
# Broad bounds matching ESP32 LEDC common range; PAL still does final check.
_BITS_MIN = 1
_BITS_MAX = 20


def _validate_servo_advanced(
    dev_name: str, spec: dict
) -> Tuple[Optional[int], Optional[str]]:
    """Return (resolution_bits | None, clock_c_enum | None)."""
    adv = parse_advanced(
        dev_name,
        spec,
        allowed_keys=frozenset({"resolution_bits", "clock_requirement"}),
        top_level_aliases=frozenset({"resolution_bits", "clock_requirement"}),
    )
    bits: Optional[int] = None
    clock_c: Optional[str] = None
    if "resolution_bits" in adv:
        bits = require_int(
            dev_name,
            "resolution_bits",
            adv["resolution_bits"],
            min_v=_BITS_MIN,
            max_v=_BITS_MAX,
        )
    if "clock_requirement" in adv:
        clock = require_string_enum(
            dev_name, "clock_requirement", adv["clock_requirement"], _VALID_CLOCK
        )
        clock_c = _CLOCK_TO_C[clock]
    return bits, clock_c


class ServoDriver(DriverBase):
    type = "servo"
    category = DriverCategory.ACTUATOR
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
        bits, clock_c = _validate_servo_advanced(dev_name, spec)

        lines = [
            f'    static const dal_servo_config_t {dev_name}_cfg = {{',
            f'        .owner = "{dev_name}",',
            f'        .pwm_channel = {pwm_channel},',
        ]
        # ADR-0034: emit advanced fields only when explicit (0 = AUTO).
        if bits is not None:
            lines.append(f'        .resolution_bits = {bits}u,')
        if clock_c is not None:
            lines.append(f'        .clock_requirement = {clock_c},')
        lines.extend([
            f'        .min_pulse_ms = {min_pulse}f,',
            f'        .max_pulse_ms = {max_pulse}f,',
            '    };',
            f'    WINK_TRY(dal_servo_init(&{dev_name}, &{dev_name}_cfg));',
        ])
        return '\n'.join(lines)

    def render_deinit(self, dev_name: str) -> str:
        return "dal_servo_deinit"

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        # Validate advanced early so build_context fails before emit.
        _validate_servo_advanced(dev_name, spec)
        macros: List[str] = []
        min_pulse = spec.get("min_pulse_ms")
        if min_pulse is not None:
            macros.append(f"#define {dev_name.upper()}_MIN_PULSE_MS {min_pulse}f")
        max_pulse = spec.get("max_pulse_ms")
        if max_pulse is not None:
            macros.append(f"#define {dev_name.upper()}_MAX_PULSE_MS {max_pulse}f")
        return macros
