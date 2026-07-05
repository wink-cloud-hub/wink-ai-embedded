"""Driver plugin base class for app_codegen.

Each DAL driver (led, button, ultrasonic, ...) ships a small plugin that
tells codegen how to render its config struct literal, init call, thunk,
and deinit call. Plugins live in tools/codegen/drivers/<type>.py and
register themselves by subclassing DriverBase (module-level registry
populated via ``__init_subclass__``).
"""
from __future__ import annotations

from typing import List


class DriverBase:
    """Abstract base for device-type codegen plugins."""

    # Subclasses must set:
    type: str = ""                       # "led", "button", "ultrasonic", ...
    is_actuator: bool = False            # generate safe-off thunk + register?
    required_fields: List[str] = []      # JSON fields that must be present

    # ── Registration hook ─────────────────────────────────────────────
    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        # Skip the abstract base itself (type == "").
        if cls.type:
            # Lazy import to avoid circular reference at module load.
            from . import _REGISTRY
            _REGISTRY[cls.type] = cls()

    # ── C emission surface ────────────────────────────────────────────
    def get_headers(self) -> List[str]:
        """C header(s) to #include for this driver (e.g. ["dal_led.h"])."""
        raise NotImplementedError

    def get_device_type(self) -> str:
        """C type name for the device instance (e.g. "dal_led_t")."""
        raise NotImplementedError

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        """Return a C block (static const config + WINK_TRY(init)) for this
        device. Inserted inside wink_device_tree_init() in declaration order.
        """
        raise NotImplementedError

    def render_deinit(self, dev_name: str) -> str:
        """Return the deinit function name (e.g. "dal_led_deinit"). The
        template wraps it in ``WINK_IGNORE_RESULT(<fn>(&<name>))``.
        """
        raise NotImplementedError

    def get_safe_off_fn(self) -> str:
        """Return the DAL safe-off function name for actuator thunk wrapping.
        Defaults to ``dal_<type>_off``; override for drivers with non-trivial
        safe-off semantics (e.g. servo uses ``dal_servo_safe_off``, motor may
        use a brake/de-energize function).
        """
        return f"dal_{self.type}_off"

    # ── Optional hooks (default: no-op) ───────────────────────────────
    def render_post_init_calls(self, dev_name: str, spec: dict) -> List[str]:
        """Extra init lines after the primary init, before actuator register
        (e.g. ``dal_button_set_long_press_ms(&btn, 3000)``). Return C lines
        without a trailing semicolon; the template adds it.
        """
        return []

    def get_service_headers(self, dev_name: str, spec: dict) -> List[str]:
        """BAL/helper headers required by services attached to this device."""
        return []

    def render_service_starts(self, dev_name: str, spec: dict) -> List[str]:
        """C lines inserted into wink_app_services_start() (without ``;``)."""
        return []

    def cmake_options(self) -> List[str]:
        """CMake option names this driver requires (default: WINK_USE_<TYPE>)."""
        return [f"WINK_USE_{self.type.upper()}"]
