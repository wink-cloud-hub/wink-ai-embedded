# DAL `output/buzzer` 落地计划评审（`03-p0-output-buzzer-plan.md`）

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-08-07 |
| **评审对象** | [`docs/implementation-plans/frontend/03-p0-output-buzzer-plan.md`](../../implementation-plans/frontend/03-p0-output-buzzer-plan.md)（🆕 Planned，编码前评审） |
| **评审性质** | **编码前计划评审（Plan Review）**——目标驱动尚未存在，重点核验"计划与真实底座/规范是否对碰"，而非既有代码合规 |
| **评审范围** | 计划全文 9 节：需求与硬件映射、variant 拓扑、`dal_buzzer.h` 结构体/API、`dal_buzzer.c` 逻辑、5 条 Safety Guard、`buzzer.yaml`、Jinja 模板、Wasm/Wokwi 映射、Checklist |
| **核验依据** | PAL 真实实现（`pal_hal.h` / `pal_pwm_router.[ch]` / `targets/esp32/pal_hal_pwm_esp32.c` / `targets/wasm/pal_hal_wasm.c`）；同类 DAL 驱动（`dal_led.*` / `dal_relay.*` / `dal_rc_servo.*` / `dal_dc_motor.*`）；DAL 规范 v3.4.3、Role 架构规范、ADR-0004/0034/0048/0051/0056；Wokwi 官方 `buzzer` 元件文档 |
| **对照样板** | PWM 路径以 `dal_rc_servo` 为骨架；GPIO 路径以 `dal_led` 为骨架；variant 枚举与 codegen 以 `dal_relay` 为骨架 |
| **结论状态** | **❌ Not Approved（需修订后再编码）**：骨架与规范意识正确（variant 一等公民、双 role 覆盖、Guard 表、codegen SSOT、POD 静态分发均符合项目范式），但发现 **11 项 P0 阻塞问题**，集中在"与真实底座未对碰"、"role/codegen 契约违反"、"ABI 与硬件事实错误"三类；编码前必须修正 |

---

## 1. 评审结论

### 1.1 总体评价

计划的**架构方向是对的**：
- 用 `dal_buzzer_variant_t` 枚举消化"无源 PWM / 有源 GPIO"拓扑差异，对外 API 冻结，符合 [`dal-best-practices.md` §3 拓扑枚举原则](../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) 与 ADR-0004 静态分发；
- 同时覆盖 `tone_generator`（默认）与 `binary_indicator`（备选）两个 role，与 Role 全景表 #7 一致；
- 5 条 Safety Guard（低功耗、资源独占、零值默认、硬件托底、非阻塞）方向正确，尤其 Guard E 禁止 delay 控制音长符合 `WINK_STRICT_NONBLOCKING`。

但计划**没有与真实底座代码完成对碰**，多处把"想象中的 API / 硬件行为"写进了方案。这些问题在文档评审阶段成本接近零，留到编码期会导致 config 结构体返工、PAL 接口增改、codegen 模板重写。11 项 P0 必须在编码前修订。

### 1.2 问题统计

| 严重度 | 数量 | 分布 |
|---|---|---|
| 🔴 P0 阻塞 | 11 | 底座对碰 5、role/codegen 契约 3、事实/ABI 3 |
| 🟠 P1 强烈建议 | 12 | 语义一致性、可维护性 |
| 🟡 P2 建议 | 5 | 健壮性 / 未来扩展 |
| **合计** | **28** | |

### 1.3 三组核心矛盾

1. **底座对碰缺失（P0-1~4、P0-9）**：引用了不存在的 `pal_pwm_router_acquire_channel()`；把 PWM 通道号与 GPIO 引脚号塞进同一个 `int16_t pin`；承诺了 PAL 不提供的"运行时改频"；`20kHz` 上限与 ESP32 LEDC 默认 13-bit 分辨率数学冲突；wasm 发声桥接（`js_pal_pwm_set_freq` / Web Audio）在真实代码里完全不存在。
2. **role / codegen 契约违反（P0-5~7）**：`is_actuator: false` 与执行器属性/`safe_off_fn` 矛盾；`binary_indicator` 用了不存在的动词 `on/off/set/is_on`（标准动词是 `activate/deactivate/toggle`）；`safe_off` 误声明为 `void` 且 stub 段漏写。
3. **事实 / ABI 错误（P0-8、P0-10、P0-11）**：ILP32 的 `_Static_assert` 布局数字手算错误；Wokwi `buzzer` 元件并无"有源/无源模式"切换；必填主引脚用 `int16_t` 违反 DAL-S-006。

---

## 2. P0 阻塞问题（编码前必须修正）

### P0-1　引用了不存在的 PAL 原语 `pal_pwm_router_acquire_channel()`

**位置**　§4.1："调用 `pal_pwm_router_acquire_channel()` 或初始化 GPIO 作为 PWM 驱动输出"。

**问题**　该函数不存在。`pal_pwm_router` 是 PAL 内部的 timer 池分配器，DAL 不应直接调用；DAL 的 PWM 驱动入口是 `pal_pwm_init_ex`。

**证据**
- `pal_pwm_router.h` 真实入口为 [`pal_pwm_router_acquire(channel, profile, out_timer_num)`](../../../wink-micro-os/pal/include/hal/pal_pwm_router.h#L59-L61)，无 `_channel` 后缀版本。
- DAL 一律经 `pal_pwm_init_ex` 驱动 PWM：[`dal_rc_servo.c:70`](../../../wink-micro-os/dal/src/actuator/dal_rc_servo.c#L70)、[`dal_dc_motor.c:145`](../../../wink-micro-os/dal/src/actuator/dal_dc_motor.c#L145)。

**解决方案**　`PASSIVE_PWM` 分支改走 `pal_pwm_init_ex`：

```c
pal_pwm_config_t pwm_cfg = {
    .freq_hz           = dev->config.default_freq_hz,
    .resolution_bits   = 0u,                         /* 0=AUTO → ESP32 13-bit */
    .clock_requirement = PAL_PWM_CLOCK_AUTO,
};
rs = pal_pwm_init_ex(dev->config.pwm_channel, &pwm_cfg);
if (wink_status_is_error(rs)) { /* 回滚 PAL_RESOURCE_PWM_CHANNEL，LOG_W */ }
WINK_IGNORE_UNUSED(pal_pwm_set_duty(dev->config.pwm_channel, 0.0f)); /* DAL-L-006 */
```

---

### P0-2　PWM 通道与 GPIO 引脚共用一个 `int16_t pin` — 寻址模型错误

**位置**　§3 config `int16_t pin`（"驱动 GPIO / PWM 引脚"）、§4.1 两个 variant 都用它。

**问题**　PWM 与 GPIO 在 PAL 是两套寻址空间：
- PWM 用逻辑通道号 `[0, PAL_PWM_CHANNELS)`，物理 GPIO 由 target 固定路由（ESP32 弱符号 [`pal_pwm_pin_map[] = {2,4,5,18,19,21,22,23}`](../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c#L20)），App **不指定 pin**；
- GPIO 才用引脚编号。

把两者塞进一个字段，ESP32 上 `PASSIVE_PWM` 会把 GPIO 号当 channel 传，`ACTIVE_GPIO` 又会把 channel 当 pin 写，两路都错。

**证据**　`rc_servo` / `dc_motor` 用 `uint8_t pwm_channel`（[dal_rc_servo.h:31](../../../wink-micro-os/dal/include/actuator/dal_rc_servo.h#L31)），无 `gpio_pin`；`led` 用 `uint16_t pin`（[dal_led.h:19](../../../wink-micro-os/dal/include/output/dal_led.h#L19)）。YAML 侧 `led.yaml` 字段名 `gpio_pin (c: pin)`，`rc_servo.yaml` 字段名 `pwm_channel`，严格区分。资源类型也不同：PWM 通道 claim `PAL_RESOURCE_PWM_CHANNEL`（[dal_rc_servo.c:61](../../../wink-micro-os/dal/src/actuator/dal_rc_servo.c#L61)），GPIO claim `PAL_RESOURCE_GPIO_PIN`（[dal_led.c:30](../../../wink-micro-os/dal/src/output/dal_led.c#L30)）。

**解决方案**　按 variant 二选一，init 校验互斥：

```c
typedef struct {
    const char          *owner;
    uint16_t             default_freq_hz;
    uint16_t             pin;          /* ACTIVE_GPIO: 必填 GPIO；PASSIVE_PWM: 不用 */
    uint8_t              pwm_channel;  /* PASSIVE_PWM: [0,PAL_PWM_CHANNELS) */
    int16_t              enable_pin;   /* 可选，-1 哨兵（见 P1-2） */
    dal_buzzer_variant_t variant;
    bool                 active_high;  /* 见 P1-1 */
} dal_buzzer_config_t;
```

- `PASSIVE_PWM`：claim `PAL_RESOURCE_PWM_CHANNEL`，不 claim GPIO_PIN；
- `ACTIVE_GPIO`：claim `PAL_RESOURCE_GPIO_PIN` + `pal_gpio_init(PAL_GPIO_OUTPUT_PUSH_PULL)`；
- fail-closed：`PASSIVE_PWM` 但 `pwm_channel >= PAL_PWM_CHANNELS`，或未知 variant → `WINK_ERR_UNSUPPORTED` / `WINK_ERR_INVALID_ARG`（best-practices §3 fail-closed）。

---

### P0-3　承诺了 PAL 不提供的"运行时改 PWM 频率"能力

**位置**　§4.2："`PASSIVE_PWM` 变体，限制 freq_hz…更新 PWM 频率并将占空比设为 50%"。

**问题**　PAL 没有 `pal_pwm_set_freq()`。更关键的是 PWM router 对**已初始化且 profile 不同**的通道直接返回 `WINK_ERR_BUSY`：带新频率调 `pal_pwm_init_ex` 不会成功，必须先 `pal_pwm_deinit` 再 init。

**证据**　[`pal_pwm_router.c:91-97`](../../../wink-micro-os/pal/src/pal_pwm_router.c#L91-L97)：channel 已 init 且 profile 不等 → `WINK_ERR_BUSY`；`pal_hal.h` 全头文件无 set_freq / update_freq；ESP32 实现未封装 `ledc_set_freq`。

**解决方案（编码前必须二选一拍板）**
- **(A) 推荐**：PAL 增补 `pal_pwm_set_freq(channel, uint32_t freq_hz)`，ESP32 封装 `ledc_set_freq`，host/wasm 加 stub。在计划"前置依赖"里登记为独立任务，`play_tone` 直接调用。这是提示音/旋律场景的正确原语，也避免 deinit/init 造成的 click/pop。
- **(B) 不改 PAL**：`play_tone` 内部走 deinit→init_ex，并**删除 §5 Guard D "平滑关断防 click/pop"的承诺**（deinit/init 瞬间 duty 必然抖动），文档标注"不适合音乐级连续变调"。

> 不能既不加 PAL 原语，又在文档里承诺平滑改频。

---

### P0-4　`MAX_FREQ_HZ 20000` 与默认 13-bit 分辨率数学冲突（真机必失败）

**位置**　§3 `DAL_BUZZER_MAX_FREQ_HZ 20000u`；§4.1 默认 `resolution_bits=0`（AUTO）。

**问题**　ESP32 LEDC 默认 13-bit、APB 80 MHz，最高输出频率约 `80e6 / 2^13 ≈ 9766 Hz`。要输出 20 kHz 须降到约 12-bit（19531 Hz）。用 AUTO(13-bit) init 20 kHz 会因 `ledc_timer_config` 失败返回 `WINK_ERR_HARDWARE`。

**证据**　[pal_hal_pwm_esp32.c:52](../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c#L52)（bits 默认 13）、[L75-L86](../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c#L75-L86)（timer config 失败返 HARDWARE）。

**解决方案**
- 把 `DAL_BUZZER_MAX_FREQ_HZ` 收窄到与 13-bit 相符的值（建议 **8000 Hz**，覆盖蜂鸣/按键提示音与大部分音符）；或
- config 暴露 `resolution_bits`，init 按目标频率自适应（蜂鸣器对 duty 分辨率不敏感，10-bit 即可上到 78 kHz），header 注明 tradeoff。
- 频率上下限校验须在 claim 资源**之前**，避免无谓占用。

---

### P0-5　`is_actuator: false` 与执行器属性 / `safe_off` / Guard 表全部矛盾

**位置**　§6 `buzzer.yaml` 第 5 行 `is_actuator: false`。

**问题**　§1.2 把 buzzer 定为 A 类 actuator_command，§5 列了低功耗/硬件托底关断，YAML 又写了 `safe_off_fn: dal_buzzer_safe_off`，但 `is_actuator: false`。

**证据**
- 同属 output 电→物理量执行器的 `led.yaml:4` / `relay.yaml:4` 均 `is_actuator: true`。
- DAL-L-020：`is_actuator: true` MUST 实现 `safe_off`；`false` MUST NOT 有空壳 safe_off。codegen schema 也强制 `is_actuator: true` 的 YAML 必须有 `safe_off_fn`。

**解决方案**　YAML 改为 `is_actuator: true`。

---

### P0-6　`binary_indicator` role binding 用错了动词名

**位置**　§6 `role_bindings.binary_indicator.verbs`：`on / off / set / toggle / is_on`。

**问题**　`binary_indicator` 标准动词是 **`activate` / `deactivate` / `toggle`**（全部 `fire_and_forget → void`），且**没有 `is_on`**。计划杜撰了 `on/off/set/is_on`。

**证据**　[dal-role-architecture-spec.md:85-90](../../../wink-micro-os/docs/dal-development-guide/dal-role-architecture-spec.md#L85-L90)；正确范例 [relay.yaml:80-88](../../../wink-micro-os/codegen/drivers/relay.yaml)（`activate → dal_relay_on`，`deactivate → dal_relay_off`）。

**解决方案**　DAL C 层可保留 `on/off/set/toggle`（与 led/relay 一致），但 binding 键名必须是标准动词：

```yaml
role_bindings:
  tone_generator:
    covers_contract: full
    verbs:
      play_tone_hz:
        template: "static inline void {{ name }}_play_tone_hz(uint32_t freq_hz) { WINK_IGNORE_RESULT(dal_buzzer_play_tone(&{{ name }}, freq_hz)); }"
      stop_tone:
        template: "static inline void {{ name }}_stop_tone(void) { WINK_IGNORE_RESULT(dal_buzzer_stop_tone(&{{ name }})); }"
  binary_indicator:
    covers_contract: full
    verbs:
      activate:
        template: "static inline void {{ name }}_activate(void) { WINK_IGNORE_RESULT(dal_buzzer_on(&{{ name }})); }"
      deactivate:
        template: "static inline void {{ name }}_deactivate(void) { WINK_IGNORE_RESULT(dal_buzzer_off(&{{ name }})); }"
      toggle:
        template: "static inline void {{ name }}_toggle(void) { WINK_IGNORE_RESULT(dal_buzzer_toggle(&{{ name }})); }"
```

> **更正记录**：作者在初评中曾误以为 `tone_generator` 的 binding 应返回 `wink_status_t`。核验 Role spec §3.1 / #12 后确认：`play_tone_hz` / `stop_tone` 契约就是 `fire_and_forget → void`，binding 返回 void **正确**。DAL C 函数 `dal_buzzer_play_tone` 仍返回 `wink_status_t`，binding 内用 `WINK_IGNORE_RESULT` 强转，与 rc_servo `set_angle` 同构。

---

### P0-7　`safe_off` 签名错误（`void` 应为 `wink_status_t`），且 stub 段漏掉

**位置**　§3 `void dal_buzzer_safe_off(dal_buzzer_t *dev);`；stub 段（计划 L166-L190）未声明 safe_off。

**问题**　`safe_off` 被 YAML `safe_off_fn` 引用，codegen 生成的 actuator thunk 要求签名 `wink_status_t(dal_buzzer_t*)`（注册到 `wink_actuator_registry`，故障路径由 `safe_off_all()` 调用）。`void` 签名编译期就不匹配。

**证据**　所有执行器统一为 `wink_status_t dal_*_safe_off(dev*)`：[dal_led.h:97](../../../wink-micro-os/dal/include/output/dal_led.h#L97)、[dal_relay.h:154](../../../wink-micro-os/dal/include/output/dal_relay.h#L154)、[dal_rc_servo.h:83](../../../wink-micro-os/dal/include/actuator/dal_rc_servo.h#L83)。DAL-L-021（**不标 `WINK_WARN_UNUSED_RESULT`**，但返回 status）、DAL-L-022（幂等 + 未初始化返 `WINK_OK`）。

**解决方案**
```c
/* 不标 WINK_WARN_UNUSED_RESULT (DAL-L-021) */
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);
```
实现参照 [dal_led.c:91-96](../../../wink-micro-os/dal/src/output/dal_led.c#L91-L96)：NULL→`WINK_ERR_INVALID_ARG`，未初始化→`WINK_OK`，否则 best-effort off。stub 段补一条（不标 WUR）：
```c
WINK_UNAVAILABLE_MSG(WINK_BUZZER_DISABLED_MSG)
wink_status_t dal_buzzer_safe_off(dal_buzzer_t *dev);
```

---

### P0-8　ILP32 的 `_Static_assert` ABI 数字手算错误

**位置**　§3 L122-L130。

**问题**　按计划给出的结构体定义，在 ILP32（ESP32 xtensa / wasm32，`enum=int=4B`）上实测：`sizeof(config)=20`、`offsetof(initialized)=23`、`sizeof(handle)=28`；计划写的是 16 / 19 / 24，**三条断言在真机 target 编译即失败**。LP64 数字（24/27/32）恰好正确，但 32 位全错。且按 P0-2 拆分 pin/pwm_channel 后布局还会再变。

**证据**　规范 §2.3 明确告诫："数字来源（实测，勿臆测）……任何改动都 MUST 用目标编译器重新核算，禁止凭直觉填写；优先整结构体 sizeof 断言。"

**解决方案**
- 结构体定稿后，用目标编译器写一次性 `printf("%zu %zu\n", sizeof(...), offsetof(...))` 实测，再回填；
- 优先保留**整结构体 `sizeof`** 断言（捕获除"末尾等尺寸替换"外几乎所有 ABI 漂移），中间字段 offset 断言最小化；
- `offsetof(dal_buzzer_t, config) == 0` 必留（DAL-S-014）。

---

### P0-9　Wasm / Wokwi 仿真映射是空头承诺

**位置**　§8："wasm PAL 通过 `notifyDutyChange` / `js_pal_pwm_set_freq` 向 UniSim/Wokwi 发送频率与占空比变化，前端触发 Web Audio API 生成方波声效。"

**问题**　真实代码完全不支持：
- wasm 桥只有 [`js_pal_pwm_set_duty(channel, duty)`](../../../wink-micro-os/targets/wasm/wasm_bridge.h#L30)，**没有 `js_pal_pwm_set_freq`、没有 `notifyDutyChange`**；
- wasm 的 [`pal_pwm_init_ex`](../../../wink-micro-os/targets/wasm/pal_hal_wasm.c#L149-L168) 只调 router 分配 timer，**频率根本没传到 JS**；只有 `set_duty` 上报；
- 全仓前端/wasm 侧无 buzzer / Web Audio / oscillator 业务桥接（grep 命中的均为 build 产物）。

**解决方案**　二选一：
- 把 wasm 发声桥接（`js_pal_pwm_set_freq` extern + JS 侧 Web Audio oscillator + Wokwi `buzzer` 接线）列为 §9 的**显式前置任务**，注明涉及 PAL/wasm/前端三层；或
- 明确 v1 范围仅 host 单测 + ESP32 真机，wasm 发声登记为后续 issue。不能让计划读起来"全 target 闭环"而实际只做一半。

---

### P0-10　Wokwi 元件事实错误：`buzzer` 没有"有源模式"

**位置**　§2.2 表格两行"Wokwi `buzzer`（无源模式）/（有源模式）"。

**问题**　Wokwi 官方 `buzzer` 是**两引脚压电蜂鸣器**（pin1 黑=负接 GND，pin2 红=正接 GPIO），Arduino 示例用 `tone(pin, freq)` 频率驱动；只有 `smooth`/`accurate` 两种**音频渲染**模式，**不存在有源/无源工作模式切换**。

**解决方案**　修正 §2.2：Wokwi `buzzer` 元件本身对应 `PASSIVE_PWM`。`ACTIVE_GPIO` variant 是为**真实硬件**有源蜂鸣模块预留的拓扑，Wokwi 仿真无对应元件变体（仿真里 active_gpio 可降级为 GPIO 高低电平但不出声）。别名表与描述据此修正。

---

### P0-11　必填主引脚类型违反 DAL-S-006

**位置**　§3 config `int16_t pin;`（必填主引脚）。

**问题**　DAL-S-006：**必填**引脚 SHOULD 用 `uint16_t`（合法 GPIO 恒 ≥0，负值类型不可能）；**可选**引脚才用 `wink_pin_t(int16_t)` + `-1` 哨兵。对 `uint16_t` 写 `pin < 0` 是恒假死代码，会触发 `-Werror=type-limits`。

**证据**　[dal-api-consistency-spec.md:106](../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md#L106)；led 必填 `uint16_t pin`（[dal_led.h:19](../../../wink-micro-os/dal/include/output/dal_led.h#L19)）vs relay 可选 `int16_t reset_pin`（[dal_relay.h:35](../../../wink-micro-os/dal/include/output/dal_relay.h#L35)）。

**解决方案**　配合 P0-2：ACTIVE_GPIO 必填 pin 用 `uint16_t`；`enable_pin` 可选用 `int16_t` / `wink_pin_t` 并在 init 校验 `-1`；PWM 用 `uint8_t pwm_channel`。

---

## 3. P1 强烈建议（影响正确性 / 一致性）

### P1-1　极性命名与 LED/Relay 统一，并明确 PWM 变体下忽略
`active_low` 在 `PASSIVE_PWM`（50% 方波）下无极性意义，是死字段；项目里 LED 用 `active_high`、relay 用 `active_low`，命名并存。建议统一为 `bool active_high`（默认 true，与 LED 一致，新驱动少一个认知分叉），并在 doxygen 注明"在 `PASSIVE_PWM` 下被忽略，设置不报错"。

### P1-2　`enable_pin` 语义完全未定义
计划列了 `enable_pin` 却没定义高/低有效、驱动方式、初始化前后电平。外置功放/电源开关的上电顺序直接决定有没有爆音。建议补 `bool enable_active_high`，并在 §4.1 明确顺序：① 先把 enable_pin 配为 push-pull 并驱动到**关断**电平 → ② 配置主驱动并零能量 → ③ 最后使能 enable_pin。v1 若不需要外置使能，直接删字段（YAGNI），不要留半成品。

### P1-3　`play_tone(0)` 语义未定义
`uint16_t freq_hz` 的 0 不在 `[20,20000]`，但 App 常见 `play_tone(map(x))` 在 x=0 时传 0。建议 header 明确：`freq_hz == 0` 等价 `stop_tone()`（幂等返 `WINK_OK`），与内部 `current_freq_hz=0 表示静音`对齐；`<MIN 且 !=0` 或 `>MAX` 返 `WINK_ERR_OUT_OF_RANGE`。

### P1-4　`on()` 与 `play_tone()` 状态机耦合未讲清
建议 §4 补一条状态机真理：**`play_tone(f)` 总是"以 f 发声并置 `is_on=true`、`current_freq_hz=f`"；`on()` 是 `play_tone(default_freq_hz)` 的语义糖**。明确 `play_tone(440)` 后 `on()` 回到默认频率（而非保持 440），消除二义性。

### P1-5　`off` / `stop_tone` 缺 `WINK_WARN_UNUSED_RESULT`，与 led 不一致
计划给 on/set/toggle/play_tone 标了 WUR，唯独 off/stop_tone 没标；而 [dal_led.h:69-70](../../../wink-micro-os/dal/include/output/dal_led.h#L69-L70) 中 on/off 都标了。建议所有非 safe_off/deinit 的公开 C 函数一致标 WUR（DAL-F-004 豁免仅 safe_off/deinit/poll）。注意：role binding 返回 void 是另一层，不影响 C 函数签名。

### P1-6　`last_status` + `get_status` 对纯同步执行器是过度设计，且错引 DAL-B-025
DAL-B-025 是 **poll 返回值语义**条款，与 last_status 字段无关。led/rc_servo/dc_motor 等纯同步执行器都没有 last_status（relay 有，是因为它有 latching pulse 异步状态机 + poll）。buzzer 无 ISR、无 poll、无异步状态机，`volatile` 也不提供原子性/内存序。建议删除 `last_status` / `get_status` / `volatile`，与 led/rc_servo 同构；错误通过返回值上报。

### P1-7　`quantity` 与 `default_role` 不匹配，量纲应按 verb 标
顶层 `quantity: binary` 但 `default_role: tone_generator`（frequency）；且字段一律 `actuator_command` 太粗。建议：
```yaml
quantity: frequency
quantities:
  activate:     { quantity: binary,    quantity_class: actuator_command }
  deactivate:   { quantity: binary,    quantity_class: actuator_command }
  toggle:       { quantity: binary,    quantity_class: actuator_command }
  play_tone_hz: { quantity: frequency, unit: hz, quantity_class: actuator_command }
  stop_tone:    { quantity: binary,    quantity_class: actuator_command }
```

### P1-8　频率参数建议用 `uint32_t`
`uint16_t` 量程够，但 PAL `pal_pwm_config_t.freq_hz` 是 `uint32_t`、dc_motor `pwm_freq_hz` 也是 `uint32_t`。建议 DAL 函数用 `uint32_t freq_hz`（内部 clamp），与底座同宽，免去隐式提升。

### P1-9　deinit 必须三段式 + best-effort LOGW（DAL-L-014/015）
计划 §4 未写 deinit 细节。PWM 路径范例见 [dal_rc_servo.c:180-213](../../../wink-micro-os/dal/src/actuator/dal_rc_servo.c#L180)：safe_off → `pal_pwm_deinit(channel)` → `pal_pwm_channel_pin` 反查 pin → `pal_gpio_reset_pin` → release `PAL_RESOURCE_PWM_CHANNEL`（失败 LOGW 不中止）→ `memset`。GPIO 路径见 [dal_led.c:98-112](../../../wink-micro-os/dal/src/output/dal_led.c#L98)。注意 memset 前先把 channel/pin/owner 拷到局部变量。补：ESP32 `pal_pwm_deinit` 内部已 reset routed pin（[pal_hal_pwm_esp32.c:127-130](../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c#L127-L130)），DAL 再 reset 一次属 belt-and-suspenders，可保留。

### P1-10　Lint 门禁命令错误
§9 第 5 条 `wink lint` 与项目约定不符，改为：
```
python wink-tools/wink.py lint arch --pack layering --pack api
```

### P1-11　两处别名表不一致
§2.3 aliases 带 `active_low`，§6 YAML 省略，需同步；`active_buzzer` 应显式 `variant: active_gpio` + `default_freq_hz: 0`（active 变体忽略频率）。

### P1-12　`is_on` codegen binding 吞掉未初始化错误
`is_on` binding 返 bool 并 `(void)` 掉 status，未初始化与"已关闭"都返 false。且 `is_on` 本就不属于 binary_indicator 标准动词（见 P0-6）。建议不在 role binding 暴露；若保留 DAL 层 `dal_buzzer_is_on(dev, bool* out)`（status-out，参考 relay），文档注明"未初始化视作 false"。

---

## 4. P2 建议（健壮性 / 未来扩展）

| 编号 | 建议 |
|---|---|
| P2-1 | **非阻塞旋律播放划界**：Guard E 禁止 delay 正确，但 BAL 目前无 melody/note 队列。计划末尾登记 follow-up："BAL melody sequencer 属后续 P1，不在本 DAL 范围"，防止日后往 DAL 塞 `play_melody(notes[], durations[])`。 |
| P2-2 | **音量控制预留**：无源蜂鸣器调 duty 即调音量（0–50%）。文档注明"未来通过 `dal_buzzer_set_volume_permille(dev, uint16_t permille)` 扩展"（ADR-0056 用 `_permille`），v1 可不实现。 |
| P2-3 | **默认 2000 Hz 给出来源**：加一句"默认值参考 Wokwi buzzer 元件标称/常见按键提示音"，避免 bikeshed。 |
| P2-4 | **零能量 init 写入 §4.1**：参考 rc_servo 在 `initialized=true` 后显式 `pal_pwm_set_duty(ch, 0.0f)`（[dal_rc_servo.c:86-88](../../../wink-micro-os/dal/src/actuator/dal_rc_servo.c#L86-L88)），不依赖 PAL init 默认 duty。 |
| P2-5 | **单元测试用例显式列全**（§9 仅写"编写单元测试"）：至少覆盖 ① variant 路由（PASSIVE 调 pwm_init_ex / ACTIVE 调 gpio_init，用 fake PAL 校验调用序列）；② 极性；③ 频率边界（0→off，19/20001→OUT_OF_RANGE，20/2000→OK）；④ init 失败资源回滚；⑤ 重复 init→ALREADY_INITIALIZED、未 init→NOT_INITIALIZED；⑥ safe_off 幂等/未初始化返 OK；⑦ deinit 后 pin/channel 复位并释放。 |

---

## 5. 整改后建议的目标结构（供修订参考）

> 仅为整合上述 P0/P1 后的最小骨架，最终以编码时实测/编译为准。

### 5.1 config / handle（整合 P0-2、P0-11、P1-1、P1-6）

```c
typedef enum {
    DAL_BUZZER_VARIANT_PASSIVE_PWM = 0,
    DAL_BUZZER_VARIANT_ACTIVE_GPIO = 1,
} dal_buzzer_variant_t;

typedef struct {
    const char          *owner;           /* DAL-S-001 首成员 */
    uint16_t             default_freq_hz; /* 0 → DAL_BUZZER_DEFAULT_FREQ_HZ */
    uint16_t             pin;             /* ACTIVE_GPIO: 必填 GPIO */
    uint8_t              pwm_channel;     /* PASSIVE_PWM: [0,PAL_PWM_CHANNELS) */
    int16_t              enable_pin;      /* 可选: -1 哨兵 */
    dal_buzzer_variant_t variant;
    bool                 active_high;     /* PASSIVE_PWM 下忽略 */
} dal_buzzer_config_t;

typedef struct {
    dal_buzzer_config_t config;           /* DAL-S-011 首成员 */
    uint32_t            current_freq_hz;  /* 0 = 静音 */
    bool                is_on;
    bool                initialized;
} dal_buzzer_t;
```

### 5.2 API 形态（整合 P0-7、P1-5、P1-8）

```c
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_init(dal_buzzer_t*, const dal_buzzer_config_t*);
wink_status_t          dal_buzzer_deinit(dal_buzzer_t*);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_on(dal_buzzer_t*);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_off(dal_buzzer_t*);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_set(dal_buzzer_t*, bool);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_toggle(dal_buzzer_t*);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_play_tone(dal_buzzer_t*, uint32_t freq_hz);
WINK_WARN_UNUSED_RESULT wink_status_t dal_buzzer_stop_tone(dal_buzzer_t*);
wink_status_t          dal_buzzer_safe_off(dal_buzzer_t*);  /* DAL-L-021: 无 WUR */
```

### 5.3 修订后的 Checklist 顺序建议

1. 拍板 P0-3（PAL 增 `pal_pwm_set_freq` vs deinit/init 限制文档化）；
2. 定稿 config/handle，用目标编译器实测 ABI 断言（P0-8）；
3. 按 P0-2/P0-4 写 init（资源 claim → 硬件 init → 零能量输出 → 置位）；
4. 按 P1-9 写 deinit 三段式；
5. 修 YAML（is_actuator、role 动词、quantity、字段名）；
6. 决定 wasm 桥接范围（P0-9）；
7. host 单测 → `lint arch --pack layering --pack api` → ESP32 build。

---

## 6. 评审结语

这份计划"形"已具备（variant、双 role、Guard、codegen SSOT 都对），缺的是"最后一公里与真实代码对碰"。11 项 P0 全部可在文档阶段以接近零成本修正；若带入编码期，代价是 config 返工、PAL 接口增改、codegen 模板重写、真机调试时才暴露 LEDC 频率失败。

**建议处置**：退回作者按 §2 的 11 项 P0 修订，P1/P2 视排期纳入；修订后无需重新开会，由评审者异步复核 P0 项即可进入编码。

---

*本评审为编码前计划评审（Plan Review），所有"证据"均指向当前仓库真实代码/规范，行号以评审日（2026-08-07）master 分支为准。*

