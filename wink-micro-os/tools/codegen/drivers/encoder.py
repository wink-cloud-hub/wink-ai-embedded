"""Encoder driver plugin for app_codegen."""
from __future__ import annotations

from typing import List

from .advanced import require_string_enum
from .base import DriverBase

_DEFAULT_PIN_B = -1
_DEFAULT_PULL = "PAL_GPIO_INPUT_PULLUP"

_VALID_PULLS = frozenset({"up", "down", "none"})
_PULL_TO_C = {
    "up": "PAL_GPIO_INPUT_PULLUP",
    "down": "PAL_GPIO_INPUT_PULLDOWN",
    "none": "PAL_GPIO_INPUT",
}


def _encoder_pull_c(dev_name: str, spec: dict) -> str:
    """Return C enum token for pull; default PAL_GPIO_INPUT_PULLUP."""
    pull = spec.get("pull")
    if pull is None:
        return _DEFAULT_PULL
    if isinstance(pull, str) and pull.startswith("PAL_GPIO_"):
        return pull
    return _PULL_TO_C[
        require_string_enum(dev_name, "pull", pull, _VALID_PULLS)
    ]


class EncoderDriver(DriverBase):
    type = "encoder"
    is_actuator = False
    required_fields = ["pin_a"]

    def get_headers(self) -> List[str]:
        return ["dal_encoder.h"]

    def get_device_type(self) -> str:
        return "dal_encoder_t"

    def render_config_init(self, dev_name: str, spec: dict) -> str:
        pin_a = spec["pin_a"]
        pin_b = spec.get("pin_b", _DEFAULT_PIN_B)
        pull_c = _encoder_pull_c(dev_name, spec)
        return (
            f'    static const dal_encoder_config_t {dev_name}_cfg = {{\n'
            f'        .owner = "{dev_name}",\n'
            f'        .pin_a = {pin_a},\n'
            f'        .pin_b = {pin_b},\n'
            f'        .pull = {pull_c},\n'
            f'    }};\n'
            f'    WINK_TRY(dal_encoder_init(&{dev_name}, &{dev_name}_cfg));'
        )

    def render_deinit(self, dev_name: str) -> str:
        return "dal_encoder_deinit"
