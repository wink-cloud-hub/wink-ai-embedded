# 06. Device Model Registry Unified Specification

<!-- i18n-meta
source: docs/zh/design/07-platform-governance/01-device-model-registry.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

The Device Model Registry is the core metadata center of the Wink-AI platform. It unifies electrical pins, business semantics, low-code properties, DAL APIs, simulation behaviors, hardware constraints, fault models, and codegen rules into a versioned device contract, eliminating fragmented configurations across frontend canvases, SchemaForms, WasmPeripheralRegistries, DAL drivers, and device_tree generators.

---

## 1. Design Goals

1. **Single Source of Truth**: Each peripheral maintains a single standard model consumed collectively by UI, codegen, simulation, testing, and deployment.
   - **Configuration Field Ownership (ADR-0034)**: Registry/schema governs field names, types, bounds, L1/L2 visibility, and migrations; Codegen plugins handle validation and C enum mappings; DAL manages semantic-to-PAL mappings; PAL targets manage `AUTO`-to-effective resolution. In C, `AUTO=0` is the ultimate runtime default.
2. **AI-Generation Friendly**: AI does not guess raw GPIO/I2C/PWM registers; it emits constrained App invocations based on Device Models.
3. **Simulation & Hardware Parity**: The same device model declares simulation behaviors and physical hardware constraints.
4. **Extensible Ecosystem**: Third-party vendors can submit new sensor, actuator, and board models following standard schemas.
5. **Version Migration**: Schema versioning and migration records guarantee historical projects remain openable, simulatable, and compilable.

---

## 2. Model Layering

```text
Device Model Registry
├── Board Model              Board definitions: Chips, pins, capabilities, flashing
├── Peripheral Model         Peripherals: LEDs, buttons, servos, sensors, displays
├── Bus Model                Buses: GPIO, PWM, ADC, I2C, SPI, UART
├── DAL API Model            Semantic APIs: Read distance, set angle, render text
├── Simulation Model         Simulation paths: Pin-level / Protocol / DAL Bypass
├── Fault Model              Disconnects, timeouts, out-of-bounds, jitter, noise
└── Codegen Model            Rules for device_tree, App blocks, SchemaForms
```

---

## 3. Peripheral Model Standard Schema

```json
{
  "$schema": "https://wink-ai.dev/schemas/device-model.v1.json",
  "schemaVersion": 1,
  "id": "hc-sr04",
  "name": {
    "en": "HC-SR04 Ultrasonic Sensor",
    "zh": "HC-SR04 超声波测距传感器"
  },
  "category": "sensor",
  "version": "1.0.0",
  "vendor": "generic",
  "visual": {
    "tagName": "wink-ultrasonic",
    "thumbnail": "assets/peripherals/hc-sr04.svg",
    "dimensions": { "width": 64, "height": 32 }
  },
  "pins": [
    { "name": "VCC", "type": "power", "voltage": "5V", "required": true },
    { "name": "GND", "type": "gnd", "required": true },
    { "name": "TRIG", "type": "digital_out", "required": true },
    { "name": "ECHO", "type": "digital_in", "required": true, "voltage": "5V" }
  ],
  "properties": [
    {
      "prop": "maxDistanceCm",
      "label": "Max Distance (cm)",
      "compType": "InputNumber",
      "defaultValue": 400,
      "min": 2,
      "max": 400
    },
    {
      "prop": "sampleIntervalMs",
      "label": "Min Sample Interval (ms)",
      "compType": "InputNumber",
      "defaultValue": 60,
      "min": 50,
      "max": 1000
    }
  ],
  "dal": {
    "header": "dal_ultrasonic.h",
    "type": "dal_ultrasonic_t",
    "instancePrefix": "ultrasonic",
    "apis": [
      {
        "name": "dal_ultrasonic_init",
        "returnType": "wink_status_t",
        "params": [
          { "name": "dev", "type": "dal_ultrasonic_t *" },
          { "name": "trig_pin", "type": "uint16_t" },
          { "name": "echo_pin", "type": "uint16_t" }
        ],
        "semantic": "init_lifecycle"
      },
      {
        "name": "dal_ultrasonic_request_measurement",
        "returnType": "wink_status_t",
        "params": [{ "name": "dev", "type": "dal_ultrasonic_t *" }],
        "semantic": "request_nonblocking",
        "note": "Recommended for App; ready on next tick on host, async captured on hardware via pal_gpio_pulse_in"
      },
      {
        "name": "dal_ultrasonic_get_cached_distance",
        "returnType": "wink_status_t",
        "params": [
          { "name": "dev", "type": "const dal_ultrasonic_t *" },
          { "name": "distance_cm", "type": "float *" }
        ],
        "semantic": "read_cached_nonblocking"
      },
      {
        "name": "dal_ultrasonic_read",
        "returnType": "wink_status_t",
        "params": [
          { "name": "dev", "type": "dal_ultrasonic_t *" },
          { "name": "distance_cm", "type": "float *" }
        ],
        "semantic": "read_distance_cm",
        "deprecated": true,
        "blocking": true,
        "attributes": ["WINK_BLOCKING", "WINK_WARN_UNUSED_RESULT"],
        "strictBuildGuard": "WINK_STRICT_NONBLOCKING",
        "note": "Blocking busy-wait (worst-case ≈60ms+); isolated via ADR-0017 and removed under WINK_STRICT_NONBLOCKING"
      }
    ]
  },
  "simulation": {
    "supportedModes": ["dal-value-bypass", "pin-level"],
    "preferredMode": "dal-value-bypass",
    "bypassImports": [
      {
        "name": "js_sim_trigger_ultrasonic",
        "returnType": "void",
        "params": [{ "name": "trig_pin", "type": "uint16_t" }]
      },
      {
        "name": "js_sim_measure_echo_pulse_us",
        "returnType": "uint32_t",
        "params": [{ "name": "trig_pin", "type": "uint16_t" }]
      }
    ]
  },
  "physicalConstraints": {
    "supplyVoltage": ["5V"],
    "echoVoltageWarning": "ECHO output is 5V; 3.3V MCUs require level shifting or voltage dividers",
    "minSampleIntervalMs": 60,
    "timeoutMs": 30,
    "captureHint": "ESP32 captures pulse width via RMT or GPIO dual-edge ISR + timer (Deferred-ISR Bottom-Half); Wasm bypasses via js_sim_measure_echo_pulse_us"
  },
  "faultModel": {
    "disconnect": true,
    "timeout": true,
    "outOfRange": true,
    "noise": {
      "enabled": true,
      "defaultStddev": 0.5
    }
  },
  "codegen": {
    "deviceTreeTemplate": "templates/device-tree/hc-sr04.c.mustache",
    "blocklyBlocks": ["read_distance_cm"],
    "aiHints": [
      "Must check wink_status_t upon read failure",
      "Do not sample at frequencies higher than sampleIntervalMs"
    ]
  }
}
```

---

## 4. Board Model Specification

```json
{
  "$schema": "https://wink-ai.dev/schemas/board-model.v1.json",
  "schemaVersion": 1,
  "id": "esp32-devkit-v1",
  "name": "ESP32 DevKit V1",
  "target": "esp32",
  "palTarget": "targets/esp32",
  "toolchain": "esp-idf",
  "flash": {
    "protocol": "webserial-esptool",
    "defaultBaudRate": 921600,
    "requiresBootPins": ["EN", "IO0"]
  },
  "capabilities": {
    "gpio": true,
    "pwm": true,
    "adc": true,
    "i2c": true,
    "spi": true,
    "uart": true,
    "wifi": true,
    "ble": true
  },
  "pins": [
    {
      "name": "GPIO4",
      "number": 4,
      "voltage": "3.3V",
      "functions": ["gpio", "pwm", "adc"],
      "safeForBoot": true
    },
    {
      "name": "GPIO0",
      "number": 0,
      "voltage": "3.3V",
      "functions": ["gpio", "pwm"],
      "safeForBoot": false,
      "notes": "Boot strapping pin; general peripheral attachment discouraged"
    }
  ]
}
```

---

## 5. Capability Matching & Static Validation

| Validation Item | Rule | Failure Action |
|---|---|---|
| Pin Type | `digital_out` cannot attach to GND-only pins | Prevents wiring |
| Voltage Domain | 5V output to 3.3V input requires level shifting | Warning / Suggests converter |
| Boot Safety | ESP32 GPIO0, GPIO2 strapping pins | Discouraged by default |
| Bus Allocation | Same I2C port allows distinct addresses; collisions blocked | Prevents deployment |
| PWM Channels | Exceeding available board PWM channels | Prevents compilation |
| Sampling Rate | App sampling rate exceeding device minimum interval | Compilation warning |

---

## 6. Codegen Consumption Matrix

```text
Device Model Registry
     │
     ├── SchemaForm Property Panel
     ├── Canvas Pin & Wiring Validation
     ├── device_tree.h / device_tree.c
     ├── App Blockly / AI Prompts
     ├── DAL Driver Build Pruning
     ├── WasmPeripheralRegistry Registration
     ├── Fault Injection Panel
     └── Golden Trace Field Descriptions
```

---

## 7. `device_tree` Generation Principles

1. `device_tree.h` exports logical instances only, shielding hardware SDK headers.
2. `device_tree.c` solidifies pins, channels, I2C addresses, and default properties.
3. Instance names originate from sanitized user semantic identifiers.
4. Generators guarantee instance name uniqueness.
5. Emits manifests capturing model versions and input hashes.
6. Generates `board_config.c` with strong definitions (`pal_pwm_pin_map`) for physical targets.

Sample:

```c
#include "device_tree.h"

const wink_device_manifest_t wink_device_manifest = {
    .schema_version = 1,
    .model_hash = "sha256:7d9f...",
    .generated_at = "2026-06-22T00:00:00Z"
};

dal_ultrasonic_t front_radar = {
    .component_id = 1001,
    .trig_pin = 4,
    .echo_pin = 5,
    .max_distance_cm = 400.0f,
    .min_sample_interval_ms = 60,
    .last_distance_cm = 0.0f,
    .last_status = WINK_OK
};
```

---

## 8. Versioning & Migration Strategy

```json
{
  "from": "hc-sr04@1.0.0",
  "to": "hc-sr04@1.1.0",
  "changes": [
    {
      "type": "addProperty",
      "path": "properties.temperatureCompensation",
      "defaultValue": false
    }
  ],
  "requiresUserConfirmation": false
}
```

---

## 9. Frontend Catalog SSOT Mapping (`embedded-frontend`)

| Registry Layer | Directory | Aggregate Facade | Consumer |
|---|---|---|---|
| Circuit Peripherals | `../../../../wink-ai/packages/embedded-frontend/src/peripherals/` | `deviceCatalog.listDevices()` | Asset library, Manifest canvas, binding validation |
| Dev Boards | `../../../../wink-ai/packages/embedded-frontend/src/boards/` | `deviceCatalog.getBoard()` / `listBoards()` | Pin resolution, Board canvas layout |
| Mechanical / World | `../../../../wink-ai/packages/embedded-frontend/src/world-assets/` | `listMechanicalModels()` / `listEnvironmentModels()` | Bindings panel, templates |
| Mapping Types | `types/mapping-registry.ts` | — | Cross-validation |
| Binding Instances | Manifest `bindings.*` | — | Simulation gating, Worker bridge |

---

## 4. MVP Peripheral DAL API Quick Reference (SSOT)

### 4.1 LED (`dal_led.h`)
| API | Return | Parameters | Semantic |
|---|---|---|---|
| `dal_led_init` | `wink_status_t` | `dev*, pin, active_high` | init_lifecycle |
| `dal_led_on` | `wink_status_t` | `dev*` | write_on |
| `dal_led_off` | `wink_status_t` | `dev*` | write_off |
| `dal_led_set` | `wink_status_t` | `dev*, on` | write_boolean |
| `dal_led_toggle` | `wink_status_t` | `dev*` | write_toggle |

### 4.2 Button (`dal_button.h`)
| API | Return | Parameters | Semantic |
|---|---|---|---|
| `dal_button_init` | `wink_status_t` | `dev*, const dal_button_config_t*` | init_lifecycle (`cfg`: owner/pin/active_low/`pull`) |
| `dal_button_poll` | `wink_status_t` | `dev*` | poll_debounce |
| `dal_button_is_pressed` | `wink_status_t` | `dev*, bool* out` | read_state |
| `dal_button_was_pressed` | `wink_status_t` | `dev*, bool* out` | read_edge_once |
| `dal_button_deinit` | `wink_status_t` | `dev*` | deinit_lifecycle |

### 4.3 Servo (`dal_rc_servo.h`)
| API | Return | Parameters | Semantic |
|---|---|---|---|
| `dal_rc_servo_init` | `wink_status_t` | `dev*, const dal_rc_servo_config_t*` | init_lifecycle |
| `dal_rc_servo_set_angle` | `wink_status_t` | `dev*, angle` | write_angle |
| `dal_rc_servo_safe_off` | `wink_status_t` | `dev*` | safe_off |
| `dal_rc_servo_deinit` | `wink_status_t` | `dev*` | deinit_lifecycle |
| `dal_rc_servo_apply_override` | `wink_status_t` | `dev*, params, len` | flash_override |

### 4.4 HC-SR04 (`dal_ultrasonic.h`)
| API | Return | Parameters | Semantic |
|---|---|---|---|
| `dal_ultrasonic_init` | `wink_status_t` | `dev*, trig_pin, echo_pin` | init_lifecycle |
| `dal_ultrasonic_request_measurement` | `wink_status_t` | `dev*` | request_nonblocking |
| `dal_ultrasonic_get_cached_distance` | `wink_status_t` | `dev*, float* distance_cm` | read_cached_nonblocking |
| `dal_ultrasonic_read` | `wink_status_t` | `dev*, float* distance_cm` | read_distance_cm (deprecated, `WINK_BLOCKING`) |

### 4.5 SSD1306 OLED (`dal_ssd1306.h`)
| API | Return | Parameters | Semantic |
|---|---|---|---|
| `dal_ssd1306_init` | `wink_status_t` | `dev*, const cfg*` | init_lifecycle |
| `dal_ssd1306_clear` | `wink_status_t` | `dev*` | write_clear |
| `dal_ssd1306_draw_text` | `wink_status_t` | `dev*, col, page, str` | write_text |
| `dal_ssd1306_flush` | `wink_status_t` | `dev*` | write_flush |

---

## 9. MVP Builtin Model Scope

| Type | Model |
|---|---|
| Board | ESP32 DevKit V1 |
| Input | Button |
| Output | LED, Servo |
| Sensor | HC-SR04 |
| Display | SSD1306 OLED |
