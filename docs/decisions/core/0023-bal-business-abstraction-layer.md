# ADR-0023：BAL（业务抽象层）正式分层建立

> **目录树与 helper 命名**：以 [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) + [ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) 为现行 SSOT。下文 §1 目录示意保留为历史快照；slot / 双轨 / 禁 PAL 头等决策仍有效。

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-06 |
| 触发 | [2026-07-06 BAL/DCST 架构重构方案](../../zh/tech-designs/core/2026-07-06-bal-dcst-architecture-refactor.md) v5 Owner 审阅决策版 |
| 影响范围 | 新增 `wink-micro-os/bal/` 正式目录；`runtime/`（新增 constant/error/API）；codegen 模板与 driver plugin；`samples/common/` 旧 helper 迁移；`wink_status.h`（新增错误码） |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0004 静态分发 vs 运行期 ops](0004-static-dispatch-vs-runtime-ops.md)、[ADR-0013 sim 协作调度器](../unisim/0013-sim-cooperative-scheduler.md)、[ADR-0014 单虚拟核](../unisim/0014-sim-single-virtual-core.md)、[ADR-0017 阻塞 API 硬隔离](0017-blocking-api-hard-isolation.md)、[ADR-0018 IRQ API 收窄](0018-pal-irq-api-narrowing.md)、[ADR-0037](0037-bal-domain-partition-and-closed-loop-motor.md)、[ADR-0038](0038-bal-naming-hard-cut-and-layer-ssot.md) |
| 关联设计规范 | [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |

---

## 背景（Context）

### 当前架构痛点

WinkMicroOS 现有分层为 **app → samples/common helper → runtime/DAL → PAL**，但 helper 散落在 `samples/common/`，存在多处架构性缺陷：

1. **Helper 不是一等公民**：放在 samples 下意味着它们被视为"示例代码"而非正式 API；CMake 链接层级混乱，AI/用户会疑惑哪些是正式接口、哪些是 sample 内部用。
2. **单轨 API 一刀切**：`wink_led_blink_start(&led, ms)` 无法让专家覆盖栈/优先级/核绑定（时序敏感的 RMT 脉冲捕获场景需要钉核+提优先级），初学者又被暴露给 2048 栈/500 优先级等 RTOS 细节。
3. **slot 池容量硬编码**：如 `wink_button_helper.c` 固定开 4 槽，多实例浪费 RAM、少实例浪费槽位；blink helper 存在 `s_next++` 环形游标 LIFO 耗尽 bug（4 次 start 后即 RESOURCE_EXHAUSTED，stop 无法回收）。
4. **BAL 公共头泄露 PAL 类型**：现有 helper 示例直接 include `pal_osal.h`，把 `pal_os_core_id_t` 暴露给应用层，破坏分层红线。
5. **无运行期周期动态调整**：`wink_periodic` 不支持 `change_period`，动态调频只能 stop+restart，对伺服/PID 闭环丢拍不可接受。
6. **无效句柄裸写魔数**：代码里随处是 `-1` 表示无效 periodic 句柄，可读性差、维护风险高。
7. **0 实例场景处理粗糙**：未启用某类设备时 helper 要么编不过、要么静默 no-op 掩盖配置错误，缺少统一的"编译期报错 + stop 安全 no-op"约定。

### 同时需要拒绝的反模式

- ❌ 引入 C++/虚表/运行期多态（与 ADR-0004 静态分发核心范式冲突）。
- ❌ 在 BAL 层做服务依赖声明（YAGNI——硬件依赖 device_tree 拓扑序保证，软件依赖 app_init 调用顺序解决）。
- ❌ 把业务逻辑写进 JSON DSL（违反"JSON 只描述静态物理世界"原则）。
- ❌ 统一 `wink_dev_start(dev, svc_enum, void*args)` 单入口（本质是通过 void*+enum 从后门引入运行期多态，AI 强类型导航失效）。
- ❌ BAL 公共头 include 任何 `pal_*.h`（分层红线）。

---

## 方案比选（Options）

### 方案 A：维持现状（仅迁移目录，不加双轨/slot/codegen）

将 `samples/common/` 原样搬到 `bal/`，仅此而已。

- ✅ 工作量最小。
- ❌ 不解决上述 7 个痛点里的任何一个（slot LIFO bug、无周期动态调整、单轨 API、魔数 -1 等全部保留）。
- ❌ 未来专家场景（如超声波钉核提优先级）没有覆盖路径。

### 方案 B：完全运行期多态 BAL（引入虚表 + void* + service 注册表）

每个 BAL service 以 `struct bal_service_ops` 虚表注册到全局链表，`bal_start(dev, svc, args)` 通过 void* 分发。

- ✅ 扩展性强——新增 service 不需改分发器。
- ❌ **违反 ADR-0004 核心范式**：本项目明确拒绝运行期虚表多态。
- ❌ AI 生成风险极高：void* 变参让 LLM 极易传错类型真机炸栈。
- ❌ wasm 运行时 `call_indirect` 性能惩罚。
- ❌ 故障路径自动 stop 全局链表有 §tech-design §3.4.1 三处硬伤（阻塞 fault 上下文、依赖顺序猜不对、双删竞态）。

### 方案 C：静态 slot 池 + 强类型双轨 API + codegen 驱动容量（**推荐**）

- **正式建立 `wink-micro-os/bal/` 作为一等分层**（在 app 与 DAL/runtime 之间）；
- **强类型命名 API**：`wink_<device>_<service>_start/start_ex/stop/set_*/is_running`（C 无重载，靠命名拓扑导航 AI）；
- **双轨设计**：`_start`（初学者默认栈/优先级/核）+ `_start_ex`（专家用 `wink_helper_opts_t` 覆盖），`_start` 是 3 行 wrapper 转调 `_start_ex(NULL opts)`；
- **per-helper 静态 slot 池**：三态状态机（FREE/STARTING/RUNNING）+ TOCTOU 二次校验自回滚；
- **BAL 头隔离 PAL 类型**：BAL 自定 `wink_bal_core_t` 枚举，`.c` 内部映射到 `pal_os_core_id_t`；
- **codegen 驱动 slot 容量**：`WINK_APP_MAX_<DEV>_INSTANCES` 宏由 codegen 从 `wink-app.json` 实例数生成，0 实例时整个 helper 编译为空 stub；
- **统一周期调度入口**：所有 BAL helper 走 `wink_periodic_start_ex`（不直接调 `wink_soft_timer`），runtime 新增 `wink_periodic_change_period()` 支持零停摆动态调频；
- **新增具名常量与错误码**：`WINK_PERIODIC_INVALID = ((wink_periodic_handle_t)0)`、`WINK_ERR_CANCELED = -19`（并发撤销的良性事件）；
- **0 实例 stub 约定**：控制/状态 API 挂 `WINK_UNAVAILABLE_MSG` 编译报错，`stop` 静默 no-op 方便通用清理路径。

- ✅ 完整解决上述 7 个痛点；
- ✅ 与 ADR-0004/0013/0014/0017/0018 全部对齐；
- ✅ 初学者/AI/专家三类人群都覆盖（§tech-design §1.2）；
- ✅ 静态 slot 池 BSS 零开销、零动态分配、零 RAM 浪费；
- ⚠️ 工作量较大：需新建 `bal/` 目录、runtime 新增 3 个 API/symbol、codegen 新增 `render_config_macros()` 钩子；
- ⚠️ 每个 helper 要写两套 API（但 `_start` 是 3 行 wrapper，可用未来 `WINK_BAL_HELPER_IMPL` 宏自动化，阶段 5 YAGNI 后做）。

---

## 决策结论（Decision）

**采纳方案 C**。核心设计点：

### 1. 目录与分层位置

```
wink-micro-os/bal/
├── include/
│   ├── wink_helper_opts.h           /* BAL 公共头唯一"外部选项"入口 */
│   ├── output/                      /* 输出器件 helper（LED blink/breath/buzzer...） */
│   ├── input/                       /* 输入器件 helper（button poll/encoder...） */
│   ├── sensor/                      /* 传感器 helper（ultrasonic 周期测/IMU...） */
│   ├── actuator/                    /* 执行器 helper（servo sweep/motor PID...） */
│   ├── display/                     /* 显示 helper（OLED 动画/数码管...） */
│   └── comm/                        /* 通信/遥测 helper（default telemetry/MQTT...） */
├── src/                             /* 镜像 include 布局 */
└── tests/                           /* host Unity 单测 */
```

严格依赖规则：
- `app → BAL → { DAL, runtime }`
- **BAL ⇢ PAL（禁止）**：BAL 公共头**不得** include 任何 `pal_*.h`，不得 expose `pal_os_core_id_t` 等 PAL 类型；
- BAL `.c` 内部可以 include `pal_irq.h`/`pal_osal.h`/`pal_log.h`（实现需要临界区、核映射、日志）；
- `codegen 输出（device_tree.h）→ BAL .c`：允许 BAL `.c` include `device_tree.h` 拿容量宏，但 `device_tree.h` 自身不得 include BAL 头（防循环依赖）；
- CMake 静态库 `wink_bal`，依赖 `wink_runtime + wink_dal`，不直接 link PAL 除外的 target-specific 物件（保持跨 target 同源编译）。

**CI 卡口**：`bal/include/**/*.h` 里不得出现 `#include.*pal_`，由一条简单 grep 检查在 CI 执行。

### 2. 核亲和类型隔离（`wink_bal_core_t`）

在 `bal/include/wink_helper_opts.h` 统一定义 BAL 自有的核亲和枚举，**不依赖** `pal_os_core_id_t`：

```c
typedef enum {
    WINK_BAL_CORE_ANY = 0,
    WINK_BAL_CORE_0   = 1,
    WINK_BAL_CORE_1   = 2,
    WINK_BAL_CORE_INVALID = -1,
} wink_bal_core_t;
```

BAL `.c` 内部用一个 `static map_core(wink_bal_core_t) → pal_os_core_id_t` 函数转换；不支持多核的 target（host/wasm/baremetal）将 CORE_0/1 映射为 ANY（"尽力而为"而非"必须"）。

### 3. 统一 Helper 选项结构（`wink_helper_opts_t`）

**所有** helper 共用同一个 `wink_helper_opts_t`（在 `wink_helper_opts.h` 中唯一定义，每个 helper `.h` 不得重复定义同名 struct，否则类型冲突）：

```c
typedef struct {
    uint32_t        stack_bytes;  /* 0 = use helper default */
    int32_t         priority;     /* <0 = use helper default */
    wink_bal_core_t core_id;      /* WINK_BAL_CORE_INVALID = use default */
    uint32_t        flags;        /* WINK_PERIODIC_LIGHT / WINK_PERIODIC_MAY_BLOCK / 0 = use helper default */
} wink_helper_opts_t;

/* 默认选项初始化宏（推荐使用，防止零初始化时将优先级/核绑定错误覆盖为 0/ANY） */
#define WINK_HELPER_OPTS_DEFAULT \
    ((wink_helper_opts_t){ .stack_bytes = 0, .priority = -1, .core_id = WINK_BAL_CORE_INVALID, .flags = 0u })

#define WINK_HELPER_OPTS(stack, prio, core) \
    ((wink_helper_opts_t){ .stack_bytes = (stack), .priority = (prio), .core_id = (core), .flags = 0u })
```

零值/`WINK_BAL_CORE_INVALID` 字段语义为"use helper default"，`_start_ex(opts=NULL)` 等价于调 `_start`。

### 4. Helper 默认常量契约（强制）

每个 helper `.h` **必须**暴露以下一组 `#define WINK_<DEV>_HELPER_DEFAULT_*` 宏，作为 `_start()` 的默认值、codegen 容量参考和文档化入口：

- `WINK_<DEV>_HELPER_DEFAULT_STACK`（栈字节，LIGHT 路径为 0）
- `WINK_<DEV>_HELPER_DEFAULT_PRIO`（FreeRTOS 优先级，LIGHT 路径为 0）
- `WINK_<DEV>_HELPER_DEFAULT_CORE`（`WINK_BAL_CORE_ANY` 等）
- `WINK_<DEV>_HELPER_MIN_PERIOD_MS`（最小合法周期，防配置过短炸时序）
- `WINK_<DEV>_HELPER_DEFAULT_FLAGS`（`WINK_PERIODIC_LIGHT` 或 `WINK_PERIODIC_MAY_BLOCK`）

**首批 Helper 默认值**（基于现有踩坑经验与 codebase 惯例）：

| Helper | 默认栈 | 默认优先级 | 默认核 | 默认 flags | MIN_PERIOD |
|---|---|---|---|---|---|
| `wink_led_blink_start(&led, ms)` | 0（LIGHT） | — | ANY | LIGHT | `WINK_RUNTIME_TICK_MS` |
| `wink_button_helper_start(&btn, poll_ms)` | 0（LIGHT） | — | ANY | LIGHT | `WINK_RUNTIME_TICK_MS` |
| `wink_sonar_helper_start(&sonar, ms)` | 3072 | 5 | ANY | MAY_BLOCK | 50ms |
| `wink_servo_sweep_start(&sv, ...)` | 2048 | 3 | ANY | MAY_BLOCK | 20ms |
| `wink_telemetry_default_start(...)` | 2048 | 1 | ANY | MAY_BLOCK | 1000ms |
| `wink_oled_animation_start(...)` | 3072 | 2 | ANY | MAY_BLOCK | 20ms (50fps 上限) |

**Button 明确归类为 LIGHT**：已走查 host/esp32/wasm 三 target，`dal_button_poll` + `pal_gpio_read` 仅做寄存器级读（<1µs），配合 `"isr_counter": true` ISR-defer 模型（ISR 计边沿，LIGHT 回调读 delta + 去抖），满足 LIGHT 契约，不加 `WINK_INTERNAL_BLOCKING_REGION` pragma。

### 5. Slot 池三态状态机 + TOCTOU 自回滚

每个 helper `.c` 维护一个 static slot 数组（容量 `WINK_APP_MAX_<DEV>_INSTANCES`），状态机：

```
FREE ──start──► STARTING ──periodic_start_ex 成功+TOCTOU 校验通过──► RUNNING
  ▲                │                                                    │
  │                │ periodic_start_ex 失败 或 并发 stop 撤销             │ stop（任何状态）
  └────────────────┴────────────────────────────────────────────────────┘
```

关键实现约束：
- **必须扫描全数组找 FREE 槽位**（不用环形游标 `s_next++`），否则 stop 无法原地回收 slot，重蹈 blink helper LIFO 耗尽 bug；
- **引入 `generation`（世代计数器）防御 ABA 竞态**：在 slot 结构体中必须包含一个 `uint32_t generation` 计数器，分配/释放 slot 时自增。在 `_start_ex` 临界区外创建任务前读取该值，二次临界区校验（TOCTOU）时除了比对 `dev` 和 `state` 之外，还必须检查 `slot->generation == expected_generation`，防范高并发下的 slot 复用冲突漏洞；
- **临界区只保护 slot 元数据**（dev/state/h/generation 字段），用真实存在的 `pal_irq_save_rtos_safe()/pal_irq_restore()`（ADR-0018 三态临界区原语）；
- `wink_periodic_start_ex` / `wink_periodic_stop` **在临界区外调用**（它们可能阻塞：任务创建/stop 等 sem 最长 500ms）；
- **TOCTOU 二次校验**：临界区外 start_ex 期间若有并发 stop 将 slot 清空，回到临界区后必须检测 `slot->dev == dev && slot->state == STARTING && slot->generation == expected_generation`，否则回滚 slot、停掉新建任务、返回 `WINK_ERR_CANCELED`；
- 幂等：同 dev 二次 start 返回 `WINK_ERR_BUSY`；`stop(NULL)` 无害；对 STARTING 态 slot 的 stop 也安全（状态机自动处理）。

### 6. Codegen 驱动 Slot 容量

`device_tree.h` 由 codegen 新增两类宏：

```c
/* 实例计数宏：驱动 BAL 静态数组大小 */
#define WINK_APP_MAX_LED_INSTANCES         1u
#define WINK_APP_MAX_BUTTON_INSTANCES      1u
#define WINK_APP_MAX_ULTRASONIC_INSTANCES  1u
#define WINK_APP_MAX_SERVO_INSTANCES       0u  /* 未使用 = 0 */
/* ... */

/* 配置常量宏：由 driver plugin 的 render_config_macros() 钩子选择性导出 */
#define BOOT_BUTTON_AUTO_POLL_MS     10u
#define BOOT_BUTTON_LONG_PRESS_MS    3000u
#define SMOKE_SONAR_USE_RMT          1
```

每个 BAL helper `.c` 用 `#ifndef WINK_APP_MAX_XXX_INSTANCES` 提供非 codegen 构建（如 host 单测）的 fallback（2u）。

### 7. 0 实例 Stub 约定

当 `WINK_APP_MAX_<DEV>_INSTANCES == 0` 时，整个 helper 编译为 stub：

- **控制/状态类 API**（start/start_ex/set_period/is_running）：挂 `WINK_UNAVAILABLE_MSG("...add a '<type>' device first")`，**编译期强制报错**，避免在无设备板型上悄然 no-op 掩盖配置错误；
- **清理类 API（stop）**：**不**挂 `WINK_UNAVAILABLE_MSG`，保持静默 no-op（`void stop(dal_xxx_t *dev) { (void)dev; }`）。理由：通用 fault/低功耗清理路径会无差别调所有 helper 的 stop，如果 stop 也编译报错就逼出大量 `#ifdef` 板型分支，违反"stop 幂等/NULL 安全"的统一心智模型。

复用现有 `WINK_UNAVAILABLE_MSG` 机制（已支持 GCC/Clang/MSVC），不另造 `__attribute__((error(...)))`。

### 8. Runtime 补齐（常量 / 错误码 / API）

Stage 1 落地以下 runtime 增量（详见 tech-design §7 阶段 1 #4-#5）：

| 符号 | 位置 | 语义 |
|---|---|---|
| `WINK_PERIODIC_INVALID` | `runtime/include/wink_tasks.h` | `((wink_periodic_handle_t)0)`；统一无效句柄表示，禁止裸写 0/-1 魔数。**理由见 Erratum-1**（2026-07-06）：handle 编码为 `slot+1`，0 天然保留为 INVALID；负值为 `wink_status_t` 错误码透传（如 WINK_ERR_RESOURCE_EXHAUSTED），不能与 INVALID 哨兵冲突。`wink_periodic_stop(h)` 对 `h <= 0` 静默 no-op 同时覆盖 INVALID 和错误码 |
| `WINK_ERR_CANCELED = -19` | `pal/include/wink_status.h` | 并发撤销（并发 stop 抢占导致 start 自回滚）——**良性可预测并发事件**，与 `WINK_ERR_INVALID_STATE`（编程错）严格区分；应用层按 CANCELED 走正常回滚路径不应触发 fault |
| `wink_periodic_change_period(h, ms)` | `runtime/include/wink_tasks.h` | 零停摆动态改周期，下个周期生效；self-set_period 重入合法；LIGHT 侧原子写 period/MAY_BLOCK 侧 `xTaskAbortDelay()`/fiber-wake 保证长改短立即生效 |
| `wink_soft_timer_change_period(h, ticks)` | `runtime/include/wink_soft_timer.h` | LIGHT 侧底层原语（供 `wink_periodic_change_period` 分派） |
| `wink_periodic_active_count()` | `runtime/include/wink_tasks.h` | 返回 RUNNING 态 periodic 句柄数；供 `wink_device_tree_deinit()` Debug 断言泄漏 |

### 9. LIGHT 路径契约（血红色警告，与 ADR-0017 配套）

所有 LIGHT 类 helper（blink/button poll/未来 tick 级采样）的回调运行在 **runtime 主 tick soft_timer 分发上下文**，必须满足铁律（详见 tech-design §3.2.7）：

1. 严禁阻塞/让出、严禁浮点重算/mutex/复杂 printf、严禁 busy-wait >10µs；
2. 三道防线：编译期（`WINK_BLOCKING` deprecated 警告 + sim 下 `WINK_STRICT_NONBLOCKING=1` 链接失败）、运行期（soft_timer WCET 监控 + LIGHT 上下文 in-flag + `WINK_ASSERT_NONBLOCKING()` 升级为 fault）、code review（LIGHT 回调体 ≤20 行）；
3. **测量抖动余量**：ESP32 等抢占式 RTOS 上 `pal_os_get_us()` 包含 ISR 抢占时间，WCET 硬阈值需预留 2-5× 余量（建议 LIGHT 100µs 预算配 200-500µs hard limit），防中断抖动误触发 fault。

不能满足契约的 helper（OLED 动画、RMT 读超声波、SD 写入）必须走 `WINK_PERIODIC_MAY_BLOCK` 独立任务路径。

### 10. 统一 Periodic 入口（容量计算简化）

所有 BAL helper **统一通过 `wink_periodic_start_ex` 调度**，不直接调 `wink_soft_timer_create`——LIGHT 路径底层仍由 soft_timer 承载，但经 `wink_periodic` 统一入口：
- 便于容量计算（`WINK_MAX_PERIODIC ≥ Σ(WINK_APP_MAX_<DEV>_INSTANCES) + 4`）；
- `change_period` 单点分发；
- soft_timer 不再需要独立池配额（codegen 自动按总量设 `-DWINK_MAX_PERIODIC=N`，soft_timer 留 4 个余量给 selftest/用户自定义）。

### 11. Self-set_period 重入语义

在 LIGHT/MAY_BLOCK 回调**内部对自身句柄**调用 `wink_periodic_change_period` 是**合法**的：
- LIGHT 侧：原子写入 period 字段，当前 callback 返回后下 tick 即按新频率派发；
- MAY_BLOCK 侧：task 主循环顶部读 period，当前迭代完成后即按新周期休眠（`xTaskAbortDelay` 保证长改短立即生效）；
- **跨 helper set_period 禁止**（属业务层状态机职责，不应由 BAL 隐式支持）；
- 列为 Stage 1 `change_period` 单测必覆盖项。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

1. **初学者 AI/初学者心智模型简化**：5 行代码启动 blink/button/sonar，零 RTOS 参数、零 pal_* 头、零 pragma（除 init 阶段 selftest 用 WINK_INIT_BLOCKING_REGION 小块包裹）。
2. **专家能力保留**：`_start_ex + WINK_HELPER_OPTS()` 可覆盖栈/优先级/核/flags，钉核提优先级应对 RMT 脉冲类时序敏感场景。
3. **零 RAM 浪费**：slot 容量由 codegen 按实际设备实例数生成；未使用 helper 0 实例 stub 不占 BSS。
4. **并发安全**：三态状态机 + TOCTOU 自回滚从设计上消灭 start/stop 竞态空指针窗口；`WINK_ERR_CANCELED` 让良性并发撤销有专属语义。
5. **分层红线可执行**：CI grep 卡口 + BAL 自定 core 枚举杜绝 BAL 头 include PAL。
6. **零停摆动态调频**：对伺服/PID 闭环场景是硬要求，`wink_periodic_change_period` 统一支持 LIGHT+MAY_BLOCK 双路径。
7. **与现有基础设施兼容**：复用 `WINK_UNAVAILABLE_MSG`、`WINK_BLOCKING`、`pal_irq_save_rtos_safe`、`WINK_IGNORE_RESULT`，不重造轮子。

### 约束 / 代价

1. **每个 helper 要写两个入口**（`_start`/`_start_ex`）：但 `_start` 是 3 行 wrapper，未来可考虑 `WINK_BAL_HELPER_IMPL` 宏自动化（YAGNI，阶段 5 后再做）。
2. **必须扫描全数组找 FREE slot**：N 个设备时 start 是 O(N) 扫描，但 N 通常 ≤8（MCU 引脚数限制），可接受。
3. **Runtime 新增 API 是纯新增**：不改旧 API 行为，保留回退路径（旧 samples/common 头保留转发 include 一个版本周期）。
4. **CI 要加 grep 卡口**：`bal/include/**/*.h` 禁 pal_ include。

### 迁移约束

- Stage 2 迁移 `samples/common/` → `bal/` 时，旧头保留转发 include 一个 release 周期，给下游 sample 平滑迁移窗口；
- blink helper 迁移必检项：修复 `s_next++` 环形游标 LIFO bug，必须"扫描全数组找 NULL dev"原地回收，配"start/stop 循环 100 次不返回 EXHAUSTED"单测；
- button helper 迁移时不加 `WINK_INTERNAL_BLOCKING_REGION`（已实查无 blocking 调用，归 LIGHT）。

---

## 遵循与后续（Compliance & Follow-up）

### 立即执行（Accepted 后）

1. **回写设计规范**：
   - `02-wink-micro-os/03-device-abstraction-layer.md`：新增 BAL 正式层描述，分层图加入 BAL；
   - `03-app-codegen/` 相关文档补充 `WINK_APP_MAX_<DEV>_INSTANCES`、配置宏、`render_config_macros()` 钩子。
2. **Stage 0 起手**：本 ADR 落地后进入阶段 0（DAL deinit 补全，见关联 ADR-0024）；BAL 目录在 Stage 1 创建。

### 实施期必做

- 每个新 BAL helper 必须配 host Unity 单测覆盖：start/stop 幂等、多实例并发、`_start_ex` 参数覆盖、set_period、is_running、NULL 安全、0 实例 stub；
- `wink_periodic_change_period` 三子任务（soft_timer 侧 / MAY_BLOCK task 侧 / 统一入口）各配单测，含 self-set_period 重入单测；
- Stage 1 完成后 host build 0 warn、codegen golden 单测全过。

### 不做（Out of Scope）

- DAL 异步 `on_data` 回调模式（独立 ADR 处理通知上下文/ISR defer）；
- BAL 服务链式依赖（YAGNI，Q9 已决）；
- BAL helper 模板宏 `WINK_BAL_HELPER_IMPL`（阶段 5 后视重复度再做）；
- `wink_dev_start(dev, svc, ...)` 统一入口（方案 B 已否决）。

---

### Erratum-1（2026-07-06）：`WINK_PERIODIC_INVALID` 值校正

**背景**：ADR §8 与下表初版写为 `((wink_periodic_handle_t)-1)`，与运行时实际 handle 编码冲突。

**代码实况**（`wink-micro-os/runtime/src/wink_runtime_tasks.c:184-191`）：
```c
/* handle == slot+1 so we never return 0 (0 is reserved as INVALID). */
return (wink_periodic_handle_t)(slot + 1);
...
void wink_periodic_stop(wink_periodic_handle_t h) {
    if (h <= 0) return;  /* rejects 0 AND negative */
```
- `wink_periodic_start_ex` 失败时直接返回 `(wink_periodic_handle_t)WINK_ERR_*`（负值透传）；
- 成功时返回 `slot+1`，永远 ≥ 1；
- 因此负值 = 错误码、0 = INVALID 哨兵，语义不重叠。

**校正决策**：`WINK_PERIODIC_INVALID` 定为 `((wink_periodic_handle_t)0)` 与代码实况对齐；`-1` 为错误码区间保留值（未来可分配为 `WINK_ERR_*`）。

**影响范围**：仅影响未实现的新增常量；现有代码（`s_periodic_light_h = 0` 初始化、`if (h <= 0)` 判无效）无需改动。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-06：Proposed（基于 tech-design v5 Owner 决策版起草）
- 2026-07-06：Accepted（Owner 审阅并采纳，并融入了默认选项初始化宏与 Slot 世代计数器等 2 项优化建议）
- 2026-07-06：Erratum-1 应用：WINK_PERIODIC_INVALID 从 -1 校正为 0，与运行时 handle=slot+1 编码及负值错误码透传约定对齐

