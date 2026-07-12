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


def main():
    parser = argparse.ArgumentParser(description="Generate wink_config.h")
    parser.add_argument("--input", required=True, help="Path to wink_app.json")
    parser.add_argument("--output", required=True, help="Path to output wink_config.h")
    parser.add_argument("--target", default="esp32", help="Target platform")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        config = json.load(f)

    tick_ms = config.get("tick_ms", 10)
    max_timers = config.get("max_soft_timers", 16)
    pwm_channels = config.get("pwm_channels", 8)

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
 * Source: {os.path.basename(args.input)}
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
