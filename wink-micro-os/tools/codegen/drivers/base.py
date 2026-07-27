"""Driver plugin base class for app_codegen.

Each DAL driver (led, button, ultrasonic, ...) ships a small plugin that
tells codegen how to render its config struct literal, init call, thunk,
and deinit call. Plugins live in tools/codegen/drivers/<type>.py and
register themselves by subclassing DriverBase (module-level registry
populated via ``__init_subclass__``).

ADR-0046: registry is the SSOT for the DAL driver universe (category,
source path, optional extra CMake fragments).
"""
from __future__ import annotations

from enum import Enum
from typing import List, Union


class DriverCategory(str, Enum):
    """DAL source/include subdirectory under dal/{src,include}/."""

    OUTPUT = "output"
    INPUT = "input"
    ACTUATOR = "actuator"
    SENSOR = "sensor"
    DISPLAY = "display"
    COMMUNICATION = "communication"
    STORAGE = "storage"


class DriverBase:
    """Abstract base for device-type codegen plugins."""

    # Subclasses must set:
    type: str = ""                       # "led", "button", "ultrasonic", ...
    is_actuator: bool = False            # generate safe-off thunk + register?
    required_fields: List[str] = []      # JSON fields that must be present

    # ADR-0046 metadata (required for registered drivers):
    category: Union[DriverCategory, str] = ""
    source_stem: str = ""                # default == type → dal_<stem>.c
    # Multi-TU / sub-options: split for --mode=source vs --mode=defs (ADR-0046).
    extra_cmake_defs: str = ""           # CACHE / compile definitions
    extra_cmake_sources: str = ""        # target_sources (source builds only)

    # ── Registration hook ─────────────────────────────────────────────
    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        # Skip the abstract base itself (type == "").
        if not cls.type:
            return
        cat = cls.category
        if isinstance(cat, DriverCategory):
            pass
        elif isinstance(cat, str) and cat:
            try:
                cls.category = DriverCategory(cat)
            except ValueError as exc:
                raise TypeError(
                    f"driver {cls.__name__!r} (type={cls.type!r}): "
                    f"category {cat!r} is not a DriverCategory value"
                ) from exc
        else:
            raise TypeError(
                f"driver {cls.__name__!r} (type={cls.type!r}): "
                f"category is required and must be a DriverCategory"
            )
        # Lazy import to avoid circular reference at module load.
        from . import _REGISTRY
        _REGISTRY[cls.type] = cls()

    def resolved_stem(self) -> str:
        return self.source_stem or self.type

    def resolved_category(self) -> str:
        cat = self.category
        return cat.value if isinstance(cat, DriverCategory) else str(cat)

    def rel_src(self) -> str:
        return f"dal/src/{self.resolved_category()}/dal_{self.resolved_stem()}.c"

    def rel_hdr(self) -> str:
        return (
            f"dal/include/{self.resolved_category()}/"
            f"dal_{self.resolved_stem()}.h"
        )

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

    def render_config_macros(self, dev_name: str, spec: dict) -> List[str]:
        """Return C macros to be defined in device_tree.h for this device."""
        return []

    def cmake_options(self) -> List[str]:
        """CMake option names this driver requires (default: WINK_USE_<TYPE>)."""
        return [f"WINK_USE_{self.type.upper()}"]

    # ── Role Interface hooks (Phase 1 app optimization) ─────────────────
    default_role: str = ""
    role_verbs: dict[str, list[str]] = {}  # e.g. {"binary_indicator": ["activate", ...]}

    def get_role_headers(self, role: str) -> List[str]:
        """Return additional C headers required by wrappers for this role
        (e.g. ["input/wink_button_events.h"] — ADR-0038 domain-prefixed)."""
        return []

    def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
        """Render a C static inline helper wrapper block for a given role and verb."""
        return ""
