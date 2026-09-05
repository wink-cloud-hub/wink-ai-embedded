# DAL 控制语义族完备性评审（驱动平面 / `type` 绑定）

| 项 | 内容 |
|----|------|
| 评审日期 | 2026-07-28 |
| 评审对象 | `wink-micro-os/dal/include/**` 全部现网外设头文件；`wink-tools/tools/codegen/drivers/*.py` |
| 评审维度 | 器件的 **控制语义族与 DAL 绑定**（工程/驱动平面，即 `wink-app.json` 的 `type`） |
| 评审视角 | 上线后 API / ABI / JSON 破坏性风险；族内完备度；异族边界 |
| 关联 | [ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)、[ADR-0050](../../decisions/core/0050-rc-servo-industrial-servo-naming.md)、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md)、[dal-best-practices.md](../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md)、[01-dal-device-abstraction.md](../../design/02-wink-micro-os/01-dal-device-abstraction.md)、[电机 taxonomy 评审](./2026-07-28-dal-actuator-motor-taxonomy-review.md) |
| 结论 | 9 个现网 `type` 中：**ultrasonic / led 最接近可冻结**；**dc_motor / rc_servo / encoder / button / ssd1306 控制原语基本正确但有冻结前必修项**；**gps / eeprom 为 stub，不可当稳定契约**。已知必来的 config 维度必须在破坏窗口内进 ABI，禁止上线后再改语义。 |

---

## 一、总裁决

| 统计 | 数量 |
|------|------|
| 现网 `type` | 9 |
| 可近冻结（小补即可） | 2（`ultrasonic`、`led`） |
| 冻结前必修 | 5（`dc_motor`、`rc_servo`、`encoder`、`button`、`ssd1306`） |
| Stub / 不可冻结 | 2（`gps`、`eeprom`） |
| 已命名预留（非本评审缺陷） | `stepper` / `industrial_servo` / `bldc` |

**一句话**：执行器核心（`dc_motor` / `rc_servo`）控制原语已对齐 ADR-0048；真正危险的是「文档已承诺、代码未进 ABI」的拓扑/行程角字段，以及 `encoder` 解码模式未钉死——后者改动会无声打坏所有闭环增益。

本评审**只评驱动平面**（`type` ↔ DAL）。Role / 意图平面与 BAL 闭环另册；文中仅标注会反噬 `type` 冻结的交叉点。

---

## 二、评审标尺

### 2.1 族内应完备的契约

1. 控制物理量与单位钉死（占空比 / 角度 / cm / 脉冲…）
2. 生命周期 `init` / `deinit` + 资源 claim；执行器 `safe_off` 语义唯一且可审计
3. 同族多拓扑用 config 枚举（如 `drive_mode`），禁止芯片级 `type` / `WINK_USE_<CHIP>`
4. 异总线 / 异控制原理 → 新 `type`（机制二），勿硬塞
5. 公共头少暴露 BAL / 平台私货；**加法字段优先，改语义必须版本化**

### 2.2 破坏性变更分级

| 级别 | 含义 | 示例 |
|------|------|------|
| ABI 破 | 改结构布局 / 函数签名 / 删符号 / 改返回码契约 | `config_t` 中部插字段且无 reserved |
| 语义破 | 同名 API 行为变了 | `safe_off` 从 coast 改 brake；编码器改 x4 解码 |
| JSON 破 | required 增减、`type` 改名、默认值改变观测行为 | `motor`→`dc_motor`；去抖默认 20→30 |
| 可接受加法 | 新可选 config、新函数、新 role 动词（旧 App 不动） | `get_speed`；`invert=false` |

---

## 三、总览矩阵

| `type` | 控制语义 | 实现 | 冻结建议 | 最大破坏性风险 |
|--------|----------|------|----------|----------------|
| `dc_motor` | 有刷 DC 开环占空比 / 有符号速度 | 已实现 | **冻结前必修** | 拓扑字段未进 ABI；扩展易成破坏性 |
| `rc_servo` | 航模开环绝对角度 (PWM) | 已实现 | **冻结前必修** | 行程角硬编码 180°；连续旋转属异族 |
| `ultrasonic` | 脉冲回波测距 (cm) | 已实现 | 可近冻结 | TOF/I2C 测距勿并入；`use_rmt` 偏平台 |
| `encoder` | 正交/单相脉冲计数 | 已实现（薄） | **冻结前必修** | 解码模式/单位未钉死；改法会破语义 |
| `led` | GPIO 二值指示 | 已实现 | 可近冻结 | PWM/RGB/WS2812 必须新 `type` |
| `button` | GPIO 二值输入 + 去抖/事件 | 已实现（厚） | **冻结前必修** | BAL IRQ 钩子暴露在公共头；API 面过大 |
| `ssd1306` | I2C 单色页式文本显示 | 已实现 | **冻结前必修** | `type` 用芯片名；SPI/兼容芯片边界模糊 |
| `gps` | UART NMEA 地理定位 | Stub | **不可冻结** | 实现前再钉契约；`float` 精度隐患 |
| `eeprom` | I2C 字节寻址非易失存储 | Stub | **不可冻结** | 命名过宽；阻塞 API 与协作式冲突 |

---

## 四、逐个 `type` 深评

### 4.1 `dc_motor` — 有刷 DC 开环占空比

| 项 | 内容 |
|----|------|
| 控制量 | signed duty ∈ `[-1.0, 1.0]` |
| Role | **无** `default_role` |
| 目录 | `dal/include/actuator/dal_dc_motor.h` |
| 冻结建议 | 冻结前必修 |

**绑定正确性**：ADR-0048 正名后，`type` 回答「H 桥有刷开环速度」而非泛称 `motor`。公共 API（`set_speed` / `brake` / `coast` / `safe_off`→`brake`）已对齐决策，是现网执行器里契约最清晰的一类。

**已完善**：

| 项 | 状态 |
|----|------|
| 控制语义命名 | 达标（禁止 `motor` 作 DAL 前缀） |
| `set_speed` 单位与 `0`=coast | 文档 + 实现一致 |
| `brake` / `coast` 显式拆分 | 达标；单脚 `brake`→`WINK_ERR_UNSUPPORTED` |
| `safe_off` → `brake` | 钉死（ADR-0048） |
| `WINK_USE_DC_MOTOR` stub | 达标 |

**族内缺口**：

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | `drive_mode` / `enable_pin` 未进 config ABI | 活规范与 `dal-best-practices` 已把 `phase_enable` vs `pwm_on_in`、STBY/nSLEEP 定为同族拓扑扩展。当前 `dal_dc_motor_config_t` 仅有 pwm+dir 脚，隐含 Phase/Enable。上线后再插枚举/字段会破 designated initializer 与 Flash override 布局。**建议**：冻结前落地枚举（默认 `phase_enable`）+ 可选 `enable_pin=-1`，或显式 `reserved`/`version` 字段。 |
| P1 | 缺方向反相与读回 API | 常见板级接线反相需要 `invert` 或 `swap_ab`；`current_speed` 已缓存但无 `get_speed`。二者可加法，但 `invert` 默认 `false` 必须文档钉死。 |
| P1 | codegen 无 Role | 缺 `open_loop_actuator` 之类角色会导致 App 直接调 `dal_*`，与 ultrasonic/led 不一致。建议补 role 动词 `set_speed`/`coast`/`brake`，**勿**把 closed-loop 并进 DAL。 |
| P2 | 头文件依赖 `pal_hal.h` | `rc_servo` 刻意不引 `pal_*`（ADR-0034）；`dc_motor` 经 `wink_pin_t` 引入 PAL。长期希望 DAL 公共头去 PAL 类型化。 |

**异族勿并入**：I2C 智能驱动、串口 ESC/VESC、步进、FOC → 独立 `type`（机制二）。同族芯片别名用 `driver_ic`→`drive_mode`，禁止 `WINK_USE_TB6612`。

---

### 4.2 `rc_servo` — 航模开环绝对角度

| 项 | 内容 |
|----|------|
| 控制量 | 角度 (°) |
| Role | `angular_actuator`（`set_angle`） |
| 目录 | `dal/include/actuator/dal_rc_servo.h` |
| 冻结建议 | 冻结前必修 |

**绑定正确性**：与 `industrial_servo` / 连续旋转舵机正交（ADR-0050）。`set_angle` + `safe_off`=limp + 脉宽校准 + advanced 时钟需求，主路径成熟。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | 行程角硬编码 0~180 | 头文件注释已承认未来 270° 应将 `max_angle` 作 config。今日钳位写死在实现里：上线后改钳位规则或加字段，所有依赖「超范围自动钳位」的 App 行为会变（**语义破**）。**冻结前**必须把 `min_angle`/`max_angle`（默认 0/180）写入 `config_t`，并写进 Flash override wire 版本策略。 |
| P1 | 缺 `get_angle`；连续旋转勿并入 | 读回可加法。360° 连续旋转舵机控制的是速度/方向而非绝对角——属**异族**，禁止用 `set_angle` 伪装。 |
| P2 | `safe_off` 注释仍写「未来 DC」 | DC 已独立；注释过时易误导审查。非 API 破，属文档债。 |

---

### 4.3 `ultrasonic` — 脉冲回波测距

| 项 | 内容 |
|----|------|
| 控制量 | 距离 cm |
| Role | `distance_sensor`（完整动词表 + BAL 事件） |
| 目录 | `dal/include/sensor/dal_ultrasonic.h` |
| 冻结建议 | **可近冻结** |

**绑定正确性**：命名正确（语义族而非 `hc_sr04`）。非阻塞双 API + 状态机 + ADR-0017 阻塞隔离 + `apply_override` + Role/事件，是传感器样板。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| P1 | 族边界：TOF / I2C 测距 | VL53 等是 flight-time / 寄存器语义，不是 Trig/Echo。强行扩 `ultrasonic` config 塞 `bus_type` 会同时破 JSON 与仿真模型。应预留独立 `type`（如 `tof_distance`），本族保持脉冲回波。 |
| P2 | `use_rmt` 与超时/量程未配置化 | `use_rmt` 偏 ESP32 平台旋钮；`max_range_cm` / timeout 若后加且改变默认超时，属语义破。建议：平台旋钮进 `advanced`；量程用可选字段且默认值与今日行为逐字节一致。 |

**冻结契约**应以 `request_measurement` + `get_cached_distance` 为准；阻塞 `dal_ultrasonic_read` 已 deprecated，保留到迁移完成可接受。

---

### 4.4 `encoder` — 脉冲/正交计数

| 项 | 内容 |
|----|------|
| 控制量 | 脉冲计数 (`int32`) |
| Role | **无** |
| 目录 | `dal/include/sensor/dal_encoder.h` |
| 冻结建议 | 冻结前必修 |

**绑定正确性**：族定位正确——反馈计数器，不是电机。但实现是「A 上升沿 + 读 B」的简化正交，公共契约未声明解码模式/CPR——**这是上线后最危险的传感器之一**。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | 解码模式未钉死 | 今日仅 A 上升沿采样 B。若日后改为 4× 正交或 PCNT 硬件，同转速下 `count` 斜率变 4 倍——所有闭环 PID 增益瞬间错（**语义破，比改函数名更狠**）。冻结前：`config` 增加 `decode_mode` 枚举（`x1_rising` / `x2` / `x4`），默认钉死为当前行为；或文档+测试锁定「仅 x1」并拒绝静默升级。 |
| **P0** | 缺 `counts_per_rev` / `invert`；单位仅有「脉冲」 | BAL closed_loop 需要物理单位。若 App/BAL 各自假设 CPR，DAL 日后加字段会迫使双端改。建议 config 可选 `counts_per_rev`（0=纯脉冲）、`invert_direction`；`get_count` 语义保持脉冲，物理换算放 BAL——但字段要早留。 |
| P1 | 公共 config 泄漏 `pal_gpio_mode_t` | 与 `button` 的 `dal_button_pull_t` 相比，encoder 直接暴露 PAL 枚举。应用 DAL 侧 pull 枚举（可加法映射）。 |
| P2 | 无 `get_rate` / 无 role / 单相与双相仅靠 `pin_b=-1` | 建议显式 `mode` 枚举替代魔法 `-1`；role 如 `pulse_counter`：`get_count`/`reset`。速度估计属 BAL 亦可，但要在文档声明 Non-goal。 |

---

### 4.5 `led` — GPIO 二值指示

| 项 | 内容 |
|----|------|
| 控制量 | on/off |
| Role | `binary_indicator` |
| 目录 | `dal/include/output/dal_led.h` |
| 冻结建议 | **可近冻结** |

**绑定正确性**：族边界清晰。`on`/`off`/`set`/`toggle` + `active_high` 完备。危险在「好心扩展」。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| P1 | 禁止把 PWM 亮度 / RGB / WS2812 并入 `led` | 亮度是占空比语义；地址型灯带是帧协议语义。并入会迫使改 API（`set(bool)`→`set(float/rgb)`）直接破 App。应独立 `type`：`pwm_led` / `rgb_led` / `ws2812`。 |
| P2 | `is_actuator=true` 但无 `safe_off`；注释提 `apply_override` 未实现 | 故障关断注册表若期望执行器全有 `safe_off`，led 应提供 `off` 绑定或从 actuator 注册表排除。`apply_override` 注释与现实不符——避免文档承诺未交付 API。 |

---

### 4.6 `button` — GPIO 二值输入 + 事件

| 项 | 内容 |
|----|------|
| 控制量 | pressed / edge / long-press |
| Role | `binary_sensor`（完整 L1/L2 动词） |
| 目录 | `dal/include/input/dal_button.h` |
| 冻结建议 | 冻结前必修 |

**绑定正确性**：核心语义（去抖稳定态 + 边沿）完整，Role 与 BAL 事件也齐。问题是公共 API 面过宽：BAL IRQ 后端钩子进了 `dal_button.h`，冻结等于把层间私货写成稳定契约。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | 分层：BAL 钩子是否算「稳定 DAL API」 | `set_event_backend` / `enable_gpio_isr` / `consume_irq_pending` / `set_irq_hook` 是 BAL 协作面。若对 App/AI 宣称整头文件稳定，后续重构 IRQ 路径必破。**建议**：拆 `dal_button_bal.h` 或标记 internal；对外冻结面收敛到 `init`/`poll`/`is_pressed`/`was_pressed`/`on_event`/`set_long_press`/`set_debounce`/`deinit`（+ ISR counter 若产品需要）。 |
| P1 | 去抖默认值双源 | DAL 宏默认 3 samples≈30ms；codegen JSON 默认 `debounce_ms=20`。行为依赖是否走 BAL `enable_events`。冻结前统一默认并写兼容矩阵。 |
| P2 | 族边界 | 电容触摸、矩阵键盘、旋转编码器按键应独立 `type`。`long_press` 已在族内合理；多击/组合键属 BAL/App。 |

---

### 4.7 `ssd1306` — I2C 单色页式文本显示

| 项 | 内容 |
|----|------|
| 控制量 | 页式帧缓冲文本 |
| Role | `text_display`（解耦了 App 对芯片名的依赖） |
| 目录 | `dal/include/display/dal_ssd1306.h` |
| 冻结建议 | 冻结前必修 |

**绑定正确性**：能力上 `clear`/`draw_text`/`flush` 对教育场景够用。驱动平面 `type` 仍叫芯片名——与 actuator「禁芯片名」张力最大。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | `type` 命名策略必须拍板 | **选项 A**：保留 `ssd1306`（诚实：命令集绑定该控制器）。**选项 B**：改 `oled_i2c_text` / `mono_oled`，芯片进 `driver_ic`。B 更贴控制语义族，但 JSON `type` 改名是破坏性。上线前二选一；选 A 则文档写明「本 type=SSD1306 命令集」，SH1106/SPI 变体用新 `type` 或 `drive_mode`，禁止静默兼容改时序。 |
| P1 | 图形原语与总线拓扑 | `draw_pixel`/`bitmap` 可加法。SPI 版、128×32 已部分用 `height`——OK。若把 SH1106 列偏移塞进同一实现且无 mode，旧屏会花屏（语义破）。建议兼容芯片用显式 `panel_variant` 枚举，默认 `ssd1306`。 |
| P2 | 缺 invert/rotation/contrast；FB 固定 1024 | 大屏或双缓冲会冲结构体尺寸（ABI）。1024 钉死需在契约写明最大 128×64；更大面板新 `type`。 |

---

### 4.8 `gps` — UART NMEA 定位（Stub）

| 项 | 内容 |
|----|------|
| 控制量 | 地理定位 |
| Role | 无 |
| 目录 | `dal/include/comm/dal_gps.h` |
| 冻结建议 | **不可冻结** |

语义方向对（地理定位，而非裸 UART）。但 `init`/`poll`/`get_position` 均返回 `WINK_ERR_UNSUPPORTED`；`WINK_STRICT_NONBLOCKING` 下 `init` 声明消失。此时冻结等于把占位签名焊死。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | 实现前重审数据契约 | `latitude`/`longitude` 用 `float` 精度不足（约 1–2 m 级）；建议定稿用 `int32` 微度或定点数。`fix_valid` 过粗（无 2D/3D/DGPS）。NMEA 专用——UBX 二进制应另 `type` 或 `protocol` 枚举。阻塞 `init` 与协作式模型冲突，应改为非阻塞状态机（对齐 ultrasonic）。 |

**上线策略**：保持 experimental；codegen 可对 stub `type` 警告；待 UART+NMEA 落地再发 1.0 契约。

---

### 4.9 `eeprom` — I2C 字节寻址 NV（Stub）

| 项 | 内容 |
|----|------|
| 控制量 | 字节寻址非易失读写 |
| Role | 无 |
| 目录 | `dal/include/storage/dal_eeprom.h` |
| 冻结建议 | **不可冻结** |

当前命名 `eeprom` 过宽，实现假设 I2C AT24 风格；SPI FRAM、片上 NVS 被误绑风险高。

| 优先级 | 问题 | 说明 |
|--------|------|------|
| **P0** | 命名与地址空间 | 建议 `type=i2c_eeprom`（或 `nv_i2c`）以匹配总线；`addr` 为 `uint16_t` 但 `capacity` 为 `uint32_t`——大容量芯片地址空间不够（实现后必改签名=破）。阻塞 `read`/`write` 无非阻塞替代，违反 ADR-0017「不允许只有 blocking」。 |
| P1 | 与平台 NVS / Flash 覆写关系 | ADR-0008 设备配置覆写走 Flash blob，与用户 EEPROM 数据面不同。文档需钉死：本 DAL 不做设备树存储后端，避免 API 被塞进「配置分区」语义。 |

---

## 五、预留族（非缺陷，但是边界）

| 预留 `type` | 控制语义 | 与现网边界 | 并入现网的代价 |
|-------------|----------|------------|----------------|
| `stepper` | 步数/位置开环 | ≠ `dc_motor` 占空比 | 若用 `dc_motor` 假装步进 → 日后拆分必破 |
| `industrial_servo` | 闭环位置/速度/力矩 | ≠ `rc_servo` 开环角 | 中文「伺服」陷阱；禁止合并（ADR-0050） |
| `bldc` | 本地 FOC/换相 | ≠ `dc_motor`；ISR 属 ADR-0047 | 安全关断三相断开，不可借用 `brake` |

**异族红线（并入现 `type` 日后必破）**：步进 / 工业伺服 / FOC / 连续旋转舵机 / TOF / PWM LED / SPI OLED / 电容触摸 —— 应保持独立 `type`（ADR-0048 机制二）。

---

## 六、跨切割面：一致性债

| 严重度 | 议题 | 说明 |
|--------|------|------|
| 中 | 能力平面不齐 | 有 `default_role`：`led` / `button` / `ultrasonic` / `rc_servo` / `ssd1306`。无：`dc_motor` / `encoder` / `gps` / `eeprom`。上线后 App 风格分裂。补 role 是加法，但动词表一旦发布也会成契约，宜与 DAL 冻结同期设计。 |
| 中 | `apply_override` 覆盖不全 | 仅 `ultrasonic` / `rc_servo` 实现。`led`/`button` 注释承诺未兑现。若产品宣称 ADR-0008 全器件可覆写，缺实现是功能债；若覆写是可选能力，应删误导注释。 |
| 低 | 头文件 PAL 泄漏不均 | `rc_servo` 去 PAL 类型；`dc_motor`/`encoder` 仍依赖 `pal_hal`。长期统一成本随冻结上升。 |
| **高** | Stub 进 registry | `gps`/`eeprom` 已在 codegen known types。AI 可能生成调用。需要 codegen 级 experimental 门禁或文档大红灯，否则「能选 type」被误解为「能用」。 |

---

## 七、上线前建议动作（按优先级）

| 优先级 | 动作 | 保护的契约 |
|--------|------|------------|
| **P0** | `dc_motor`：落地 `drive_mode` + `enable_pin`（默认兼容今日接线） | 同族拓扑扩展不破 ABI |
| **P0** | `rc_servo`：config 增加 `max_angle`（默认 180）并版本化 override | 行程角语义 |
| **P0** | `encoder`：钉死 `decode_mode` + 可选 CPR/invert；或书面锁定仅 x1 | 闭环计数斜率 |
| **P0** | `button`：拆分/标记 BAL 内部 API；对外冻结面收敛 | 层间重构自由 |
| **P0** | `ssd1306`：拍板 `type` 名策略 + `panel_variant` 默认 | 显示兼容与 JSON |
| **P0** | `gps`/`eeprom`：experimental 门禁；实现前重审数值类型与非阻塞 | 避免焊死占位签名 |
| P1 | 补 `dc_motor` / `encoder` role；统一 debounce 默认 | App 调用风格 |
| P1 | 文档红线：PWM LED / TOF / 连续旋转舵机 / 步进 禁止并入现 `type` | 族边界 |
| P2 | DAL 公共头去 PAL 类型；清理过时注释与未实现 `apply_override` 承诺 | 可维护性 |

---

## 八、正面结论

`ultrasonic`（非阻塞测距）与 `led`（二值指示）在控制语义、API 形态与 Role 上最接近「发布级」。

`dc_motor` / `rc_servo` 的控制原语也对，但必须先把「已知必来的 config 维度」在破坏窗口内塞进 ABI，而不是上线后再打补丁。

---

## 九、后续流转建议

按文档体系：

1. Owner 确认本评审 P0 项采纳范围
2. 需要决策拍板的项（`ssd1306` 命名策略、`encoder` 是否仅锁定 x1、`button` 公共面收敛）→ 开 ADR 或并入已有 ADR 修订
3. 采纳项 → 写实施计划（③）或并入既有 [dal-bal-followup](../../implementation-plans/core/2026-07-28-dal-bal-followup-plan.md)
4. Accepted 决策 → 回写 [01-dal-device-abstraction.md](../../design/02-wink-micro-os/01-dal-device-abstraction.md) 与 [dal-best-practices.md](../../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md)

---

## 十、补遗（2026-07-28）：结合用户稳定面讨论的再评估

> 背景：同日讨论并起草 [用户稳定面绝缘设计](../../tech-designs/tools/2026-07-28-user-surface-insulation-design.md)。Owner 裁决：**不上板卡模板锁引脚**；绝缘重点为 **App C → Role**；`type`/引脚仍由各 App `wink-app.json` 灵活填写。本补遗从「尽量未来不引入破坏性变更」角度，评估原文完备性并给出优先级修正。

### 10.1 原文覆盖得好的部分（保持）

| 项 | 为何仍关键 |
|----|------------|
| 控制语义 / 异族边界（步进、工业伺服、TOF、PWM LED…） | Role **挡不住**语义并族；并错 `type` 日后必破 |
| `encoder` 解码模式、`rc_servo` 行程角、`dc_motor` 拓扑字段 | **语义破 / ABI 破**会穿透 Role（同名动词行为变了） |
| Stub（gps/eeprom）不可当稳定契约 | 与绝缘设计 experimental 门禁一致 |
| 破坏性分级（ABI / 语义 / JSON / 加法） | 标尺正确，应继续用 |

### 10.2 原文的结构性缺口（结合后续讨论）

原文自我限定「只评驱动平面」是对的，但从**产品破坏面**看，缺下面几块：

| 缺口 | 说明 | 建议 |
|------|------|------|
| **未区分「对谁破」** | `dal_*` 签名破 ≠ App Role 用户破；JSON 改脚是**可接受灵活代价**，不是缺陷 | 每个 P0 标注受众：`App-Role` / `JSON-author` / `DAL-direct` / `BAL-internal` |
| **Role 完备性仅 P1** | 绝缘策略下，缺 `default_role` 会逼用户直调 `dal_*`，反而**放大**破坏面 | 将「补 dc_motor/encoder role」升为与驱动 P0 **同波必做**（仍属加法，但发布门禁） |
| **Role 动词一旦发布即契约** | 原文提了但未列「动词冻结清单 / BAL-backed 标注」 | 与绝缘设计 §3 对齐；事件类动词单独标 BAL-backed |
| **无「可接受变更」白名单** | 易把「改引脚 JSON」也当成要防的破 | 钉死：同 App 改 advanced 接线 = 正常；破的是改单位/解码/动词/required 语义默认值 |
| **契约测试未进建议动作** | 语义破靠文档钉不住 | 补：encoder 边沿 golden、DC safe_off→brake、Role 行为 host 测 |
| **仿真 Manifest `type` 对齐** | 原文未写；改名/新 type 会破仿真侧 | 补：unisim / DriverBase.type 与 codegen known_types 同步门禁 |
| **BAL closed_loop 交叉** | encoder 斜率变会打坏 BAL，但评审未点名 BAL 测试 | 补：闭环 sample/单测作 decode 变更的回归锚点 |
| **错误码 / Convenience 返回约定** | 如 `read_distance` 失败 → `-1.0f`；改约定破 App | 补：Role Convenience 错误哨兵写入冻结表 |
| **Flash override wire 版本** | 仅 rc_servo/ultrasonic 提及布局；扩展 config 易忘版本 | 凡进 config 的新字段：无 override 则声明 Non-goal；有 override 必须 bump wire ver |
| **板卡模板** | 初稿绝缘方案提过；Owner 已否决近程 | 评审**不必**再推模板；接线灵活是 intentional Non-goal of insulation |

### 10.3 破坏面重标（建议替换「一刀切 P0」心智）

| 变更类型 | 对 Role-only App C | 对 JSON 作者（保留写引脚） | 冻结前优先级 |
|----------|-------------------|----------------------------|--------------|
| DAL 符号/签名改，Role 包装消化 | 无感 | 无感 | 可后做 |
| 控制语义/单位/解码/钳位/safe_off | **破行为** | 可能破 | **真 P0** |
| 同族必来的 config（drive_mode、max_angle）默认兼容今日 | 无感 | JSON 可加法字段 | **真 P0**（抢破坏窗口） |
| 补 Role 动词（加法） | 受益 | 无感 | **发布门禁 P0**（防逼直调 DAL） |
| `button` BAL 钩子拆头 | 无感（若只用 Role） | 无感 | **P1**（护 DAL/BAL 演进，非用户面） |
| `ssd1306` type 改名 | Role 可挡 App C | **JSON 破** | 若保留芯片名 → **降为文档 P1**；若要语义改名 → 须在上线前拍板（真 P0） |
| 改某 App 的 trig_pin | 无感 | 该 App 故意改 | **非破坏**（灵活接线） |
| gps/eeprom 焊死 stub 签名 | 若误用则破 | 误选 type | experimental 门禁 **P0** |

### 10.4 建议修正后的动作优先级

**仍为真 P0（语义/ABI 窗口）：**

1. `encoder`：钉死 `decode_mode`（或书面+测试锁 x1）+ 可选 invert/CPR  
2. `rc_servo`：`max_angle`（默认 180）进 config；override 版本策略  
3. `dc_motor`：`drive_mode` + `enable_pin`（默认兼容今日 Phase/Enable）  
4. `gps`/`eeprom`：experimental 门禁；实现前重审数值类型/非阻塞  
5. **补齐** `dc_motor` / `encoder` 的 `default_role` + wrappers（发布门禁；与绝缘 Wave 1 合并）  
6. Role Convenience / 单位 / safe_off 行为写入契约测试（至少 encoder + dc_motor + ultrasonic）

**可降为 P1（不挡「Role 用户少破坏」）：**

- `button` 公共头拆 BAL 钩子（层间整洁；用户只用 Role 时不急）  
- `ssd1306`：若 Owner 接受保留芯片名 `type`，则只需文档 + 可选 `panel_variant` 加法；**不必强改 type 名**  
- debounce 双源统一、PAL 头泄漏、apply_override 注释清理  

**明确不做 / 非目标（与绝缘裁决对齐）：**

- 板卡模板锁引脚  
- 从用户 JSON 删除 `type`/引脚  

### 10.5 总评：考虑全面了吗？

| 维度 | 完备度 | 评语 |
|------|--------|------|
| 驱动族边界与已知 ABI 坑 | **高** | 可作 DAL 冻结清单基线 |
| 「语义破穿透 Role」类风险 | **高** | encoder/行程角/拓扑是命门 |
| 用户稳定面（Role）与发布门禁 | **中** | 原文有交叉点，优先级偏低估 |
| 对谁破 / 可接受变更（改脚） | **低→需补** | 本补遗 §10.3 补齐 |
| 契约测试 / 仿真 type 对齐 / BAL 回归 | **低→需补** | 建议并入实施计划 |
| 产品模板/简单模式 | **不适用** | Owner 已否决近程模板 |

**一句话**：原文作为**驱动平面冻结清单**已经够用且应执行其语义类 P0；若目标是「尽量未来不破上层」，还需把 **Role 发布门禁、破坏受众分层、契约测试、experimental、ssd1306 命名降级策略** 一并纳入——见本补遗与绝缘设计 Wave 1。

### 10.6 文档交叉引用

- 用户面机制：[2026-07-28-user-surface-insulation-design.md](../../tech-designs/tools/2026-07-28-user-surface-insulation-design.md)  
- 流转：本评审语义 P0 ∥ 绝缘 Wave 1（role+lint）；二者并行，宣称对外稳定前对齐 §10.4 真 P0

---

*补遗写入后，§七建议动作以 §10.4 为准作优先级解释；§七原文保留为驱动视角快照。*

