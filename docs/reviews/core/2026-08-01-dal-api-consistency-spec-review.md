# DAL API 一致性规范评审报告

评审对象: wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md v1.0.0 (Draft)
评审日期: 2026-08-01
结论: 有条件通过 (Rework Required) - 结构可用, 内容需以现存代码与 codegen SSOT 为基准重写

## 总体评分

| 维度 | 评分 | 说明 |
|------|------|------|
| 文档结构与可读性 | 8/10 | 四维度划分清晰, 表格化动词库直观, 适合 AI 与新人检索 |
| 与代码库现状一致性 | 2/10 | 10 个现存驱动几乎全部不合规; 多条黑名单打脸自家 API |
| 与既有 SSOT 对齐 | 2/10 | 自造第二套 YAML 元模型, 与 ADR-0046 registry / codegen roles 冲突 |
| 嵌入式工程完备性 | 4/10 | 缺并发/ISR, 阻塞与超时, 单位量纲, 失效安全四大核心章节 |
| 可执行/可 lint 性 | 3/10 | 正文 30+ 条要求, 仅 3 条给了校验规则, 且其中 1 条技术上不可实现 |

一句话: 框架对了, 但当前是愿望清单而非规范, 按现状打开 CI 会导致全仓库红或规则被降级成注释, 两周内即成死文档。

---

## 一. 与代码库现状的冲突 (Blocking)

规范声称的强制项与实际 10 个驱动 (led, dc_motor, rc_servo, button, encoder, ultrasonic, mono_oled, eeprom, gps, button_bal) 大面积不符。

### C-01 五大生命周期 API: 现状 0 个驱动合规 (Blocking)

规范 2.2 要求所有驱动 100% 具备 init/deinit/safe_off/reset/get_state。实测:

| API | 现状 |
|-----|------|
| init | 10/10 具备, 合规 |
| deinit | 9/10 具备 (led.yaml 未声明 deinit_fn) |
| safe_off | 仅 dc_motor, rc_servo 真实具备; led 是 safe_off_fn: dal_led_off 别名; button/encoder/eeprom/gps/mono_oled/ultrasonic 均为 safe_off_fn: "" |
| reset | 0/10。encoder 有 dal_encoder_reset 但语义是计数清零 (对应 3.2 的 zero), 不是规范定义的软复位 |
| get_state | 0/10。eeprom 是 dal_eeprom_get_status(dev, out_state), 签名与命名都不符 |

问题不在代码, 而在规范: codegen YAML 用 safe_off_fn: "" 明确表达了"本器件无安全关断语义", 这是正确的工程判断 (按钮/编码器/EEPROM 没有需要紧急切断的物理输出)。规范把它一律定为 MUST 是错的。

建议: 拆成三档。init/deinit 为 MUST; safe_off 仅当 is_actuator: true 或 category in (actuator, output) 时 MUST, 其余 MUST-NOT (避免生成空壳); reset/get_state 降为 SHOULD 并给出与 eeprom 既有 get_status 的收敛方案。

### C-02 布尔查询 API 与 ADR-0001 / lint 规则自相矛盾 (Blocking)

规范 3.1 写 is_on(dev), 3.2 写 is_<condition>(dev) 并示例 dal_button_is_pressed(dev), 暗示返回 bool。

但同一份文档 2.3 要求"必须统一返回 wink_status_t"; 实际代码是 wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed); 并且 lint rules/api.yaml 已有规则 STATUS-NOT-BOOL-PUBLIC (deny bool dal_\w+\s*\(, refs ADR-0001) 禁止 bool 返回的公开 DAL API。

规范内部矛盾, 且与已实施的 lint 规则冲突。必须明确: 谓词查询一律 wink_status_t + bool *out_<pred> 出参, 表格里的 is_on(dev) 写法要更正。

### C-03 黑名单误伤自家既成 API (Blocking)

3.2 黑名单含 get_value, fetch_data, sample_now, get_dist。但仓库现存:

- dal_encoder_get_count(dev, *out_count)
- dal_ultrasonic_get_cached_distance(dev, *distance_cm)
- dal_button_get_edge_count(dev, *out_count)
- dal_eeprom_get_status(dev, *out_state)
- dal_gps_get_position(dev, *pos)

这些 get_* 表达的是"读取已缓存值, 不触发物理采样", 与 read_* (触发一次实测) 是本仓库刻意区分的两种语义 (见 ultrasonic 的 request_measurement / get_cached_distance / read 三段式)。规范只禁词不给语义模型, 会诱导开发者把 get_cached_distance 改名成 read_cm, 直接破坏"是否触发硬件时序"这一关键契约。

建议: 黑名单只保留真正含糊的 fetch_data / sample_now / get_dist (缩写), 同时正式确立 read_* = 触发采样(可能阻塞), get_* = 只读缓存(非阻塞, 不碰硬件) 这一对语义, 并写进 API Contract 注释的必填字段。

### C-04 第一参数规则漏掉 const 正确性 (Major)

2.3 规定"第一参数必须是 dal_<type>_t *dev"。实际所有 getter 都是 const dal_<type>_t *dev (dal_dc_motor_get_speed, dal_encoder_get_count, dal_gps_get_position, dal_eeprom_get_status ...)。

按字面执行规范会要求删掉 const, 是明确的倒退。应补一条: 不修改设备状态的查询类 API MUST 使用 const dal_<type>_t *dev; 修改状态的 MUST 用非 const。这本身也是一条易于 lint 的强规则。

### C-05 领域分类引入了第三套分类词 (Major)

规范 3 提出 4 大 Trait: Output/Actuator, Sensor/Input, Display/Visual, Comm/Storage。

实际存在两套已落地的分类:
- 目录: dal/include/{actuator, output, input, sensor, display, storage, comm} = 7 类
- codegen YAML category: actuator, output, input, sensor, display, storage, comm + is_actuator 布尔 + default_role (7 个 role: binary_indicator, binary_sensor, open_loop_actuator, angular_actuator, distance_sensor, pulse_counter, text_display)

规范的 Trait 是第三套词汇, 且没给映射表。同时 ADR-0048 已按控制语义细分 actuator (dc_motor/stepper/rc_servo/industrial_servo/bldc), 规范把 RC Servo 与 DC Motor 一起塞进"Output/Actuator 用同一套动词表", 与 ADR-0048"关断语义随器件, 不外推通用范式"的决策直接冲突。

建议: 删掉 Trait 这个新词, 直接复用 category + role; 或者明确给出 Trait = category 的多对一映射表并说明 Trait 只用于动词库分组, 不参与任何代码生成。

### C-06 7.1 的 YAML 示例是虚构 schema (Blocking)

规范给出的示例:

    name: dc_motor
    trait: Actuator
    lifecycle: {init_fn, deinit_fn, safe_off_fn}
    standard_verbs: {set_speed, get_speed}

真实 codegen/drivers/dc_motor.yaml 是:

    codegen_schema: "1.1"
    type: dc_motor
    category: actuator
    is_actuator: true
    experimental: false
    default_role: open_loop_actuator
    ic_to_variant_map: {...}
    fields: {pwm_channel: {tier, type, required}, ...}
    config: {c_type, config_type, headers, deinit_fn, safe_off_fn}
    role_bindings: {open_loop_actuator: {verbs: {set_speed: {template: ...}}}}

字段名 (name vs type, trait vs category, lifecycle vs config) 全部不同, 缺失 codegen_schema 版本号, 缺失 fields 的 tier 分层 (stable/advanced, 关联 dal-best-practices 3.0), 缺失 role_bindings 的 Jinja template 形态, 也未提 codegen/roles/*.yaml 这个 role 契约 SSOT。

按 ADR-0046, 驱动全集 SSOT 是 codegen drivers registry。规范复述一套错的 schema, 等于制造第二真相源。必须删除示例, 改为引用 codegen/README.md 与 roles/*.yaml, 并锁定 codegen_schema: "1.1"。

补充: 规范正文写"每个 DAL 驱动必须在 wink-micro-os/codegen/drivers/ 提供 YAML", 而 ADR-0046 正文写的是 wink-micro-os/tools/codegen/drivers/*.py (registry)。实测 wink-micro-os/tools/codegen/drivers 不存在, 现实路径是 wink-micro-os/codegen/drivers/*.yaml。ADR-0046 本身也已过期, 建议一并勘误。

### C-07 lint 路径三个版本互相矛盾 (Major)

- 本规范 7: wink-tools/tools/lint 规则引擎 + codegen/drivers/*.yaml
- CLAUDE.md: wink-tools/lint/rules/*.yaml (不存在)
- 实测: wink-tools/tools/lint/rules/{api,layering,user_surface}.yaml + packs/drivers.py (rule_id: drivers.registry_consistency, drivers.role_binding_coverage)

规范里的 Rule-DAL-01/02/03 这三个 ID 在引擎里不存在, 现有 rule_id 命名法是 <pack>.<rule> 小写点分。ID 体系需与引擎对齐, 否则报错信息与文档无法互查。

### C-08 Rule-DAL-03 (config_t 无填充) 技术上不成立 (Major)

"校验 config_t 成员布局是否自然对齐无填充"这条不可实现且描述错误:

1. 成员降序排列只能减少内部填充, 无法消除结构体尾部对齐填充。
2. dal_dc_motor_config_t 实际顺序是 const char *owner; uint8_t pwm_channel; wink_pin_t dir_pin_a; wink_pin_t dir_pin_b; uint32_t pwm_freq_hz; dal_dc_motor_variant_t variant; wink_pin_t enable_pin; bool invert; - 这不是降序 (uint8_t 排在 uint32_t 前面), 现存代码已违规。
3. 更重要的是: config 结构体成员顺序在本项目有另一个约束 - dal_dc_motor.h 注释明确写了 "Future serialization follows config member order", 而 rc_servo/ultrasonic 已有 apply_override(void *dev, const uint8_t *params, uint16_t len) 按成员序反序列化。为省几字节填充而重排成员, 会破坏 override 线格式兼容性。规范的这条规则与向后兼容红线 (6) 直接打架。

建议改为可验证形式: 禁止位域, 禁止 #pragma pack, 新增成员只能追加在末尾, 尺寸/布局变化必须过 _Static_assert(sizeof(...)==N) 与两 target ABI 快照 diff。填充优化降为非强制建议。

---

## 二. 缺失章节 (资深嵌入式视角, 按重要性排序)

这是本规范最大的问题: 它花了大量篇幅规范"动词拼写", 却回避了嵌入式驱动规范真正必须回答的问题。以下五章缺失, 其中并发与失效安全属于安全相关缺陷。

### M-01 并发与 ISR 契约 (Critical, 缺失)

规范仅在注释模板里放了一行 Thread-safe: No; ISR-safe: No, 而实际代码已经深度涉及中断与多核:

    dal_button.h:88   volatile uint32_t edge_count;   /* ISR 写 / poll 读 */
    dal_button.h:97   volatile bool irq_pending;      /* set by ISR, cleared by consume */
    dal_ultrasonic.h:46-49
        volatile float last_distance;          /* volatile: SMP cross-core reader */
        volatile uint32_t last_pulse_us;
        volatile wink_status_t last_status;
        volatile dal_ultrasonic_state_t state;
    dal_encoder.h:45  volatile int32_t count;

必须补充的强制条款:

1. volatile 不等于原子, 也不提供内存序。ESP32-S3 是双核 SMP, volatile 不产生 acquire/release 屏障。规范必须明确: 跨 ISR/跨核共享字段仅允许"单字宽 + 单写者 + 读者容忍旧值"模式; 任何 read-modify-write (如 count += delta) MUST 使用 PAL 原子或临界区。
2. 多字段快照撕裂风险。ultrasonic 的 last_distance / last_pulse_us / last_status / state 是四个独立 volatile 字段, 调用方无法获得一致快照 - 可能读到 state==DONE 但 last_distance 还是上一轮的值。这是真实 bug 温床。规范 MUST 规定其一: (a) seqlock 序号模式; (b) 单个原子快照结构 + 版本号; (c) 至少钉死读取顺序契约 (先读 payload 后读 state, 或反之) 并在头注释声明。
3. ISR 可调用 API 白名单。需要一个 WINK_ISR_SAFE 属性宏 (对齐既有 WINK_BLOCKING 风格) 并规定: ISR-safe API MUST NOT 分配内存/取锁/调用日志/阻塞; ISR 上下文调用非 ISR-safe API 由 lint 静态检查。
4. 回调上下文归属。dal_button_on_event 的回调在 ISR 还是任务上下文? 回调内允许调用哪些 API? 没有这条, 5.2 的回调规范不可用。
5. deinit 与 ISR 的竞态。dal_button_deinit 注释写"卸载 ISR 并清理", 规范必须规定顺序: 先禁中断 -> 再等待 in-flight 回调结束 -> 再释放资源 -> 最后置 initialized=false。反序会 use-after-deinit。

### M-02 失效安全与 safe_off 语义 (Critical, 不完整)

规范 2.2 只写"立即将硬件切断物理输出, 回到安全状态", 但这远不够:

1. safe_off 有两种物理后果。dc_motor 有 brake (H 桥短接制动, 急停) 和 coast (全关滑行), 后果完全不同。ADR-0048 已钉死 dal_dc_motor_safe_off 绑定 brake。规范必须引用该决策, 并明确 safe_off 只是"绑定到某个具体关断原语的别名", 而不是新的独立语义。
2. 应急路径不应可失败。safe_off 会在 watchdog / panic / assert 失败 / 上层异常回滚路径被调用。此时返回 wink_status_t 让调用方"检查错误"没有意义, 而 WINK_WARN_UNUSED_RESULT 会迫使调用方写 (void) 或吞掉返回值。建议: safe_off MUST 满足 idempotent + 未初始化时调用安全返回 OK + 尽可能 ISR-safe + 不依赖调度器与堆; 或直接定为 void。规范需明示裁决。
3. 缺"谁负责调用"。系统级 safe-all 由谁遍历? 是否有 wink_actuator_registry (dal-best-practices 与 ADR-0048 都提到) 统一驱动? 规范对此完全沉默, 而这是安全链路的关键一环。
4. Init-to-Ready 对执行器是危险默认。规范 6.1 规定"init 成功后器件默认进入 READY 或 IDLE"。对 LED 无害, 对电机/舵机则必须明确: init MUST 使输出处于零能量状态 (duty=0 / enable 引脚 inactive), 严禁 init 即通电或保持上一次 duty。当前 dc_motor init 的行为需要被规范明确背书。

### M-03 阻塞, 超时与实时性预算 (Critical, 缺失)

代码里已有成熟约定, 规范一字未提:

    WINK_BLOCKING WINK_WARN_UNUSED_RESULT wink_status_t dal_ultrasonic_read(...)
    dal_gps_init_blocking(...)
    dal_eeprom_read_blocking(...) / dal_eeprom_write_blocking(...)

应补条款:
1. 任何可能阻塞调用者的 API MUST 带 _blocking 后缀且 MUST 标注 WINK_BLOCKING (可 lint: 有后缀无属性/有属性无后缀均报错)。
2. API Contract 注释的 Blocking 字段 MUST 给出最坏阻塞时间的数值上界 (如 HC-SR04 最坏约 38ms 超时), 而不是当前模板里的 Yes(ms) 占位符。
3. 超时值 MUST 来自 config 或编译期常量宏, MUST NOT 硬编码在函数体内。
4. 非阻塞 API MUST NOT 内部 busy-wait 超过约定的微秒级上界 (给一个数, 如 100us), 否则必须改为 request/poll 三段式。
5. 需说明与仿真 Asyncify 的关系: 阻塞 API 在 wasm 端如何落地 (已有 reviews/2026-06-24 phase1-asyncify-deep-dive), 规范应引用并给出 DAL 作者的约束。

### M-04 单位与量纲命名法 (Major, 缺失)

这是低代码/AI 生成场景最高收益的一条, 规范完全没有。现状混乱:

- 有单位后缀: read_cm, distance_cm, pwm_freq_hz, long_press_ms, debounce_ms, last_pulse_us
- 无单位/无值域: dal_dc_motor_set_speed(float speed) - 实际是 [-1.0, 1.0] 归一化 (由头注释和 ADR-0048 定义), 但函数签名与规范表格 dal_dc_motor_set_speed(dev, 80) 的示例给出的是 80, 暗示百分比。规范示例与真实值域直接矛盾, AI 照抄示例会输出 80 倍超量程。
- dal_rc_servo_set_angle(float angle) 无单位后缀 (度), 无值域声明。

应补条款: 所有物理量参数与出参名 MUST 带单位后缀 (_cm/_mm/_ms/_us/_hz/_deg/_pct) 或明确为无量纲归一化并在名字里体现 (_norm / _ratio); 值域 MUST 在 Contract 注释的新增 Range 字段声明; 越界 MUST 返回 WINK_ERR_INVALID_ARG 还是饱和截断 (saturate) 需二选一钉死 - 这对执行器是安全问题。

### M-05 异步三段式 (request / poll / get_result) 未被承认 (Major, 缺失)

规范 5.2 只给了 register_callback, 但仓库真正的主流异步模式是三段式状态机:

    dal_ultrasonic_request_measurement -> (poll/ISR) -> dal_ultrasonic_get_cached_distance
    dal_eeprom_request_read/request_write -> dal_eeprom_poll -> dal_eeprom_get_read_result / get_status
    dal_button_poll / dal_gps_poll

这套模式契合协作式调度与双 target 同源, 应升格为规范的一等公民 (第五维度), 并规定:
1. 命名: request_<op> / poll / get_<op>_result / get_status。
2. 状态机基线值域: IDLE / BUSY / DONE / ERROR, 以及 BUSY 时重复 request MUST 返回 WINK_ERR_BUSY (eeprom 已如此)。
3. state 与错误码的正交关系: 何时用返回值报错 vs 何时把错误存进 last_status (ultrasonic 用了后者)。
4. poll 的调用频率契约与 poll 未被调用时的降级行为。

### M-06 stub / 未实现器件契约 (Major, 缺失)

WINK_UNAVAILABLE_MSG(WINK_<X>_DISABLED_MSG) 出现在全部 10 个头文件 (裁剪禁用态), 且 gps / eeprom 是 stub - dal_gps.h 注释明确写 "当前 stub 实现将 *dev 清零, dev->initialized=false, 未 claim UART 资源", 返回 WINK_ERR_UNSUPPORTED。

这是本项目重要机制 (关联 ADR-0039 双模裁剪, fail-closed), 规范完全没写。应补: stub MUST 返回 WINK_ERR_UNSUPPORTED; MUST NOT claim 任何资源; MUST 在 YAML 标 experimental: true; MUST NOT 使 initialized=true (避免上层误判可用)。同时需说明 init 返回非 OK 但结构体已清零时, 上层重试与 deinit 的合法性。

### M-07 双 target 一致性缺可验证条款 (Major)

1 把"两端同源"列为核心目标, 但全文没有一条可执行约束。建议补:
1. dal/ 层 MUST NOT 出现 #ifdef SIMULATION / ESP_PLATFORM / __EMSCRIPTEN__ (下沉到 PAL/targets)。这条与 CLAUDE.md"Bypass 范围收窄"一致, 且可直接 lint。
2. 同一 .c MUST 同时进入两 target 构建, 禁止 per-target 分叉源文件。
3. 行为差异 (如仿真不模拟 I2C NAK) MUST 在头注释新增 Simulation-parity 字段声明。
4. 时间相关 API MUST 通过 PAL 时钟, MUST NOT 直接用平台 API。

---

## 三. 应删除或降级的设计 (设计层反对意见)

### D-01 强烈建议删除 4 (统一 IOCTL 窗口) - 与 ADR-0004 冲突

规范提出:

    wink_status_t dal_<type>_control(dal_<type>_t *dev, uint32_t cmd, void *arg);

反对理由:

1. 违背 ADR-0004 精神。ADR-0004 选择编译期静态分发, 拒绝运行时 ops/vtable, 核心收益是类型安全 + 可静态分析 + 零间接开销。uint32_t cmd + void *arg 是把运行时分发从"函数指针表"换成"switch 表", 类型安全完全丧失, 是同一问题的另一种形态。lint 的 NO-OPS-VTABLE 规则挡不住它, 但它带来的正是该规则想消除的危害。
2. 摧毁工具链可分析性。本项目的核心价值是 codegen 能从 DAL 提取能力生成 role verb 与前端描述 (codegen/roles/*.yaml + role_bindings 模板)。control(cmd, void*) 的能力面无法被静态提取, 无法生成 Jinja 模板, 无法生成 UniSim 虚拟外设绑定, 也无法给 AI 生成器提供类型约束。规范 4 声称"防止大量零散特有 API", 但零散的具名 API 恰恰是可分析的, 而单个 control 是不可分析的 - 优化方向反了。
3. void *arg 的双 target ABI 风险。arg 指向的结构体在 wasm32 与 xtensa 下布局可能不同, 且没有任何编译期检查, 是同源编译最容易埋雷的形态。
4. 与 3 的黑名单精神矛盾。文档一边严禁开发者自创动词以保证一致性, 一边开一个任意 cmd 的后门, 所有非标能力都会涌入这里, 最终 DAL 一致性由 control 的 cmd 枚举地狱承接。

替代方案: 器件特有能力使用具名 typed API (dal_can_set_filter(dev, const dal_can_filter_t *f)), 并在 YAML 用 device_specific: true 标记, 使其不进入通用 role verb 平面。若确需保留 control, MUST 限定为 #ifdef WINK_DEBUG 下的调试通道, 且 MUST NOT 被 codegen/App 层调用 (可 lint)。

建议为此单独开 ADR 裁决, 不要在一份"一致性规范"里悄悄引入分发机制变更。

### D-02 5.1 功耗 suspend/resume 应标为 Reserved

现状: 无系统 PM 框架, 0 个驱动实现 suspend/resume。规范 5.1 用"必须与系统 PM 框架协同"的措辞暗示这是现行要求, 风险是 codegen 或新驱动作者生成一堆返回 WINK_OK 的空壳函数 - 空壳 suspend 比没有 suspend 危险得多 (上层以为已省电/已停止输出)。

建议: 明确标注 Reserved (Phase N, 待 PM 框架 ADR), 并写"当前 MUST NOT 实现"。同时预留时需回答: suspend 期间是否保持输出? resume 后是否恢复 suspend 前的 duty/角度? 未 suspend 时调 resume 返回什么? 这三问不答清, 接口形态定不下来。

### D-03 5.2 回调签名与既有代码冲突, 需二选一

规范: dal_<type>_register_callback(dev, cb, void *user_data), 回调签名 (dev, event, user_data)。
现状: wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx), 另有 dal_button_set_irq_hook(fn, ctx)。

差异: 动词 (register_callback vs on_event), 上下文参数名 (user_data vs ctx), 且现状还有一个全局 (非 per-dev) 的 irq_hook 形态未被规范覆盖。

建议: 采用既有 on_event + ctx (改动小, 且 on_event 更短更符合 3 的动词简洁风格), 把 register_callback 从规范删除; 同时为 set_irq_hook 这类进程级钩子单列一节说明其与 per-device 回调的分工。并补 M-01.4 的回调上下文契约与"回调内禁止调用的 API 清单"。

### D-04 6.2 结构体追加兼容性与 apply_override 序列化冲突

6.2 说"新增内部状态位必须追加在末尾, 保证 {0} 初始化不受影响"。方向对, 但漏了两点:

1. 这条只覆盖 dal_<type>_t (句柄), 未覆盖 dal_<type>_config_t。而 config 才是真正有跨边界兼容性要求的: apply_override 按 config 成员顺序反序列化 (dal_dc_motor.h 注释 "Future serialization follows config member order")。config 的成员顺序是线格式的一部分, 必须比句柄更严格: MUST 只追加, MUST NOT 重排, 且 override 需要显式版本号/长度校验 (现签名是 (void *dev, const uint8_t *params, uint16_t len), 靠 len 隐式判版本是脆弱的)。
2. 这条与 2.1 的"成员按尺寸降序排列"直接矛盾 (追加末尾必然破坏降序)。规范内部必须裁决优先级: 兼容性 > 填充优化。

### D-05 apply_override 的 void *dev 是既有技术债, 规范应正视

    wink_status_t dal_rc_servo_apply_override(void *dev, const uint8_t *params, uint16_t len);
    wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);

这两处是 2.3"第一参数必须是 dal_<type>_t *dev"的真实违规点 (void * 擦除类型, 本质是为了统一函数指针表 - 即被 ADR-0004 禁止的运行时分发的残留)。规范应显式列入合规矩阵的例外并给出收敛计划, 而不是假装不存在。这也正好说明 D-01 的 control(void *arg) 会把这类债务规模化。

---

## 四. 文档工程与体系归属问题

### E-01 头文件 GBK 乱码 (Major, 实际缺陷)

除 dal_dc_motor.h (英文注释) 外, 以下头文件的中文注释在 UTF-8 下解码为乱码, 说明文件以 GBK/CP936 保存:

dal_rc_servo.h, dal_gps.h, dal_mono_oled.h, dal_button.h, dal_button_bal.h, dal_led.h, dal_encoder.h, dal_ultrasonic.h, dal_eeprom.h

影响: (a) GCC/Clang 在 -Wall 下可能对非法 UTF-8 字节报警, 两 target 表现不一致; (b) AI 与 codegen 读取头注释提取 Contract 元数据时得到乱码, 直接削弱本项目"AI 可读驱动"的核心能力; (c) git diff 与代码评审不可读。

同时 codegen/drivers/dc_motor.yaml 与 led.yaml 的中文注释也有乱码 (白名�? / 派生�?), 属同类问题。

建议: 规范补一条 源文件 MUST 为 UTF-8 无 BOM (可 lint, 成本极低), 并单开一个纯编码修复提交批量转换。

### E-02 API Contract 注释模板不足, 且未强制字段

现模板只有 Preconditions / Blocking / Thread-safe+ISR-safe / Error-codes 四项。基于上文缺口, 建议扩为强制 9 项:

    Preconditions / Postconditions / Range (值域与单位) / Blocking (含最坏 ms 数值) /
    Thread-safe / ISR-safe / Reentrancy / Simulation-parity / Error-codes

现状 rc_servo / mono_oled / ultrasonic / eeprom 已写了 Postconditions, 但 led / button / encoder 部分函数只有一行 @return, 完全没有 Contract 块。这属于可 lint 项 (公开 API 必须含 API Contract: 且必填字段齐全), 应纳入规则并给基线 allowlist。

### E-03 文档定位错误 (Major, 体系问题)

按 CLAUDE.md 的四层文档体系, 本文件当前混合了三种性质的内容:
- 设计真相 (2/3/6 的 API 契约与命名) -> 属于 (1) 设计规范, 应回写 docs/design/02-wink-micro-os/01-dal-device-abstraction.md (该文已有 3 DAL 接口设计规范 / 6 外设分类边界 / 7 Deinit 与 Bus-Owner 等对应章节, 会与本文重复甚至冲突)
- 尚未裁决的机制变更 (4 control 窗口, 5.1 PM, 5.2 回调) -> 应先各开 ADR
- 工具链约束 (7) -> 属于 codegen/lint 的 SSOT, 应引用而非复述

而 dal-development-guide/README.md 已声明"规范 SSOT 勿在本目录另起一套架构真相", 并把 DAL 架构与 API 契约的 SSOT 指向 01-dal-device-abstraction.md。本文件放在此目录并自称"DAL API 设计与一致性规范", 恰好违反了同目录 README 的规矩。

建议路径:
1. 本文件降级为 docs/tech-designs/unisim/2026-07-20-co-simulation-plugin-contract.md (技术设计规格)。
2. 有争议的三处机制各开 ADR (control 窗口 / PM / 回调与 ISR 契约)。
3. ADR Accepted 后, 把最终条款回写 01-dal-device-abstraction.md, 并在 dal-development-guide/ 只保留一份"如何遵循"的 Howto 摘要 + 指向活规范的链接。
4. 若希望它成为可执行门禁, 则同时产出 implementation-plans/ 的 lint 规则实施计划。

### E-04 缺少版本, 例外与生效范围机制

作为门禁类规范, 缺三样东西:
1. 规则 ID 化: 每条要求需 [DAL-x-nnn] 形式的稳定 ID, 便于 lint 报错与代码内 // lint-allow: DAL-C-003 (reason) 标注。引擎已有 engine/allowlist.py, 有基础设施。
2. 生效范围与迁移期: 新增驱动 error, 存量驱动 warning + 冻结基线。规范必须写清"存量豁免到何时, 由谁清账"。
3. 例外审批流程: 谁能批准偏离 (如 apply_override 的 void *dev), 记录在哪 (建议 YAML 里 lint_exceptions 字段, 而非散落在代码注释)。

---

## 五. 优点 (值得保留的部分)

避免评审只列问题, 以下设计判断是正确且有价值的:

1. 四维度分解框架 (通用契约 / 领域动词 / 扩展 / 演进兼容) 结构清晰, 保留。
2. 动词黑名单机制本身很有价值 - 对 AI 生成场景尤其有效, 只需把词表与现状对齐, 且给出语义解释而非仅禁词。
3. 6 的向后兼容三红线 (Init-to-Ready, 末尾追加, 保留同步 API) 方向正确, 是低代码平台的关键约束, 只需补充执行器安全例外与 config 序列化约束。
4. 强制 WINK_WARN_UNUSED_RESULT + wink_status_t 与 ADR-0001 一致, 且已在代码里高度落实 (仅 safe_off 应急路径与少量 void 函数需要例外裁决)。
5. 要求 config 内嵌为句柄首成员 + bool initialized, 与全部 10 个驱动一致, 是已验证的良好范式, 建议进一步补 _Static_assert(offsetof(dal_x_t, config) == 0)。
6. 强制 owner 字段用于静态资源认领, 与 device_tree / 资源冲突检测 (resource_conflict app) 联动, 是本项目的正确设计, 建议补"owner MUST 指向静态存储期字符串, MUST NOT 指向栈/堆"这一可 lint 条款。
7. 提出用 YAML 元数据驱动 lint 的思路正确, 只是 schema 写错了对象。

---

## 六. 现状合规矩阵 (建议直接纳入规范 v1.1)

图例: Y 合规 / N 不合规 / - 不适用 / ~ 部分合规

| 驱动 | init | deinit | safe_off | reset | get_state | 谓词返回 status | const getter | Contract 注释齐 | UTF-8 |
|------|------|--------|----------|-------|-----------|-----------------|--------------|------------------|-------|
| led | Y | ~ (YAML 缺 deinit_fn) | ~ (别名 off) | N | N | - | - | ~ | N |
| dc_motor | Y | Y | Y (=brake, ADR-0048) | N | N | - | Y | Y | Y |
| rc_servo | Y | Y | Y | N | N | - | - | Y | N |
| button | Y | Y | - | N | N | Y | Y | ~ | N |
| encoder | Y | Y | - | ~ (语义=zero) | N | - | Y | N | N |
| ultrasonic | Y | Y | - | N | ~ (state 在句柄内, 无 getter) | - | Y | Y | N |
| mono_oled | Y | Y | - | N | N | - | - | Y | N |
| eeprom | Y | Y | - | N | ~ (get_status) | - | Y | Y | N |
| gps | Y | Y | - | N | N | - | Y | Y | N |

统计: reset 0/9, get_state 0/9 严格合规, safe_off 仅 2 个真实实现, UTF-8 仅 1/9。

结论: 若 v1.0 条款按 error 上线, 9 个驱动全部报错。必须先做本矩阵 + 基线冻结, 再逐条提级。

---

## 七. 修订建议与优先级

### P0 (发布 v1.1 前必须完成)

1. 删除 4 统一 IOCTL 窗口, 或降级为 debug-only 并另开 ADR 裁决 (D-01)。
2. 修正谓词 API 返回值矛盾 (C-02): 统一 wink_status_t + bool *out_。
3. 重写 7.1 YAML 示例为真实 schema, 或改为引用 codegen SSOT + 锁定 codegen_schema 1.1 (C-06)。
4. 五大 API 拆 MUST/SHOULD/N.A., 承认 safe_off_fn: "" 的语义 (C-01)。
5. 删除或改写 Rule-DAL-03 (C-08), 明确 兼容性 > 填充优化 (D-04)。
6. 新增 并发与 ISR 契约 章 (M-01), 至少覆盖 volatile 非原子, 多字段撕裂, ISR-safe 白名单, deinit 竞态。
7. 新增 失效安全 章 (M-02), 裁决 safe_off 是否可失败, 并写死执行器 init 零能量。
8. 新增 阻塞与超时 章 (M-03), 把 WINK_BLOCKING / _blocking 后缀写成强制条款。
9. 修正 set_speed 示例值域 (M-04): 表格里的 80 改为 0.8f 并标注 [-1.0, 1.0]。
10. 加入 合规矩阵 + 迁移期 + 规则 ID 化 (E-04, 六)。

### P1 (v1.1 内应完成)

11. 新增 单位与量纲 章 (M-04) 与 Range 契约字段。
12. 把 request/poll/get_result 异步三段式升为一等公民 (M-05)。
13. 新增 stub / 裁剪禁用态契约 (M-06)。
14. 新增 双 target 可验证条款 (M-07), 含 dal 层禁 #ifdef 平台宏。
15. 5.1 PM 标 Reserved (D-02); 5.2 回调改为 on_event + ctx 并补上下文契约 (D-03)。
16. 补 const 正确性规则 (C-04) 与 owner 静态存储期规则。
17. 统一 Trait 与 category/role 的映射, 或删除 Trait 概念 (C-05)。
18. 修正 3.2 黑名单, 确立 read_ (触发采样) vs get_ (读缓存) 语义 (C-03)。

### P2 (后续)

19. 修复 9 个头文件与 2 个 YAML 的 GBK 乱码, 加 UTF-8 lint (E-01)。
20. 文档归位: 降为 tech-design, 决策开 ADR, 结论回写 01-dal-device-abstraction.md (E-03)。
21. 勘误 ADR-0046 与 CLAUDE.md 里的过期路径 (C-06, C-07)。
22. 扩充 Contract 注释模板为 9 必填字段并纳入 lint (E-02)。
23. apply_override 的 void *dev 列入例外并给收敛计划 (D-05)。

---

## 八. 建议的后续 ADR

| 编号 (建议) | 主题 | 需裁决的问题 |
|------|------|--------------|
| ADR-0053 | DAL 器件特有能力的表达形态 | 是否允许 control(cmd, void*); 若否, device_specific typed API 如何被 codegen 标记与排除 |
| ADR-0054 | DAL 并发与 ISR 安全契约 | volatile 使用边界; 多字段快照一致性方案 (seqlock vs 原子快照 vs 读序契约); WINK_ISR_SAFE 属性; 回调上下文; deinit 竞态顺序 |
| ADR-0055 | safe_off 的失效安全语义 | 是否 void 化; 是否要求 ISR-safe/idempotent; 系统级 safe-all 由谁遍历 (actuator registry) |
| ADR-0056 | 器件级功耗模型 (Reserved) | suspend 期间输出状态; resume 语义; 与系统 PM 框架的关系 (可延后) |

---

## 九. 评审员总结

这份文档展现了对一致性问题的正确直觉, 也抓住了低代码/AI 生成平台最痛的点 (命名碎片化会直接降低 AI 生成正确率)。但它当前的失效模式很典型: 用大量篇幅规范了"最容易写的部分"(动词拼写), 回避了"最难但最关键的部分"(并发, 时序, 安全, 量纲), 同时因为没有以现存代码和既有 SSOT 为基准, 制造了第二真相源。

三条最重要的意见:

1. 规范必须描述并约束现实, 而不是描述理想。请先做合规矩阵, 再定 MUST。凡是 9/9 不合规的条款, 要么改条款, 要么给迁移计划, 不能两者皆无。
2. 删掉 control(cmd, void*)。这是全文唯一会造成长期架构损害的设计, 它会在两年内把 DAL 一致性和 codegen 可分析性一起吃掉, 而它想解决的"零散 API"问题其实不是问题。
3. 补齐并发与失效安全两章。代码里已有 volatile 跨核共享和四字段撕裂风险, 而规范只用一行注释模板打发。对一个要跑电机和舵机的运行时, 这是安全相关缺失, 优先级高于任何动词表。

修订后 (完成 P0) 可评为通过。
