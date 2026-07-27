"""Button driver plugin for app_codegen."""
from __future__ import annotations

from typing import List, Optional

from .advanced import parse_advanced, require_string_enum
from .base import DriverBase, DriverCategory


# Allowed values for the JSON ``event_drive`` field (ADR-0031).
_VALID_EVENT_DRIVES = ("soft_poll", "gpio_irq")
# Documented default when ``debounce_ms`` is omitted (ADR-0031 §决策结论 #3).
_DEFAULT_DEBOUNCE_MS = 20

_VALID_PULLS = frozenset({"auto", "up", "down", "none"})
_PULL_TO_C = {
    "auto": "DAL_BUTTON_PULL_AUTO",
    "up": "DAL_BUTTON_PULL_UP",
    "down": "DAL_BUTTON_PULL_DOWN",
    "none": "DAL_BUTTON_PULL_NONE",
}


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
    if drive == "gpio_irq" and "gpio_pin" not in spec and "use_onboard" not in spec:
        print(
            f"error: device '{dev_name}': event_drive 'gpio_irq' requires "
            f"'gpio_pin' (or 'use_onboard' that supplies one)",
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

    # ADR-0034: advanced.pull only (no top-level pull alias).
    parse_advanced(
        dev_name,
        spec,
        allowed_keys=frozenset({"pull"}),
        top_level_aliases=frozenset({"pull"}),
    )


def _button_pull_c(dev_name: str, spec: dict) -> Optional[str]:
    """Return C enum token for advanced.pull, or None when omitted (= AUTO)."""
    adv = parse_advanced(
        dev_name,
        spec,
        allowed_keys=frozenset({"pull"}),
        top_level_aliases=frozenset({"pull"}),
    )
    if "pull" not in adv:
        return None
    pull = require_string_enum(dev_name, "pull", adv["pull"], _VALID_PULLS)
    return _PULL_TO_C[pull]


class ButtonDriver(DriverBase):
    type = "button"
    category = DriverCategory.INPUT
    is_actuator = False
    required_fields = ["gpio_pin"]
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
            # events.h is the single event API path — both L1 verbs
            # (enable_events / disable_events) and the L2 verbs
            # (start_auto_poll / stop_auto_poll) generate calls into it.
            # ADR-0038: PUBLIC include is bal/include root only — domain prefix required.
            return ["input/wink_button_events.h"]
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
                # ADR-0032 (B-class): emit a static-const
                # wink_button_event_config_t and call the primary
                # `wink_button_enable_events`. Drive/period/debounce all
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
                    f"return wink_button_enable_events(&{dev_name}, &cfg); }}"
                )
            elif verb == "disable_events":
                return (
                    f"static inline void {dev_name}_disable_events(void) {{ "
                    f"wink_button_disable_events(&{dev_name}); }}"
                )
            elif verb == "start_auto_poll":
                # ADR-0032 L2 A-class shim: constructs a local soft-poll cfg
                # and forwards to the primary `wink_button_enable_events`.
                # `poll_ms` overrides the JSON auto_poll_ms at call time;
                # debounce comes from the compile-time
                # `{NAME}_DEBOUNCE_MS` macro emitted by render_config_macros.
                return (
                    f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
                    f"{dev_name}_start_auto_poll(uint32_t poll_ms) {{ "
                    f"const wink_button_event_config_t cfg = {{ "
                    f".drive = WINK_BUTTON_DRIVE_SOFT_POLL, "
                    f".auto_poll_ms = poll_ms, "
                    f".debounce_ms = {dev_name.upper()}_DEBOUNCE_MS, "
                    f".wake_from_sleep = false }}; "
                    f"return wink_button_enable_events(&{dev_name}, &cfg); }}"
                )
            elif verb == "stop_auto_poll":
                return (
                    f"static inline void {dev_name}_stop_auto_poll(void) {{ "
                    f"wink_button_disable_events(&{dev_name}); }}"
                )
        return ""

    def get_device_type(self) -> str:
        return "dal_button_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        # Validate ADR-0031 schema rules here — this is the earliest per-device
        # hook that build_context() calls with the fully resolved spec.
        _validate_button_spec(dev_name, spec)
        pin = spec["gpio_pin"]
        active_low_c = "true" if spec.get("active_low", True) else "false"
        pull_c = _button_pull_c(dev_name, spec)
        lines = [
            f'    static const dal_button_config_t {dev_name}_cfg = {{',
            f'        .owner = "{dev_name}",',
            f'        .pin = {pin},',
            f'        .active_low = {active_low_c},',
        ]
        # ADR-0034: emit .pull only when advanced.pull is explicit (0 = AUTO).
        if pull_c is not None:
            lines.append(f'        .pull = {pull_c},')
        lines.append('    };')
        lines.append(f'    WINK_TRY(dal_button_init(&{dev_name}, &{dev_name}_cfg));')
        return '\n'.join(lines)

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
