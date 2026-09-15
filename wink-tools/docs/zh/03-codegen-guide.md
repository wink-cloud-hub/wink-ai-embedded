<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 03 · 代码生成指南

WinkMicroOS 采用数据驱动的代码生成：解析可读的 JSON 描述文件，在编译期生成设备树、Arduino 兼容绑定与构建配置。

---

## 1. 生成产物

运行 `winkcli build host --app <app>`（或 `winkcli gen app-schema --app <app>`）后，构建树中生成：

| 产物 | 说明 |
|---|---|
| `device_tree.h / .c` | DAL 设备实例数组与全局设备树 API |
| `wink_arduino_bindings.h / .cpp` | Arduino 语法兼容 API（`pinMode` / `digitalWrite` 等） |
| `wink_config.h` | 主循环 Tick、软定时器上限、仿真堆内存配额等宏 |
| `app_options.cmake` | 模块裁剪开关（如 `WINK_USE_LED` / `WINK_USE_MOTOR`） |
| `device_tree.json` | 前端仿真使用的设备树描述（供在线仿真器加载） |

---

## 2. 配置文件规范

### 2.1 板级定义（板卡注册表中的 `*.json`）

描述板卡元数据、板载固定外设、引脚排针映射（`$board.headers.<KEY>` 可引用）：

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

> 板卡注册表的字段字典与新增板卡步骤见仓库：`wink-tools/tools/codegen/boards/`。

### 2.2 应用配置（`wink-app.json`）

位于每个 App 目录根部，声明使用的板卡、MCU 与外设清单：

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

要点：

- `board` 决定引脚编号与默认内存配额；`mcu` 未声明时继承板级默认；
- 外设 `type` 由驱动注册表解析（见 §4），生成的设备树实例名即 `devices` 中的键名；
- 引脚可直接写数值，也可用 `"$board.headers.<KEY>"` 引用板级排针映射。

---

## 3. 内存配额解析顺序

`sim_heap_quota_kb` 支持多层级配置，按以下顺序取第一个命中值：

1. `target_config.memory.sim_heap_quota_kb`
2. `target_config.sim_heap_quota_kb`
3. 根级 `sim_heap_quota_kb`
4. 板级默认：`board.json` 的 `metadata.memory.sim_heap_quota_kb`
5. 系统兜底：`256` KB

---

## 4. 声明式驱动扩展（`wink-micro-os/codegen/drivers/`）

标准外设（`led`、`button`、`rc_servo`、`ultrasonic`、`dc_motor`、`encoder` 等）均以 **YAML** 定义。在 `wink-micro-os/codegen/drivers/` 下新增 `my_device.yaml` 即可完成扩展：

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

完成后运行测试验证生成：

```bash
winkcli test
```
