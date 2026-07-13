"""Button driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .base import DriverBase


# Allowed values for the JSON ``event_drive`` field (ADR-0031).
_VALID_EVENT_DRIVES = ("soft_poll", "gpio_irq")
# Documented default when ``debounce_ms`` is omitted (ADR-0031 §决策结论 #3).
_DEFAULT_DEBOUNCE_MS = 20


def _validate_button_spec(dev_name: str, spec: dict) -> None:
    """Enforce ADR-0031 schema rules for a single button device.

    Called from ``ButtonDriver.render_config_init`` so every codegen entry
    point (CLI, unit tests via ``build_context``) picks the check up.

    On violation: raise ``SystemExit(2)`` with a message on stderr. The
    exit code matches other schema validation errors in ``app_codegen``.
    """
    import sys

    drive = spec.get("event_drive", "soft_poll")
    if not isinstance(drive, str) or drive not in _VALID_EVENT_DRIVES:
        print(
            f"error: device '{dev_name}': invalid event_drive: {drive!r} "
            f"(allowed: {list(_VALID_EVENT_DRIVES)})",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if drive == "soft_poll" and spec.get("auto_poll_ms") is None:
        print(
            f"error: device '{dev_name}': event_drive 'soft_poll' requires "
            f"'auto_poll_ms' (ADR-0031)",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if drive == "gpio_irq" and "pin" not in spec and "use_onboard" not in spec:
        print(
            f"error: device '{dev_name}': event_drive 'gpio_irq' requires "
            f"'pin' (or 'use_onboard' that supplies one)",
            file=sys.stderr,
        )
        raise SystemExit(2)
    # Coerce/validate debounce_ms early so a bad type fails codegen (not C).
    deb_raw = spec.get("debounce_ms", _DEFAULT_DEBOUNCE_MS)
    try:
        deb = int(deb_raw)
    except (TypeError, ValueError):
        print(
            f"error: device '{dev_name}': 'debounce_ms' must be an integer "
            f"(got {deb_raw!r})",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if deb < 0:
        print(
            f"error: device '{dev_name}': 'debounce_ms' must be >= 0 "
            f"(got {deb})",
            file=sys.stderr,
        )
        raise SystemExit(2)


class ButtonDriver(DriverBase):
    type = "button"
    is_actuator = False
    required_fields = ["pin"]
    default_role = "binary_sensor"
    role_verbs = {
        "binary_sensor": [
            "is_active",
            "is_active_status",
            "was_active",
            "was_active_status",
            # L1: enable/disable event stream (period from JSON auto_poll_ms)
            "enable_events",
            "disable_events",
            # L2: explicit poll period override
            "start_auto_poll",
            "stop_auto_poll",
        ]
    }

    def get_headers(self) -> List[str]:
        return ["dal_button.h"]

    def get_role_headers(self, role: str) -> List[str]:
        if role == "binary_sensor":
            # events.h is the new L1 path (enable_events / disable_events);
            # helper.h is retained for the L2 verbs start_auto_poll /
            # stop_auto_poll that keep the pre-S2 signature.
            return ["wink_button_events.h", "wink_button_helper.h"]
        return []

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        if role == "binary_sensor":
            if verb == "is_active":
                return f"static inline bool {dev_name}_is_active(void) {{ bool p = false; WINK_IGNORE_RESULT(dal_button_is_pressed(&{dev_name}, &p)); return p; }}"
            elif verb == "is_active_status":
                return f"WINK_WARN_UNUSED_RESULT static inline wink_status_t {dev_name}_is_active_status(bool *out_active) {{ return dal_button_is_pressed(&{dev_name}, out_active); }}"
            elif verb == "was_active":
                return f"static inline bool {dev_name}_was_active(void) {{ bool p = false; WINK_IGNORE_RESULT(dal_button_was_pressed(&{dev_name}, &p)); return p; }}"
            elif verb == "was_active_status":
                return f"WINK_WARN_UNUSED_RESULT static inline wink_status_t {dev_name}_was_active_status(bool *out_pressed) {{ return dal_button_was_pressed(&{dev_name}, out_pressed); }}"
            elif verb == "enable_events":
                # S2: emit a static-const wink_button_event_config_t and
                # call wink_button_events_start. Drive/period/debounce all
                # come from JSON per ADR-0031.
                drive_str = spec.get("event_drive", "soft_poll")
                if drive_str == "gpio_irq":
                    drive_enum = "WINK_BUTTON_DRIVE_GPIO_IRQ"
                else:
                    drive_enum = "WINK_BUTTON_DRIVE_SOFT_POLL"
                # auto_poll_ms: mandatory for soft_poll (validator enforces);
                # for gpio_irq default to 0 so the BAL runtime picks the
                # effective debounce follow-up period.
                ms_raw = spec.get("auto_poll_ms")
                if ms_raw is None:
                    if drive_str == "gpio_irq":
                        ms = 0
                    else:
                        # Shouldn't happen: validator rejects soft_poll
                        # without auto_poll_ms. Emit nothing to avoid a
                        # broken wrapper (parity with S1 behaviour).
                        return ""
                else:
                    ms = int(ms_raw)
                deb = int(spec.get("debounce_ms", _DEFAULT_DEBOUNCE_MS))
                return (
                    f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
                    f"{dev_name}_enable_events(void) {{ "
                    f"static const wink_button_event_config_t cfg = {{ "
                    f".drive = {drive_enum}, "
                    f".auto_poll_ms = {ms}u, "
                    f".debounce_ms = {deb}u, "
                    f".wake_from_sleep = false }}; "
                    f"return wink_button_events_start(&{dev_name}, &cfg); }}"
                )
            elif verb == "disable_events":
                return (
                    f"static inline void {dev_name}_disable_events(void) {{ "
                    f"wink_button_events_stop(&{dev_name}); }}"
                )
            elif verb == "start_auto_poll":
                return f"WINK_WARN_UNUSED_RESULT static inline wink_status_t {dev_name}_start_auto_poll(uint32_t poll_ms) {{ return wink_button_helper_start(&{dev_name}, poll_ms); }}"
            elif verb == "stop_auto_poll":
                return f"static inline void {dev_name}_stop_auto_poll(void) {{ wink_button_helper_stop(&{dev_name}); }}"
        return ""

    def get_device_type(self) -> str:
        return "dal_button_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        # Validate ADR-0031 schema rules here — this is the earliest per-device
        # hook that build_context() calls with the fully resolved spec.
        _validate_button_spec(dev_name, spec)
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
        # Also validate here so entry points that only call render_config_macros
        # (e.g. tests exercising build_context which iterates config_macros
        # before render_config_init) still fail fast on bad input.
        _validate_button_spec(dev_name, spec)
        lines: List[str] = []
        ms_poll = spec.get("auto_poll_ms")
        if ms_poll is not None:
            lines.append(f"#define {dev_name.upper()}_AUTO_POLL_MS {ms_poll}u")
        ms_long = spec.get("long_press_ms")
        if ms_long is not None:
            lines.append(f"#define {dev_name.upper()}_LONG_PRESS_MS {ms_long}u")

        # ADR-0031: debounce_ms is a first-class field (default 20). Emit
        # the macro unconditionally so app / BAL code can reference a
        # canonical value regardless of whether JSON provided one.
        debounce = int(spec.get("debounce_ms", _DEFAULT_DEBOUNCE_MS))
        lines.append(f"#define {dev_name.upper()}_DEBOUNCE_MS {debounce}u")

        # ADR-0031: event_drive selector — emit one of two mutually
        # exclusive marker macros. Match style used by USE_RMT / ACTIVE_HIGH
        # (defined as `true` when active). Downstream code can either check
        # #ifdef {NAME}_EVENT_DRIVE_GPIO_IRQ or read the boolean at compile
        # time — both work with `#define X true` because `true` is a valid
        # preprocessor token that evaluates as truthy in #if.
        drive = spec.get("event_drive", "soft_poll")
        if drive == "gpio_irq":
            lines.append(f"#define {dev_name.upper()}_EVENT_DRIVE_GPIO_IRQ true")
        else:
            lines.append(f"#define {dev_name.upper()}_EVENT_DRIVE_SOFT_POLL true")
        return lines

    def render_deinit(self, dev_name: str) -> str:
        return "dal_button_deinit"
