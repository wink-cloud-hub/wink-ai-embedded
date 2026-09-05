# 06. Device Model Registry 统一器件模型规范

Device Model Registry 是 Wink-AI 平台的核心元数据中心。它将外设的电气引脚、业务语义、低代码属性、DAL API、仿真行为、真机约束、错误模型与代码生成规则统一到一份可版本化的器件契约中，避免同一器件信息分散在前端画布、SchemaForm、WasmPeripheralRegistry、DAL 驱动和 device_tree 生成器中。

---

## 1. 设计目标

1. **单一事实源**：每个外设只维护一份标准模型，供 UI、代码生成、仿真、测试、编译部署共同消费。
   - **配置字段所有权（ADR-0034）**：Registry/schema 负责字段名、类型、范围、L1/L2 可见性与迁移规则；Codegen plugin 负责校验与 C 枚举映射；DAL 负责语义→PAL 映射；PAL target 负责 AUTO→effective 与硬件可实现性。C 中 `AUTO=0` 是运行期默认行为的最终权威。
2. **AI 生成友好**：AI 不直接猜测 GPIO、I2C、PWM 等底层细节，而是基于器件模型生成受约束的 App 调用。
3. **仿真与真机一致**：同一个器件模型同时声明仿真行为和真机约束，减少虚实行为偏差。
4. **可扩展生态**：第三方器件厂商可以按规范提交新的传感器、执行器和板卡模型。
5. **版本可迁移**：通过 schema version 和 migration 机制保证历史项目可打开、可仿真、可编译。

---

## 2. 模型分层

```text
Device Model Registry
├── Board Model              开发板模型：芯片、引脚、能力、烧录方式
├── Peripheral Model         外设模型：LED、按钮、舵机、传感器、屏幕
├── Bus Model                总线模型：GPIO、PWM、ADC、I2C、SPI、UART
├── DAL API Model            业务语义 API：读距离、设角度、显示文本
├── Simulation Model         仿真通路：Pin-level / Protocol / DAL Bypass
├── Fault Model              断线、超时、越界、抖动、噪声
└── Codegen Model            device_tree、App block、SchemaForm 生成规则
```

---

## 3. 外设模型标准结构

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
      "label": "最大测距距离 (cm)",
      "compType": "InputNumber",
      "defaultValue": 400,
      "min": 2,
      "max": 400
    },
    {
      "prop": "sampleIntervalMs",
      "label": "最小采样间隔 (ms)",
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
        "note": "App 推荐；host 单 tick ready，真机经 pal_gpio_pulse_in async capture"
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
        "note": "阻塞 busy-wait (worst-case ≈60ms+)；ADR-0017 三层硬隔离首个应用点：#ifndef WINK_STRICT_NONBLOCKING 包围声明+实现，协作式调度构建下从符号表消失；App/runtime 10ms tick 不得调用，迁移完成后移除"
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
    "echoVoltageWarning": "ECHO 为 5V 电平，3.3V MCU 需要分压或电平转换",
    "minSampleIntervalMs": 60,
    "timeoutMs": 30,
    "captureHint": "ESP32 用 RMT 或 GPIO 双沿 ISR + timer 捕获 echo 脉宽（Deferred-ISR 下半部，runtime tick 内禁 busy-wait）；wasm 经 js_sim_measure_echo_pulse_us 旁路"
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
      "读取失败时必须检查 wink_status_t",
      "不要高于 sampleIntervalMs 频率反复采样"
    ]
  }
}
```

---

## 4. Board Model 开发板模型

开发板模型定义 MCU 能力、引脚复用、电压域、烧录协议和 PAL target。

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
      "notes": "启动模式选择引脚，默认不推荐普通外设占用"
    }
  ]
}
```

---

## 5. 能力匹配与自动校验

低代码画布在连线时必须执行静态校验：

| 校验项 | 规则 | 失败处理 |
|---|---|---|
| 引脚类型 | digital_out 不能连接到 gnd-only pin | 阻止连线 |
| 电压域 | 5V 输出接 3.3V 输入需电平转换 | 警告或推荐转换模块 |
| 启动安全 | ESP32 GPIO0、GPIO2 等启动敏感脚 | 默认不推荐 |
| 总线占用 | 同一 I2C port 可共享地址，不可地址冲突 | 阻止部署 |
| PWM 资源 | 超过目标板 PWM 通道数量 | 阻止编译 |
| 采样频率 | App 采样频率超过器件最小间隔 | 编译警告 |

---

## 6. 代码生成消费路径

```text
Device Model Registry
     │
     ├── SchemaForm 属性面板
     ├── 画布引脚与连线校验
     ├── device_tree.h / device_tree.c
     ├── App Blockly / AI 代码提示
     ├── DAL 驱动编译选择
     ├── WasmPeripheralRegistry 注册
     ├── 故障注入面板
     └── Golden Trace 字段描述
```

---

## 7. device_tree 生成原则

1. `device_tree.h` 只暴露逻辑实例，不暴露硬件平台 SDK。
2. `device_tree.c` 固化引脚、通道、I2C 地址、属性默认值。
3. 所有实例名称来自用户语义命名，必须经过 C identifier sanitization。
4. 生成器必须保证实例名称唯一。
5. 生成器必须输出 manifest，记录器件模型版本和生成输入 hash。
6. 生成 `board_config.c` 必须为物理 target 提供 PWM channel→GPIO 路由（强定义 `pal_pwm_pin_map`）；host/wasm 构建不链接此文件。

示例：

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

## 8. 版本与迁移策略

1. `schemaVersion` 管理模型结构版本。
2. `version` 管理单个器件模型语义版本。
3. 项目文件保存所引用的模型 id、version 和 hash。
4. 打开旧项目时，如果本地 Registry 版本较新，必须执行 migration。
5. migration 不允许静默改变物理引脚、默认电压和 DAL API 语义。

迁移记录示例：

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

## 9. 前端 Catalog SSOT 映射（embedded-frontend）

平台 Device Model Registry 在前端工作台的分层落地如下（2026-07-11 Catalog SSOT 收敛后）：

| Registry 层 | 目录 | 聚合 Facade | 消费方 |
|-------------|------|-------------|--------|
| 电路外设 | `../../../../wink-ai/packages/embedded-frontend/src/peripherals/` | `deviceCatalog.listDevices()` | 资产库、Manifest→画布、binding 校验 |
| 开发板 | `../../../../wink-ai/packages/embedded-frontend/src/boards/` | `deviceCatalog.getBoard()` / `listBoards()` | 引脚解析、Board 画布布局 |
| 机械/环境 | `../../../../wink-ai/packages/embedded-frontend/src/world-assets/` | `listMechanicalModels()` / `listEnvironmentModels()` | Bindings 面板、模板 |
| 映射类型 | `types/mapping-registry.ts` | — | `allowed*Mappings` 交叉校验 |
| Binding 实例 | Manifest `bindings.*` | — | simulate 门禁、W3c Worker 桥 |

派生规则：

- `catalog/derive-catalog-entry.ts`：`peripherals` → `DeviceCatalogEntry`
- `catalog/derive-board-catalog-entry.ts`：`boards` → board 型 `DeviceCatalogEntry`
- **`device-catalog.ts` 禁止手写业务条目**，只做 merge + query

新增 Board 时复制 `boards/esp32-devkit-v1/`，在 `boards/index.ts` 注册；画布布局写入 `definition.canvas`，`peripheral-pins.ts` 的 `boardDescriptor` 仅为兼容 re-export。

---

## 4. MVP 外设 DAL API 模型速查（SSOT）

以下为 MVP 全部外设的 DAL 命名 API 签名摘要，供 AI codegen / Blockly / 静态检查消费。

### 4.1 LED (`dal_led.h`)
| API | 返回 | 参数 | 语义 |
|---|---|---|---|
| `dal_led_init` | `wink_status_t` | `dev*, pin, active_high` | init_lifecycle |
| `dal_led_on` | `wink_status_t` | `dev*` | write_on |
| `dal_led_off` | `wink_status_t` | `dev*` | write_off |
| `dal_led_set` | `wink_status_t` | `dev*, on` | write_boolean |
| `dal_led_toggle` | `wink_status_t` | `dev*` | write_toggle |

### 4.2 Button (`dal_button.h`)

| API | 返回 | 参数 | 语义 |
|---|---|---|---|
| `dal_button_init` | `wink_status_t` | `dev*, const dal_button_config_t*` | init_lifecycle（`cfg`: owner/pin/active_low/`pull`；`pull=0`→AUTO） |
| `dal_button_poll` | `wink_status_t` | `dev*` | poll_debounce（`NONE`+未注入 → `WINK_ERR_DISCONNECTED`） |
| `dal_button_is_pressed` | `wink_status_t` | `dev*, bool* out` | read_state |
| `dal_button_was_pressed` | `wink_status_t` | `dev*, bool* out` | read_edge_once |
| `dal_button_deinit` | `wink_status_t` | `dev*` | deinit_lifecycle |

**L1/L2 可见性（ADR-0034）：** L1 JSON：`pin`/`active_low`/event 字段；L2：`advanced.pull`（`auto|up|down|none`）。

### 4.3 Servo (`dal_rc_servo.h`)

| API | 返回 | 参数 | 语义 |
|---|---|---|---|
| `dal_rc_servo_init` | `wink_status_t` | `dev*, const dal_rc_servo_config_t*` | init_lifecycle（含可选 `resolution_bits`/`clock_requirement`，DAL 自有枚举，不泄漏 `pal_*`） |
| `dal_rc_servo_set_angle` | `wink_status_t` | `dev*, angle` | write_angle |
| `dal_rc_servo_safe_off` | `wink_status_t` | `dev*` | safe_off |
| `dal_rc_servo_deinit` | `wink_status_t` | `dev*` | deinit_lifecycle |
| `dal_rc_servo_apply_override` | `wink_status_t` | `dev*, params, len` | flash_override（wire v1=9B；**不含** advanced） |

**L1/L2 可见性（ADR-0034）：** L1：`pwm_channel`/`min_pulse_ms`/`max_pulse_ms`；L2：`advanced.resolution_bits` / `advanced.clock_requirement`。
### 4.4 HC-SR04 (`dal_ultrasonic.h`)
| API | 返回 | 参数 | 语义 |
|---|---|---|---|
| `dal_ultrasonic_init` | `wink_status_t` | `dev*, trig_pin, echo_pin` | init_lifecycle |
| `dal_ultrasonic_request_measurement` | `wink_status_t` | `dev*` | request_nonblocking |
| `dal_ultrasonic_get_cached_distance` | `wink_status_t` | `dev*, float* distance_cm` | read_cached_nonblocking |
| `dal_ultrasonic_read` | `wink_status_t` | `dev*, float* distance_cm` | read_distance_cm (deprecated, `WINK_BLOCKING` — ADR-0017 三层硬隔离；`-DWINK_STRICT_NONBLOCKING=1` 下符号消失) |

### 4.5 SSD1306 OLED (`dal_ssd1306.h`)
| API | 返回 | 参数 | 语义 |
|---|---|---|---|
| `dal_ssd1306_init` | `wink_status_t` | `dev*, const cfg*` | init_lifecycle |
| `dal_ssd1306_clear` | `wink_status_t` | `dev*` | write_clear |
| `dal_ssd1306_draw_text` | `wink_status_t` | `dev*, col, page, str` | write_text |
| `dal_ssd1306_flush` | `wink_status_t` | `dev*` | write_flush |

---

## 9. 最小落地范围

MVP 阶段优先内置以下模型：

| 类型 | 模型 |
|---|---|
| Board | ESP32 DevKit V1 |
| 输入 | Button |
| 输出 | LED、Servo |
| 传感器 | HC-SR04 |
| 显示 | SSD1306 OLED |

该范围足以覆盖 GPIO、PWM、ADC、I2C、DAL Bypass 和 WebSerial 烧录闭环。
