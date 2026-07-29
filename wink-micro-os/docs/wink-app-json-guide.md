# 📘 `wink-app.json` 全景配置手册与规范

`wink-app.json` 是嵌入式应用与仿真引擎的 **SSOT（Single Source of Truth，唯一真相源）** 配置文件。所有硬件资源绑定、GPIO 映射与外设参数均在此文件内完成声明。

| 项 | 内容 |
|----|------|
| **用户稳定面 SSOT** | [user-surface-insulation-design.md](../../docs/design/tech-designs/2026-07-28-user-surface-insulation-design.md) |
| **关联计划** | [user-surface-phase1-plan.md](../../docs/design/implementation-plans/2026-07-28-user-surface-phase1-plan.md) |
| **关联评审** | [dal-control-semantic-completeness-review §10](../../docs/design/reviews/2026-07-28-dal-control-semantic-completeness-review.md)；[user-surface-phase1-plan-review.md](../../docs/design/reviews/2026-07-28-user-surface-phase1-plan-review.md) |
| **字段分层摘要** | [dal-best-practices §3.0](./dal-development-guide/dal-best-practices.md) |
| **Codegen 扩展根（ADR-0051）** | 各 `type` 的 codegen 描述与 Role bindings 外置在 [`wink-micro-os/codegen/`](../codegen/README.md)，加外设无需改 `wink-tools` 源码 — 见 [adding-peripheral.md](./dal-development-guide/adding-peripheral.md) |

---

## 📑 一、 顶层结构与全局规范

### 1. 整体 JSON 骨架

配置文件由 **板级信息** 与 **外设列表** 两大部分组成：

```json
{
  "app_name": "oled_dashboard",      // 应用唯一标识（必须与目录名一致）
  "board": "esp32_devkitc",          // 目标开发板型号
  "devices": {                       // 所有外设实例集合
    "<instance_id>": {               // 实例唯一 ID（格式建议：位置/功能_类型，如 status_oled）
      "type": "<peripheral_type>",   // 外设类型（必须与 Manifest 声明一致）
      // ... 外设参数及引脚配置（按 type 的 schema，非全局统一字段集）
    }
  }
}
```

> **字段分层**：跨外设必填的只有 `type`（控制语义族 / DAL 绑定）。可选 `role` 为 **App 侧 Role Interface**（缺省 `default_role`，生成 `{name}_{verb}`）——**不是 BAL**，也不是产品级「左轮/云台」意图。`drive_mode` / `enable_pin` / `driver_ic` **不是**全局通用字段。摘要见 [dal-best-practices §3.0](./dal-development-guide/dal-best-practices.md)；**如何挂 Role** 见 [role-interface-codegen.md](./dal-development-guide/role-interface-codegen.md)。
>
> **稳定面 vs 驱动面（Phase 1）**：**无板卡模板**——每个 App 在本文件写全 `type` 与引脚/总线（接线灵活）。`stable` 字段（如 `role`、`max_angle`、`long_press_ms`）影响业务语义；`advanced` 字段（如 `drive_mode`、`decode_mode`、`enable_pin`、`*_pin`）为驱动/接线面。改 advanced 引脚是正常操作，不是「破坏用户面」。
>
> **Experimental 类型**：`gps`、`eeprom` 为实验性 stub（DAL 现返回 `WINK_ERR_UNSUPPORTED`），codegen 会 stderr 警告，不宜作为稳定用户面；正式 Role 待 UART/I2C 后端落地后再提供。

### 2. 板级配置 (Board Level)

| 字段 | 类型 | 是否必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `app_name` | `string` | **是** | 应用程序名称（snake_case 命名）。 |
| `board` | `string` | **是** | 指定目标物理开发板型号（例如 `esp32_devkitc`）。系统会根据此字段加载对应的硬件默认总线定义。 |

### 3. 核心转换与引脚识别契约 (CRITICAL)

为了实现自动化代码生成与 TS/C 双端映射，系统遵循以下命名契约：

1. **蛇形与驼峰自动转换**：JSON 中使用 `snake_case`（如 `active_high`），解析后会自动转为 TS 侧的 `camelCase`（如 `activeHigh`）。
2. **引脚识别规则 (`*_pin`)**：
   - 字段名**必须以 `_pin` 结尾**（例如 `gpio_pin`、`trig_pin`、`sda_pin`），系统才会将其提取至 `pinMapping`。
   - 若忘记加 `_pin`（例如误写成 `"pin": 2` 或 `"sda": 21`），该字段会被错误归类为普通属性 `properties`，导致物理引脚无法绑定！

---

## 🔌 二、 常用外设配置指南

### 1. GPIO 直连外设 (Button / LED)

单引脚点对点 GPIO 控制的外设。

#### (1) `button` - GPIO 按键
```json
"user_button": {
  "type": "button",
  "gpio_pin": 10,
  "active_low": true,
  "auto_poll_ms": 10,
  "debounce_ms": 20
}
```
* `gpio_pin` (*number*, 必填): 按键连接的逻辑 GPIO 引脚号。
* `active_low` (*boolean*, 选填, 默认 `true`): 低电平有效（上拉按键，按下时 GPIO 为 0）。
* `debounce_ms` (*number*, 选填, 默认 `20`): 软件去抖时间（单位：毫秒）。

#### (2) `led` - GPIO LED
```json
"status_led": {
  "type": "led",
  "gpio_pin": 2,
  "active_high": true
}
```
* `gpio_pin` (*number*, 必填): LED 阳极/控制端的 GPIO 引脚号。
* `active_high` (*boolean*, 选填, 默认 `true`): 高电平点亮（`true` 时 GPIO 输出 HIGH 点亮）。

---

### 2. I2C 总线外设 (OLED SSD1306 等)

I2C 属于**总线型外设**（多设备共享总线），支持总线端口与默认引脚回退。

#### (1) 核心配置字段（以 `ssd1306` 显示屏为例）

> **`type` 保留芯片名 `ssd1306`**（Phase 1 Owner 裁决：不改名）。App 通过 Role `text_display`（`clear` / `draw_text` / `flush`）交互，不依赖 JSON 中的芯片字符串。异族 SPI 面板 → 新 `type` 或未来 `panel_variant`。

```json
"status_oled": {
  "type": "ssd1306",
  "i2c_bus": 0,
  "i2c_addr": 60,
  "sda_pin": 21,
  "scl_pin": 22
}
```

| 字段名 | 类型 | 是否必填 | 默认逻辑 / 说明 |
| :--- | :--- | :--- | :--- |
| `type` | `string` | **是** | 固定的外设类型标识，如 `"ssd1306"`。 |
| `i2c_bus` | `number` | **是** | 指定使用的硬件 I2C 控制器/总线编号（`0` 代表 I2C0，`1` 代表 I2C1）。 |
| `i2c_addr` | `number` | **是** | 外设的 7 位 **十进制** I2C 从机地址。<br>🔹 `60` 即十六进制 `0x3C`（SSD1306 默认地址）<br>🔹 `61` 即十六进制 `0x3D`（修改背面选择电阻后） |
| `sda_pin` | `number` | **选填** | SDA 数据引脚号。若省略，系统自动回退使用板级定义（`board`）的 `i2c_bus` 默认 SDA 引脚。 |
| `scl_pin` | `number` | **选填** | SCL 时钟引脚号。若省略，系统自动回退使用板级定义（`board`）的 `i2c_bus` 默认 SCL 引脚。 |

#### (2) SDA / SCL 引脚解析与回退机制
```text
wink-app.json 显式填写 sda_pin / scl_pin？
   ├── YES ───> 使用指定的物理 GPIO (如 sda_pin: 18, scl_pin: 19)
   └── NO  ───> 自动读取开发板 esp32_devkitc.json 中 i2c_bus(如 bus.i2c0) 的默认引脚 (SDA: 21, SCL: 22)
```

#### (3) 多 I2C 设备配置典型场景
* **场景 A：默认单屏幕（最简配置）**
  ```json
  "status_oled": { "type": "ssd1306", "i2c_bus": 0, "i2c_addr": 60 }
  ```
* **场景 B：同一总线挂载多个不同设备（OLED + 传感器）**
  共享 `i2c_bus: 0`，用不同的 `i2c_addr`（如 `60` 与 `104`）区分。
* **场景 C：挂载 2 个相同类型的 OLED 屏幕**
  * 方式 1：改背板电阻，使地址分别为 `60` (0x3C) 和 `61` (0x3D)。
  * 方式 2：使用不同的 `i2c_bus` 或不同的 `sda_pin`/`scl_pin` 引脚组。

---

### 3. 定时器 / 脉冲传感器外设 (Ultrasonic 等) *(待扩展)*

```json
"front_radar": {
  "type": "ultrasonic",
  "trig_pin": 4,
  "echo_pin": 5,
  "use_rmt": false,
  "auto_poll_ms": 50
}
```

---

### 4. PWM / 执行器外设 (Servo / Motor / Buzzer)

#### (1) `dc_motor` - 有刷直流（H 桥）

> 默认拓扑 **`in_in`**（PWM + IN_A + IN_B）——**不是**业界 Phase/Enable。`phase_enable` / `pwm_on_in` 为**预留**值，未实现时 init fail-closed。

```json
"left_motor": {
  "type": "dc_motor",
  "role": "open_loop_actuator",
  "pwm_channel": 0,
  "dir_pin_a": 18,
  "dir_pin_b": 19
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `type` | 是 | 固定 `"dc_motor"`（语义族；不是芯片名） |
| `role` | 否 | 缺省 `open_loop_actuator` → `{name}_set_speed` / `coast` / `brake` / `safe_off` |
| `pwm_channel` / `dir_pin_a` | 是 | 见 codegen `required_fields`；`dir_pin_b` 可选（单方向） |
| `drive_mode` | 否 | advanced；默认省略 = `in_in`；仅 `"in_in"` 已实现 |
| `enable_pin` | 否 | advanced；STBY/nSLEEP（高有效）；`-1` 或未写 = 不用 |
| `driver_ic` | 否 | 一般不需要；与 `drive_mode` 易重复 |

**IN/IN 真值表**（`dir_pin_a`=A，`dir_pin_b`=B）：

```text
dir_a  dir_b | state
  0      0   | coast
  1      0   | forward
  0      1   | reverse
  1      1   | brake (short)
```

**`safe_off` 层级**：有 `enable_pin` → brake（若可）+ 拉低 enable；无 enable 双脚 → brake；单脚 → `WINK_ERR_UNSUPPORTED`（见 ADR-0048）。

#### (2) `rc_servo` - 航模舵机

```json
"neck_servo": {
  "type": "rc_servo",
  "pwm_channel": 0,
  "min_pulse_ms": 0.5,
  "max_pulse_ms": 2.5,
  "max_angle": 180
}
```

| 字段 | 说明 |
|------|------|
| `min_pulse_ms` / `max_pulse_ms` | stable；脉宽范围（ms） |
| `max_angle` | stable；0 或未写 → **180°**；钳位 `angle ∈ [0, effective_max_angle]` |
| 脉宽映射 | `pulse_ms = min_pulse + (angle / effective_max_angle) * (max_pulse - min_pulse)` |

Role 缺省 `angular_actuator` → `{name}_set_angle`（fire-and-forget）。

#### (3) `encoder` - 旋转编码器（脉冲计数）

```json
"wheel_encoder": {
  "type": "encoder",
  "pin_a": 34,
  "pin_b": 35
}
```

| 字段 | 说明 |
|------|------|
| `decode_mode` | advanced；默认 `x1_rising`；`x2`/`x4` 未实现 → init 失败 |
| `invert` | advanced；**交换 A/B 方向语义**，非简单取负计数 |
| Role | 缺省 `pulse_counter` → `get_count` / `reset`；**无 CPR**（物理换算在 BAL） |

x1 协议：A 上升沿采 B；B 高 ++，B 低 --；无 `pin_b` 仅递增。

舵机等其它执行器仍用各自字段，**不要**套用上表 H 桥扩展列。

---

## ⚙️ 三、 高级属性与驱动选项 (Advanced)

*(后续补充 `use_rmt`, `auto_poll_ms`, `event_drive` 等底层驱动参数)*

---

## ❓ 四、 常见报错与排查指南 (Troubleshooting)

1. **`pin 'sda' not in Manifest.pins`**
   * ❌ **原因**：写成了 `"sda": 21` 或 `"pin": 2`，缺少 `_pin` 后缀。
   * ✅ **解决**：修改为 `"sda_pin": 21` 或 `"gpio_pin": 2`。

2. **I2C 物理引脚冲突 (Pin Conflict)**
   * ❌ **原因**：声明了相同的 `i2c_bus: 0`，但给不同设备指定了不同的 `sda_pin`。
   * ✅ **解决**：在同一个 `i2c_bus` 上的所有设备必须使用相同引脚；如需使用不同引脚，请更换 `i2c_bus`。

3. **屏幕无显示或通信失败**
   * ❌ **原因**：`i2c_addr` 填写了十六进制字符串（如 `"0x3C"`）或填写错误。
   * ✅ **解决**：`i2c_addr` 必须填 **十进制数字**（`0x3C` 转换为十进制为 `60`）。
