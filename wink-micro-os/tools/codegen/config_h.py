#!/usr/bin/env python3
"""Generate wink_config.h from wink_app.json.

Parses application configuration JSON and generates a C header file with
compile-time configuration macros. Ensures single source of truth (SSOT)
for runtime parameters like tick period and soft timer limits.
"""
import json
import argparse
import os
import sys

# Binary SDK pack ceilings (ADR-0028 §5)
_BINARY_MAX_SOFT_TIMERS = 32
_BINARY_PWM_CHANNELS = 16


def _resolve_sdk_mode(args):
    """Return effective SDK mode from CLI flag or WINK_SDK_MODE env."""
    if args.sdk_mode:
        return args.sdk_mode
    env_mode = os.environ.get("WINK_SDK_MODE", "").strip().lower()
    if env_mode in ("binary", "source"):
        return env_mode
    return None


def _enforce_binary_ceilings(max_timers, pwm_channels):
    """Fail if consumer config exceeds Binary SDK pack ceilings (ADR-0028 §5)."""
    errors = []
    if max_timers > _BINARY_MAX_SOFT_TIMERS:
        errors.append(
            f"max_soft_timers={max_timers} exceeds Binary SDK ceiling "
            f"({_BINARY_MAX_SOFT_TIMERS}); use Source SDK or reduce the value."
        )
    if pwm_channels > _BINARY_PWM_CHANNELS:
        errors.append(
            f"pwm_channels={pwm_channels} exceeds Binary SDK ceiling "
            f"({_BINARY_PWM_CHANNELS}); use Source SDK or reduce the value."
        )
    if errors:
        print("[config_h] Binary SDK config ceiling violation (ADR-0028):", file=sys.stderr)
        for msg in errors:
            print(f"  - {msg}", file=sys.stderr)
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description="Generate wink_config.h")
    parser.add_argument("--input", required=False, default=None, help="Path to wink_app.json")
    parser.add_argument("--output", required=True, help="Path to output wink_config.h")
    parser.add_argument("--target", default="esp32", help="Target platform")
    parser.add_argument(
        "--sdk-mode",
        choices=["source", "binary"],
        default=None,
        help="SDK consumption mode; binary enforces pack config ceilings (ADR-0028)",
    )
    args = parser.parse_args()

    config = {}
    if args.input and os.path.exists(args.input):
        with open(args.input, "r", encoding="utf-8") as f:
            config = json.load(f)
    else:
        print("[config_h] No input file or input file not found. Falling back to default configuration.")

    tick_ms = config.get("tick_ms", 10)
    max_timers = config.get("max_soft_timers", 16)
    pwm_channels = config.get("pwm_channels", 8)

    if _resolve_sdk_mode(args) == "binary":
        _enforce_binary_ceilings(max_timers, pwm_channels)

    # Map target names to platform macros (consistent with existing convention)
    target_macro = {
        "esp32": "WINK_TARGET_ESP32",
        "wasm": "WINK_TARGET_WASM",
        "host": "WINK_TARGET_HOST",
        "baremetal": "WINK_TARGET_BAREMETAL",
    }.get(args.target, "WINK_TARGET_UNKNOWN")

    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)

    # Generate header content - follows project naming conventions
    content = f"""/**
 * @file wink_config.h
 * @brief Auto-generated compile-time configuration from wink_app.json.
 *
 * THIS FILE IS AUTO-GENERATED - DO NOT EDIT MANUALLY!
 * Any manual changes will be OVERWRITTEN on the next build.
 *
 * Source: {os.path.basename(args.input) if args.input else "None (default)"}
 * Target: {args.target}
 *
 * Configuration Single Source of Truth (SSOT):
 *   - WINK_RUNTIME_TICK_MS: Main loop tick period
 *   - WINK_MAX_SOFT_TIMERS: Maximum concurrent soft timers
 *   - WINK_TARGET_*: Platform identification macro
 *   - PAL_PWM_CHANNELS: Number of PWM channels for motor control
 */

#ifndef WINK_CONFIG_H
#define WINK_CONFIG_H

#undef WINK_RUNTIME_TICK_MS
#define WINK_RUNTIME_TICK_MS    ({tick_ms}U)

#undef WINK_MAX_SOFT_TIMERS
#define WINK_MAX_SOFT_TIMERS    ({max_timers}U)

#undef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS        ({pwm_channels}U)

#define {target_macro}           (1)

#endif /* WINK_CONFIG_H */
"""

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[OK] Generated {args.output}")
    print(f"     tick_ms = {tick_ms}, max_soft_timers = {max_timers}, target = {args.target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
