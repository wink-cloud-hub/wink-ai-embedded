# DAL 各 `type` 语义化定义与函数定义合理性及防破坏性变更评审报告

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-07-30 |
| **评审范围** | DAL 7 大分类目录与已落地 9 个 `type`（`button`, `led`, `dc_motor`, `rc_servo`, `encoder`, `ultrasonic`, `mono_oled`, `gps`, `eeprom`）的结构体、变量、函数定义合理度，未来演进完备性与防破坏性变更能力。 |
| **关联设计** | [01-dal-device-abstraction §6](../../design/02-wink-micro-os/01-dal-device-abstraction.md)（分类边界与控制语义） |
| **关联 ADR** | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）、[ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md)（非阻塞硬隔离）、[ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 清场铁律）、[ADR-0034](../../decisions/core/0034-dal-progressive-config-disclosure.md)（渐进式配置）、[ADR-0043](../../decisions/tools/0043-yaml-driven-layer-lint.md)（YAML 驱动分层 lint 门禁）、[ADR-0046](../../decisions/core/0046-dal-driver-registry-ssot.md)（`type` SSOT）、[ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)（控制语义命名） |
| **关联评审** | [2026-07-28-dal-control-semantic-completeness-review](./2026-07-28-dal-control-semantic-completeness-review.md)、[2026-07-29-wokwi-elements-dal-type-coverage-review](../frontend/2026-07-29-wokwi-elements-dal-type-coverage-review.md) |
| **结论状态** | **Approved with Action Items（三审定稿）**；可近冻结 **3** 项，冻结前必修 **4** 项，不可冻结/严重缺陷 **2** 项。 |
| **修订轨迹** | **一审**设计意图评估 → **二审**对照头文件降级/升级多项 P0，发现 prune stub 可见性非单调 → **三审**融合资深嵌入式复核：修正过时结论（`rc_servo.max_angle` 已落地）、补齐 STRICT 能力完备、ADR-0024 清场、共享总线、并发契约、Role 哨兵与 Flash wire 版本等缺口。 |

---

## 1. 总体结论与架构裁决

在 C 语言静态分发（Zero Malloc / POD 数据结构 / 零 runtime 虚表）架构下，DAL 的设计原则是：**API 与 API 数据流契约即最高稳定面**。

针对现网已落地的 9 个 DAL `type`，评估结论如下：

1. **成熟度分布**：
   - **【可近冻结】（3 个）**：`ultrasonic`（非阻塞状态机完备）、`led`（二值指示语义纯粹）、`rc_servo`（`max_angle` 已进 `config_t` 且运行时钳位已落地；残留仅为 `get_angle` 与 Flash wire v2，属低风险 API/序列化增量）。
   - **【冻结前必修】（4 个）**：`dc_motor`（缺 `invert`/`get_speed` + PAL 类型泄漏）、`encoder`（解码已锁定，残留 CPR 与 PAL 泄漏）、`button`（BAL IRQ 私货泄漏至公开 ABI）、`mono_oled`（缺 `panel_variant`；图形原语可后置）。
   - **【不可冻结/严重缺陷】（2 个）**：`gps`、`eeprom`（仍为 Stub；`WINK_STRICT_NONBLOCKING` 下阻塞 API 整段剔除导致**能力缺失**；prune stub 与本体守卫**非对称**导致符号可见性非单调）。
2. **防破坏性变更的核心防线（三维抽象心法）**：
   - **`type` = 驱动护城河**：通信协议、物理控制量或底层驱动机制改变，必须新建 `type`，绝对禁止强行打补丁合并。
   - **`drive_mode` = 拓扑避风港**：驱动机制不变、仅硬件接线/H 桥芯片变了，在 `type` 的 `config_t` 内部用 `drive_mode` 枚举消化。
   - **`role` = 应用隔离层**：App 层通过 `role`（如 `open_loop_actuator`、`pulse_counter`）调用，避免直接耦合 `dal_*` 具体 API 签名。

### 1.1 代码复核修订（二审：对照实际头文件逐行验证）

初稿部分结论基于设计意图与 codegen 侧观察。对照实际头文件（`dal_encoder.h`、`dal_dc_motor.h`、`dal_gps.h`、`dal_eeprom.h`、`dal_button.h`）逐行复核后，修订如下：

| 序号 | 初稿结论 | 代码实况 | 修订裁决 |
|---|---|---|---|
| ① | `encoder` 的 `decode_mode` 未强行锁定（🚨 最高级别隐患） | `dal_encoder.h:16-20` 已用枚举锁定 `X1_RISING=0` 为默认，`X2/X4` 标记 `reserved -> init UNSUPPORTED`（fail-closed）；`invert` 亦已在 `config_t:37` | **P0 → P1 降级**：语义破坏已被结构性防死，残留仅 `counts_per_rev` 缺失（纯增强）与计数回绕语义未钉死 |
| ② | `dc_motor` 的 `drive_mode`/`enable_pin` 存在结构体缺口、未进 ABI | `dal_dc_motor.h:53-61` 二者早已是 `config_t` 成员 | **修正**：无结构体缺口；真实残件仅 `invert` 与 `get_speed` 缺失（后者可复用 `dal_dc_motor_t:68` 的 `current_speed`，无需改结构体，低风险纯 API 追加） |
| ③ | GPS/EEPROM 仅"init 被宏剔除无法初始化" | 更深层问题：prune stub 的守卫条件与本体不对称，导致符号可见性**非单调** | **升级为架构级 P0**（详见 §3.8/§3.9、§4 追加项 6a） |
| ④ | `button` 的 IRQ 钩子"暴露在公开头文件" | `dal_button.h:340-380` 显示 5 个 BAL 内部函数在 prune stub 侧被**二次复制固化** | **危害加倍**：拆分内部头文件时须同步迁移 stub 声明（详见 §3.1、§4 追加项 6a） |
| ⑤ | `pal_hal.h` 类型泄漏列为 P2 长远清理 | `dal_encoder.h:7`、`dal_dc_motor.h:7` 已在 `config_t` 直接暴露 `wink_pin_t`/`pal_gpio_mode_t`，属现存 ABI 地雷 | **P2 → P1 升级**（详见 §4 追加项 7） |

**二审新增横断风险**：全 DAL 的 `deinit` 阻塞模型未定型——`dal_gps_deinit`/`dal_eeprom_deinit` 涉及 I2C/UART 资源释放却无阻塞契约、无 `WINK_WARN_UNUSED_RESULT`，返回值可被静默忽略（详见 §4 追加项 8a）。

### 1.2 三审修订（资深嵌入式复核：纠偏过时结论并补齐系统性缺口）

| 序号 | 二审/初稿表述 | 三审裁定 | 影响 |
|---|---|---|---|
| ⑥ | `rc_servo` 行程角硬编码、须把 `max_angle` 加入 `config_t`（P0） | `dal_rc_servo_config_t` **已含** `max_angle`（0→180°）；`dal_rc_servo.c` 已用 `servo_effective_max_angle()` 钳位 | **移出 P0 结构体项**；`rc_servo` 升为「可近冻结」。残留：`get_angle`、Flash override wire v1 **不含** `max_angle`（须 wire v2 + §4 追加项 10） |
| ⑦ | 仅强调 prune stub「可见性单调」 | ADR-0017 本意是剔除**阻塞调用**，不是剔除器件能力。仅强制 stub ⊇ 本体时，最差合法态是 STRICT 下 `init` **两边都消失**——单调但仍不可用 | **拆成 6a 可见性单调 + 6b STRICT 能力完备**（见 §4.1） |
| ⑧ | `deinit` 只要求属性宏 | 真正冻结风险在 [ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md) 硬件清场（GPIO reset / 停 PWM·RMT / 卸 ISR / 不清共享 bus / 幂等 / ≤50ms） | **追加项 8 拆为 8a 契约标注 + 8b 清场可测** |
| ⑨ | GPS 经纬度「`double` 或微度整数」并列 | ESP32 soft-`double` 成本高，且跨 target 对齐更麻烦 | **冻结面优先 `int32_t lat_udeg` / `lon_udeg`**；并预留 fix 质量字段 |
| ⑩ | EEPROM 仅升 `addr` +「提供异步或澄清」 | 缺页写/ACK、共享 I2C、wear Non-goal；非阻塞应选明确形态 | **钉死 ultrasonic 同构状态机（或 worker 队列）+ 共享 bus 契约** |
| ⑪ | 全量 `sensor/*`/`actuator/*` 必须预留 `enable_pin` | 过宽；与 Zero-as-Default / GPIO0 哨兵冲突（`dc_motor` 已踩过） | **改为按电源域/待机脚按需预留，sentinel 只用 `-1`** |
| ⑫ | POD「Padding 零容忍」 | 跨编译器要的是 `sizeof`/`offsetof` 稳定，不是 padding==0 | **改为 ABI layout freeze asserts** |
| ⑬ | Role 哨兵用 `-1.0f` / `0.0f` | 对 AI codegen 极不安全（贴障/倒车/静止无法区分） | **优先 `(status, out-param)`；哨兵须 out-of-band 并进 lint 白名单** |
| ⑭ | `mono_oled` 的 `draw_pixel`/`draw_bitmap` 作 P0 | 拖死冻结面；`text_display` Role 可只冻结 text 面 | **`panel_variant`=P0；绘图原语=P1** |

**三审新增横断缺口（二审未覆盖）**：并发契约（Thread/ISR/Callback-context）、encoder 计数回绕、ultrasonic `use_rmt` 后端语义、共享总线生命周期、Flash `wire_version`、Stub type 仿真保真分级（详见 §4.2）。

---

## 2. DAL `type` 分类与语义化边界解析

DAL 采用**主要意图判定规则 (Primary Intent Rule)** 进行 7 大目录归类：

```
dal/include/
├── input/        # 人机交互输入 (button, analog_knob, keypad, ir_receiver)
├── output/       # 简单执行输出 (led, buzzer, relay, led_bar)
├── actuator/     # 运动执行器 (dc_motor, rc_servo, stepper, industrial_servo, bldc)
├── sensor/       # 物理量采集 (encoder, ultrasonic, analog_sensor, digital_sensor, motion, imu...)
├── comm/         # 通信协议外设 (gps...)
├── display/      # 文本/图形显示 (mono_oled, lcd_char, tft, led_matrix, seg_display)
└── storage/      # 非易失存储与时钟 (eeprom, rtc, sdcard)
```

### 2.1 关键分类防混淆硬伤裁决
- **`actuator` 内部绝不按“电机”统称命名**：
  - `dc_motor`：开环 PWM 占空比 / 有符号速度（`-1.0~1.0f`）。
  - `rc_servo`：航模开环 PWM 绝对角度（默认行程 `0~180°`，可由 `max_angle` 扩展至 270° 等；**连续旋转 360° 舵机禁止归入本 type**）。
  - `stepper`：步数 / 位置（开环脉冲+方向）。
  - `industrial_servo`：闭环位置/速度/力矩（总线型，ADR-0050）。
  - `bldc`：换相 / FOC 本地驱动（ADR-0026/0047）。
  - **防破规则**：严禁把连续旋转 360° 舵机塞入 `rc_servo`；严禁把步进电机伪装成 `dc_motor`。
- **`input` vs `sensor` 异类隔离**：
  - `input/analog_knob`：HMI 调参旋钮，返回 `0.0~1.0f` 归一化比例。
  - `sensor/analog_sensor`：物理量传感器（NTC/光敏），返回原始 ADC/mV。
  - **防破规则**：两者绝对不能统一为 `analog_input`，否则会引发 `input` 与 `sensor` 语义混乱。
- **数字开关阈值传感器与 PIR 隔离**：
  - 双输出传感器 DO 引脚映射为新增的 `sensor/digital_sensor`（通用二值比较器）。
  - `sensor/motion` 严格限定为纯粹的 PIR 人体红外移动侦测语义。
- **App/Role 层 Convenience API 错误处理约定**（三审修订）：
  - **首选**：Role Convenience 返回 `wink_status_t`，结果经 out-param 写出（与 DAL 同源，AI 不易误读）。
  - **若必须提供标量 Convenience**（如 `float get_distance()`）：错误时须返回**明确 out-of-band** 哨兵，并在 Role 规范 + codegen/lint 白名单中钉死。
    - 距离：**禁止**用 `-1.0f`（易被当成 1cm 级障碍）；推荐 `NAN`，或文档化的极端哨兵（如 `-1e9f`）且生成代码不得当物理量参与算术。
    - 速度：**禁止**用 `0.0f` 表示错误（与静止/倒车无法区分）；应返回 status，或使用 `NAN`。
    - 按键：布尔 Convenience 在错误时的语义须显式规定（推荐不提供无 status 的布尔 Convenience）。
  - **防破规则**：哨兵值与允许用法必须进 lint；禁止 AI 把哨兵当物理量静默使用。

---

## 3. 现网 9 个 `type` 的结构体、变量与方法定义深度审计

### 3.1 `button` — 按键输入 (`input/button`)
- **对应头文件**：[`wink-micro-os/dal/include/input/dal_button.h`](../../../../wink-micro-os/dal/include/input/dal_button.h)
- **现有定义**：
  - `config_t`：`owner`, `pin`, `active_low`, `pull` (AUTO/UP/DOWN/NONE)。
  - `dal_button_t` 结构体：包含了 `stable_pressed`、`last_reported`、`event_cb`、`long_press_ms`、`edge_count`，以及 `event_backend`、`gpio_isr_registered`、`irq_pending`。
- **合理度与完备性分析**：
  - ❌ **严重分层污染**：`dal_button_set_event_backend`、`dal_button_set_irq_hook`、`dal_button_consume_irq_pending` 是 **BAL 层 IRQ 守护进程专用的协作 API**。直接暴露在 DAL 公开头文件中，导致 App/AI 会误以为这是公共稳定 API。如果未来重构 BAL 中断框架，会导致 API 破坏。
  - 🚨 **二审升级：污染被 prune stub 二次固化**：`dal_button.h:340-380` 复核发现，`set_event_backend`、`set_irq_hook`、`consume_irq_pending`、`enable_gpio_isr`、`disable_gpio_isr` 这 5 个 BAL 内部函数在 driver-off 的 prune stub 段（`#if !WINK_USE_BUTTON`）中被**逐一复制声明**。这意味着它们不是"偶然出现在公开头文件"，而是被构建系统**双重固定为一级公开 ABI**——无论驱动开关状态如何都对外可见，破坏面因此加倍。
  - ⚠️ **三审补充：并发/回调上下文未钉死**：`on_event` 与 IRQ hook 的 `Callback-context`（ISR vs Task）必须在冻结面文档与 API Contract 中明示；ISR 路径禁止阻塞/堆分配（对齐 grilling Q4）。
- **避免未来破坏性变更的修正建议**：
  - **[P0 冻结项]** 拆分内部头文件（如 `dal_button_bal.h`）；对外冻结面仅保留 `init`, `poll`, `is_pressed`, `was_pressed`, `on_event`, `set_long_press_ms`, `set_debounce_ms`, `deinit`（及已公开且确属 App 面的 edge-count API，若保留须同步钉死 ISR 契约）。
  - **[P0 冻结项·二审补充]** 拆分时必须**同步迁移 prune stub 侧的 5 个 BAL 函数声明**，否则会残留"本体已移走、stub 仍公开"的非对称可见性缺陷（与 §4 追加项 6a 同源）。
  - **[P0·三审]** 在公开 API Contract 中钉死 `Thread-safe` / `ISR-safe` / `Callback-context`；`deinit` 须满足 ADR-0024（卸 ISR、GPIO reset、幂等）。

---

### 3.2 `led` — 二值指示灯 (`output/led`)
- **对应头文件**：[`wink-micro-os/dal/include/output/dal_led.h`](../../../../wink-micro-os/dal/include/output/dal_led.h)
- **现有定义**：
  - `config_t`：`owner`, `pin`, `active_high`。
  - APIs：`init`, `on`, `off`, `set`, `toggle`, `deinit`。
- **合理度与完备性分析**：
  - ✅ **极度纯粹**：对于二值指示灯非常完备。
- **避免未来破坏性变更的修正建议**：
  - **[防破红线]** 绝对不能为了支持 PWM 调光或 RGB 灯带而在 `dal_led_set` 里扩展参数（例如把 `bool on` 改成 `float brightness`）。PWM 调光灯应使用 `pwm_led`；RGB WS2812 阵列应使用 `display/led_matrix`。

---

### 3.3 `dc_motor` — 有刷直流电机 (`actuator/dc_motor`)
- **对应头文件**：[`wink-micro-os/dal/include/actuator/dal_dc_motor.h`](../../../../wink-micro-os/dal/include/actuator/dal_dc_motor.h)
- **现有定义**：
  - `config_t`：`owner`, `pwm_channel`, `dir_pin_a`, `dir_pin_b`, `pwm_freq_hz`, `drive_mode`, `enable_pin`。
  - APIs：`init`, `set_speed`, `brake`, `coast`, `safe_off`, `deinit`。
- **合理度与完备性分析**：
  - ✅ **二审修正：结构体无缺口**：复核 `dal_dc_motor.h:53-61`，`drive_mode`（IN_IN=0 默认，PHASE_ENABLE/PWM_ON_IN 为 `reserved -> init UNSUPPORTED` fail-closed）与 `enable_pin`（含完整的 `-1`/`0`/`>0` 归一化契约）**均已在 `dal_dc_motor_config_t` 中定义并进 ABI**。初稿"未进 Flash 覆写 ABI"的判断不成立。
  - ⚠️ **缺少读回 API 与反相标志**：缺少 `dal_dc_motor_get_speed`（获取当前设定的速度），且缺乏 `bool invert`（解决电机左右反接问题）。注意 `dal_dc_motor_t:68` 已缓存 `current_speed`，因此 `get_speed` 是**无需改结构体的低风险纯 API 追加**。
  - ⚠️ **PAL 类型泄漏**：`dal_dc_motor.h:7` 直接 `#include "pal_hal.h"`，`config_t` 的 `dir_pin_a/b`、`enable_pin` 暴露为 `wink_pin_t`。PAL 引脚类型表示一旦变更将连锁击穿本 `type` 的 ABI（见 §4 追加项 7）。
  - ⚠️ **三审补充：`enable_pin` 哨兵与 Zero-as-Default 张力**：头文件已规定 `0` 与 `-1` 均可表示 unused，导致 GPIO0 无法作 enable。冻结前应收敛为 **unused 仅 `-1`**（新字段/新 type），并在文档标明既有归一化行为的兼容窗口。
  - ⚠️ **三审补充：并发**：`set_speed`/`brake`/`coast`/`safe_off` 默认 `Thread-safe: No`；多任务访问须外部互斥，须在 Contract 钉死。
- **避免未来破坏性变更的修正建议**：
  - **[P0 冻结项]** 必须在发布前将 `bool invert` (默认 `false`) 加入 `dal_dc_motor_config_t`；补齐 `dal_dc_motor_get_speed` API（首参必须为 `const dal_dc_motor_t *dev`）。
  - **[P0 Flash Override 契约]** 扩展 `config_t` 时遵循 §4 追加项 10（短 payload 填默认 + 显式 wire 版本策略）。
  - **[P1]** 消除公开头对 `pal_hal.h` 的依赖（DAL 稳定整型别名，`.c` 内转换）。

---

### 3.4 `rc_servo` — 航模舵机 (`actuator/rc_servo`)
- **对应头文件**：[`wink-micro-os/dal/include/actuator/dal_rc_servo.h`](../../../../wink-micro-os/dal/include/actuator/dal_rc_servo.h)
- **现有定义**：
  - `config_t`：`owner`, `pwm_channel`, `resolution_bits`, `clock_requirement`, `min_pulse_ms`, `max_pulse_ms`, `max_angle`。
  - APIs：`init`, `set_angle`, `safe_off`, `apply_override`, `deinit`。
- **合理度与完备性分析**：
  - ✅ **三审修正：行程角已配置化**：`max_angle` 已在 `config_t`；实现侧 `servo_effective_max_angle()` 对 `<=0` 回落 `180.0f`，`set_angle` 按有效上限钳位。初稿/二审「硬编码 180、必须加字段」的结构体缺口**不成立**。
  - ⚠️ **缺少读回 API**：缺乏 `dal_rc_servo_get_angle`（可复用 `dal_rc_servo_t.current_angle`，低风险纯 API 追加）。
  - ⚠️ **Flash wire 滞后**：`apply_override` wire v1（≥9B）**不含** `max_angle`（头文件已标明未来 wire v2）。仅靠「短 payload 填默认」不够——字段重排时仍会静默错读，须配合 `wire_version`（§4 追加项 10）。
- **避免未来破坏性变更的修正建议**：
  - **[已达成]** `config.max_angle` + 运行时钳位。
  - **[P1 增强项]** 补齐 `dal_rc_servo_get_angle`（首参 `const dal_rc_servo_t *dev`）。
  - **[P1 / 序列化]** Flash override wire v2 纳入 `max_angle`，并遵守追加项 10；在 v2 落地前保持 v1 行为与注释一致。

---

### 3.5 `encoder` — 编码器 (`sensor/encoder`)
- **对应头文件**：[`wink-micro-os/dal/include/sensor/dal_encoder.h`](../../../../wink-micro-os/dal/include/sensor/dal_encoder.h)
- **现有定义**：
  - `config_t`：`owner`, `pin_a`, `pin_b`, `pull`, `decode_mode`, `invert`。
  - APIs：`init`, `get_count`, `reset`, `deinit`。
- **合理度与完备性分析**：
  - ✅ **二审修正：语义破坏已被结构性防死**：复核 `dal_encoder.h:16-38`，`decode_mode` 已是枚举并锁定 `DAL_ENCODER_DECODE_X1_RISING = 0` 为默认，`X2/X4` 明确标记 `reserved -> init UNSUPPORTED`（fail-closed）；`invert` 亦已在 `config_t:37`。初稿"最高级别语义破坏隐患（未强行锁定）"的前提已不成立，**风险从 P0 降为 P1**。真正残留仅 `counts_per_rev` 缺失（纯增强）。
  - ⚠️ **PAL 类型泄漏**：`dal_encoder.h:7` 直接 `#include "pal_hal.h"`，`config_t` 暴露 `wink_pin_t pin_a/pin_b` 与 `pal_gpio_mode_t pull`。与 `dc_motor` 同属现存 ABI 地雷（见 §4 追加项 7）。
  - ⚠️ **缺少物理参数配置**：缺少 `uint16_t counts_per_rev` (CPR，默认为 0，纯脉冲)。
  - ⚠️ **三审补充：计数回绕语义未钉死**：`get_count` 的有符号/无符号与 32-bit wrap 对差分里程计至关重要；冻结前须在 Contract 写明「自然回绕、由上层做差分」，禁止日后悄然改类型宽度。
- **避免未来破坏性变更的修正建议**：
  - **[已达成]** `decode_mode` 枚举锁定与默认值、`invert` 均已在头文件落地，无需再动。
  - **[P1 增强项]** 在配置中加入可选的 `counts_per_rev`（默认 0）。DAL 保持返回原始 `count`，物理单位换算保留在 BAL/Role 层。
  - **[P1 契约]** 钉死 count 类型与 wrap 语义；消除 PAL 类型泄漏。

---

### 3.6 `ultrasonic` — 超声波测距 (`sensor/ultrasonic`)
- **对应头文件**：[`wink-micro-os/dal/include/sensor/dal_ultrasonic.h`](../../../../wink-micro-os/dal/include/sensor/dal_ultrasonic.h)
- **现有定义**：
  - `config_t`：`owner`, `trig_pin`, `echo_pin`, `use_rmt`。
  - APIs：`init`, `request_measurement`, `get_cached_distance`, `read` (deprecated / `WINK_BLOCKING`)，`deinit`。
- **合理度与完备性分析**：
  - ✅ **架构标杆**：采用了 `IDLE -> MEASURING -> READY -> ERROR` 异步状态机，且符合 `WINK_STRICT_NONBLOCKING` 规范；阻塞 `read` 的硬隔离模式应作为 gps/eeprom 改造范本。
  - ⚠️ **微小缺口**：缺少 `max_timeout_ms` / `max_distance_cm` 配置（目前内部写死 30ms / ~500cm）。
  - ⚠️ **三审补充：`use_rmt` 语义**：属**后端/能力选择**，非电气拓扑。冻结时应定为：默认路径行为稳定；RMT 不可用时 **fail-closed** 到已文档化的 GPIO 路径或返回 `UNSUPPORTED`，禁止默默切换后端导致时序/精度变化。
- **避免未来破坏性变更的修正建议**：
  - **[P1 增强项]** `config_t` 预留 `max_distance_cm`（0 时自动设为默认 500cm），保持向前兼容。
  - **[P1 契约]** 文档化 `use_rmt` 的能力宏/降级策略。

---

### 3.7 `mono_oled` — 单色 OLED (`display/mono_oled`)
- **对应头文件**：[`wink-micro-os/dal/include/display/dal_mono_oled.h`](../../../../wink-micro-os/dal/include/display/dal_mono_oled.h)
- **现有定义**：
  - `config_t`：`i2c_port`, `i2c_addr`, `width`, `height`, `owner`。
  - APIs：`init`, `clear`, `draw_text`, `flush`, `deinit`。
- **合理度与完备性分析**：
  - ⚠️ **名称与类型问题**：现网头文件与 codegen 中 `type` 绑定为 `ssd1306`（具体芯片名）还是 `mono_oled`（控制语义族）存在双轨问题。从防破坏性变更角度，DAL 头文件推荐定义为 `dal_mono_oled.h`，并通过 `panel_variant` 枚举消化 SSD1306 / SH1106 屏驱动微小差异。
  - ⚠️ **图形原语缺失（非冻结阻塞）**：目前仅有 `draw_text`。`draw_pixel`/`draw_bitmap`/`set_contrast`/`set_rotation` 属能力增量；`text_display` Role 可仅以 text+clear+flush 作为冻结面。
  - ⚠️ **三审补充：共享 I2C**：与 `eeprom` 等同 bus 共存时，deinit **仅卸 client**，禁止销毁共享 bus（ADR-0024）。
- **避免未来破坏性建议**：
  - **[P0 冻结项]** `config_t` 增加 `uint8_t panel_variant`（0=SSD1306，Zero-as-Default）；钉死 `mono_oled` 为控制语义 `type`，芯片名仅作 JSON/板级别名。
  - **[P1 增强项]** 扩展 `draw_pixel`、`draw_bitmap`（及可选 contrast/rotation），避免未来画图需求迫使破坏 text 冻结面。
  - **[P0 契约]** deinit 遵守共享 bus 所有权；可测 `init→deinit→init` 幂等。

---

### 3.8 `gps` — GPS 定位 (`comm/gps`)
- **对应头文件**：[`wink-micro-os/dal/include/comm/dal_gps.h`](../../../../wink-micro-os/dal/include/comm/dal_gps.h)
- **现有定义**：
  - `config_t`：`owner`, `uart_port`, `baudrate`, `rx_buffer_size`。
  - APIs：`init` (BLOCKING), `poll`, `get_position`, `deinit`。
- **合理度与完备性分析**：
  - 🚨 **致命：STRICT 下能力缺失**：现网 `dal_gps_init` 被标记为 `WINK_BLOCKING` 且包在 `#ifndef WINK_STRICT_NONBLOCKING` 内。严格非阻塞模式下 `init` 符号消失，App 无法初始化。这违反 ADR-0017「剔除阻塞调用、保留可用非阻塞面」的意图。
  - 🚨 **致命：prune stub 守卫非对称（可见性非单调）**：`init` 本体受 `#ifndef WINK_STRICT_NONBLOCKING`（53-76 行）守卫，但 driver-off stub（121-133 行）**仅由 `#if !WINK_USE_GPS` 守卫**。宏组合下可见性自相矛盾：
    - `WINK_USE_GPS=OFF` + `STRICT_NONBLOCKING=1`：stub 侧**恢复** `dal_gps_init`（带 `WINK_UNAVAILABLE_MSG`）；
    - `WINK_USE_GPS=ON`  + `STRICT_NONBLOCKING=1`：init 声明**完全消失**。
    
    同一符号时隐时现，是链接期/代码生成期定时炸弹。**须与「能力完备」一并修复**：禁止「两边都藏起来」的假合规。
  - ❌ **数值类型精度隐患**：`latitude`/`longitude` 为 `float`（约米级误差）。
  - ⚠️ **三审补充：定位质量字段缺失**：仅有 `fix_valid` 不足以支撑 Role；冻结前应预留 `fix_quality` 与 `hdop`（或 `pdop`）、以及 UTC/`time_valid`，避免日后扩字段破 ABI。
  - ⚠️ **`deinit` 契约缺失**：无阻塞标注、无 `WINK_WARN_UNUSED_RESULT`；且须满足 UART 资源释放 + ADR-0024 清场（见追加项 8a/8b）。
- **避免未来破坏性变更的修正建议**：
  - **[P0 修正项·能力完备]** 提供**始终可见**的非阻塞 `dal_gps_init`：仅配置 UART / 初始化解析器状态，**不等待首帧 NMEA**；数据在 `dal_gps_poll` 中推进。若保留阻塞 init 变体，挂 `WINK_BLOCKING` 且仅阻塞变体受 STRICT 剔除。
  - **[P0 修正项·可见性]** 修复 prune stub 守卫非对称：stub 守卫 ⊇ 本体守卫；`WINK_STRICT_NONBLOCKING` **不改变**非阻塞生命周期符号的可见性集合（lint：追加项 6a）。同时强制 6b：`WINK_USE_GPS=ON` 时 STRICT 下仍具备可用生命周期面。
  - **[P0 修正项·坐标 ABI]** 经纬度改为 `int32_t lat_udeg` / `lon_udeg`（微度，如 `39908712`）。**不推荐**以 `double` 作为默认冻结面（ESP32 soft-double 成本与跨 target 对齐风险）。海拔等可另定单位（如 `alt_mm`）并文档化。
  - **[P1]** 预留 `fix_quality` / `hdop` / 时间有效位；仿真 Stub 返回 `UNSUPPORTED` 的 App 可见契约（追加项 14）。

---

### 3.9 `eeprom` — EEPROM 存储 (`storage/eeprom`)
- **对应头文件**：[`wink-micro-os/dal/include/storage/dal_eeprom.h`](../../../../wink-micro-os/dal/include/storage/dal_eeprom.h)
- **现有定义**：
  - `config_t`：`owner`, `i2c_port`, `i2c_addr`, `capacity_bytes`, `page_size`, `write_time_ms`。
  - APIs：`init` (BLOCKING), `read` (BLOCKING), `write` (BLOCKING), `deinit`。
- **合理度与完备性分析**：
  - 🚨 **致命：STRICT 下能力缺失**：`init`/`read`/`write` 全部在 `#ifndef WINK_STRICT_NONBLOCKING` 下；STRICT=1 时无任何读写 API。
  - 🚨 **致命：prune stub 守卫非对称**：与 GPS 同构——本体受 STRICT 守卫，stub 仅受 `WINK_USE_EEPROM` 守卫（详见 §3.8 与追加项 6a/6b）。
  - ⚠️ **地址类型限制**：`addr`/`len` 为 `uint16_t`，`capacity_bytes` 为 `uint32_t`；>64KB 器件无法完整寻址。
  - ⚠️ **`deinit` 契约缺失**：无阻塞/unused-result 标注；且涉及 I2C client 释放（追加项 8a/8b、11）。
  - ⚠️ **三审补充：页写与 busy 语义未钉死**：跨页拆分、`write_time_ms` / ACK 轮询超时与错误码须在 Contract 中写明。
  - ⚠️ **三审补充：wear-leveling = Non-goal**：DAL 不做磨损均衡或 KV 语义；禁止 AI 把文件系统语义塞进本 type。
- **避免未来破坏性变更的修正建议**：
  - **[P0 修正项]** 寻址与长度升格为 `uint32_t addr` / `uint32_t len`（或至少 `addr` 为 `uint32_t`，并校验 `addr+len` 不溢出且不越界）。
  - **[P0 修正项·非阻塞形态]** 采用与 ultrasonic **同构**的状态机（推荐）：`request_read`/`request_write` + `poll`/`get_status` + 完成回调或 READY 态；**或** DAL 内 worker 队列（grilling Q5）。阻塞 `read`/`write` 可保留供 host 单测，挂 `WINK_BLOCKING`，STRICT 下剔除但**不得**剔除非阻塞孪生。
  - **[P0 修正项·可见性]** 与 GPS 统一修复 stub/本体守卫，并满足 6b 能力完备。
  - **[P0 契约]** 共享 I2C：deinit 只卸本设备 client，不 `del` 共享 bus；与 `mono_oled` 同 bus 场景纳入单测（追加项 11）。

---

## 4. 防范破坏性变更的架构总结与 Checklist

为了避免未来由于需求变更导致 DAL API / ABI / JSON 发生破坏性变更，需在 API 冻结前执行以下 Checklist：

| 校验维度 | 防破坏性规程 | 实施规则 |
|---|---|---|
| **1. 低功耗/电源域预留（三审收窄）** | 有待机脚才预留 `enable_pin` | **不**要求所有 `sensor/*`/`actuator/*` 机械塞入。仅当器件存在芯片级 enable/STBY/nSLEEP 或板级电源开关时，在冻结前纳入 `config_t`。unused sentinel **推荐仅 `-1`**；禁止把 `0` 当 unused（GPIO0 冲突，见 `dc_motor` 既有张力）。 |
| **2. Zero-as-Default** | 零值即默认 | 所有 `config_t` 的数值/枚举字段，`0` 必须映射为默认安全行为（如 `baudrate=0` → `9600`，`max_angle=0` → `180.0f`，`panel_variant=0` → SSD1306）。**引脚类字段不适用「0=默认」**，应使用显式 sentinel。 |
| **3. 非阻塞硬隔离 + 能力完备** | 严禁在默认 `init` 中死等；STRICT 下仍可用 | 默认生命周期 API 毫秒级完成；禁止在 `init` 中等待传感器首帧。阻塞变体挂 `WINK_BLOCKING` 并可被 STRICT 剔除；**必须**另有非阻塞孪生（追加项 6b）。 |
| **4. 拓扑与物理量隔离** | 物理量归 `type`，接线归 `drive_mode` | 控制物理量（角度 vs 占空比 vs 脉冲）不同则新建 `type`；接线变动用 `drive_mode` 消化。后端能力开关（如 `use_rmt`）不得伪装成拓扑枚举。 |
| **5. 上层 Role 绝缘** | App C 绝缘层保障 | 每个 DAL `type` 可映射到 Role 动词（如 `binary_sensor`, `open_loop_actuator`, `pulse_counter`, `text_display`）。Convenience 错误处理遵守 §2.1（优先 status+out-param）。 |

### 4.1 机械化不变量（建议固化为 `wink lint` 规则，ADR-0043）

以下不变量**必须由 lint/单测机械强制**。它们不针对单个 `type`，而是防止 `type` 数量增长后同类缺陷反复再生：

| 追加项 | 防破坏性规程 | 实施规则（lint / 单测化） | 现状违规 |
|---|---|---|---|
| **6a. 声明可见性单调性** | prune stub 守卫条件 ⊇ 本体声明守卫 | 对 `WINK_USE_<X>` × `WINK_STRICT_NONBLOCKING` 全组合求每个公开符号可见性集合；「时隐时现」即报错。 | `gps`、`eeprom`；`button` 拆分内部头时若不同步迁移 stub 会引入 |
| **6b. STRICT 能力完备** | `WINK_USE_X=ON` 时，STRICT=1 下仍具备非阻塞生命周期面 | 至少存在可用的非阻塞 `init` + 运行期推进 API（`poll`/`request_*`）+ `get_*`/`read` 完成态 + `deinit`。**禁止**靠「两边都剔除」满足 6a。 | `gps`、`eeprom`（STRICT 下整段能力消失） |
| **7. PAL 类型零暴露** | 公开 `config_t`/句柄不得直接出现 `wink_pin_t`/`pal_*_t` | lint：公开头是否 `#include "pal_hal.h"` 或引用 PAL 类型；改用 DAL 稳定整型别名，`.c` 内转换。可分批一次性改完所有 pin 字段。 | `encoder`、`dc_motor` |
| **8a. set/get 对称 + deinit 属性** | 有 `set_*` 控制量须有 `get_*`；`deinit` 有契约标注 | lint：①缺 `get_speed`/`get_angle`；②`get_*` 首参为 `const`；③`deinit` 缺 blocking 属性或 `WARN_UNUSED_RESULT`。 | `dc_motor`、`rc_servo`；`gps`/`eeprom` deinit |
| **8b. deinit 清场可测（ADR-0024）** | 非伪 deinit | host：`init→use→deinit→init` 幂等且可重申领资源；真机抽检 GPIO reset / 停 PWM·RMT / 卸 ISR；共享 bus 不在 client deinit 中销毁。 | 历史「只 `pal_resource_release`」伪 deinit 风险；stub type 落地时必须一次做对 |
| **9. ABI layout freeze** | 跨 target 布局稳定；禁用 `#pragma pack` | 成员按对齐需求降序排列；以 `_Static_assert`/`offsetof`/`sizeof` 在 Xtensa / Wasm / Host 矩阵比对。**不要求 padding==0**（与对齐冲突时允许必要 padding，但布局必须冻结且一致）。 | 需静态测试覆盖全 DAL `struct` |
| **10. Flash Override 版本化容错** | 短 payload 填默认 + 显式版本 | ①`len < new_len` 时未覆盖字段保留 compile-time 默认，不返回 `INVALID_ARG`；②引入 `wire_version`（或等价字段 bitmap），禁止仅靠前缀字节在字段重排时静默错读。 | `rc_servo` wire v1 不含 `max_angle`；后续 `invert` 等同理 |
| **11. 共享总线所有权** | client deinit ≠ bus deinit | OLED/EEPROM 等同 I2C：deinit 只卸自身；bus 由 device_tree/bus-owner 逆序释放。lint/单测覆盖同 port 多 client。 | `mono_oled`、`eeprom`（及未来同 bus 器件） |
| **12. 并发与回调上下文** | Contract 钉死 Thread/ISR/Callback | 公开 API 必须声明 `Thread-safe` / `ISR-safe` / `Callback-context`；ISR 回调禁阻塞与堆分配。 | `button` IRQ 面；encoder 计数；actuator set_* |
| **13. Role 哨兵 / Convenience 安全** | 禁止危险默认哨兵 | lint：距离/速度 Convenience 不得用 `-1.0f`/`0.0f` 表示错误；优先 status+out-param；若有哨兵须在白名单。 | §2.1 旧约定需迁移 |
| **14. Stub / 仿真保真契约** | `UNSUPPORTED` 对 App 可见且稳定 | Stub type 在 Wasm 与真机均返回同一类错误码；codegen 不得假设 `initialized=true`；文档标明保真等级。 | `gps`、`eeprom` |

> **设计原则**：单发的「改某个字段」修复只能止血一次；只有把 §4 的 1~5 项原则 + 追加项 6a~14 写成 `wink lint` / host 单测可执行规则，才能在 `type` 扩展到 7 分类 × 多实例后持续拦住破坏性变更。此为本次评审最重要的长效结论。

### 4.2 三审补充：冻结前仍须显式裁决的开放点

| 开放点 | 建议裁决 |
|---|---|
| GPS 时间与精度字段 | 冻结前至少占位 `fix_quality` + `hdop`（或 `pdop`）+ `time_valid`/UTC，即便首版解析器填默认 |
| EEPROM wear / KV | 明文 Non-goal；上层若需要用独立 storage Role/服务 |
| `enable_pin`  polarity | 若预留 enable，默认高有效须在 config 或文档钉死；需要低有效时用枚举而非静默改语义 |
| 连续旋转舵机 | 独立 `type`（或明确 Non-goal），永不并入 `rc_servo` |

---

## 5. 优先落地 Roadmap

> 三审已按代码实况与架构完备性调整：`rc_servo.max_angle` 降级移出 P0；STRICT 能力完备与 ADR-0024 清场升入硬门禁；OLED 绘图原语降为 P1。

1. **P0（发布硬门禁）**：
   - 修复 `gps`、`eeprom` 的 **6a 可见性单调** + **6b STRICT 能力完备**（非阻塞生命周期孪生；**禁止**只把 stub 一并藏进 STRICT）。同步写入 `wink lint`。
   - GPS：非阻塞 `init`（不等首帧）；`lat`/`lon` 改为微度整数；补 `deinit` 8a/8b。
   - EEPROM：`addr`（及必要的 `len`）升 `uint32_t`；落地非阻塞状态机（或 worker）；共享 I2C deinit 契约；补 8a/8b。
   - `dal_dc_motor`：增加 `invert` 与 `get_speed`。
   - `dal_button`：BAL IRQ API 迁出公开头，**同步迁移 prune stub**；钉死 Callback-context；deinit 清场可测。
   - `dal_mono_oled`：增加 `panel_variant`（0=SSD1306）；钉死语义 `type` 名与共享 bus deinit。
   - ~~`dal_encoder` 钉死 `decode_mode`~~ **[二审：已实现]**；~~`dal_rc_servo` 增加 `max_angle`~~ **[三审：已实现]**。
2. **P1（能力增强）**：
   - 消除 `encoder`/`dc_motor` 对 `pal_hal.h` 的公开泄漏（追加项 7）+ lint。
   - `dal_rc_servo_get_angle`；Flash override wire v2（含 `max_angle`）+ 追加项 10。
   - `dal_encoder`：`counts_per_rev`（默认 0）+ count wrap 契约。
   - `dal_ultrasonic`：`max_distance_cm`；`use_rmt` 降级策略文档化。
   - `dal_mono_oled`：`draw_pixel` / `draw_bitmap`（及可选 contrast/rotation）。
   - 补齐 `dc_motor`/`encoder` 的 `default_role` 包装；Role Convenience 按 §2.1 / 追加项 13 迁移。
   - 全 type：`deinit` 8a 属性 + 执行器/button 的 8b 清场单测；并发契约（追加项 12）。
3. **P2（长远清理）**：
   - 全量 DAL 头文件统一渐进式配置披露；收敛剩余 PAL 引用。
   - GPS 质量/时间字段填满；Stub 保真分级进文档与测试（追加项 14）。
   - 将 §4 原则 1~5 与追加项 **6a~14** 全部纳入 CI 门禁，作为 API 冻结的自动化前置条件。

---

## 6. 三审意见摘要（已回写正文）

本节省略独立附录体例；以下要点均已融入 §1.2、§2.1、§3、§4、§5：

1. STRICT 模式要**保能力**，不只保符号单调（6a+6b）。
2. `deinit` 要对齐 ADR-0024 **硬件清场**，不只对齐属性宏（8a+8b）。
3. 用代码实况降级过时 P0（`rc_servo.max_angle`；`encoder.decode_mode` 维持二审结论）。
4. GPS 坐标优先整数微度；EEPROM 明确异步形态与共享 bus；Role 哨兵去危险默认；Checklist 收窄 `enable_pin`、改写 padding 目标、补并发/wire 版本/仿真契约。

