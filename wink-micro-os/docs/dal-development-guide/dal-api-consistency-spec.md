# DAL API 设计与一致性规范 (DAL API Consistency Specification)

| 项 | 内容 |
|----|------|
| **规范版本** | v1.0.0 (Draft for Review) |
| **状态** | 拟定中 / 待评审 |
| **适用范围** | `wink-micro-os` 器件抽象层 (`dal/`) 驱动开发与代码生成器 (`codegen`) |
| **关联活规范** | [`docs/design/02-wink-micro-os/01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| **关联 ADR** | [ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md) (静态分发), [ADR-0043](../../../docs/design/decisions/0043-yaml-architecture-rules.md) (Lint规约), [ADR-0048](../../../docs/design/decisions/0048-actuator-control-semantic-naming.md) (语义命名) |

---

## 1. 背景与架构目标

`wink-micro-os` 的 **DAL（Device Abstraction Layer，器件抽象层）** 承载了各种传感器、执行器、显示屏与通信模块的逻辑驱动。随着系统集成的硬件种类不断增加，如果缺乏统一的 API 规范，容易出现以下痛点：

1. **命名碎片化**：不同开发者可能对相同动作混用 `turn_on` / `enable` / `start` / `on`。
2. **工具链适配成本高**：代码生成器 (`app_codegen.py`) 和静态检查器 (`wink.py lint`) 难以用统一的模式自动化提取设备能力。
3. **应用层学习曲线陡峭**：用户在掌握 `dal_led` 后，无法自然推导出 `dal_dc_motor` 或 `dal_ultrasonic` 的通用使用范式。

### 核心设计目标

* **高度一致性 (Consistency)**：统一生命周期范式与领域动词库。
* **零破坏性演进 (Non-breaking Evolution)**：新特性的补充（如功耗、回调、状态机）绝不破坏现有上层 App/BAL 代码的调用逻辑。
* **两端同源 (Dual-Target Simulation Consistency)**：保持 Wasm 仿真与真实 SoC 硬件跑同一套 C 驱动代码。
* **静态化与零动态分配 (Zero Dynamic Allocation)**：POD 句柄模式，无 vtable 虚表开销。

---

## 2. 核心维度一：基础通用契约 (Mandatory Core Contract)

所有 DAL 器件驱动（无论属于哪种硬件类型）**必须 100% 具备**以下统一数据模式与 5 大通用生命周期 API。

### 2.1 数据结构与句柄规范

1. **配置结构体**：`dal_<type>_config_t`
   * 必须包含 `const char *owner;`（设备树实例名与静态资源占用标识）。
   * 结构体成员按数据类型尺寸降序排列（如 `uint32_t` → `uint16_t` → `bool`），消除自然对齐填充空隙。
2. **实例句柄**：`dal_<type>_t`
   * 必须为 POD (Plain Old Data) 结构体，首个成员必须内嵌 `dal_<type>_config_t config;`。
   * 必须包含 `bool initialized;` 状态标志。

### 2.2 五大通用生命周期 API 范式

```c
/* 1. 初始化：认领资源，配置物理引脚/总线，校验参数 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_init(dal_<type>_t *dev, const dal_<type>_config_t *cfg);

/* 2. 反初始化：释放引脚/总线资源，关闭输出，置 initialized=false */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_deinit(dal_<type>_t *dev);

/* 3. 安全归位/应急切断：立即将硬件切断物理输出，回到安全状态 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_safe_off(dal_<type>_t *dev);

/* 4. 软件复位：重置器件内部软件状态机与硬件寄存器 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_reset(dal_<type>_t *dev);

/* 5. 状态查询：获取器件当前统一状态 */
dal_<type>_state_t dal_<type>_get_state(const dal_<type>_t *dev);
```

### 2.3 函数签名与契约注释规范

* **返回值**：必须统一返回 `wink_status_t`（`WINK_OK` 为 0，负数为错误），且必须加上 `WINK_WARN_UNUSED_RESULT` 强制上层检查错误。
* **第一参数**：必须是 `dal_<type>_t *dev` 句柄指针。
* **API Contract 注释**：所有公开头文件函数必须标注以下契约元数据：
  ```c
  /**
   * @brief ...
   * @note API Contract:
   *   - Preconditions: dev 非 NULL；init 已成功。
   *   - Blocking: No / Yes(ms).
   *   - Thread-safe: No; ISR-safe: No.
   *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED.
   */
  ```

---

## 3. 核心维度二：领域 Trait 分类与动词规范 (Domain Conventions)

根据物理器件的业务属性，DAL 将所有硬件收敛划分为 **4 大领域 Trait**。各领域内的动词必须严格遵循统一词库，**严禁私有自创动词**。

```
                              DAL 器件领域划分
                                     │
         ┌──────────────────┬────────┴─────────┬──────────────────┐
         ▼                  ▼                  ▼                  ▼
  [ Output/Actuator ]    [ Sensor/Input ]    [ Display/Visual ]  [ Comm/Storage ]
  输出与执行器类         传感器与输入类       显示屏与视觉类      通信与存储类
```

### 3.1 输出/执行器大类 (Output / Actuator Trait)

* **涵盖器件**：LED, Relay, DC Motor, RC Servo, Stepper Motor, Buzzer.
* **允许的标准规范动词**：

| 动词 / API 范式 | 适用场景 | 示例 API |
| :--- | :--- | :--- |
| `on(dev)` | 开启/点亮 | `dal_led_on(dev)` |
| `off(dev)` | 关闭/熄灭 | `dal_led_off(dev)` |
| `toggle(dev)` | 翻转状态 | `dal_led_toggle(dev)` |
| `set(dev, bool on)` | 显式设置开关 | `dal_led_set(dev, true)` |
| `set_<property>(dev, val)` | 设置物理量（速度/角度/频率等） | `dal_dc_motor_set_speed(dev, 80)`<br>`dal_rc_servo_set_angle(dev, 90.0f)` |
| `get_<property>(dev, *val)`| 获取当前物理量设置 | `dal_dc_motor_get_speed(dev, &speed)` |
| `is_on(dev)` | 查询当前是否激活 | `dal_led_is_on(dev)` |

* **黑名单（禁用词）**：❌ `turn_on`, `enable_output`, `run_motor`, `spin`, `start_pwm`.

---

### 3.2 传感器/输入大类 (Sensor / Input Trait)

* **涵盖器件**：Ultrasonic, Temp/Humidity, Button, Encoder, IMU, Photoelectric.
* **允许的标准规范动词**：

| 动词 / API 范式 | 适用场景 | 示例 API |
| :--- | :--- | :--- |
| `read(dev, *out)` | 同步读取通用测量值 | `dal_encoder_read(dev, &count)` |
| `read_<metric>(dev, *out)`| 读取具体物理量（厘米/摄氏度等） | `dal_ultrasonic_read_cm(dev, &cm)`<br>`dal_dht11_read_temp_c(dev, &temp)` |
| `read_async(dev, *buf, len)`| 异步/DMA 读取 | `dal_adc_read_async(dev, buffer, 64)` |
| `read_state(dev, *state)` | 读取逻辑状态（如按键按下/弹起） | `dal_button_read_state(dev, &st)` |
| `is_<condition>(dev)` | 快速条件判断 | `dal_button_is_pressed(dev)` |
| `calibrate(dev)` / `zero(dev)` | 传感器校准/相对值清零 | `dal_encoder_zero(dev)` |

* **黑名单（禁用词）**：❌ `get_value`, `fetch_data`, `sample_now`, `get_dist`.

---

### 3.3 显示屏/视觉大类 (Display Trait)

* **涵盖器件**：Mono OLED, TFT LCD, Segment LED, E-Paper.
* **允许的标准规范动词**：

| 动词 / API 范式 | 适用场景 | 示例 API |
| :--- | :--- | :--- |
| `clear(dev)` | 清空显存/屏幕 | `dal_mono_oled_clear(dev)` |
| `update(dev)` / `flush(dev)` | 将显存刷入物理屏幕 | `dal_mono_oled_update(dev)` |
| `set_brightness(dev, level)`| 设置背光/亮度 | `dal_lcd_set_brightness(dev, 128)` |
| `draw_<shape>(dev, ...)` | 基础绘制函数 | `dal_mono_oled_draw_pixel(dev, x, y, color)` |

* **黑名单（禁用词）**：❌ `clean_screen`, `refresh_display`, `light_on`.

---

### 3.4 通信/存储大类 (Storage & Comm Trait)

* **涵盖器件**：EEPROM, SPI Flash, GPS, LoRa, CAN Node.
* **允许的标准规范动词**：

| 动词 / API 范式 | 适用场景 | 示例 API |
| :--- | :--- | :--- |
| `read(dev, addr, buf, len)` | 读取数据块 | `dal_eeprom_read(dev, 0x10, data, 32)` |
| `write(dev, addr, buf, len)`| 写入数据块 | `dal_eeprom_write(dev, 0x10, data, 32)` |
| `erase(dev, addr, len)` | 擦除存储扇区 | `dal_flash_erase(dev, 0x1000, 4096)` |
| `flush(dev)` | 刷入写缓冲区 | `dal_storage_flush(dev)` |

---

## 4. 核心维度三：扩展控制窗口 (Unified IOCTL Window)

为防止特定硬件的特殊属性（如 CAN 过滤器配置、IMU 采样滤波深度、OLED 翻转显示）导致 DAL 出现大量零散的特有 API，所有 DAL 支持统一控制窗口扩展：

```c
/* 通用扩展控制口 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_control(dal_<type>_t *dev, uint32_t cmd, void *arg);
```

* `cmd` 命名规则：`DAL_<TYPE>_CMD_<ACTION>`（如 `DAL_CAN_CMD_SET_FILTER`）。
* 非通用、高频特有操作强制收敛入 `control()`，保持通用 API 面的简洁。

---

## 5. 核心维度四：功耗模式与异步回调规范

### 5.1 功耗模式 API 规范 (Power Modes)

器件级功耗 API 必须与系统 PM 框架协同：

```c
/* 挂起/进入器件级低功耗模式 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_suspend(dal_<type>_t *dev);

/* 唤醒/恢复器件全速工作模式 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_resume(dal_<type>_t *dev);
```

### 5.2 异步事件回调注册 (Async Callbacks)

涉及异步中断触发或 DMA 传输完成的器件，采用统一的回调注册签名：

```c
typedef void (*dal_<type>_event_cb_t)(dal_<type>_t *dev, dal_<type>_event_t event, void *user_data);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_register_callback(dal_<type>_t *dev, dal_<type>_event_cb_t cb, void *user_data);
```

---

## 6. 向下兼容性保障法则 (Non-breaking Evolution Rules)

在演进和扩展 DAL API 时，必须遵守以下三条硬性红线：

1. **初始化即就绪策略 (Init-to-Ready)**：
   * `dal_<type>_init()` 成功后，器件默认进入 `READY` 或 `IDLE` 状态。
   * **禁止** 要求旧调用方必须显式调用新加的 `start()` 才能工作。
2. **结构体追加兼容性**：
   * 若在 `dal_<type>_t` 中新增内部状态位，必须追加在结构体末尾。
   * 保证已有的 `{0}` 静态清零初始化不受影响。
3. **保持同步 API 存续**：
   * 增加异步与回调 API 时，原有的同步阻塞读写 API 必须完整保留。

---

## 7. 工具链自动化约束与 CI 强校验

本规范由 `wink-tools/tools/lint` 规则引擎与 YAML 驱动元数据（`codegen/drivers/*.yaml`）强行约束。

### 7.1 YAML 元数据驱动 Trait 声明范例

每个 DAL 驱动必须在 `wink-micro-os/codegen/drivers/` 目录下提供对应 YAML 规范描述：

```yaml
name: dc_motor
trait: Actuator
c_type: dal_dc_motor_t
config_type: dal_dc_motor_config_t
headers: [dal_dc_motor.h]
lifecycle:
  init_fn: dal_dc_motor_init
  deinit_fn: dal_dc_motor_deinit
  safe_off_fn: dal_dc_motor_stop
standard_verbs:
  set_speed: dal_dc_motor_set_speed
  get_speed: dal_dc_motor_get_speed
```

### 7.2 CI Lint 校验项

在代码提交与构建时，运行 `python wink-tools/wink.py lint` 校验以下规则：
* **Rule-DAL-01**：校验头文件中所有 API 是否均带有 `wink_status_t` 返回值及 `WINK_WARN_UNUSED_RESULT`。
* **Rule-DAL-02**：校验函数命名是否落在对应 Trait 的规范动词集中（禁止黑名单动词）。
* **Rule-DAL-03**：校验 `config_t` 成员布局是否自然对齐无填充。
