"""Ultrasonic driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase, DriverCategory

_MIN_DISTANCE_EVENT_PERIOD_MS = 50


def _validate_ultrasonic_spec(dev_name: str, spec: dict) -> None:
    """ADR-0033: auto_poll_ms defaults to 50; values < 50 are ERROR."""
    raw = spec.get("auto_poll_ms")
    if raw is None:
        return
    try:
        ms = int(raw)
    except (TypeError, ValueError) as exc:
        raise SystemExit(
            f"ultrasonic '{dev_name}': auto_poll_ms must be an integer"
        ) from exc
    if ms < _MIN_DISTANCE_EVENT_PERIOD_MS:
        raise SystemExit(
            f"ultrasonic '{dev_name}': auto_poll_ms={ms} < "
            f"{_MIN_DISTANCE_EVENT_PERIOD_MS} (HC-SR04 crosstalk budget, ADR-0033)"
        )


class UltrasonicDriver(DriverBase):
    type = "ultrasonic"
    category = DriverCategory.SENSOR
    is_actuator = False
    required_fields = ["trig_pin", "echo_pin"]
    default_role = "distance_sensor"
    role_verbs = {
        "distance_sensor": [
            "request_measurement",
            "read_distance",
            "read_distance_status",
            "enable_distance_events",
            "disable_distance_events",
        ]
    }

    def get_headers(self) -> List[str]:
        return ["dal_ultrasonic.h"]

    def get_role_headers(self, role: str) -> List[str]:
        if role == "distance_sensor":
            # ADR-0038: PUBLIC include is bal/include root only — domain prefix required.
            return ["sensor/wink_ultrasonic_distance_events.h"]
        return []

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        if role == "distance_sensor":
            if verb == "request_measurement":
                return (
                    f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
                    f"{dev_name}_request_measurement(void) {{ "
                    f"return dal_ultrasonic_request_measurement(&{dev_name}); }}"
                )
            if verb == "read_distance":
                return (
                    f"static inline float {dev_name}_read_distance(void) {{ "
                    f"float d = -1.0f; "
                    f"WINK_IGNORE_RESULT(dal_ultrasonic_get_cached_distance(&{dev_name}, &d)); "
                    f"return d; }}"
                )
            if verb == "read_distance_status":
                return (
                    f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
                    f"{dev_name}_read_distance_status(float *out_dist_cm) {{ "
                    f"return dal_ultrasonic_get_cached_distance(&{dev_name}, out_dist_cm); }}"
                )
            if verb == "enable_distance_events":
                _validate_ultrasonic_spec(dev_name, spec)
                ms = int(spec.get("auto_poll_ms", _MIN_DISTANCE_EVENT_PERIOD_MS))
                return (
                    f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
                    f"{dev_name}_enable_distance_events(void) {{ "
                    f"static const wink_ultrasonic_distance_event_config_t cfg = {{ "
                    f".period_ms = {ms}u }}; "
                    f"return wink_ultrasonic_enable_distance_events(&{dev_name}, &cfg); }}"
                )
            if verb == "disable_distance_events":
                return (
                    f"static inline void {dev_name}_disable_distance_events(void) {{ "
                    f"wink_ultrasonic_disable_distance_events(&{dev_name}); }}"
                )
        return ""

    def get_device_type(self) -> str:
        return "dal_ultrasonic_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        _validate_ultrasonic_spec(dev_name, spec)
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
        macros = [f"#define {dev_name.upper()}_USE_RMT {use_rmt_c}"]
        ms = int(spec.get("auto_poll_ms", _MIN_DISTANCE_EVENT_PERIOD_MS))
        macros.append(f"#define {dev_name.upper()}_AUTO_POLL_MS {ms}u")
        return macros
