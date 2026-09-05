# DAL `rc_servo` 驱动对照规范 v3.4.1（ADR-0056）全面合规评审

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-08-03 |
| **评审范围** | `dal_rc_servo.h`、`dal_rc_servo.c`、`codegen/drivers/rc_servo.yaml`、`test/unit/dal/test_dal_rc_servo.c`、`test_dal_abi_freeze.c` 逐规则审计 |
| **基准规范** | [`dal-api-consistency-spec.md` v3.4.1](../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md)（含 [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) §9 量纲两分类、DAL-S-006 引脚类型约定） |
| **驱动成熟度** | `experimental: true`——A 类量可**直接破坏性整型化**，无需 deprecation 窗口（ADR-0056 §6、DAL-XP-111） |
| **关联 ADR** | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md)（apply_override）、[ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 清场）、[ADR-0034](../../decisions/core/0034-dal-progressive-config-disclosure.md)（渐进披露）、[ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)（safe_off=duty=0 limp）、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（A 类整型化） |
| **对照样板** | [`dc_motor`](../../../../wink-micro-os/dal/include/actuator/dal_dc_motor.h)（Golden Ref）、[`led`](../../../../wink-micro-os/dal/include/output/dal_led.h)（v3.4.1 已整改） |
| **结论状态** | **Resolved（2026-08-03 整改完成）**：10 项不合规全部修复；host 单测 21/21 PASS（`-Werror` 零警告）；`wink lint` 无 finding；A 类整型化完成（float→uint16_t ddeg/us），BAL/app 层同步 |

---

## 1. 评审结论

`rc_servo` 是航模 PWM 舵机驱动（50Hz，脉宽 0.5~2.5ms 映射 0~180°），`experimental: true`。

**优点（值得保留）：**
- safe_off 语义注释极佳——明确标注"duty=0=limp 仅适用舵机，不得外推到 DC 电机"，是 ADR-0048 的范例级注释
- PWM 频率用派生常量 `SERVO_PERIOD_MS = 1000.0f/50`，无 magic 20.0f
- deinit 正确通过 `pal_pwm_channel_pin()` 查询路由引脚并 `pal_gpio_reset_pin()`，比 led 更完整
- `apply_override` 轻校验后不写半状态（非法值不修改任何字段）
- clock_requirement 用具名枚举而非裸数字
- `pal_pwm_init_ex` 失败时正确回滚 PWM resource claim

**问题按严重度：**

| # | 严重度 | 规则 | 问题 |
|---|--------|------|------|
| F-1 | **P1 Bug** | DAL-L-006/状态一致性 | `set_angle` 在 `pal_pwm_set_duty` **之前**写 `dev->current_angle`；PAL 失败时缓存谎报硬件已到位 |
| F-2 | **P1 安全** | DAL-L-022 | `safe_off` 未初始化返 `NOT_INITIALIZED`，应急路径应返 `WINK_OK`（与 led F-2 同类） |
| F-3 | **P1 规范** | DAL-U-023/030 | A 类命令 `set_angle(float)` + `float current_angle` 违反"Full 也用定标整数"；experimental 可直接改 `uint16_t angle_ddeg` |
| F-4 | P2 | DAL-L-006 | init 后不显式写 duty=0，依赖 ESP32 PAL `.duty=0` 默认值；host PAL 不记录初始 duty |
| F-5 | P2 | DAL-EC-020/021/023 | 驱动零日志（无 LOG_TAG、无 LOG_I/W） |
| F-6 | P2 | DAL-L-014 | deinit 的 `pal_resource_release` 返回值被 `WINK_IGNORE_UNUSED` 静默丢弃 |
| F-7 | P3 | DAL-BC-010/§2.3 | 缺 32/64 位分档 `sizeof`/`offsetof(initialized)` ABI 断言 |
| F-8 | P3 | §15 | 全部函数 Contract 缺 Side-effects；init 缺 Range；deinit 缺 Postconditions/Error-codes |
| F-9 | P3 | DAL-S-015 | `apply_override` 不检查 `!initialized`，可在 init 后静默改写 config 且不重配硬件 |
| F-10 | P3 | DAL-U-001/004 | 参数名裸 `angle` 无单位后缀；应为 `angle_ddeg` |

> **评级图例**：✅ 合规 · ❌ 不合规（应修） · ⚠️ 部分合规 / 待落地 · ℹ️ 观察项 · N/A 不适用
> **末列"是否解决"留空**，整改后回填。

---

## 2. 逐规则合规清单

### 2.1 数据结构与句柄（§2）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-S-001 | config 首成员 `owner` | ✅ | `dal_rc_servo.h:31` | |
| DAL-S-002 | owner 静态存储串 | ✅ | init 校验非 NULL | |
| DAL-S-003 | 成员按尺寸降序（SHOULD） | ⚠️ | config：ptr(8)→u8→u8→u8→float×3。3 个 u8 后有 1B 填充再对齐 float，可接受；句柄 float→bool 有 3B 尾填充。整型化后可改善 | |
| DAL-S-004 | 序列化线序不重排 | ⚠️ | `apply_override` wire v1 按 `pwm_channel@0, min_pulse_ms@1, max_pulse_ms@5` 编码；**不含 max_angle**，注释已说明。整型化会改 wire 格式（experimental 允许） | |
| DAL-S-005 | 禁位域/`#pragma pack` | ✅ | 无 | |
| DAL-S-006 | 引脚类型：必填 uint16_t / 可选 wink_pin_t | ✅ | rc_servo 用 `uint8_t pwm_channel`（PWM 通道号，非 GPIO pin），无 GPIO pin 字段；通道号 0~7 用 uint8 合理 | |
| DAL-S-010 | POD 句柄 | ✅ | `dal_rc_servo_t:49-53` | |
| DAL-S-011 | config 首成员 | ✅ | `:50` + `_Static_assert(offsetof==0)` `:56` | |
| DAL-S-012 | 含 `bool initialized` | ✅ | `:52` | |
| DAL-S-013 | `{0}` 零初始化安全 | ✅ | | |
| DAL-S-014 | SHOULD offsetof 断言 | ✅ | `:56`（仅 config==0；缺整尺寸断言见 F-7） | |
| DAL-S-015 | init 后 config 不可变（专用 API 除外） | ❌ | **F-9**：`apply_override` 可在 init 后改写 `s->config` 且不重配 PWM 硬件；应加 `if (s->initialized) return WINK_ERR_INVALID_ARG` | |
| §2.3/BC-010 | 分档 ABI 断言 | ❌ | **F-7**：仅有 `offsetof(config)==0`，缺 dc_motor 那样的 `#if INTPTR_MAX` 分档 sizeof/offsetof 断言 | |
| DAL-S-020 | Full 下 init 不 malloc | ✅ | 无堆 | |
| DAL-S-021 | 堆须 Eager 声明 | N/A | | |

### 2.2 生命周期（§3）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-L-001 | init 校验 dev/cfg 非 NULL | ✅ | `dal_rc_servo.c:43`；额外校验 owner NULL `:44` | |
| DAL-L-002 | 深拷贝 cfg→config | ✅ | `memcpy` `:69`，随后规范化 min/max pulse `:70-71` | |
| DAL-L-003 | 成功置 initialized=true | ✅ | `:73`（在 `pal_pwm_init_ex` 成功之后，正确延迟） | |
| DAL-L-004 | 重复 init 返 ALREADY_INITIALIZED | ✅ | `:45` | |
| DAL-L-005 | 最小防御校验 | ✅ | channel 上界 `:46`；clock_requirement 上界 `servo_map_pwm_config:28`；min/max pulse 规范化 `:48-49` | |
| DAL-L-006 | init 后零能量 | ❌ | **F-4**：init 成功后不调用 `pal_pwm_set_duty(ch, 0)`。ESP32 `pal_pwm_init_ex` 设 `.duty=0`（侥幸安全），但 host PAL 不记录初始 duty；DAL 应主动写 0 | |
| DAL-L-007 | 失败回 initialized=false | ✅ | claim/pwm_init 失败均在置位前返回 | |
| DAL-L-008 | init 失败资源回滚 | ✅ | `pal_pwm_init_ex` 失败时 release PWM claim `:63-64`。单资源，回滚完整；虽非 goto-cleanup 模式但逻辑正确 | |
| DAL-L-010 | deinit 幂等 | ✅ | `:137` 未 init 返 OK | |
| DAL-L-011 | 清场顺序：safe_off→停 PWM→GPIO reset→释放→memset | ✅ | `:144` safe_off → `:147` pal_pwm_deinit → `:152-154` GPIO reset → `:158` release → `:161` memset。顺序正确 | |
| DAL-L-012 | ISR 先禁中断等 in-flight | N/A | 无 ISR | |
| DAL-L-013 | 共享总线只释放自身 client | N/A | 独占 PWM 通道 | |
| DAL-L-014 | 清场失败 LOG_W | ❌ | **F-6**：`pal_resource_release` 返回值 `WINK_IGNORE_UNUSED` `:158`，失败无日志 | |
| DAL-L-015 | deinit best-effort + initialized=false | ✅ | memset `:161` | |
| DAL-L-020 | actuator 有 safe_off | ✅ | `dal_rc_servo_safe_off` + YAML `safe_off_fn` | |
| DAL-L-021 | safe_off 不标 WARN_UNUSED_RESULT | ✅ | `dal_rc_servo.h:104` 无该属性（正确） | |
| DAL-L-022 | safe_off 未初始化返 WINK_OK | ❌ | **F-2**：`:98` 返 `NOT_INITIALIZED`。与 led 同类 bug——safe_off_all 在 panic/watchdog 路径调用时未初始化是合法态 | |
| DAL-L-023 | safe_off 不依赖调度器/堆 | ✅ | 仅 `pal_pwm_set_duty` | |
| DAL-L-024 | SHOULD ISR-safe | ✅ | 如实声明 `ISR-safe: No`（调 pal_pwm_set_duty） | |
| DAL-L-025 | safe_off 绑定具体原语并声明 | ✅ | duty=0 limp，头注释 `:99-102` 含架构红线说明，范例级 | |

### 2.3 签名 / 返回值 / 动词（§4–§5）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-F-001 | 返回 wink_status_t | ✅ | 全部 | |
| DAL-F-002 | 禁 bool 返回 | ✅ | | |
| DAL-F-004 | WARN_UNUSED_RESULT + 白名单 | ✅ | init/set_angle/apply_override 标注；safe_off/deinit 不标（白名单正确） | |
| DAL-F-010 | 首参句柄 | ⚠️ | `apply_override(void *dev, ...)` 违反，是规范明列的已知技术债（DAL-F-010 例外表），待 override 类型参数化后消除 | |
| DAL-F-011 | 查询类 const dev | N/A | 无 getter（见 ℹ️ O-1） | |
| DAL-F-012 | 修改类非 const | ✅ | set_angle/safe_off/init/deinit | |
| DAL-F-013 | 出参 `out_` 前缀 | N/A | 无出参 | |
| DAL-F-014 | init 第二参 const cfg | ✅ | | |
| DAL-F-020 | 错误返回时出参不变 | N/A | | |
| §5.1 | `dal_<type>_<verb>` 格式 | ✅ | set_angle/safe_off/apply_override/deinit/init | |
| §5.3.1 | 动词在标准库 | ✅ | set_angle 是执行器标准动词 | |
| 黑名单 | 无 turn_on/run_motor 等 | ✅ | | |
| DAL-V-001~003 | 器件特有 API 用具名 typed API，禁 IOCTL | ✅ | apply_override 是 Flash 覆写专用 API，非 IOCTL | |

### 2.4 并发 / 阻塞 / 失效安全（§6–§8）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-C-040/042 | 默认非线程安全并标注 | ✅ | 每个公开 API 头注释均有 `Thread-safe: No` | |
| DAL-C-001~003 | volatile/RMW 规则 | ✅ | 无跨核/ISR 共享 volatile 字段 | |
| DAL-C-020~022 | ISR 安全声明 | ✅ | ISR-safe: No 如实标注 | |
| DAL-B-001~014 | 阻塞/超时/busy-wait | ✅ | 全非阻塞，无 busy-wait，init <100ms | |
| DAL-BUF-001~003 | DMA Buffer | N/A | 无 DMA | |
| DAL-E-001 | safe_off 绑定具体原语 | ✅ | duty=0 limp，注释含红线 | |
| DAL-E-002 | safe_off 简单确定 | ✅ | 一次 PWM 写 | |
| DAL-BC-001 | Init-to-Ready | ✅ | init 后立即可 set_angle，无 arm 步骤 | |

### 2.5 单位、量纲与值域（§9，ADR-0056 重点）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-U-001/004 | 物理量带封闭后缀 | ❌ | **F-10**：参数名裸 `angle`，应为 `angle_ddeg`（0.1°后缀）。config 的 `min_pulse_ms`/`max_pulse_ms` 后缀正确 | |
| DAL-U-010 | Range 声明 | ⚠️ | set_angle 注释写了 `0.0~effective_max_angle`，但 init Contract 无 Range 字段 | |
| DAL-U-011 | A 类越界钳位饱和无回卷 | ✅ | `:81-83` 显式 clamp 到 [0, effective_max]，无溢出。整型化后须确保 uint16 不超 65535 | |
| DAL-U-020/021 | YAML quantity/quantity_class | ❌ | rc_servo.yaml 无 `quantity`/`quantity_class`。angle 是 A 类 actuator_command，须声明 | |
| **DAL-U-023** | **A 类全 Profile 定标整数，Full 禁 float** | ❌ | **F-3**：`set_angle(float angle)` + `float current_angle` 违反新规。rc_servo 是 experimental，可直接改 `uint16_t angle_ddeg` | |
| DAL-U-025 | A 类字面量直接写整数 | ❌ | 当前 `set_angle(dev, 90.0f)`；目标 `set_angle(dev, 900)`（90.0°） | |
| DAL-U-026 | A 类 API 头注释含 3 个具名字面量 | ⚠️ | 注释写了 0.0~180.0 但无"900=90°"之类的整数刻度示例。整型化时补 | |
| DAL-U-027/028 | 符号规范 | ✅ | 角度无反方向（0~max），用 `uint16_t` 无符号正确 | |
| DAL-U-029 | 乘法中间值提升 32 位 | ℹ️ | 当前 float 运算无溢出；整型化后 `pulse_us` 换算须用 `(uint32_t)` 中间值 | |
| **DAL-U-030** | **setter/getter/句柄同表示** | ❌ | **F-3**：setter/getter/句柄全 float，须同步改 `uint16_t angle_ddeg` | |
| DAL-U-022 | 禁弱 typedef | ✅ | 无 `dal_angle_t` 之类 | |

### 2.6 裁剪 / 双 Target / 兼容（§11–§13）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-P-001 | WINK_USE_RC_SERVO 开关 | ✅ | `dal_rc_servo.h:137` | |
| DAL-P-002 | 裁剪 stub 带 WINK_UNAVAILABLE_MSG | ✅ | `:141-149`，全 5 函数覆盖 | |
| DAL-P-003 | 禁用提示指引启用 | ✅ | `WINK_RC_SERVO_DISABLED_MSG` | |
| DAL-T-001 | dal/ 无平台宏 | ✅ | `.c` 无 `#ifdef SIMULATION/ESP_PLATFORM` | |
| DAL-T-002 | 同源文件进两 target | ✅ | | |
| DAL-T-003 | 时间走 PAL | N/A | 无时间调用 | |
| DAL-BC-001~005 | 向后兼容红线 | ✅ | experimental，允许破坏性改型 | |
| DAL-BC-010 | ABI 断言 | ❌ | F-7（同 §2.1） | |
| DAL-BC-012 | override wire 携带 schema_version | ⚠️ | wire v1 无 version 字段。规范标注"待定义"，当前 9B 固定格式 | |

### 2.7 错误码与日志（§14）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-EC-001/002 | 用通用错误码 | ✅ | INVALID_ARG/NOT_INITIALIZED/ALREADY_INITIALIZED/BUSY/RESOURCE_EXHAUSTED | |
| DAL-EC-010 | 重复 init 返 ALREADY_INITIALIZED | ✅ | `:45` | |
| DAL-EC-020 | init 成功 SHOULD INFO | ❌ | **F-5**：无 LOG_I | |
| DAL-EC-021 | init 失败 SHOULD WARN | ❌ | **F-5**：claim/pwm_init 失败直接返回，无 LOG_W | |
| DAL-EC-022 | ISR 不打日志 | ✅ | 无 ISR | |
| DAL-EC-023 | tag `dal_<type>` | ❌ | **F-5**：`.c` 无 `#define LOG_TAG`，未 include `pal_log.h` | |
| DAL-EC-030 | init 防御校验 | ✅ | channel/owner/pulse 校验 | |

### 2.8 API Contract 注释（§15）

| 函数 | Pre | Post | Range | Blocking | Thread-safe | ISR-safe | Side-effects | Error-codes | 状态 | 是否解决 |
|------|-----|------|-------|----------|-------------|----------|--------------|-------------|------|----------|
| init | ✅ | ✅ | ❌ 缺 | ✅ | ✅ | ✅ | ❌ 缺（claim PWM、写 duty=0） | ✅ | ❌ F-8 | |
| set_angle | ✅ | ✅ | ⚠️ 文字描述 | ✅ | ✅ | ✅ | ❌ 缺（写 PWM duty、写 current_angle） | ✅ | ❌ F-8 | |
| safe_off | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺（写 duty=0） | ✅ | ❌ F-8 | |
| apply_override | ✅ | ✅ | N/A | ✅ | — | — | ❌ 缺（改写 config 字段） | ✅ | ❌ F-8 | |
| deinit | ✅ | ❌ 缺结构化 Post | N/A | ✅ | ✅ | — | ⚠️ 注释提了 ADR-0024 但无 Side-effects 字段 | ❌ 缺（仅写 @return WINK_OK，NULL 实际返 INVALID_ARG） | ❌ F-8 | |

- Thread-safe/ISR-safe 字段全覆盖（满足 DAL-C-042 门槛）。✅
- 亮点：safe_off 注释含架构红线说明，质量高。

### 2.9 Codegen YAML（§16）

| 项 | 状态 | 说明 | 是否解决 |
|----|------|------|----------|
| codegen_schema 1.1 | ✅ | | |
| type/category/is_actuator/experimental | ✅ | rc_servo/actuator/true/**true** | |
| default_role | ✅ | angular_actuator | |
| fields 声明 | ✅ | pwm_channel/resolution_bits/clock_requirement/min_pulse_ms/max_pulse_ms/max_angle/role | |
| config 映射 | ✅ | c_type/config_type/headers/deinit_fn/safe_off_fn 齐全 | |
| role_binding set_angle | ⚠️ | 当前 `template: "...set_angle(float angle)..."`；整型化后须改为 `uint16_t angle_ddeg` | |
| quantity/quantity_class | ❌ | 缺；angle 须标 `quantity_class: actuator_command`（F-3 配套） | |
| profiles 显式声明 | ℹ️ | 未写，默认 Full；可补 `profiles: [full]` | |
| constraints max_angle | ✅ | min:0.0001, equals:0→omit(warn), equals:180→omit | |

### 2.10 状态缓存一致性（横切关注点）

| 规则 | 状态 | 说明 | 是否解决 |
|------|------|------|----------|
| **状态缓存只在硬件成功后更新** | ❌ | **F-1**：`set_angle:84` 在 `pal_pwm_set_duty:91` **之前**写 `dev->current_angle = angle`。若 PAL 返回错误，缓存谎报角度已到位。dc_motor 范式是先 apply_dir_and_duty 成功后再写 current_speed | |

---

## 3. 不合规项整改清单（按优先级）

| # | 严重度 | 规则 | 整改建议 | 是否解决 |
|---|--------|------|----------|----------|
| F-1 | **P1 Bug** | 状态一致性 | `current_angle_ddeg` 在 `pal_pwm_set_duty` 成功**之后**才写入 | ✅ 2026-08-03 `dal_rc_servo.c:97`（先硬件后缓存）；新增 `test_cache_updated_after_hardware_success` |
| F-2 | **P1 安全** | DAL-L-022 | `safe_off` 未初始化返 `WINK_OK` | ✅ 2026-08-03 `:104`；新增 `test_safe_off_before_init_returns_ok` |
| F-3 | **P1 规范** | DAL-U-023/030 | A 类整型化：`set_angle(uint16_t angle_ddeg)`、`uint16_t current_angle_ddeg`、config `min_pulse_us`/`max_pulse_us`/`max_angle_ddeg`、`(uint32_t)` 中间值防溢出、YAML 模板+quantity_class | ✅ 2026-08-03 全部完成；BAL sweep 在数学域保留 float 但在 BAL→DAL 边界转换 ddeg；app 层同步 |
| F-4 | P2 | DAL-L-006 | init 显式 `pal_pwm_set_duty(ch, 0)` | ✅ 2026-08-03 `:75`；新增 `test_init_writes_zero_duty` |
| F-5 | P2 | DAL-EC-020/021/023 | LOG_TAG + LOG_I/LOG_W | ✅ 2026-08-03 `#define LOG_TAG "dal_rc_servo"` + `pal_log.h`；init 成功 LOG_I，claim/init 失败 LOG_W |
| F-6 | P2 | DAL-L-014 | deinit release 失败 LOG_W | ✅ 2026-08-03 `release_pwm_claim_logged()` |
| F-7 | P3 | DAL-BC-010/§2.3 | 分档 ABI 断言（64 位实测 24/32/26，32 位推导 16/20/18） | ✅ 2026-08-03 `dal_rc_servo.h:62-71`；64 位编译验证通过 |
| F-8 | P3 | §15 | 全函数 Side-effects；init Range；deinit Post/Error-codes；safe_off Side-effects | ✅ 2026-08-03 全部补齐，含具名字面量示例（900=90°、1800=180°） |
| F-9 | P3 | DAL-S-015 | apply_override 加 `if (initialized) return INVALID_ARG` | ✅ 2026-08-03 `:113`；新增 `test_apply_override_rejects_after_init` |
| F-10 | P3 | DAL-U-001/004 | `angle` → `angle_ddeg` | ✅ 2026-08-03（随 F-3 完成） |

### ABI 断言常量核算（F-7）

**当前 float 版本（实测 64 位 gcc 16）：**

| | 64 位 host（LP64） | 32 位 target（ILP32，推导） |
|---|---|---|
| `sizeof(config_t)` | 24 | 20 |
| `sizeof(handle_t)` | 32 | 28 |
| `offsetof(initialized)` | 28 | 24（config 20 + float current_angle 4） |

> 32 位推导：ptr=4B，u8×3=3B + 1B pad + float×3=12B → config=20B；handle=20+4(current_angle)+1(bool)+3pad=28B。

**整型化后版本（推导，须在 32 位 target 编译验证）：**

config 字段排列建议（uint16 靠前减少填充）：
```c
typedef struct {
    const char *owner;              /* 4/8 B */
    uint16_t min_pulse_us;          /* 500~2500 */
    uint16_t max_pulse_us;
    uint16_t max_angle_ddeg;        /* 0=default 1800 */
    uint8_t  pwm_channel;
    uint8_t  resolution_bits;
    dal_rc_servo_clock_requirement_t clock_requirement; /* uint8 */
} dal_rc_servo_config_t;
```

| | 64 位（实测） | 32 位（推导） |
|---|---|---|
| `sizeof(config_t)` | 24 | 16 |
| `sizeof(handle_t)` | 32 | 20 |
| `offsetof(initialized)` | 26 | 18 |

> 64 位实测：`sizeof(config)=24, sizeof(handle)=32, offsetof(initialized)=26`。32 位比 float 版本省 8B（20 vs 28）。断言常量须用 ESP32/wasm 目标编译器实测后填入，勿臆测。

---

## 4. 观察项（ℹ️，非 MUST）

| # | 观察 | 建议 |
|---|------|------|
| O-1 | 无 `get_angle` const getter；句柄缓存 `current_angle` 但无公开读回 API | 可补 `dal_rc_servo_get_angle(const dev, uint16_t *out_angle_ddeg)`（整型化后），符合 DAL-F-011 const getter 范式。非 MUST，但 dc_motor 有 get_speed |
| O-2 | `apply_override` wire v1 不含 `max_angle`/`resolution_bits`/`clock_requirement` | 注释已说明 Non-goal；整型化改 wire 格式时可考虑 v2 加 `schema_version`（DAL-BC-012） |
| O-3 | rc_servo 未在 `runtime/src/wink_actuator_registry.c` 静态注册 | 确认 codegen 是否自动注册；若否，按 DAL-E-010 SHOULD 注册 |
| O-4 | config 的 `min_pulse_ms`/`max_pulse_ms` 是 float 但 YAML `emit: macro` 生成 C 宏 | 整型化后改为 `min_pulse_us`（整数宏），消除 codegen 生成 float 字面量 |
| O-5 | `servo_map_pwm_config` 是 static 但不依赖句柄状态，纯函数 | 设计良好，可保留 |
| O-6 | deinit 注释中的 ADR-0024 checklist 非常完整（10 项逐条标注） | 值得作为其他驱动的 deinit 注释模板推广 |

---

## 5. 亮点（值得其他驱动学习）

1. **safe_off 架构红线注释**（`:99-102`）：明确区分舵机 duty=0=limp vs DC 电机 duty=0=coast，禁止外推——这是全 DAL 最好的 safe_off 语义文档。
2. **deinit ADR-0024 checklist**（`:131-135`）：10 项逐条标注 N/A 或实现方式。
3. **PWM 频率派生常量**（`:8`）：`SERVO_PERIOD_MS = 1000.0f/FREQ`，无 magic number。
4. **`pal_pwm_init_ex` + `pal_pwm_config_t`**：不直接暴露 PAL 类型到公开头（ADR-0034），通过 static 映射函数隔离。
5. **apply_override 校验严格**：非法 channel/pulse 不写任何字段（half-state-free）。
6. **config 规范化在 init 内完成**：`min_pulse_ms`/`max_pulse_ms` 的默认值回退在 `:48-49` 统一处理，深拷贝后写回 dev->config。

---

## 6. 总评

rc_servo 驱动**设计质量高于平均水平**（safe_off 注释、deinit checklist、PWM 抽象、apply_override 校验都是亮点），主要差距集中在：

1. **A 类量纲整型化（F-3）**：这是 experimental 驱动对齐 v3.4.0 的核心工作，改完即可成为"A 类定标整数 + 物理刻度（ddeg）"的执行器 Golden Ref（dc_motor 是 float 迁移前例外，rc_servo 是新规样板）。
2. **两个 P1 行为 bug**（F-1 缓存时序、F-2 safe_off 未初始化语义）——与 led 同类问题，修法明确。
3. **工程完备度**（日志/ABI 断言/Contract）照 led/dc_motor 样板补齐。

整改后 rc_servo 可作为"**A 类绝对物理量整数定标（0.1° ddeg）+ experimental 直接整型化**"的参考标杆，与 dc_motor（"stable float 迁移前现状"）形成互补。

**建议执行顺序**：F-1（移缓存行）→ F-2（safe_off 语义）→ F-4（init duty=0）→ F-3（整型化，含 F-10/YAML/role_binding）→ F-5/F-6（日志/释放）→ F-9（override 守卫）→ F-7（ABI 断言）→ F-8（Contract）。整型化改动较大但 experimental 无兼容负担，可一次性完成。

---

*本评审为时间点快照（文档第④层），归档后不随代码变动修改；整改结果在 §3 末列"是否解决"回填。*

