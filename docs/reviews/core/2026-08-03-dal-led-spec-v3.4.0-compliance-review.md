# DAL `led` 驱动对照规范 v3.4.0（ADR-0056）全面合规评审

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-08-03 |
| **评审范围** | `wink-micro-os/dal/include/output/dal_led.h`、`wink-micro-os/dal/src/output/dal_led.c`、`wink-micro-os/codegen/drivers/led.yaml`、`wink-micro-os/test/unit/dal/test_dal_led.c` 逐文件、逐规则审计 |
| **基准规范** | [`dal-api-consistency-spec.md` v3.4.0](../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md)（含 [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) 新增 §9 量纲两分类） |
| **适用前提** | **led 不需要向后兼容**，可按最新最佳规范直接整改（flag-day） |
| **关联 ADR** | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）、[ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md)（deinit 清场）、[ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)（执行器命名/safe_off 绑定）、[ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（量纲 A/B 两分类） |
| **对照样板** | [`dc_motor`](../../../../wink-micro-os/dal/include/actuator/dal_dc_motor.h)（v3.4.0 Golden Reference） |
| **结论状态** | **Resolved（2026-08-03 整改完成）**：6 项不合规已全部修复并通过 host 单测（12/12 PASS，-Werror 零警告）+ `wink lint`；F-5 经核验为**误报**（pin 为 uint16_t 不可能为负，上界由 pal_gpio_init 校验），已排除 |

---

## 1. 评审结论

`led` 是二值指示类输出（`category: output`，`is_actuator: true`，`experimental: false`，Full-only）。整体结构正确（POD、config 首成员、静态分发、裁剪 stub 完备、双 target 无平台宏）。审计发现 6 项不合规（F-1~F-4、F-6、F-7），按严重度：

- **❌ F-1（功能缺陷，P1）：init 不显式写 off 电平，违反 DAL-L-006 零能量契约。** `dal_led_init` 调 `pal_gpio_init(OUTPUT)` 后从不调用 `pal_gpio_write` 写熄灭电平。ESP-IDF `gpio_config(GPIO_MODE_OUTPUT)` 不设输出锁存器初值（复位默认 0），对 `active_high` 恰好熄灭，但对 **`active_low` 配置 LED 会在 init 后点亮**——"init 即通电"，正是 DAL-L-006 严禁的。**✅ 已修**。
- **❌ F-2（安全语义，P1）：safe_off 未初始化不返回 WINK_OK，违反 DAL-L-022。** YAML 原绑定 `safe_off_fn: dal_led_off`，而 `dal_led_off` 在 `!initialized` 时返回 `WINK_ERR_NOT_INITIALIZED`。safe_off 在 watchdog/panic/rollback 路径被 `safe_off_all` 调用时，未初始化是合法态，必须返回 WINK_OK。**✅ 已修**（新增独立 `dal_led_safe_off`）。
- **❌ F-3（可观测性，P2）：驱动零日志，违反 DAL-EC-020/021/023。** `.c` 原无 `LOG_TAG`、不 include `pal_log.h`。**✅ 已修**。
- **❌ F-4（deinit 可追溯，P2）：清场失败未 LOG_W，违反 DAL-L-014。** 原 `pal_resource_release` 返回值被 `WINK_IGNORE_UNUSED` 静默丢弃。**✅ 已修**。
- ~~**F-5（防御校验）：init 不校验 pin 范围**~~ —— **核验为误报**：`pin` 是 `uint16_t` 不可能为负，加 `<0` 检查触发 `-Werror=type-limits`；上界由 `pal_gpio_init` 校验并在失败时回滚。已排除。
- **❌ F-6（ABI 防护，P3）：缺 32/64 位 `sizeof`/`offsetof` 编译期断言，违反 DAL-BC-010/§2.3。** ✅ 已修（64 位实测 16/24/17；32 位 8/12/9 由编译期断言在真机 target 校验）。
- **❌ F-7（Contract 注释，P3）：缺 `Side-effects` 等字段。** ✅ 已修。

验证：host 单测 **12/12 PASS**（`-Wall -Wextra -Werror` 零警告），`wink lint --pack layering --pack api` 无 finding。另列观察项见 §4（O-1 已随 F-2 解决）。

> **评级图例**：✅ 合规 · ❌ 不合规（应修） · ⚠️ 部分合规 / 待落地 · ℹ️ 观察项（非硬伤） · N/A 不适用
> **末列"是否解决"留空**，整改后由执行者填写（✅ 已解决 / 日期 / commit）。

---

## 2. 完整合规检查清单（逐条规则，无遗漏）

### 2.1 数据结构与句柄（§2）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-S-001 | config 首成员 `const char *owner` | ✅ | `dal_led.h:22` | |
| DAL-S-002 | owner 指向静态存储期字符串 | ✅ | 头注释 `:22` 声明；init 校验非 NULL | |
| DAL-S-003 | 成员按尺寸降序（SHOULD） | ✅ | config：ptr(4/8)→uint16(2)→bool(1)，自然对齐无填充 | |
| DAL-S-004 | 序列化场景不重排成员 | N/A | 暂无 `apply_override`/wire 格式 | |
| DAL-S-005 | 禁位域 / `#pragma pack` | ✅ | 无 | |
| DAL-S-010 | 句柄为 POD | ✅ | `dal_led_t` `:36-40` | |
| DAL-S-011 | config 内嵌为首成员 | ✅ | `:37` + `_Static_assert(offsetof==0)` `:43` | |
| DAL-S-012 | 含 `bool initialized` | ✅ | `:39` | |
| DAL-S-013 | 支持 `{0}` 零初始化 | ✅ | 零值=安全默认态 | |
| DAL-S-014 | SHOULD 添加 `offsetof(config)==0` 断言 | ✅ | `:43` | |
| DAL-S-015 | init 后 config 不可变 | ✅ | 仅 init `:21` 写 config，之后不写 | |
| §2.3 / DAL-BC-010 | 按 `INTPTR_MAX` 32/64 分档的 `sizeof`/`offsetof` ABI 断言（SHOULD） | ❌ | **F-6**：仅有 `offsetof(config)==0`，缺整尺寸/`initialized` 偏移的分档断言（对照 dc_motor.h:82-90） | |
| DAL-S-020 | Full 下 init SHOULD NOT malloc | ✅ | 全程 PAL claim，无堆 | |
| DAL-S-021 | 若用堆须 Eager 声明 | N/A | 无堆 | |
| DAL-S-022 | Micro No-Malloc / Flash Zero-Copy | N/A | 当前 Full-only | |

### 2.2 生命周期（§3）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-L-001 | init 校验 dev/cfg 非 NULL | ✅ | `dal_led.c:7`；额外校验 owner NULL `:8` | |
| DAL-L-002 | 深拷贝 cfg→dev->config | ✅ | `memcpy` `:21` | |
| DAL-L-003 | 成功置 initialized=true | ✅ | `:23`（硬件成功之后，正确） | |
| DAL-L-004 | 重复 init 返回 ALREADY_INITIALIZED | ✅ | `:9` | |
| DAL-L-005 | 最小防御校验（含引脚范围） | ❌ | **F-5**：校验 dev/cfg/owner/initialized，但**不校验 `cfg->pin` 范围**（负 pin 被强转 uint32 传入 claim）。对照 dc_motor.c:77 校验 `dir_pin_a < 0` | |
| DAL-L-006 | 执行器 init 后零能量，严禁 init 即通电 | ❌ | **F-1**：init `:15` 调 `pal_gpio_init(OUTPUT)` 后**从不写 off 电平**；ESP32 `gpio_config` 输出锁存器复位默认 0，`active_low` 配置下 LOW=点亮，LED init 即通电 | |
| DAL-L-007 | 失败回 initialized=false 可 safe-deinit | ✅ | claim/gpio 失败均在置位前返回，dev 未被污染 | |
| DAL-L-008 | init 失败资源链式回滚 | ✅ | `pal_gpio_init` 失败时 release claim `:17`；单一 GPIO 资源，回滚完整 | |
| DAL-L-010 | deinit 幂等（未 init 返回 OK） | ✅ | `:56` | |
| DAL-L-011 | 清场顺序：停输出→硬件 reset→释放资源→清零 | ✅ | off→`pal_gpio_reset_pin`→`pal_resource_release`→`memset` `:59-74` | |
| DAL-L-012 | 用 ISR 时先禁中断等 in-flight | N/A | 无 ISR | |
| DAL-L-013 | 共享总线只释放自身 client | N/A | 独占 GPIO | |
| DAL-L-014 | 底层清场失败 MUST LOG_W | ❌ | **F-4**：`pal_resource_release` 返回值被 `WINK_IGNORE_UNUSED` `:71` 静默丢弃，无 LOG_W（对照 dc_motor `release_resource_logged`） | |
| DAL-L-015 | deinit best-effort + initialized=false | ✅ | 恒返回 WINK_OK，`memset` 清零 `:74` | |
| DAL-L-020 | is_actuator:true MUST 有 safe_off | ✅ | YAML `safe_off_fn: dal_led_off`（规范 §8.1 允许的"YAML 绑定"形态，无独立符号） | |
| DAL-L-021 | safe_off MUST NOT 标 WARN_UNUSED_RESULT | ⚠️ | `dal_led_off` `:89` 标了 `WINK_WARN_UNUSED_RESULT`，但它同时被绑定为 safe_off。与规范 §8.1"led→dal_led_off"范例存在张力——见观察项 O-1 | |
| DAL-L-022 | safe_off 幂等 + 未初始化返回 WINK_OK | ❌ | **F-2**：绑定的 `dal_led_off` 在 `!initialized` 时返回 `WINK_ERR_NOT_INITIALIZED`（`dal_led_set:37`），违反 safe_off 未初始化返回 WINK_OK | |
| DAL-L-023 | safe_off 不依赖调度器/堆 | ✅ | 仅 GPIO 写 | |
| DAL-L-024 | SHOULD ISR-safe | ✅ | 如实声明 `ISR-safe: No`（保守、诚实） | |
| DAL-L-025 | safe_off 绑定具体原语并在头注释声明 | ⚠️ | YAML 绑定 dal_led_off，但**头文件无 safe_off 行为说明**；建议加一行"safe_off = off" | |

### 2.3 函数签名与返回值（§4）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-F-001 | 全公开 API 返回 wink_status_t | ✅ | 全部 | |
| DAL-F-002 | MUST NOT 返回 bool | ✅ | 无 | |
| DAL-F-003 | 布尔谓词用出参 | N/A | 无布尔谓词查询 API | |
| DAL-F-004 | WARN_UNUSED_RESULT + 白名单（safe_off/poll/deinit 豁免） | ✅ | init/on/off/set/toggle 标注；deinit 不标（白名单）。safe_off 张力见 L-021/O-1 | |
| DAL-F-010 | 首参为实例句柄 | ✅ | 全部 | |
| DAL-F-011 | 查询类用 const dev | N/A | led 无 getter（见 O-2） | |
| DAL-F-012 | 修改类用非 const dev | ✅ | on/off/set/toggle/init/deinit | |
| DAL-F-013 | 出参 `out_` 前缀 | N/A | 无出参 | |
| DAL-F-014 | init 第二参 const cfg | ✅ | `:64` | |
| DAL-F-020 | 错误返回时出参不变 | N/A | 无出参 | |
| DAL-F-021 | 调用方错误路径不读出参 | N/A | | |
| DAL-F-022 | 调用方 SHOULD 清零结构体出参 | N/A | | |
| DAL-8B-F-001 | Micro 禁 void* 虚分发 | N/A | 当前 Full-only（未来 Micro 仍须遵守） | |
| DAL-8B-F-002 | Micro 具名函数、函数名去 `_8b_` | N/A | 未来 `dal_led_on(dal_led_8b_t*)`（ADR-0056） | |

### 2.4 动词与命名（§5）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| §5.1 | `dal_<type>_<verb>` 格式 | ✅ | on/off/set/toggle/deinit/init | |
| §5.3.1 | 动词在标准库（on/off/toggle/set/safe_off） | ✅ | 全部为二值输出标准动词 | |
| 黑名单 | 禁用 turn_on/enable_output/run_motor 等 | ✅ | 无 | |
| DAL-V-001/002/003 | 器件特有 API 用具名 typed API，禁 IOCTL | N/A | 无特有 API | |
| DAL-V-010 | was_* 读后清原子性 | N/A | 无 was_* | |
| — | SHOULD 提供状态查询（`is_on`） | ℹ️ | **O-2**：句柄缓存 `is_on` 但无公开 `dal_led_is_on(const dev, bool *out)`；建议补（非 MUST） | |

### 2.5 并发 / ISR / 线程安全（§6）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-C-040 | 默认非线程安全，调用方串行化 | ✅ | 每个公开 API 头注释均 `Thread-safe: No` | |
| DAL-C-041 | 仅显式声明才允许并发 | ✅ | 全部声明 No | |
| DAL-C-042 | Thread-safe 缺失默认 No，SHOULD lint warning | ✅ | 全部显式标注 | |
| DAL-C-043 | 不同实例可并发（不共享资源时） | ✅ | 各持独立 pin | |
| DAL-C-001/002/003 | volatile 单字宽单写者 / RMW 用原子 / 禁空口"无需临界区" | ✅ | 无 volatile 共享字段；`is_on`/`initialized` 在外部串行化契约下访问 | |
| DAL-C-010 | 多字段快照一致性 | N/A | 无跨核/ISR 共享多字段 | |
| DAL-C-020/021/022 | ISR 上下文禁令 / ISR-safe 白名单 / 如实声明 | ✅ | 无 ISR 调用；ISR-safe: No 如实声明 | |
| DAL-C-030/031 | 回调上下文归属声明 | N/A | 无回调 | |

### 2.6 阻塞 / 超时 / 异步（§7）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-B-001/002 | 阻塞 API 命名 + WINK_BLOCKING | ✅ | 全部非阻塞，无需标注 | |
| DAL-B-003 | 阻塞上界数值声明 | N/A | | |
| DAL-B-004 | 阻塞 API 在 STRICT 守卫内 | N/A | 无阻塞 API | |
| DAL-B-010 | 超时值来自 config/常量 | N/A | 无超时 | |
| DAL-B-011 | 非阻塞 API 内部 busy-wait ≤100µs | ✅ | 无 busy-wait | |
| DAL-B-012 | 禁裸空循环长等待，用 PAL 时钟 | ✅ | 无循环 | |
| DAL-B-013 | 阻塞 ≤ TWDT×50% | N/A | | |
| DAL-B-014 | init >100ms 应标 blocking | ✅ | init 为 GPIO claim+init，远低于 100ms | |
| DAL-BUF-001/002/003 | DMA Buffer 归属/对齐/Cache | N/A | 无 DMA | |
| DAL-B-020~025 | 异步三段式状态机 | N/A | led 为同步二值输出 | |

### 2.7 失效安全（§8）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-E-001 | safe_off 绑定具体关断原语 | ⚠️ | 绑定 dal_led_off，但头注释未声明具体行为（见 L-025） | |
| DAL-E-002 | safe_off 简单、确定性好 | ✅ | 仅一次 GPIO 写 | |
| DAL-E-010 | SHOULD 注册到 actuator_registry | ℹ️ | **O-3**：`runtime/src/wink_actuator_registry.c` 未静态引用 led；若由 codegen 注册则忽略，否则 SHOULD 注册 | |
| DAL-BC-001 | Init-to-Ready，不引入 enable()/arm() 前置 | ✅ | init 后即可 on/off，无 arm 步骤 | |

### 2.8 单位、量纲与值域（§9，ADR-0056）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-U-001/002 | 物理量带单位后缀 | ✅ | led 为**二值输出**，`set(dev, bool on)` 是离散命令而非标量物理量，无刻度后缀需求；`pin`/`pwm_channel` 为引脚/通道号（dc_motor 同样裸名） | |
| DAL-U-010 | API Contract MUST 声明 Range | ⚠️ | init Contract **无 Range 字段**（应声明 pin 合法范围）；on/off/set/toggle 无标量参数，N/A | |
| DAL-U-011 | A 类越界钳位饱和无回卷 | N/A | 二值命令，无连续值域 | |
| DAL-U-020/021 | YAML 声明 quantity/quantity_class | ℹ️ | **O-4**：led 无标量 A 类量（on/off 是离散 bool），故 quantity 字段非必需；若将来加亮度（brightness_promille），MUST 标 `quantity_class: actuator_command` 并用定标整数 | |
| DAL-U-022 | 禁弱 typedef 量纲别名 | ✅ | 无 `dal_*_t` 量纲 typedef | |
| DAL-U-023 | A 类全 Profile 定标整数，Full 禁 float | N/A | 无标量控制量（bool 离散命令）；无 float | |
| DAL-U-027/028 | 符号规范 | N/A | | |
| DAL-U-029 | 定标乘法中间值提升 32 位 | N/A | 无乘法换算 | |
| DAL-U-030 | setter/getter/句柄同表示 | N/A | | |
| DAL-8B-U-001~003 | Micro 整型量纲 | N/A | Full-only | |

### 2.9 事件回调（§10）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-CB-001~003 | on_event 命名 / ctx 上下文 / cb=NULL 注销 | N/A | led 无事件回调 | |
| §10.3 | 全局钩子放 BAL 内部头 | N/A | | |

### 2.10 裁剪 / Stub / 禁用态（§11）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-P-001 | 支持 WINK_USE_<TYPE> 开关 | ✅ | `dal_led.h:145` `WINK_USE_LED` | |
| DAL-P-002 | 裁剪 stub 带 WINK_UNAVAILABLE_MSG | ✅ | `:149-160` 全部 6 个函数覆盖 | |
| DAL-P-003 | 禁用提示指引启用方式 | ✅ | `WINK_LED_DISABLED_MSG` 指向 wink-app.json | |
| DAL-P-004 | 阻塞 stub 在 STRICT 守卫内 | N/A | 无阻塞 API | |
| DAL-P-010/011/012 | 运行时 stub 返回 UNSUPPORTED、不 claim 资源、不置 initialized | N/A | 无运行时 stub（全部已实现） | |
| DAL-P-013 | stub 头注释 @experimental | N/A | | |
| DAL-P-014 | YAML 用 experimental 标记 | ✅ | `experimental: false` | |

### 2.11 双 Target 一致性（§12）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-T-001 | dal/ 源码 MUST NOT 出现平台宏 | ✅ | `dal_led.c` 无 `#ifdef SIMULATION/ESP_PLATFORM/__EMSCRIPTEN__` | |
| DAL-T-002 | 同一 .c 进 ESP32 + Wasm 两 target | ✅ | 无 per-target 分叉 | |
| DAL-T-003 | 时间通过 PAL | N/A | 无时间调用 | |
| DAL-T-010 | SHOULD 声明 Simulation-parity 行为差异 | ℹ️ | host `pal_gpio_init` 不碰电平、esp32 配置输出；若有行为差异可加 `Simulation-parity` 字段（SHOULD，非 MUST） | |

### 2.12 向后兼容与演进（§13）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-BC-001 | Init-to-Ready | ✅ | 见 §2.7 | |
| DAL-BC-002 | 句柄新增字段只追加 | ✅ | 当前布局稳定；`{0}` 清零安全 | |
| DAL-BC-003 | config 只追加不重排 | ✅ | 无 apply_override 线序负担 | |
| DAL-BC-004 | 增异步时保留同步 API | N/A | | |
| DAL-BC-005 | 兼容性 > 填充优化 | ✅ | | |
| DAL-BC-010 | SHOULD ABI 编译期断言 | ❌ | **F-6**（同 §2.3） | |
| DAL-BC-011/012 | override 长度校验 / wire schema_version | N/A | 暂无 apply_override（头注释提 ADR-0008 为未来项） | |
| DAL-BC-020~023 | 废弃函数 deprecation 纪律 | N/A | 本次可 flag-day，无废弃函数 | |

### 2.13 错误码与可观测性（§14）

| 规则 ID | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|---------|------|------|-------------|----------|
| DAL-EC-001/002 | 优先用通用错误码 | ✅ | INVALID_ARG / NOT_INITIALIZED / ALREADY_INITIALIZED + PAL 透传 | |
| DAL-EC-003/004 | 器件特有错误码落预留段 | N/A | 无特有错误码 | |
| DAL-EC-010 | 重复 init 返回 ALREADY_INITIALIZED | ✅ | `:9` | |
| DAL-EC-011 | MUST NOT 隐式 deinit 再 init | ✅ | 直接返回，不隐式清理 | |
| DAL-EC-020 | init 成功 SHOULD INFO 日志 | ❌ | **F-3**：init 成功无 `LOG_I`（对照 dc_motor.c:188） | |
| DAL-EC-021 | init 失败 SHOULD WARN 日志 | ❌ | **F-3**：claim 失败 `:13`、gpio_init 失败 `:16` 均无 `LOG_W` 直接返回 | |
| DAL-EC-022 | ISR MUST NOT 打日志 | ✅ | 无 ISR | |
| DAL-EC-023 | 日志 tag SHOULD `dal_<type>` | ❌ | **F-3**：`dal_led.c` 无 `#define LOG_TAG`、未 include `pal_log.h`，驱动零日志 | |
| DAL-EC-030 | init 最小防御校验 | ❌ | **F-5**（同 L-005）：缺 pin 范围校验 | |
| DAL-EC-031 | codegen 层完整语义校验 | ✅ | YAML 声明 gpio_pin required、active_high bool | |

### 2.14 API Contract 注释（§15）

> 模板 ★必填：Preconditions、Postconditions、Range、Blocking、Thread-safe、ISR-safe、Error-codes。Side-effects 为模板列出字段。

| 函数 | Pre | Post | Range | Blocking | Thread-safe | ISR-safe | Side-effects | Error-codes | 状态 | 是否解决 |
|------|-----|------|-------|----------|-------------|----------|--------------|-------------|------|----------|
| init | ✅ | ✅ | ❌ 缺 pin 范围 | ✅ | ✅ | ✅ | ❌ 缺（claim GPIO、配置方向） | ✅ | ❌ **F-7** | |
| on | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺（写 GPIO、写 is_on） | ✅ | ❌ **F-7** | |
| off | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺 | ✅ | ❌ **F-7** | |
| set | ✅ | ✅ | N/A（on 参数已文档化） | ✅ | ✅ | ✅ | ❌ 缺 | ✅ | ❌ **F-7** | |
| toggle | ✅ | ✅ | N/A | ✅ | ✅ | ✅ | ❌ 缺 | ✅ | ❌ **F-7** | |
| deinit | ✅ | ❌ 缺结构化 Postconditions | N/A | ✅ | ✅ | — | ❌ 缺（reset GPIO、release、memset） | ❌ 缺（仅 @return 写 WINK_OK，NULL 实际返 INVALID_ARG） | ❌ **F-7** | |

- Thread-safe/ISR-safe 字段全覆盖（满足 DAL-C-042 lint 门槛）。✅

### 2.15 Codegen YAML（§16）

| 项 | 要求 | 状态 | 证据 / 说明 | 是否解决 |
|----|------|------|-------------|----------|
| codegen_schema | 当前 "1.1" | ✅ | `led.yaml:1` | |
| type / category / is_actuator / experimental | 必填 | ✅ | led / output / true / false | |
| default_role | 角色绑定 | ✅ | `binary_indicator`（role 文件存在） | |
| config.c_type / config_type / headers / deinit_fn | codegen 自动默认 | ✅ | 已核实 `yaml_schema.py:448-455` 自动补 `dal_led_t`/`dal_led_config_t`/`[dal_led.h]`/`dal_led_deinit`；稀疏写法合法 | |
| config.safe_off_fn | actuator MUST 非空 | ✅ | `dal_led_off`（但行为违反 L-022，见 F-2） | |
| fields 声明 | gpio_pin→pin 映射、active_high | ✅ | `c: pin` 映射正确；active_high bool default true | |
| role_bindings | 可省略（1:1 派生） | ✅ | 注释说明由 `roles/*.yaml` 派生 | |
| profiles | 显式声明支持的 Profile | ℹ️ | 未写，默认 Full；Full-only 可补 `profiles: [full]`（非 MUST） | |
| quantity / quantity_class | A 类标量必填 | ℹ️ | **O-4**：二值 bool 非物理标量，暂不需要 | |

### 2.16 编码与文件（附录 B）

| 规则 | 状态 | 证据 | 是否解决 |
|------|------|------|----------|
| DAL-ENC-001 UTF-8 无 BOM | ✅ | 文件正常 | |

---

## 3. 不合规项整改清单（按优先级）

| # | 严重度 | 规则 | 问题 | 整改建议 | 是否解决 |
|---|--------|------|------|----------|----------|
| F-1 | **P1 功能** | DAL-L-006 | init 不显式写 off 电平，`active_low` 下 init 即点亮 | `pal_gpio_init` 成功后、置 initialized 前，显式 `pal_gpio_write(pin, off_level)`（active_high→false，active_low→true），写失败回滚 | ✅ 2026-08-03 `dal_led.c:49-60`；新增 `test_init_leaves_led_off_active_high/low` |
| F-2 | **P1 安全** | DAL-L-022 | 绑定的 safe_off（dal_led_off）未初始化返 NOT_INITIALIZED | 新增独立 `dal_led_safe_off(dev)`（不标 WARN_UNUSED_RESULT、未初始化返 WINK_OK），YAML 改绑它；registry.h 示例同步 | ✅ 2026-08-03 新增 `dal_led_safe_off`；`led.yaml` safe_off_fn 改绑；新增 2 个 safe_off 单测 |
| F-3 | P2 观测 | DAL-EC-020/021/023 | 驱动零日志 | `.c` 加 `#define LOG_TAG "dal_led"` + include `pal_log.h`；init 成功 LOG_I，claim/gpio_init/off-level 失败 LOG_W | ✅ 2026-08-03（单测可见 `[I][dal_led] init ready`） |
| F-4 | P2 观测 | DAL-L-014 | deinit release 失败被静默 | 抽 `release_gpio_claim`，release 失败 LOG_W 但不中断清场 | ✅ 2026-08-03 `dal_led.c:11-22` |
| ~~F-5~~ | ~~P2~~ | ~~DAL-L-005~~ | ~~init 不校验 pin 范围~~ | **误报排除**：`pin` 是 `uint16_t`（非 int16_t `wink_pin_t`），不可能为负，加 `pin<0` 会触发 `-Werror=type-limits`；上界由 `pal_gpio_init` 校验，失败已回滚 claim。头注释已修正为准确表述 | ✅ 排除（非问题） |
| F-6 | P3 ABI | DAL-BC-010/§2.3 | 缺分档 sizeof/offsetof 断言 | 补 `#if INTPTR_MAX==INT32_MAX`（config=8/handle=12/init@9）+ `#else` 64 位档（16/24/17，实测） | ✅ 2026-08-03 `dal_led.h:45-58`；64 位编译已验证 |
| F-7 | P3 注释 | §15 | Contract 缺 Side-effects（全部）、deinit 缺 Post/Error-codes、init 缺 Range | 按 §15 模板补齐全部字段，并为新增 safe_off 写完整 Contract | ✅ 2026-08-03 |

---

## 4. 观察项（ℹ️，非 MUST，供决策）

| # | 观察 | 建议 |
|---|------|------|
| O-1 | ~~`dal_led_off` 既是普通动词又是绑定的 safe_off，WARN_UNUSED_RESULT 属性有张力~~ | ✅ **已随 F-2 解决**：新增独立 `dal_led_safe_off`（不标警告、未初始化返 OK），YAML 改绑它；`dal_led_off` 保留 WARN_UNUSED_RESULT |
| O-2 | 句柄缓存 `is_on` 但无公开查询 API | 可补 `dal_led_is_on(const dal_led_t *dev, bool *out_on)`（const getter，符合 DAL-F-011），让 App/单测可读回状态。非 MUST |
| O-3 | led 是否注册进 `wink_actuator_registry` 未在 runtime 静态表中见到 | 确认 codegen 是否据 YAML 自动注册；若否，按 DAL-E-010 SHOULD 注册，使 safe_off_all 能覆盖 led |
| O-4 | led 为二值输出，无标量 A 类量；当前无 `quantity`/`quantity_class` 合理 | 将来若加 `set_brightness_promille(uint16_t)`，MUST 按 ADR-0056 用定标整数 + YAML 标 `quantity_class: actuator_command`，禁止 float |

---

## 5. 总评

led 驱动**骨架正确、是合格的二值输出 DAL**：POD/config 首成员/静态分发、裁剪 stub 完备、双 target 零平台宏、deinit 清场顺序正确、单元测试覆盖 NULL/未初始化/active-high/active-low/toggle/deinit 循环无泄漏。

对照 v3.4.0 的差距集中在**两类**：
1. **安全/功能**（F-1 init 零能量、F-2 safe_off 未初始化语义）——这两条是 MUST，且 F-1 会在 `active_low` 硬件上真实表现为"上电亮灯"，应优先修；
2. **工程完备度**（日志、ABI 断言、Contract 注释、pin 校验）——照 dc_motor Golden Ref 补齐即可，无架构改动。

量纲规范（ADR-0056）对 led 基本无压力：on/off 是离散命令而非标量物理量，不涉及 float/定标整数之争。整改后 led 可作为"二值输出类"的轻量 Golden Ref（dc_motor 是"执行器带 safe_off/制动语义"的重量 Golden Ref）。

**建议执行顺序**：F-1 → F-2（联动 O-1）→ F-5 → F-3/F-4 → F-6 → F-7；O-2/O-3/O-4 视需要。全部为驱动内改动，不触及 PAL/BAL，无破坏性影响。

---

*本评审为时间点快照（文档第④层），归档后不随代码变动修改；整改结果在 §3 末列"是否解决"回填，并在实施计划/提交中跟踪。*

---

## 6. 整改记录（2026-08-03）

按"led 不需要兼容、完全对齐 v3.4.0 最新最佳规范"的要求，一次性 flag-day 整改：

**代码变更**
- `dal_led.h`：新增 32/64 位分档 ABI 断言（F-6）；新增 `dal_led_safe_off` 声明 + 裁剪 stub（F-2）；全部函数 Contract 补 Side-effects/Postconditions/Range/Error-codes（F-7）；init 零能量说明。
- `dal_led.c`：重写——`#define LOG_TAG "dal_led"` + `pal_log.h`（F-3）；`release_gpio_claim` 记录 release 失败 LOG_W（F-4）；init 显式 `pal_gpio_write(off_level)` 并在写失败时回滚（F-1）；独立 `dal_led_safe_off`（未初始化返 WINK_OK，DAL-L-022）。
- `led.yaml`：`safe_off_fn` 改绑 `dal_led_safe_off`。
- `wink_actuator_registry.h`：文档示例 `dal_led_off`→`dal_led_safe_off`。
- `test_dal_led.c`：新增 4 个用例（init 熄灭 ×2 极性、safe_off 未初始化 OK、safe_off 已初始化熄灭）；删除一个基于 F-5 误报的用例。

**验证**
- host 单测：**12/12 PASS**（MinGW gcc 16，`-Wall -Wextra -Werror` 零警告零错误）；64 位 ABI 断言实测通过（config=16 / handle=24 / offsetof(initialized)=17）。
- `wink lint --pack layering --pack api`：No findings。
- 同步重新生成 2 个 codegen golden（仅 led safe_off 改名 1 行 ×2）。

**未混入的预存问题（非本次引入）**
- `wink-tools/codegen/tests/test_motor_encoder.py::test_dc_motor_open_loop_actuator_role_wrappers` 失败：断言旧 `dal_dc_motor_set_speed`，而 dc_motor 此前已重命名为 `set_speed_promille`（会话前改动）。属独立的 codegen 测试漂移，不在本次 led 整改范围，应由 dc_motor 重构任务同步测试。

