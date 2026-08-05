# DAL Role 面板与能力角色架构规范 (Role Architecture Spec)

| 项 | 内容 |
|---|---|
| **规范名称** | Wink OS DAL Role 面板与能力角色规范 (Role Architecture Spec) |
| **文档定位** | 全项目 Role 面板抽象、能力动词契约与 29 DAL Type 映射的 **SSOT (Single Source of Truth)** |
| **创建日期** | 2026-08-05 |
| **现行版本** | v1.0.0 (Wink OS DAL Spec v2.0 基础) |
| **关联规范** | [`dal-best-practices.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3、[`role-interface-codegen.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/docs/dal-development-guide/role-interface-codegen.md) (操作指南)、[ADR-0056](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) (物理量量纲)、[ADR-0051](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/design/decisions/0051-scannable-codegen-extension-roots.md) |

---

## 1. 架构总纲与三维抽象定位

### 1.1 三维抽象边界
在 Wink OS 外设抽象体系中，必须严格区分以下三个维度，杜绝跨层污染：

```text
wink-app.json 实例声明
  │
  ├─► type        → 控制语义族 / 哪个 DAL（驱动平面）
  │                 回答「是什么驱动 / 控什么量」，如 dc_motor, ultrasonic, led
  │
  ├─► role        → 面向 App 的能力角色接口（能力平面；可缺省）
  │                 回答「当什么用」，由 Codegen 生成 {instance}_{verb} 封装
  │
  └─► variant     → 同族变体（拓扑平面；变了才写）
                    回答「同族内怎么接线 / 解码 / 面板」，如 direct_gpio, ssr, latching_dual_pin
```

> 💡 **核心架构定稿句**：
> - **`type`** 回答「是什么驱动」（绑定 `dal_*` C 驱动代码与底层硬件控制协议）。
> - **`role`** 回答「当什么用」（由 Codegen 包装一层 `dal_*`，向 App 提供友好且具有多态性的 `{name}_{verb}` 动词）。
> - **`role` 不是 BAL**：BAL 是独立的可复用算法层（PID、闭环控制、按键事件引擎等）；Role 仅仅是 Codegen 编译期生成的内联封装门面 (Facade)。

---

## 2. Role 5 大域分类正交矩阵

为了覆盖 Wokwi 50 元件 ↔ 29 DAL `type` 的全量映射，同时避免 Role 数量随外设膨胀，全系统 Role 划分为 **5 大能力域、19 个正交 Role**：

```
                ┌────────────────────────────────────────────────────────┐
                │             Wink OS 能力平面 (Role Plane)              │
                └────────────────────────────────────────────────────────┘
                                            │
    ┌───────────────────┬───────────────────┼───────────────────┬───────────────────┐
    ▼                   ▼                   ▼                   ▼                   ▼
【A. 离散与开关域】   【B. 连续物理传感域】  【C. 运动与声学执行域】 【D. 显示与像素阵列域】  【E. 存储与通信总线域】
- binary_sensor     - analog_input      - open_loop_actuator- text_display        - byte_storage
- binary_indicator  - distance_sensor   - angular_actuator  - graphic_display     - block_storage
- level_indicator   - environment_sensor- position_actuator - pixel_array         - real_time_clock
- pulse_counter     - motion_sensor     - tone_generator                          - location_provider
```

---

## 3. 19 个标准 Role 全量定义与 Verb 动词契约

### 3.1 错误等级说明 (`error_class`)
每一个 Role 动词必须显式声明其错误响应等级：
* **`fire_and_forget`**：无返回值（`void`），内部使用 `WINK_IGNORE_RESULT` 强转。适用于高频/开环下发。
* **`convenience`**：标量便捷返回（如 `bool` / `float`），发生在错误时返回约定安全降级值（如 `-1.0f`）并打印 Log。
* **`normal`**：契约诚实接口，显式返回 `wink_status_t`，带 `WINK_WARN_UNUSED_RESULT` 属性，参数通过 out 指针输出。
* **`fatal`**：关键服务/事件启动接口，显式返回 `wink_status_t`，带 `WINK_WARN_UNUSED_RESULT` 属性，调用方必须检查。

---

### 3.2 19 个标准 Role 动词契约表

#### 域 A：离散与开关域 (Binary & Multi-level Signals)

##### 1. `binary_sensor`
* **描述**：二值数字输入（开关、按键、二值比较器触发器等）。
* **Verbs**：
  * `is_active` (`convenience` -> `bool`)：返回当前逻辑活动状态。
  * `is_active_status` (`normal` -> `wink_status_t`)：输出活动状态指针。
  * `was_active` (`convenience` -> `bool`)：边沿上升沿检测（读后自动清零/锁存复位）。
  * `was_active_status` (`normal` -> `wink_status_t`)：输出上升沿检测状态指针。
  * `enable_events` (`fatal` -> `wink_status_t`)：开启底层事件引擎。**回调上下文须由驱动实现在头注释中自声明**（ISR context / Timer task / Dedicated task）；`isr_safe` 字段在 `roles/*.yaml` 中显式标注（见 §5）。
  * `disable_events` (`fire_and_forget` -> `void`)：关闭底层事件引擎。**保证：调用返回后，任何 pending 回调不再被触发**（若有在途回调，同步等待其完成，避免竞态与野指针）。
  * `start_auto_poll` (`fatal` -> `wink_status_t`)：开启软定时轮询。
  * `stop_auto_poll` (`fire_and_forget` -> `void`)：停止软定时轮询。

##### 2. `binary_indicator`
* **描述**：二值数字输出指示器（LED、继电器线圈、固态开关等）。
* **Verbs**：
  * `activate` (`fire_and_forget` -> `void`)：激活/导通指示器。
  * `deactivate` (`fire_and_forget` -> `void`)：断开/关闭指示器。
  * `toggle` (`fire_and_forget` -> `void`)：翻转指示器状态。

##### 3. `level_indicator`
* **描述**：多级/条形段数指示器（LED Bar Graph）。
* **Verbs**：
  * `set_level_promille` (`fire_and_forget` -> `void`)：按千分比 (`[0, 1000]`) 设置指示位阶。
  * `clear` (`fire_and_forget` -> `void`)：清空所有指示。

##### 4. `pulse_counter`
* **描述**：脉冲计数器（正交编码器、单向脉冲计数等）。读取原始脉冲累计数，**不含 CPR 换算**；角速度/位移换算由 App 或 BAL 层负责。对应 `encoder` DAL type（`x1` 编码模式，A 上升沿采 B；B 高 → count+，B 低 → count−）。
* **Verbs**：
  * `get_count` (`convenience` -> `int32_t`)：读取当前累计脉冲数（有符号，正转 +，反转 −）。错误时返回 `INT32_MIN`。
  * `get_count_status` (`normal` -> `wink_status_t`)：契约诚实读取脉冲数（out 指针输出）。
  * `reset` (`fire_and_forget` -> `void`)：将计数器清零。

---

#### 域 B：连续物理传感域 (Continuous Physical Quantities)

##### 5. `analog_input`
* **描述**：连续标量模拟量输入（电位器、NTC 原始量、光敏电阻、气体 AO、称重原始量等）。
* **Verbs**：
  * `read_promille` (`convenience` -> `uint16_t`)：读取归一化千分比值 (`0~1000`)。
  * `read_mv` (`convenience` -> `uint16_t`)：读取采集引脚电压 (mV)。
  * `read_promille_status` (`normal` -> `wink_status_t`)：契约诚实读取千分比。

##### 6. `distance_sensor`
* **描述**：距离测量传感器（超声波 HC-SR04、ToF 测距等）。
* **Verbs**：
  * `request_measurement` (`normal` -> `wink_status_t`)：发起非阻塞测量请求。
  * `read_distance` (`convenience` -> `float`)：读取距离 cm 值（错误返回 `-1.0f`）。
  * `read_distance_status` (`normal` -> `wink_status_t`)：契约诚实读取距离 cm 值。

##### 7. `environment_sensor`
* **描述**：环境温湿度/气压多字段传感器（DHT22、BME280 等）。
* **Verbs**：
  * `read_temperature_celsius` (`convenience` -> `float`)：读取摄氏温度。
  * `read_humidity_promille` (`convenience` -> `uint16_t`)：读取相对湿度千分比 (`0~1000`)。
  * `read_environment_status` (`normal` -> `wink_status_t`)：契约诚实读取温湿度。

##### 8. `motion_sensor`
* **描述**：多轴运动/姿态传感器（MPU6050 6轴 IMU 等）。
* **Verbs**：
  * `read_accel_mg` (`normal` -> `wink_status_t`)：读取三轴加速度 (mg)。
  * `read_gyro_dps` (`normal` -> `wink_status_t`)：读取三轴角速度 (dps)。
* **Wave B 预留扩展动词**（当前驱动实现返回 `WINK_ERR_UNSUPPORTED`）：
  * `read_quaternion` (`normal` -> `wink_status_t`)：读取 DMP 融合姿态四元数（WXYZ），适用于 MPU6050 DMP 模式。

---

#### 域 C：运动与声学执行域 (Actuators & Motion Control)

##### 9. `open_loop_actuator`
* **描述**：开环速度执行器（DC 直流电机、开环步进电机等）。
* **行为语义契约**：
  * `set_speed_promille` 正值为正转方向，负值为反转；`0` 等效 `coast`（高阻），越界值饱和钳位（禁止回卷）。
  * `coast` — 物理含义：H 桥四管全关，电机进入**高阻自由滑行**（非制动，反 EMF 不被抑制）。
  * `brake` — 物理含义：**短路制动**（low-side brake），电机绕组两端接地，反 EMF 快速耗散制动。
  * `safe_off` — 失效安全策略：优先执行 `brake`；无法 brake（单方向脚无 `dir_pin_b`）时返回 `WINK_ERR_UNSUPPORTED`（**严禁 silent coast**，见 ADR-0048）。
* **Verbs**：
  * `set_speed_promille` (`normal` -> `wink_status_t`)：按千分比 (`[-1000, 1000]`) 设置开环转速。
  * `coast` (`normal` -> `wink_status_t`)：惰性滑行（高阻）。
  * `brake` (`normal` -> `wink_status_t`)：短路制动。
  * `safe_off` (`normal` -> `wink_status_t`)：安全关断。

##### 10. `angular_actuator`
* **描述**：角度位置控制执行器（RC 舵机）。
* **Verbs**：
  * `set_angle` (`fire_and_forget` -> `void`)：设置绝对角度 (`0.0f~180.0f` 度)。

##### 11. `position_actuator`
* **描述**：精准位置/脉冲步数执行器（步进电机目标位置模式）。
* **Verbs**：
  * `move_steps` (`normal` -> `wink_status_t`)：相对移动指定步数。
  * `move_to_step` (`normal` -> `wink_status_t`)：绝对移动到目标步数。
  * `is_busy` (`convenience` -> `bool`)：查询脉冲运动是否进行中。

##### 12. `tone_generator`
* **描述**：声学/频率发声执行器（无源蜂鸣器、压电扬声器）。
* **Verbs**：
  * `play_tone_hz` (`fire_and_forget` -> `void`)：按指定 Hz 频率发声。
  * `stop_tone` (`fire_and_forget` -> `void`)：停止发声。

---

#### 域 D：显示与像素阵列域 (Displays & Pixel Arrays)

##### 13. `text_display`
* **描述**：字符/点阵屏文本模式（LCD1602、LCD2004、Mono OLED 文本模式）。
* **Verbs**：
  * `clear` (`fire_and_forget` -> `void`)：清屏。
  * `draw_text` (`fire_and_forget` -> `void`)：在指定行列绘制文本。
  * `flush` (`fire_and_forget` -> `void`)：物理刷屏。

##### 14. `graphic_display`
* **描述**：彩屏/高阶图形屏（ILI9341 SPI 彩屏）。
* **Framebuffer 所有权契约**：`fill_rect` 写入内部 framebuffer；`request_flush` 发起 DMA 传输。**单缓冲实现中，在 `is_flush_done` 返回 `true` 前调用 `fill_rect` 属于 undefined behavior（画面撕裂风险）**。实现双缓冲（ping-pong）的驱动须在头文件中声明 `WINK_GFX_DOUBLE_BUFFERED` 宏。
* **Verbs**：
  * `fill_rect` (`fire_and_forget` -> `void`)：填充矩形区域（写入内部 framebuffer，不立即发起传输）。
  * `request_flush` (`normal` -> `wink_status_t`)：发起 DMA 异步刷屏。
  * `is_flush_done` (`convenience` -> `bool`)：查询 DMA 刷屏是否完成。

##### 15. `pixel_array`
* **描述**：可寻址 RGB/单色灯阵与灯环（WS2812 / NeoPixel）。
* **Verbs**：
  * `set_pixel_color` (`fire_and_forget` -> `void`)：设置单个像素 RGB 颜色。
  * `set_brightness` (`fire_and_forget` -> `void`)：设置全局亮度 (`0~255`)。
  * `show` (`fire_and_forget` -> `void`)：将颜色数据刷新至灯阵。
  * `clear` (`fire_and_forget` -> `void`)：清空所有像素 RGB 颜色为 0。
  * `get_pixel_count` (`convenience` -> `uint16_t`)：读取物理灯珠总数。

---

#### 域 E：存储与通信总线域 (Storage & Communication)

##### 16. `real_time_clock`
* **描述**：RTC 实时时钟（DS1307 等）。
* **Verbs**：
  * `get_time_epoch_s` (`normal` -> `wink_status_t`)：读取 UNIX 时间戳 (秒)。
  * `set_time_epoch_s` (`normal` -> `wink_status_t`)：设置 UNIX 时间戳 (秒)。

##### 17. `location_provider`
* **描述**：地理定位提供者（GPS/GNSS NMEA 模块）。
* **Verbs**：
  * `get_coordinates` (`normal` -> `wink_status_t`)：读取经纬度与海拔。
  * `is_fix_valid` (`convenience` -> `bool`)：查询定位是否有效。

##### 18. `byte_storage`
* **描述**：字节寻址持久化存储（EEPROM、Fram 等）。
* **Verbs**：
  * `read_bytes` (`normal` -> `wink_status_t`)：读取指定字节偏移的数据。
  * `write_bytes` (`normal` -> `wink_status_t`)：写入指定字节偏移的数据。
  * `get_capacity_bytes` (`convenience` -> `uint32_t`)：获取存储器总字节容量。

##### 19. `block_storage`
* **描述**：块/扇区设备寻址存储（MicroSD 卡、SPI Flash 块设备等）。
* **Verbs**：
  * `read_blocks` (`normal` -> `wink_status_t`)：读取 LBA 扇区块数据。
  * `write_blocks` (`normal` -> `wink_status_t`)：写入 LBA 扇区块数据。
  * `get_block_count` (`convenience` -> `uint32_t`)：获取总块数。
  * `get_block_size` (`convenience` -> `uint16_t`)：获取单块字节数（如 512B）。

---

## 4. 全量 29 个 DAL Type ↔ Default Role 映射全景表

| # | DAL Category | DAL `type` | 对应 Wokwi 组件 | 官方 `default_role` | 可选 alternative roles |
|---|---|---|---|---|---|
| 1 | input | `button` | `pushbutton`, `slide-switch` | `binary_sensor` | — |
| 2 | input | `analog_knob` | `potentiometer`, `slide-pot` | `analog_input` | — |
| 3 | input | `keypad` | `membrane-keypad` | `binary_sensor` | — |
| 4 | input | `ir_receiver` | `ir-receiver` | `binary_sensor` | — |
| 5 | output | `led` | `led`, `rgb-led` | `binary_indicator` | — |
| 6 | output | `relay` | `ks2e-m-dc5` | `binary_indicator` | — |
| 7 | output | `buzzer` | `buzzer` | `tone_generator` | `binary_indicator` |
| 8 | output | `led_bar` | `led-bar-graph` | `level_indicator` | `pixel_array` |
| 9 | actuator | `dc_motor` | — (H桥) | `open_loop_actuator` | — |
| 10 | actuator | `rc_servo` | `servo` | `angular_actuator` | — |
| 11 | actuator | `stepper` | `stepper-motor` | `open_loop_actuator` | `position_actuator` |
| 12 | sensor | `ultrasonic` | `hc-sr04` | `distance_sensor` | — |
| 13 | sensor | `encoder` | `ky-040` | `pulse_counter` | `analog_input` |
| 14 | sensor | `analog_sensor` | `ntc`, `photoresistor`, `gas` | `analog_input` | — |
| 15 | sensor | `digital_sensor` | `gas`(DO), `flame`(DO) | `binary_sensor` | — |
| 16 | sensor | `temp_humidity` | `dht22` | `environment_sensor` | — |
| 17 | sensor | `motion` | `pir-motion-sensor` | `binary_sensor` | — |
| 18 | sensor | `imu` | `mpu6050` | `motion_sensor` | — |
| 19 | sensor | `load_cell` | `hx711` | `analog_input` | — |
| 20 | sensor | `heart_rate` | `heart-beat-sensor` | `analog_input` | — |
| 21 | display | `lcd_char` | `lcd1602`, `lcd2004` | `text_display` | — |
| 22 | display | `mono_oled` | `ssd1306` | `text_display` | `graphic_display` |
| 23 | display | `tft` | `ili9341` | `graphic_display` | — |
| 24 | display | `led_matrix` | `neopixel`, `neopixel-matrix` | `pixel_array` | — |
| 25 | display | `seg_display` | `7segment` | `text_display` | `level_indicator` |
| 26 | storage | `eeprom` | — | `byte_storage` | — |
| 27 | storage | `sdcard` | `microsd-card` | `block_storage` | — |
| 28 | storage | `rtc` | `ds1307` | `real_time_clock` | — |
| 29 | comm | `gps` | — (NMEA) | `location_provider` | — |

> **⚠️ 当前 Phase 语义降级说明**
> - `keypad` → `binary_sensor`：当前仅检测「任意键按下」，**不解码矩阵键值**（哪行哪列）；若需键值读取，Escape Hatch 调用底层 `dal_keypad_read_key()`。Wave B 可考虑新增 `matrix_keypad` 专用 Role。
> - `ir_receiver` → `binary_sensor`：当前仅检测红外载波有无，**不解码 NEC/RC5/RC6 协议帧**（地址码 + 命令码）；帧解码能力留待 Wave B 新增专用 Role（如 `ir_remote_receiver`）。

---

## 5. Codegen 契约与 `roles/*.yaml` 对应关系

在系统构建与 Codegen 期间：
1. 每个 Role 在 **`wink-micro-os/codegen/roles/<role>.yaml`** 必须有对应的契约文件，仅声明 `id` 和 `verbs`（包含 `error_class`）。
2. 各 DAL 驱动在 **`wink-micro-os/codegen/drivers/<type>.yaml`** 中指定 `default_role`，并在 `role_bindings` 中为该 Role 提供 Jinja 渲染模板 `template`。
3. `wink-app.json` 中若未显式指定 `"role"` 字段，Codegen 自动消费 `default_role` 生成 `{instance}_{verb}` 门面代码。
4. **Role template 行数约束**：每个 verb 的 `template` 字符串须保持**单行调用（≤ 3 行展开等效）**，禁止在 template 中编写业务逻辑；同 type 实例数 ≥ 8 的场景（如多路 LED / 继电器阵列），应统一按 `_channel` / `index` 参数范式替代独立生成函数（如 `led_bar_set_channel_level(uint8_t index, ...)`），防止 `static inline` 代码尺寸线性膨胀。
5. **`device_tree.h` 增量编译优化**：Codegen 输出须做 content-hash diffing——内容未变时不 touch 文件时间戳，避免大型 App 中所有 `#include device_tree.h` 的翻译单元触发不必要的重编译。
