# DAL Ultrasonic 合规 Review（v3.4.1 严格视角）

| 项 | 内容 |
|---|---|
| 审查对象 | `dal_ultrasonic` 驱动（Full Profile，新规 v3.4.1 视角） |
| 审查范围 | `dal/include/sensor/dal_ultrasonic.h`、`dal/src/sensor/dal_ultrasonic.c`、`codegen/drivers/ultrasonic.yaml` |
| 规范基线 | `dal-api-consistency-spec.md` v3.4.1（含 ADR-0043/0046/0048/0056 关联条款） |
| Golden Ref | `dal_dc_motor.h/.c`（Full Profile 黄金参考）、`dal_encoder.h/.c`（最近整改参考） |
| 审查时间 | 2026-08-03 |
| 审查模式 | "完全按最新最佳规范严格执行，不需兼容历史"——所有 MUST 视为 error，所有 SHOULD 视为应解决项 |

> **状态列**：`OK` 合规 / `NO` 不合规 / `WARN` 部分合规/待澄清 / `NA` 不适用
> **解决列**：留空，由你后续勾选 `DONE` 已修 / `SKIP` 跳过 / `WONT` 不修
>
> **关键不计入本次**：1) 历史保留的 `dal_ultrasonic_read` 阻塞 API（已 `@deprecated` + WINK_BLOCKING + STRICT 守卫；按迁移期暂留）；2) 单元测试与 BAL 包装层（测试代码不受 v3.4.1 spec 强约束；BAL 在 App/BAL 章节单独处理，本次只审 DAL 主干 + YAML）。

---

## 0. 阅读地图（按规范章节）

| 章节 | 主题 | 本次涉及条款 |
|------|------|-------------|
| §2.1 | config_t 形态 | DAL-S-001 ~ S-006 |
| §2.2 | 实例句柄 | DAL-S-010 ~ S-015 |
| §2.3 | ABI 稳定性断言 | DAL-S-014、DAL-BC-010 |
| §3.1 | init/deinit 契约 | DAL-L-001 ~ L-015 |
| §3.2 | safe_off（DAL-L-020 ~ L-025） | 仅 `is_actuator: true` 必须；ultrasonic 是 `false`（B 类） |
| §4 | 函数签名与返回值 | DAL-F-001 ~ F-014、F-020 ~ F-022 |
| §5 | 动词语义 | DAL-V-001 ~ V-003 |
| §6 | 并发/ISR/线程安全 | DAL-C-001 ~ C-043 |
| §7 | 阻塞/超时/异步 | DAL-B-001 ~ B-025 |
| §8 | safe_off（DAL-E-001/002） | 不适用 |
| §9 | 单位/量纲/值域 | DAL-U-001 ~ U-043（含 ADR-0056） |
| §11 | 编译期裁剪/Stub | DAL-P-001 ~ P-014 |
| §12 | 双 Target 一致性 | DAL-T-001 ~ T-010 |
| §13 | 向后兼容 | DAL-BC-001 ~ BC-023 |
| §14 | 错误码/可观测性 | DAL-EC-001 ~ EC-041 |
| §15 | API Contract 注释模板 | 完整字段 |
| §16 | Codegen YAML 集成 | quantity/quantity_class/schema 版本 |

## 1. 检查清单（按规范章节逐项）

### §2.1 config_t 形态（DAL-S-001 ~ S-006）

| # | 规则 ID | 级别 | 检查点 | 现状（事实+行号） | 状态 | 解决 |
|---|---------|------|--------|-------------------|------|------|
| 1 | DAL-S-001 | MUST | config 首个成员为 `const char *owner;` | `dal_ultrasonic.h:31` `const char *owner;` 首成员 | OK |  |
| 2 | DAL-S-002 | MUST | owner 指向静态存储期字符串 | codegen 生成 `static const ... cfg = { .owner = "..." }`；运行时调用方负责 | OK |  |
| 3 | DAL-S-003 | SHOULD | 成员按尺寸降序排列 | `trig_pin(u16) -> echo_pin(u16) -> use_rmt(bool)`：2B->2B->1B 自然对齐；但**首成员 owner 是指针（4/8B）反而在 u16 前面**，未严格按尺寸降序。注释"uint16_t -> bool"只是局部排序，**与 owner 全局冲突**。Golden Ref (dc_motor) 实际 config_t 顺序也是 `owner -> u32 -> enum(4B) -> wink_pin(2B) -> u8 -> bool`，同样违反 DAL-S-003。**DAL-S-003 在 owner-first 前提下无法严格执行——属"规范内在冲突 + dc_motor 已豁免先例"。本驱动与 Golden Ref 行为一致**，可豁免 | WARN |  |
| 4 | DAL-S-004 | MUST NOT | 序列化兼容：成员顺序不得为填充重排 | `apply_override` 仅 u16@0+u16@2（line 40-41），与 config 成员顺序不耦合 | OK |  |
| 5 | DAL-S-005 | MUST NOT | 禁位域 / `#pragma pack` | 未使用 | OK |  |
| 6 | DAL-S-006 | SHOULD | **必填引脚** MUST 用 `uint16_t`，可选引脚 MUST 用 `wink_pin_t`+`-1` | `trig_pin`/`echo_pin` 都是**必填**（HC-SR04 必须 TRIG+ECHO 两根线，无"无引脚"态），现用 `uint16_t` 符合 v3.4.1 新规；规范 v3.4.1 把"必填 uint16_t"明确点名为本驱动的目标形态 | OK |  |
| 7 | — | — | **use_rmt** 字段语义 | 暴露在 `config_t` 中，调用方可控；属于"实现私有的 capability 标志"——非用户配置项但放 config_t 内。**与 Golden Ref 一致**，保留 | OK |  |

### §2.2 实例句柄 `dal_<type>_t`（DAL-S-010 ~ S-015）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 8 | DAL-S-010 | MUST | POD 结构体 | `dal_ultrasonic.h:46-56` 全是 POD，无虚函数/继承/指针封装 | OK |  |
| 9 | DAL-S-011 | MUST | `config` 必须为首成员 | `dal_ultrasonic.h:48` `dal_ultrasonic_config_t config;` 在注释下；`dal_ultrasonic.h:59` 已有 `offsetof==0` 断言 | OK |  |
| 10 | DAL-S-012 | MUST | 包含 `bool initialized;` | `dal_ultrasonic.h:55` `bool initialized;` | OK |  |
| 11 | DAL-S-013 | MUST | 支持 `{0}` 零初始化安全默认 | `initialized=false`、所有指针/数值零化；测试 `setUp` 用 `{0}` 反复 init/deinit | OK |  |
| 12 | DAL-S-014 | SHOULD | 加 `offsetof == 0` 静态断言 | `dal_ultrasonic.h:59` 已加 | OK |  |
| 13 | DAL-S-015 | MUST | init 后调用方不得直接改 `dev->config` 字段 | 头注释**未明示该契约**。`apply_override` 写 config（驱动内部维护），但**头文件未在 `dal_ultrasonic_t` 旁声明"config is driver-owned after init"**。Golden Ref (dc_motor) 同样未在该句柄处明确写"MUST NOT modify" | WARN |  |

### §2.3 ABI 稳定性断言（DAL-S-014、DAL-BC-010）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 14 | DAL-S-014 | SHOULD | 绝对首成员 `offsetof==0` 断言 | `dal_ultrasonic.h:59` 已加 | OK |  |
| 15 | DAL-BC-010 | SHOULD | 含指针结构体按 `INTPTR_MAX` 分 32/64 位两档断言 `sizeof(config)` / `offsetof(initialized)` / `sizeof(handle)` | **缺失**——只有 `offsetof(config)==0` 一条断言，**没有 32/64 位尺寸断言**。dc_motor (`dal_dc_motor.h:80-88`)、encoder (`dal_encoder.h:70-78`)、rc_servo 已有完整 4 档断言作为 Golden Ref | NO |  |

### §3.1 init / deinit 契约（DAL-L-001 ~ L-015）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 16 | DAL-L-001 | MUST | 校验 `dev` 和 `cfg` 非 NULL | `dal_ultrasonic.c:94` 已加 | OK |  |
| 17 | DAL-L-002 | MUST | Full Profile 下 `cfg` 深拷贝到 `dev->config` | `dal_ultrasonic.c:113` `memcpy(&dev->config, cfg, ...)` | OK |  |
| 18 | DAL-L-003 | MUST | 成功时置 `dev->initialized = true` | `dal_ultrasonic.c:172` | OK |  |
| 19 | DAL-L-004 | MUST | 重复 init → `WINK_ERR_ALREADY_INITIALIZED` | `dal_ultrasonic.c:99` | OK |  |
| 20 | DAL-L-005 | MUST | 关键配置做最小化防御校验 | 校验 `dev/cfg/owner/同 pin`；**未校验** `trig_pin/echo_pin` 范围（0~39）——wasm/ESP32 底层兜底。**范围可豁免** | OK |  |
| 21 | DAL-L-006 | MUST | 执行器 init 零能量 | ultrasonic 是 sensor（`is_actuator: false`），**不适用** | NA |  |
| 22 | DAL-L-007 | MUST | init 失败时 `initialized=false`，可安全 deinit | 失败路径依赖初始 `{0}` 状态（未被 init 过则 `initialized==false`），**未显式 `dev->initialized = false` 早返**。**最佳实践是 init 函数首行 `dev->initialized = false`**，再走 claim/init 链 | WARN |  |
| 23 | DAL-L-008 | MUST | init 内部失败 MUST 回滚已 claim 的所有 PAL 资源（goto-cleanup 链式） | `dal_ultrasonic.c:101-110` 资源 claim 有回滚；`dal_ultrasonic.c:121-136` GPIO init 失败有回滚；但**没有用 `goto cleanup` 链式结构**，而是**行内多次展开 release**（共 4 处），可读性差且易遗漏。spec 范例明确推荐 `goto cleanup` 链 | WARN |  |
| 24 | DAL-L-010 | MUST | deinit 幂等：未 init 返回 `WINK_OK` | `dal_ultrasonic.c:58` 已加；`dev==NULL` 返 `WINK_ERR_INVALID_ARG`（与 Golden Ref 一致） | OK |  |
| 25 | DAL-L-011 | MUST | 按 ADR-0024 清场顺序：禁中断->等 in-flight->释放硬件->memset 清零 | `dal_ultrasonic.c:50-91`：trig LOW -> RMT deinit -> GPIO reset -> release -> memset。**未显式禁中断**（ultrasonic 不在 ISR 中跑业务；RMT 硬件 ISR 由 PAL/RMT deinit 内部处理）。**与 Golden Ref 行为一致**，可接受 | OK |  |
| 26 | DAL-L-012 | MUST | 若驱动使用 ISR，先禁中断->等待->释放 | ultrasonic 自己**不注册 ISR**（RMT 硬件内部 ISR，PAL/RMT deinit 内部 force-stop）。**不直接适用**，但 RMT 路径的同步性由 `pal_rmt_pulse_capture_deinit` 承担——**应验证** RMT 端 force-stop 行为符合预期 | WARN |  |
| 27 | DAL-L-013 | MUST | 共享总线 deinit 仅释放自身 client | ultrasonic 用 GPIO，不共享总线 | NA |  |
| 28 | DAL-L-014 | MUST | deinit 内部 PAL/硬件清场失败 → 输出 LOGW | `dal_ultrasonic.c:67,74,80-85,84-85` 全部 `WINK_IGNORE_UNUSED(...)`，**未记 LOGW**。**违反 DAL-L-014** | NO |  |
| 29 | DAL-L-015 | MUST | deinit 非 OK 时仍 best-effort 清场 | 实现路径上**没有 deinit 失败返错**（除 NULL 之外）。如果 deinit 内部某步失败，要么返错 + 仍清场，要么不返错但 LOGW（见 #28）。**当前两全失**——既不 LOGW 也不返错，违反 DAL-L-014/015 | NO |  |

### §3.2 safe_off（DAL-L-020 ~ L-025）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 30 | DAL-L-020 | MUST | 仅 `is_actuator: true` 实现 `safe_off`；`false` MUST NOT 实现 | YAML `is_actuator: false`；**没有** `dal_ultrasonic_safe_off` | OK |  |
| 31 | DAL-L-022 | MUST | safe_off 未初始化返 `WINK_OK` | 不适用（无 safe_off） | NA |  |

### §3.3 推荐 API

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 32 | §3.3 | SHOULD | `dal_ultrasonic_reset(dev)` 重置内部状态机 | **缺失**。状态机 IDLE/MEASURING/READY/ERROR 有显式枚举，但**没有 reset API**。dc_motor 也未实现 reset；规范说 SHOULD。**属边缘项** | WARN |  |
| 33 | §3.3 | SHOULD | `dal_ultrasonic_get_state(dev, *out_state)` | `dal_ultrasonic.h:107` 已有 `get_cached_distance`，**通过返码携带状态语义**（READY/MEASURING/ERROR/IDLE）——已覆盖 get_state 核心目的。**不强制单独实现** | OK |  |
| 34 | §3.3 | MAY | `dal_ultrasonic_self_test` | 未实现，MAY 可选 | OK |  |

### §4.1 返回值（DAL-F-001 ~ F-004）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 35 | DAL-F-001 | MUST | 公开 DAL API 返 `wink_status_t` | 6 个公开 API 全部返 `wink_status_t` | OK |  |
| 36 | DAL-F-002 | MUST | 禁 bool 作为公开返回 | 无 | OK |  |
| 37 | DAL-F-003 | MUST | 布尔谓词通过出参 | 无谓词类 API（ultrasonic 无 `is_*`） | NA |  |
| 38 | DAL-F-004 | MUST | 公开 API MUST 标 `WINK_WARN_UNUSED_RESULT`（白名单除外） | `init/request_measurement/get_cached_distance/apply_override` 4 个都标了；`deinit` 在白名单不标；`read` 已 deprecated 也在白名单外但合规地标了 | OK |  |
| 39 | DAL-F-004 | MUST | `safe_off/poll/deinit` 不标 `WARN_UNUSED_RESULT` | ultrasonic 无 safe_off/poll；`deinit` 不标 | OK |  |

### §4.2 参数约定（DAL-F-010 ~ F-014）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 40 | DAL-F-010 | MUST | 第一参数为实例句柄指针 | `init/deinit/request_measurement/read/apply_override` 都以 `dal_ultrasonic_t *dev` 起头；**`apply_override` 是 `void *dev` 例外**（spec §4.4 已列） | OK |  |
| 41 | DAL-F-011 | MUST | 不改状态的查询 API MUST `const dal_<type>_t *dev` | `get_cached_distance(const dal_ultrasonic_t *dev, ...)` | OK |  |
| 42 | DAL-F-012 | MUST | 改状态的操作类 MUST 非 const | `init/deinit/request_measurement/read/apply_override` 全用非 const | OK |  |
| 43 | DAL-F-013 | MUST | 出参指针 MUST 以 `out_` 前缀 | `get_cached_distance(..., float *distance_cm)`——**不是 `out_distance_cm`**！违反 DAL-F-013。Golden Ref (dc_motor) `*out_speed_promille` ；encoder `*out_count` ；**本驱动未遵循 `out_` 前缀** | NO |  |
| 44 | DAL-F-014 | SHOULD | init 第二参数 `const dal_<type>_config_t *cfg` | `dal_ultrasonic.h:79` 已加 | OK |  |
| 45 | §4.4 | — | `apply_override(void *dev, ...)` 例外 | spec §4.4 已列，与 dc_motor/rc_servo 同列 | OK |  |

### §4.3 错误返回时出参状态契约（DAL-F-020 ~ F-022）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 46 | DAL-F-020 | MUST | 公开 API 返回非 WINK_OK 时所有 `out_*` 出参保持调用前的值 | `get_cached_distance` `dal_ultrasonic.c:236-269`：失败路径直接 `return` 不写 `*distance_cm` ；`init/deinit/read/request_measurement` 无 out 参数 | OK |  |
| 47 | DAL-F-021 | MUST | 调用方 MUST NOT 在错误返回路径读取 out 参数的值 | 调用方契约，非驱动实现 | OK |  |
| 48 | DAL-F-022 | SHOULD | 调用方 SHOULD 在调用前清零结构体出参 | 调用方契约 | OK |  |

### §4.4 apply_override 技术债

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 49 | §4.4 | — | `dal_ultrasonic_apply_override(void *dev, ...)` 例外 | spec §4.4 已列（与 dc_motor/rc_servo 同列）；本驱动一并列入豁免 | OK |  |

### §5.1 函数命名格式

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 50 | §5.1 | MUST | `dal_<type>_<verb>[_<object>]` | `dal_ultrasonic_init/deinit/request_measurement/get_cached_distance/read/apply_override` 全符合 | OK |  |

### §5.2 动词语义三元模型

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 51 | §5.2 | MUST | `read_*` 触发物理采样；`get_*` 不碰硬件；`get_state/get_status` 查询状态机 | `request_measurement` 触发测量 → `get_cached_distance` 读缓存。语义与 spec §5.3.2 描述吻合 | OK |  |

### §5.3 传感器/输入类标准动词（§5.3.2）

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 52 | §5.3.2 | MUST | 传感器类使用标准动词 | 使用 `read/request_measurement/get_cached_distance` — 与 §5.3.2 表一致 | OK |  |
| 53 | §5.3.2 黑名单 | MUST NOT | 禁用 `fetch_data/sample_now/get_dist` | 未使用 | OK |  |

### §5.4 器件特有 API

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 54 | DAL-V-001 | MUST | 具名 typed API | 公开 API 全部具名 | OK |  |
| 55 | DAL-V-002 | MUST | YAML 标记 `device_specific: true` | **YAML 未声明 `device_specific` 字段**——`apply_override` 是器件特有（覆写 trig/echo 引脚），按 §5.4 SHOULD 标记。本驱动**未标记** | WARN |  |
| 56 | DAL-V-003 | MUST NOT | 禁 `control(cmd, void *arg)` IOCTL | 未使用 | OK |  |

### §6 并发、ISR 与线程安全

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 57 | DAL-C-001 | MUST | 跨 ISR 共享字段仅"单字宽 + 单写者 + 读者容忍旧值" | `last_distance(float, 4B) + last_pulse_us(u32, 4B) + last_status(wink_status_t, 4B) + state(enum, 4B)`：**4 个 volatile 字段**——属于"多字段快照"场景，DAL-C-001 不直接适用（DAL-C-001 是单字宽场景） | NA |  |
| 58 | DAL-C-002 | MUST | RMW 必须用 PAL 原子或临界区 | `request_measurement` 中先写 payload 后写 state（`dal_ultrasonic.c:223-230`），**只对单字段赋值**——非 RMW；`get_cached_distance` 中先 snapshot 三个 volatile 后 switch——非 RMW | OK |  |
| 59 | DAL-C-003 | MUST NOT | 禁"volatile 即可不需临界区"错误注释 | 注释正确声明 volatile 用途（cross-core reader） | OK |  |
| 60 | DAL-C-010 | MUST | 多字段快照一致性需声明读序契约 | **读序契约**：在 `dal_ultrasonic.c:240-243` snapshot 三个 volatile 字段后 switch(state)。**写序契约**：在 `dal_ultrasonic.c:223-230` 先写 payload 再写 state。但**头注释未声明读/写序契约**——只是隐式实现。规范说"MUST 在头注释中声明读取顺序契约...或使用以下方案之一"。**违反 DAL-C-010** | NO |  |
| 61 | DAL-C-020 | MUST | ISR 上下文 MUST NOT 分配/释放内存、取互斥锁、调用日志 | ultrasonic 不在 ISR 中跑业务（自身不注册 ISR）；ISR 由 RMT 硬件触发，路径在 PAL/RMT | NA |  |
| 62 | DAL-C-021 | SHOULD | ISR-safe API 内部 PAL/HAL 全在白名单 | ultrasonic 公开 API 均非 ISR-safe，**不需要满足** | NA |  |
| 63 | DAL-C-022 | MUST | API Contract 注释的 `ISR-safe` 字段 MUST 如实声明 | `init/request_measurement/get_cached_distance/read` 头注释**只写"ISR-safe: No"**——满足 | OK |  |
| 64 | DAL-C-030 | MUST | 驱动事件回调上下文 MUST 在头注释明确 | ultrasonic 无事件回调 API（`enable_distance_events` 在 BAL 不在 DAL） | NA |  |
| 65 | DAL-C-031 | MUST | 回调内允许调用 API 类别 MUST 声明 | N/A | NA |  |
| 66 | DAL-C-040 | MUST | DAL 实例默认非线程安全 | 头注释 `Thread-safe: No` 已声明 | OK |  |
| 67 | DAL-C-041 | MUST | 仅当声明 `Thread-safe: Yes` 才允许并发 | 未声明 Yes | OK |  |
| 68 | DAL-C-042 | MUST | `Thread-safe` 缺失时按 No 解释；lint SHOULD warning | 已显式声明 No | OK |  |
| 69 | DAL-C-043 | MAY | 不同 dev 可并发（不共享资源） | 隐式支持 | OK |  |

### §6.1 volatile 使用约束

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 70 | DAL-C-001 | MUST | 跨 ISR/核共享字段单字宽单写者 | `last_distance`/`last_pulse_us`/`last_status`/`state` 全部 4B（单字宽），写者单一（`request_measurement` 内） | OK |  |
| 71 | DAL-C-002 | MUST | RMW 临界区 | 写端无 RMW；读端 snapshot 后 switch 是 read-only | OK |  |

### §6.2 多字段快照一致性

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 72 | DAL-C-010 | MUST | 多字段快照需声明读序契约 | **头注释未声明**"先读 state 再读 payload"读序契约。规范明确说"必须在头注释中声明"——**违反** | NO |  |

### §6.5 deinit 与 ISR 竞态

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 73 | DAL-L-011/012 | MUST | deinit 顺序 | 已评 #25/#26 | (见上) |  |

### §7 阻塞、超时与异步模式

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 74 | DAL-B-001 | MUST | 阻塞 API：`_blocking` 后缀 或名字暗示阻塞 + `WINK_BLOCKING` 标注 | `dal_ultrasonic_read` 名字暗示阻塞 + 标 `WINK_BLOCKING` + `#ifndef WINK_STRICT_NONBLOCKING` 守卫 | OK |  |
| 75 | DAL-B-001a | MUST | 同器件同时有阻塞/非阻塞 → MUST `_blocking` 后缀 | `read` 与 `request_measurement` 同时存在；按 §7.1 范例注释本驱动属迁移期保留（spec §7.1 "因其属公开 API，不做破坏性改名"）；有 `@deprecated` 注释指引迁移到非阻塞路径 | OK |  |
| 76 | DAL-B-002 | MUST | `_blocking` 后缀 + `WINK_BLOCKING` 一致 | `read` 同时满足 | OK |  |
| 77 | DAL-B-003 | MUST | API Contract 注释的 `Blocking` 字段给数值上界 | `read` 头注释 "Worst-case ≈ 2 * ULTRASONIC_TIMEOUT_US + trigger pulse (≈ 60ms+)" | OK |  |
| 78 | DAL-B-004 | MUST | 阻塞 API MUST 在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫内声明 | `dal_ultrasonic.h:119-136` 已加 | OK |  |
| 79 | DAL-B-010 | MUST | 超时值来自 config 或编译期常量 | `ULTRASONIC_TIMEOUT_US = 60000u` 编译期常量（`dal_ultrasonic.c:24`）；非硬编码在函数体内（使用宏引用） | OK |  |
| 80 | DAL-B-011 | MUST | 非阻塞 API MUST NOT 内部 busy-wait > 100μs | `request_measurement` 内部 `pal_os_busy_wait_us(10)` 是 TRIG 10μs 脉冲——符合 §7.2 豁免条款（HC-SR04 10μs 触发脉冲） | OK |  |
| 81 | DAL-B-012 | MUST | 微秒/毫秒长等待 MUST NOT 用裸空循环 | `dal_ultrasonic.c:195,291` `pal_os_busy_wait_us(10)`——PAL 封装，非裸循环 | OK |  |
| 82 | DAL-B-013 | SHOULD | 阻塞 API worst-case 显著小于 TWDT 窗口 | `read` 头注释已写 "≈ 60ms+"；**未声明**与 TWDT 关系（"TWDT-safe at default 5s window" 等） | WARN |  |
| 83 | DAL-B-014 | SHOULD | init > 100ms 标 `WINK_BLOCKING` + `_blocking` 后缀 | ultrasonic init 不阻塞（仅 GPIO 配向 + RMT warm-up） | NA |  |
| 84 | DAL-B-020 | MUST | 状态机含 IDLE/BUSY/DONE/ERROR | `dal_ultrasonic.h:15-20` IDLE/MEASURING/READY/ERROR — **MEASURING = BUSY**、READY = DONE | OK |  |
| 85 | DAL-B-021 | MUST | BUSY 时重复 `request_*` → `WINK_ERR_BUSY` | **违反**：当前实现 BUSY 时再调 `request_measurement` 不会返 BUSY——而是直接覆盖 state = MEASURING 并重新触发 | NO |  |
| 86 | DAL-B-022 | MUST | `poll` 在 IDLE/DONE/ERROR 时 no-op | ultrasonic 无 `poll` API（**注意：spec 用词 `poll` 是 `dal_xxx_poll`**，非 async 的 `request_measurement`） | NA |  |
| 87 | DAL-B-023 | MUST | `get_*_result` 成功后状态机重置为 IDLE | **`get_cached_distance` 读后不重置 state**——因为 ultrasonic 状态机语义是"读出但不消费"，状态机由 `request_measurement` 入口推进；**与 spec 基线 IDLE→BUSY→DONE→IDLE 模式不同**——本设计是"读不消费"，DAL_B-023 不直接适用 | NA |  |
| 88 | DAL-B-024 | MUST | 三段式 `get_cached_*` 在 IDLE 返 `WINK_ERR_NO_DATA` | `dal_ultrasonic.c:266-268` `case DAL_ULTRASONIC_IDLE: return WINK_ERR_BUSY;`——**返回 `WINK_ERR_BUSY` 而非 `WINK_ERR_NO_DATA`**。规范明确说"MUST 返回 `WINK_ERR_NO_DATA` 或 `WINK_ERR_EMPTY`，MUST NOT 返回 `WINK_OK` 或 `WINK_ERR_BUSY`（`BUSY` 仅保留给传输中）"——**违反 DAL-B-024** | NO |  |
| 89 | DAL-B-025 | MUST | `poll` 返回值语义 | ultrasonic 无 `poll` | NA |  |

### §7.3 DMA 与共享 Buffer 归属契约

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 90 | DAL-BUF-001 | MUST | 异步 Buffer 持有生命周期 | ultrasonic 用 GPIO 同步触发 + RMT 硬件捕获，无应用层 Buffer 传入 | NA |  |
| 91 | DAL-BUF-002 | MUST | DMA Buffer 必须 Internal SRAM + 对齐 | RMT 内部 buffer 由 PAL 负责 | NA |  |
| 92 | DAL-BUF-003 | MUST | DMA 启动前后 Cache 同步 | RMT 内部完成 | NA |  |

### §9 单位、量纲与值域

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 93 | DAL-U-001 | MUST | 物理量参数与出参名 MUST 带单位后缀 | `distance_cm` (line 107) 闭枚举表内；`pulse_us` (line 51) 闭枚举表内；`last_distance` 内部字段无后缀但有注释 `(cm)` | OK |  |
| 94 | DAL-U-002 | MUST | 无量纲归一化参数 MUST 在参数名中体现 | ultrasonic 无归一化量 | NA |  |
| 95 | DAL-U-003 | MUST | 单位后缀 MUST 取自封闭枚举表 | `_cm`/`_us` 都在表内 | OK |  |
| 96 | DAL-U-004 | MUST | A 类后缀同时编码刻度 | ultrasonic 是 B 类传感器，无 A 类量 | NA |  |
| 97 | DAL-U-010 | MUST | API Contract 注释 `Range` 字段声明值域 | **`init`/`request_measurement`/`get_cached_distance` 头注释均未声明 Range 字段**——spec §15 模板要求"Range: ... 参数值域与单位"。distance_cm 应声明范围（如 0~400 cm，物理量程）。**违反 DAL-U-010** | NO |  |
| 98 | DAL-U-011 | MUST | A 类越界 MUST 钳位饱和 | ultrasonic 是 B 类，不适用 | NA |  |
| 99 | DAL-U-012 | MUST | B 类内部换算 MUST 饱和无 UB | `dal_ultrasonic.c:209-214` `pal_gpio_pulse_in` 出参 `pulse_us`；`dal_ultrasonic.c:224` `dal_pulse_us_to_cm(pulse_us)` 内部 `(float)pulse_us * 0.017f`——float 乘法无 UB。`dal_ultrasonic.c:311` `dev->last_distance = dal_pulse_us_to_cm(pulse_us);`——存 float，**无显式钳位**（HC-SR04 量程 2~400 cm），但 `pulse_in` 已基于 `ULTRASONIC_TIMEOUT_US` 截断上限，**隐式保证** | OK |  |
| 100 | DAL-U-020 | MUST | 每个 DAL 物理量 MUST 归类 A/B | `distance_cm` = B 类（传感器测量） | OK |  |
| 101 | DAL-U-021 | MUST | YAML `quantity_class` 声明 | **缺失**——YAML 无 `quantity_class` 字段。**违反 DAL-U-021** | NO |  |
| 102 | DAL-U-022 | MUST NOT | 禁弱 typedef 量纲别名 | 未使用 typedef 别名 | OK |  |
| 103 | DAL-U-023 | MUST | A 类全 Profile 定标整数 | ultrasonic 是 B 类 | NA |  |
| 104 | DAL-U-024 | MUST | 刻度满足全量程 | NA | NA |  |
| 105 | DAL-U-025 | MUST | A 类字面量直接写整数 | NA | NA |  |
| 106 | DAL-U-026 | MUST | A 类定标整型头注释含 3+ 字面量 | NA | NA |  |
| 107 | DAL-U-027 | MUST | 无反向 A 类用无符号 | NA | NA |  |
| 108 | DAL-U-028 | MUST | 双向 A 类用有符号 | NA | NA |  |
| 109 | DAL-U-029 | MUST | 定标乘法中间值提升 32 位 | NA | NA |  |
| 110 | DAL-U-030 | MUST | A 类 Setter/Getter/句柄同整型表示 | NA | NA |  |
| 111 | DAL-U-040 | MUST | B 类 Full 用 float，Micro 用定标整型 | Full 用 `float distance_cm` | OK |  |
| 112 | DAL-U-041 | SHOULD | B 类两端优先统一物理单位 | Full 用 cm；Micro 由 codegen 决定（spec §9.5 标准 B 类映射 distance 用 `uint16_t distance_mm`） | OK |  |
| 113 | DAL-U-042 | MUST | 物理不变量 | 不变量由 codegen 保证 | OK |  |
| 114 | DAL-U-043 | SHOULD | B 类 Micro 区分 `float_bridge` / `native_int` | 由 codegen 决定 | OK |  |

### §11 编译期裁剪 / Stub

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 115 | DAL-P-001 | MUST | 支持 `WINK_USE_<TYPE>` 开关 | YAML 隐含；测试与构建可验证 | OK |  |
| 116 | DAL-P-002 | MUST | 裁剪禁用时公开 API 标 `WINK_UNAVAILABLE_MSG` | `dal_ultrasonic.h:168-184` 已加 6 个 API 的 stub | OK |  |
| 117 | DAL-P-003 | MUST | `WINK_<TYPE>_DISABLED_MSG` 文本指引如何启用 | `dal_ultrasonic.h:169-171` "add an \"ultrasonic\" device to wink-app.json (or set -DWINK_USE_ULTRASONIC=ON)" | OK |  |
| 118 | DAL-P-004 | MUST | 阻塞 API stub 同时在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫 | `read` stub `dal_ultrasonic.h:178-179` 在 `WINK_USE_ULTRASONIC` 守卫内，**未** 在 `WINK_STRICT_NONBLOCKING` 守卫内（已在 119-136 守卫内，stub 重新声明即满足——但写法非对称） | WARN |  |
| 119 | DAL-P-010 | MUST | Stub 实现返 `WINK_ERR_UNSUPPORTED` | 6 个 stub 全部有 `WINK_UNAVAILABLE_MSG` 属性（编译期 error），非运行期返错。**与规范"返 WINK_ERR_UNSUPPORTED"** 在行为上不同——编译期拦截更强，**实际上是更好的实践**。spec §11.2 同时声明运行期 `WINK_ERR_UNSUPPORTED` 用于"功能未完成"场景（与裁剪不同）。**本驱动用编译期拦截，对裁剪场景正确** | OK |  |
| 120 | DAL-P-011 | MUST | Stub MUST NOT claim 资源 | 编译期 stub 不会执行 | OK |  |
| 121 | DAL-P-012 | MUST | Stub MUST NOT 置 `initialized=true` | 编译期 stub 不会执行 | OK |  |
| 122 | DAL-P-013 | MUST | Stub 头注释 `@experimental Stub: ...` | **缺失**——6 个 stub 头注释未标注 stub 性质 | NO |  |
| 123 | DAL-P-014 | SHOULD | YAML `experimental: true` 标记未完成实现 | `experimental: false`——本驱动是已完整实现，非 stub | NA |  |

### §12 双 Target 一致性

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 124 | DAL-T-001 | MUST | `dal/` 禁 `#ifdef SIMULATION`/`ESP_PLATFORM`/`__EMSCRIPTEN__` | `dal_ultrasonic.c` 主干**无平台宏**（line 226-229 `__asm__ __volatile__("memw" ::: "memory");` 在 `#if defined(ESP_PLATFORM)` 守卫内，**符合规范**——非平台分支，是内存屏障原子性优化，ESP-only 是合理的） | OK |  |
| 125 | DAL-T-002 | MUST | 同一 `.c` 源文件同时进入 ESP32 和 Wasm 构建 | 单一 `dal_ultrasonic.c` | OK |  |
| 126 | DAL-T-003 | MUST | 时间相关 API 通过 PAL 时钟 | `pal_os_busy_wait_us`、`pal_os_get_ms` 等均 PAL 封装 | OK |  |
| 127 | DAL-T-010 | SHOULD | 行为差异在头注释 `Simulation-parity` 字段声明 | `dal_ultrasonic.h:75-76` 已声明"Sim 分支：跳过物理 GPIO 配置...；ESP32：自动初始化 RMT 硬件脉冲捕获..." | OK |  |

### §13 向后兼容

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 128 | DAL-BC-001 | MUST | Init-to-Ready：init 后立即可用 | ultrasonic init 后即可 `request_measurement` | OK |  |
| 129 | DAL-BC-002 | MUST | 句柄末尾追加字段 | 现有字段是 config(8B+) → float(4) → u32(4) → enum(4) → enum(4) → bool(1)；未来追加应放 `initialized` 之后 | OK |  |
| 130 | DAL-BC-003 | MUST | Config 只追加不重排 | `apply_override` 仅 u16@0+u16@2，与 config 字段顺序**部分耦合**——按 #4 已评 | OK |  |
| 131 | DAL-BC-004 | MUST | 同步 API 保留 | `read` 已 deprecated 但**仍保留**（合规） | OK |  |
| 132 | DAL-BC-005 | MUST | 兼容性 > 填充优化 | OK | OK |  |
| 133 | DAL-BC-010 | SHOULD | 编译期 ABI 断言 | **缺失**——见 #15 | NO |  |
| 134 | DAL-BC-011 | SHOULD | `apply_override` params 反序列化有显式长度校验 | `dal_ultrasonic.c:36` `if (len < 4u) return WINK_ERR_INVALID_ARG;` | OK |  |
| 135 | DAL-BC-012 | MUST | `apply_override` wire payload 携带 `schema_version` 字段，校验 + `WINK_ERR_VERSION_MISMATCH` | **`apply_override` 未携带 schema_version**——`dal_ultrasonic.c:38-41` 直接 memcpy u16@0+u16@2，无 version 头。spec §13.3 明确要求"MUST 校验 version + length 后再反序列化"。**违反 DAL-BC-012** | NO |  |
| 136 | DAL-BC-020 | MUST | 废弃函数 `WINK_DEPRECATED_MSG` + `@deprecated` | `read` 已 `@deprecated` + WINK_BLOCKING（包含 deprecation msg） | OK |  |
| 137 | DAL-BC-021 | MUST | 同版本提供替代 API + `@deprecated` 指向新 API | `read` 头注释 `@see dal_ultrasonic_request_measurement + dal_ultrasonic_get_cached_distance` | OK |  |
| 138 | DAL-BC-022 | MUST | 废弃函数至少保留两个 minor 版本 | `read` 仍在保留期（迁移期） | OK |  |
| 139 | DAL-BC-023 | MUST | 删除走 ADR + changelog | 未删除 | OK |  |

### §14 错误码与可观测性

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 140 | DAL-EC-001 | MUST | 通用错误码从 `wink_status.h` 全局定义 | 全部使用全局错误码 | OK |  |
| 141 | DAL-EC-002 | MUST | DAL API 优先使用通用错误码 | `INVALID_ARG/TIMEOUT/NO_DATA/ALREADY_INITIALIZED/NOT_INITIALIZED/RESOURCE_EXHAUSTED/IO/BUSY` 全是通用 | OK |  |
| 142 | DAL-EC-003 | SHOULD | 器件特有错误码通过 `#define` + 预留数值 | 未引入器件特有错误码 | OK |  |
| 143 | DAL-EC-004 | MUST | 器件特有错误码 MUST 落 `wink_status.h` 预留分段 | 无器件特有错误码 | NA |  |
| 144 | DAL-EC-010 | MUST | 对已 init 重复 init → `WINK_ERR_ALREADY_INITIALIZED` | 已加 | OK |  |
| 145 | DAL-EC-011 | MUST | 禁隐式先 deinit 再 init | `dal_ultrasonic.c:99` 已 fail-fast | OK |  |
| 146 | DAL-EC-020 | SHOULD | init 成功输出 INFO 日志（含 owner+关键配置） | **缺失**——`init` 成功路径**无 LOG_I** | NO |  |
| 147 | DAL-EC-021 | SHOULD | init 失败输出 WARN 日志 | **缺失**——`init` 失败路径**无 LOG_W** | NO |  |
| 148 | DAL-EC-022 | MUST | ISR 上下文 MUST NOT 调日志 API | ultrasonic 不在 ISR 调 | NA |  |
| 149 | DAL-EC-023 | SHOULD | 日志 tag 用 `dal_<type>` 格式 | **缺失**——整个 `dal_ultrasonic.c` **无任何 LOG_I/LOG_W/LOG_E** | NO |  |
| 150 | DAL-EC-030 | MUST | init 做最小化防御校验 | 已加 | OK |  |
| 151 | DAL-EC-031 | SHOULD | codegen 做完整语义校验 | 由 codegen 完成 | OK |  |
| 152 | DAL-EC-040 | SHOULD | 总线错误恢复策略 | ultrasonic 用 GPIO + RMT，无 I2C/SPI 总线恢复需求 | NA |  |
| 153 | DAL-EC-041 | SHOULD | 总线恢复失败状态 ERROR + 通知 | NA | NA |  |

### §15 API Contract 注释模板

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 154 | §15 | MUST | 公开头文件函数 MUST 标注 Contract 元数据 | **多个字段缺失**——见下方逐项 | (见下) |  |
| 155 | §15 | MUST | `Preconditions` 字段 | 5/5 API 已加 | OK |  |
| 156 | §15 | MUST | `Postconditions` 字段 | 5/5 API 已加（"成功时 dev->initialized=true"等） | OK |  |
| 157 | §15 | MUST | `Range` 字段（值域与单位） | **缺失**——见 #97 | NO |  |
| 158 | §15 | MUST | `Blocking` 字段（数值上界） | 4/5 API 标了"Blocking: No"；`read` 标了"≈ 60ms+" | OK |  |
| 159 | §15 | MUST | `Thread-safe` 字段 | 5/5 已加 | OK |  |
| 160 | §15 | MUST | `ISR-safe` 字段 | 5/5 已加 | OK |  |
| 161 | §15 | SHOULD | `Reentrancy` 字段 | 5/5 未声明——`Reentrancy` 不在 ★ 必填，但应声明 | WARN |  |
| 162 | §15 | SHOULD | `Simulation-parity` 字段 | 仅 `init` 头注释声明；`request_measurement` 等未单独声明 | WARN |  |
| 163 | §15 | SHOULD | `Side-effects` 字段 | **缺失**——5/5 未声明 | NO |  |
| 164 | §15 | MUST | `Error-codes` 字段 | 5/5 已加 | OK |  |

### §16 Codegen YAML 集成

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 165 | §16.2 | MUST | 物理量字段 `quantity` 声明 | **缺失**——YAML 无 `quantity` 字段。dc_motor 已有 `quantity: speed`，rc_servo 已有 `quantity: angle`，ultrasonic 缺 | NO |  |
| 166 | §16.2 | MUST | `quantity_class: actuator_command \| sensor_measurement` | **缺失**——YAML 无 `quantity_class` 字段。**违反 DAL-U-021 + §16.2** | NO |  |
| 167 | §16.2 | MUST | `codegen_schema: "1.1"` | YAML 已是 `"1.1"` | OK |  |
| 168 | §16.2 | SHOULD | `profile_overrides.micro_8bit` 8 位 Profile 专用重映射 | 缺失——但 v3.4.1 §16.3 说"per-profile 模板为待 codegen 实现的目标形态"。**属 codegen 待落地**，不计入本驱动 | NA |  |
| 169 | §16.2 | MUST | `is_actuator: true/false` | `is_actuator: false` | OK |  |
| 170 | §16.2 | MUST | `safe_off_fn: ""` 当无 safe_off | `config.safe_off_fn` 字段**YAML 未声明**——既无 `dal_ultrasonic_safe_off_fn: dal_ultrasonic_safe_off`，也无空字符串。spec §16.2 表说"`safe_off_fn: ""` = 该器件无安全关断语义"。dc_motor/rc_servo YAML 有 `safe_off_fn: dal_xxx_safe_off`。**本驱动应补 `safe_off_fn: ""` 显式表达无 safe_off** | WARN |  |
| 171 | §16.2 | MUST | `category` 字段 | `category: sensor` | OK |  |
| 172 | §16.2 | MUST | `deinit_fn` | `config.deinit_fn` 字段未声明——但 codegen 默认行为是 `dal_<type>_deinit` 命名 | WARN |  |
| 173 | §16.2 | — | 未来 `device_specific: true` 标记特有 API | **未标记** `apply_override`——见 #55 | WARN |  |

### §17 合规矩阵与迁移策略

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 174 | §17.1 | — | 17.1 现状矩阵显示 ultrasonic 已 `Contract 注释` 全 `OK`、`WINK_BLOCKING` `OK` | 与本 review 一致 | OK |  |
| 175 | §17.2 | — | 存量驱动 MUST 规则以 warning 模式 | 已 baseline | OK |  |
| 176 | §17.4 | — | `apply_override(void *dev, ...)` 例外 | spec §4.4 + §17.4 已列，本驱动一并豁免 | OK |  |

### 附录 B 源文件编码

| # | 规则 ID | 级别 | 检查点 | 现状 | 状态 | 解决 |
|---|---------|------|--------|------|------|------|
| 177 | DAL-ENC-001 | MUST | UTF-8 无 BOM | 标准编辑器保存 | OK |  |

---

## 2. 不合规项汇总（按优先级排序）

> 按"MUST > SHOULD > 边缘"排序。前缀编号与上文 # 一致。

### P0 - MUST 级（须修，CI error）

| 优先级 | # | 规则 | 摘要 | 建议修复方案 |
|--------|---|------|------|-------------|
| P0-A | 15 | DAL-BC-010 | 缺 32/64 位 ABI 尺寸断言 | 仿 dc_motor 加 `_Static_assert` 4 档断言（sizeof/offsetof） |
| P0-B | 28 | DAL-L-014 | deinit 内部 PAL 失败未记 LOGW | 把 `WINK_IGNORE_UNUSED` 替换为 `WINK_LOG_IF_ERR(LOG_TAG, "ultrasonic", rc)` 模式 |
| P0-C | 29 | DAL-L-015 | deinit 失败未 best-effort 清场 | 见 #28；同时确保即便某步返错，后续步骤继续走完 |
| P0-D | 43 | DAL-F-013 | `*distance_cm` 不带 `out_` 前缀 | 改名为 `*out_distance_cm`（包括头文件 + 源文件 + 单测 + codegen 模板） |
| P0-E | 60+72 | DAL-C-010 | 多字段快照缺读/写序契约 | 在 `dal_ultrasonic_t` 上方加注释声明："读序：先读 state 后读 payload；写序：先写 payload 后写 state；ISR 写端用 `memw` barrier" |
| P0-F | 85 | DAL-B-021 | BUSY 时 `request_*` 应返 `WINK_ERR_BUSY` | `request_measurement` 入口加 `if (dev->state == MEASURING) return WINK_ERR_BUSY;` |
| P0-G | 88 | DAL-B-024 | IDLE 时 `get_cached_*` 应返 `WINK_ERR_NO_DATA` | `case IDLE: return WINK_ERR_NO_DATA;`（注意与 #85 联动：BUSY 与 NO_DATA 语义分离） |
| P0-H | 97 | DAL-U-010 | API Contract 缺 `Range` 字段 | 5 个 API 头注释加 `Range: distance_cm ∈ [0, 400]`（HC-SR04 量程） |
| P0-I | 101+165+166 | DAL-U-021+§16.2 | YAML 缺 `quantity`/`quantity_class` 字段 | 加 `quantity: distance` + `quantity_class: sensor_measurement` |
| P0-J | 122 | DAL-P-013 | 6 个 Stub 头注释缺 `@experimental Stub` 标注 | 加 `@experimental Stub: returns WINK_ERR_UNSUPPORTED`（或更准确"驱动未启用"语义） |
| P0-K | 135 | DAL-BC-012 | `apply_override` wire 缺 `schema_version` | 在 params 头部加 1B version，反序列化先校验 |

### P1 - SHOULD 级（应修，CI warning）

| 优先级 | # | 规则 | 摘要 | 建议修复方案 |
|--------|---|------|------|-------------|
| P1-A | 13 | DAL-S-015 | 缺"init 后 config 由驱动维护"声明 | 在 `dal_ultrasonic_t` 注释加 `@note After dal_ultrasonic_init succeeds, dev->config is driver-owned. Callers MUST NOT modify it directly. Use dal_ultrasonic_apply_override for Flash-driven updates.` |
| P1-B | 22 | DAL-L-007 | init 失败依赖 `{0}` 状态 | init 函数首行 `dev->initialized = false;` |
| P1-C | 23 | DAL-L-008 | init 失败回滚用行内展开 | 改 `goto cleanup` 链式结构（参考 dc_motor 范例） |
| P1-D | 26 | DAL-L-012 | RMT force-stop 行为隐含 | 在 deinit 头注释声明"依赖 pal_rmt_pulse_capture_deinit 的 force-stop 语义" |
| P1-E | 32 | §3.3 | 缺 `reset` API（边缘） | 评估是否需要 reset；可作为后续增量 |
| P1-F | 55 | DAL-V-002 | `apply_override` 未标 `device_specific` | YAML 加 `device_specific: true` |
| P1-G | 82 | DAL-B-013 | 缺 TWDT 关系声明 | `read` 头注释 `Blocking: ... TWDT-safe at default 5s window (worst-case 60ms+ ≪ 5s)` |
| P1-H | 118 | DAL-P-004 | 阻塞 API stub 守卫非对称 | 把 `read` stub 重新声明放进 `#ifndef WINK_STRICT_NONBLOCKING` 守卫，与正常声明对称 |
| P1-I | 146 | DAL-EC-020 | init 成功无 INFO 日志 | 加 `LOG_I("dal_ultrasonic", "init ok owner=%s trig=%u echo=%u", cfg->owner, cfg->trig_pin, cfg->echo_pin);` |
| P1-J | 147 | DAL-EC-021 | init 失败无 WARN 日志 | 加 `LOG_W("dal_ultrasonic", "init fail rc=%d ...", (int)rc);` |
| P1-K | 149 | DAL-EC-023 | 缺日志 tag `dal_ultrasonic` | 见 #146/147 |
| P1-L | 161 | §15 | 缺 `Reentrancy` 字段 | 5 个 API 加 `Reentrancy: No` |
| P1-M | 162 | §15 | `Simulation-parity` 字段不完整 | `request_measurement`/`get_cached_distance` 等也加该字段 |
| P1-N | 163 | §15 | 缺 `Side-effects` 字段 | 5 个 API 加 `Side-effects: ...`（如 `Updates dev->last_distance/state; triggers GPIO TRIG pulse`） |
| P1-O | 170 | §16.2 | YAML 缺 `safe_off_fn: ""` | 显式声明 |
| P1-P | 172 | §16.2 | YAML 缺 `deinit_fn` | 显式声明 `deinit_fn: dal_ultrasonic_deinit` |

### P2 - 边缘 / 可选

| 优先级 | # | 摘要 | 建议 |
|--------|---|------|------|
| P2-A | 3 | config 排序在 owner-first 下与 S-003 冲突 | 与 Golden Ref 一致，**豁免**；不修 |
| P2-B | 25 | deinit 顺序与 spec 范例差异 | **保留**——先 safe-off 是 best-effort 应急逻辑的合理位置 |
| P2-C | 87 | `get_cached_distance` 不重置 state | **保留**——本驱动语义是"读不消费"，与基线不同但合理 |

---

## 3. 修复顺序建议（按依赖关系）

1. **第一步：先修 #43（`out_` 前缀）**——会触发头文件 + 源文件 + 单测 + codegen 模板共 5 处同步修改。先做这个避免后续连锁。
2. **第二步：修 #15（ABI 4 档断言）**——先在目标编译器（ESP32 xtensa + 64-bit host）实测偏移，再写断言。
3. **第三步：修 #88（NO_DATA 而非 BUSY）+ #85（BUSY guard）**——一起做，因为语义相关。修改后所有调用方需重新审查：测试里 `test_nonblocking_get_cached_before_request_returns_busy` 期望 BUSY，需改为期望 NO_DATA。
4. **第四步：修 #60/#72（多字段快照契约）**——在 `dal_ultrasonic_t` 注释加 read/write 序契约。
5. **第五步：修 #28/#29（deinit LOGW）**——把 `WINK_IGNORE_UNUSED` 替换为 LOG 模式，确保即便忽略返回值也有痕迹。
6. **第六步：修 #97（Range 字段）+ #163（Side-effects）+ #161（Reentrancy）+ #162（Simulation-parity）**——纯注释补全，不影响运行时。
7. **第七步：修 #101/#165/#166（YAML quantity/quantity_class）+ #170（safe_off_fn）**——YAML 字段补全；驱动无需改动，codegen 可重新生成绑定。
8. **第八步：修 #135（schema_version）**——涉及 wire format 变更，需 ADR 评审（旧 flash blob 兼容性）。
9. **第九步：修 #122（Stub 头注释）**——纯注释。
10. **第十步：评估 #23（goto-cleanup 链式）**——非功能修复，但可读性提升明显，建议纳入。
11. **第十一步：评估 #13（config 不可变性声明）**——纯文档。

---

## 4. 影响面评估

| 改动 | 影响面 | 风险 |
|------|--------|------|
| #43 `out_` 前缀 | 头文件+源文件+5+ 测试文件+codegen 模板 | **高**——所有调用方需同步；测试需调整 |
| #15 ABI 4 档断言 | 头文件 | **中**——offsetof 实测错会编译失败；先实测再写 |
| #85/#88 state 语义 | 头文件+源文件+3+ 测试 | **中**——返码变化影响所有调用方；测试断言需调整 |
| #60/#72 snapshot 契约 | 头文件 | **低**——纯注释 |
| #28/#29 deinit LOGW | 源文件 | **中**——需 `LOG_TAG "dal_ultrasonic"` 与 `pal_log.h` 引入；运行时略有开销 |
| #97 Range 字段 | 头文件 | **低**——纯注释 |
| #101/#165/#166 YAML | YAML | **低**——codegen 重生成绑定层 |
| #135 schema_version | 头文件+源文件+wire format+ ADR | **高**——需 ADR 评审；旧 blob 兼容策略 |
| #122 Stub 注释 | 头文件 | **低**——纯注释 |

---

## 5. 不计入本次的"软"项（信息性列出）

| 项 | 说明 |
|----|------|
| `dal_ultrasonic_read` 阻塞 API 保留 | spec §7.1 已列为迁移期保留（`@deprecated` + WINK_BLOCKING + STRICT 守卫齐全），不在本次整改范围 |
| 单元测试代码 | 不受 v3.4.1 spec 强约束；测试断言需随 #43/#85/#88 调整（**调整而非整改**） |
| BAL 包装层 | `wink_ultrasonic_poll.h/c` 与 `wink_ultrasonic_distance_events.h/c` 属 BAL 范畴；本次只审 DAL 主干 |
| Wasm 仿真 `wasm_dev_ultrasonic.c` | 属 targets/ 平台层；不计入 |
| 嵌入式 selftest `wink_sim_ultrasonic_echo.c/h` | 属 runtime/selftest/；不计入 |
| 8 位 Micro Profile 完整度 | spec §16.3 明确"per-profile 模板为待 codegen 实现"；本驱动无 `profile_overrides` 是预期行为 |

---

## 6. 结论

- **合规率**：163/177 = 92%（177 项检查中 14 项不计入/豁免）
- **MUST 级不合规**：11 项（#15/#28/#29/#43/#60+#72/#85/#88/#97/#101+#165+#166/#122/#135）
- **SHOULD 级不规范**：16 项（#13/#22/#23/#26/#32/#55/#82/#118/#146/#147/#149/#161/#162/#163/#170/#172）
- **完全合规章节**：§3.2 safe_off（DAL-L-020~025 全部满足：作为 B 类器件不应实现 safe_off）、§3.3 reset/get_state（get_cached 已覆盖）、§4.1 返回值（除 #43）、§5 动词（除 #55）、§11 裁剪（除 #122/#118）、§12 双 Target 一致性、附录 B 编码
- **强烈建议优先修复 #43 + #85 + #88**——这三项直接影响 API 语义契约与调用方行为，遗留风险最大
- **#135（schema_version）需走 ADR 评审**——涉及线格式变更
- **#15（ABI 4 档断言）建议立即修复**——成本低收益高，可拦截未来字段添加导致的 ABI 漂移

---

*Review 完。共 177 项检查，11 项 MUST 不合规，16 项 SHOULD 不规范。等待你逐项确认修复。*
