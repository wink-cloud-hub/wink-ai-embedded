<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/03-codegen-guide.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 03 · Codegen Guide

WinkMicroOS uses data-driven code generation: readable JSON descriptors are parsed at build time to generate the device tree, Arduino-compatible bindings and build configuration.

---

## 1. Generated artifacts

Running `winkcli build host --app <app>` (or `winkcli gen app-schema --app <app>`) produces, in the build tree:

| Artifact | Description |
|---|---|
| `device_tree.h / .c` | DAL device instance array and the global device-tree API |
| `wink_arduino_bindings.h / .cpp` | Arduino-compatible API (`pinMode` / `digitalWrite`, …) |
| `wink_config.h` | Macros for tick period, soft-timer ceiling, sim heap quota |
| `app_options.cmake` | Module pruning switches (e.g. `WINK_USE_LED` / `WINK_USE_MOTOR`) |
| `device_tree.json` | Device-tree descriptor consumed by the online simulator |

---

## 2. Descriptor formats

### 2.1 Board definition (registry `*.json`)

Describes board metadata, onboard fixed devices and pin-header aliases (`$board.headers.<KEY>`):

```json
{
  "metadata": {
    "board_name": "stc89c52_devboard",
    "family": "mcs51",
    "mcu": "at89c52",
    "memory": { "sim_heap_quota_kb": 256 }
  },
  "onboard_devices": {
    "status_led": { "type": "led", "gpio_pin": 8, "active_low": true }
  },
  "headers": { "P3.2": 26, "P1.0": 8 }
}
```

> The field dictionary and the steps to add a board live in `wink-tools/tools/codegen/boards/`.

### 2.2 Application config (`wink-app.json`)

Placed at the root of each app; declares the board, MCU and device list:

```json
{
  "app_name": "mcs51_button_led",
  "board": "stc89c52_devboard",
  "mcu": "at89c52",
  "tick_ms": 10,
  "devices": {
    "btn": { "type": "button", "gpio_pin": 26, "active_low": true },
    "led": { "type": "led", "gpio_pin": 8, "active_high": false }
  }
}
```

Notes:

- `board` determines pin numbering and the default memory quota; `mcu` falls back to the board default when omitted;
- device `type` is resolved through the driver registry (see §4); the generated device-tree instance name is the key under `devices`;
- pins accept raw numbers or `"$board.headers.<KEY>"` references.

---

## 3. Memory quota resolution order

`sim_heap_quota_kb` supports layered configuration; the first hit wins:

1. `target_config.memory.sim_heap_quota_kb`
2. `target_config.sim_heap_quota_kb`
3. root-level `sim_heap_quota_kb`
4. board default: `board.json` → `metadata.memory.sim_heap_quota_kb`
5. fallback: `256` KB

---

## 4. Declarative driver extensions (`wink-micro-os/codegen/drivers/`)

Standard peripherals (`led`, `button`, `rc_servo`, `ultrasonic`, `dc_motor`, `encoder`, …) are defined in **YAML**. Drop `my_device.yaml` into `wink-micro-os/codegen/drivers/` to extend:

```yaml
type: my_device
codegen_schema: '1.1'
category: sensor
is_actuator: false
headers:
  - 'dal/include/sensor/dal_my_device.h'

fields:
  - name: pin
    type: int
    tier: required
    doc: 'GPIO pin for my_device'

config:
  init_template: 'dal_my_device_init(&g_dev_{{ dev_name }}, {{ pin }});'
  deinit_fn: 'dal_my_device_deinit'
```

Verify the generation:

```bash
winkcli test
```
