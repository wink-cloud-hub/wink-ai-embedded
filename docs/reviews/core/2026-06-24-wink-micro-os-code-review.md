# Wink-AI 嵌入式（WinkMicroOS）· 代码实现层评审报告

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-06-24 |
| 评审对象 | `wink-micro-os/` C 代码实现（PAL / DAL / runtime(BAL) / trace / 三 target / samples / 构建 / 测试） |
| 评审基线 | `.claude/skills/embedded-best-practice`（`_embedded-shared/` + `references/static-dispatch/`）、`.claude/rules/c-code.md`、ADR-0001/0002/0003/0004/0005 |
| 评审视角 | 资深嵌入式架构师（安全关键系统编码纪律 + 静态分发架构合规） |
| 评审方法 | 逐文件精读（~20 个核心 C 文件 + 10 份 CMake）+ 对照 SHARED/pitfalls/grilling/safety-checklist 逐条核验，全程只读 |
| 综合评分 | **8.2 / 10**（范式落地优秀，工程纪律强；2 处真实 bug + 1 处架构违背被 host 测试现状掩盖） |
| 关联决策 | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md) · [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md) · [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md) · [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md) · [ADR-0005](../../decisions/core/0005-degraded-status-segment.md) |
| 关联评审 | [2026-06-22 架构（设计文档）评审](./2026-06-22-architecture-review.md)（本报告为代码层互补） |

---

## 一、总体判断

**这是一份范式落地干净、工程纪律高、但存在被现状掩盖的真实缺陷的代码实现。** 静态分发（POD + 命名 API + 编译期绑定）执行到位，错误码体系、零动态分配、BAL/PAL 分层隔离、仿真同源测试这几项达到生产级水准。值得注意的是，**代码实现实际上已领先于并修正了 2026-06-22 设计评审指出的若干遗留问题**——例如 `wink_status.h` 已落地 ADR-0001 的负数错误码（`WINK_OK=0`、负数=错误），`dal_ultrasonic.c` 已演进到 ADR-0003 决策2 的细粒度旁路（仅旁路底层物理量、换算两端同源）。

但有三类问题需要正视：

1. **2 处真实构建/运行期 bug**（wasm 挂起配置引用死符号、ultrasonic 阻塞 polling 破坏调度）——它们被"当前主要跑 host 单测、esp32 仍是骨架、wasm 构建路径未真正执行"的现状掩盖，一旦真正构建 wasm 或移植 esp32 就会暴露。
2. **1 处与项目自身 grilling 答案直接矛盾的架构违背**——DAL 把阻塞原样传播到 BAL，违反 grilling 问题5 的官方设计回答。
3. **若干安全关键编码硬规则违规**（单行 `if` 无大括号、PAL 全 `bool` 丢语义、trace 并发契约未声明）。

可作为"host 单测 + 仿真 demo"的可用基线，但 P0 项必须前置闭环才能谈"同源编译到真机能跑"。

---

## 二、架构合规性亮点（值得肯定）

| # | 亮点 | 落地位置 | 依据 |
|---|------|----------|------|
| ✓1 | **静态分发纯净**：`dal_ultrasonic_t`/`dal_servo_t` 是 POD，无 vtable / `container_of` / `*_ops`；`device_tree.c` 静态实例化 | `dal/include/*`、`samples/avoidance_car/device_tree.c` | ADR-0004 |
| ✓2 | **错误码体系完整且已修正符号**：`wink_status_t` 负数分段 + `wink_status_is_error()` 辅助函数 + `WINK_WARN_UNUSED_RESULT` 便携宏 + **无正数 warning 段**（降级归 `-50s`，故 `if(status<0)` 对降级仍正确捕获） | `pal/include/wink_status.h` | ADR-0001/0005、error-codes.md |
| ✓3 | **避开头号雷**：App 用 `if (wink_status_is_error(s))` 判错，而非 `if (s)` | `samples/avoidance_car/app_main.c:22` | error-codes.md「头号脚雷」 |
| ✓4 | **分层严格**：BAL 只调 `dal_*` 命名 API，不碰 PAL / 寄存器 | `samples/avoidance_car/app_main.c` | AI 禁令1 |
| ✓5 | **仿真同源优秀**：仿真分支仅旁路底层物理量（trigger + echo pulse），换算 `dal_pulse_us_to_cm` 两端共享，并有 `test_dal_ultrasonic_sim.c` 专门回归守卫——比 `simulation.md` 文档里的旧 L1 直通形态更优，是 ADR-0003 决策2 的正确落地 | `dal/src/dal_ultrasonic.c:14-34`、`test/test_dal_ultrasonic_sim.c` | ADR-0003 决策2、c-code.md §2 |
| ✓6 | **host 一等 target + 协作式虚拟时间**：`pal_hal_host.c` 用虚拟时间推进驱动真机 `#else` polling 分支，使真机代码也被 host 测试覆盖 | `targets/host/pal_hal_host.c:36-54` | c-code.md §2「Bypass 收窄」 |
| ✓7 | **零动态分配**：trace 静态环形缓冲、device_tree 全 `.data/.bss`、无任何 `malloc` | `trace/src/wink_trace.c`、`samples/avoidance_car/device_tree.c` | lifecycle §1 |
| ✓8 | **CI 严格**：`-Wall -Wextra -Werror` + Unity + sim/真机分支分别编译测试 | `test/CMakeLists.txt:25,49,86` | tooling、safety-checklist 阶段1 |
| ✓9 | **SSOT 闭环意识强**：`wasm_bridge.h` 集中 JS 导入声明、`wink_status.h` 逐字对齐 error-fault-model、多处 ADR 回写注释 | `targets/wasm/wasm_bridge.h`、`pal/include/wink_status.h` | docs-adr.md §2 |

---

## 三、发现清单（按严重性分级）

> 严重性分级沿用 `safety-checklist.md`：**致命**（系统故障/崩溃）｜**高**（真实 bug / 内存泄漏 / 竞态 / 架构违背）｜**中**（编码硬规则违规 / 校验缺失）｜**低**（风格 / 小优化）。安全关键项目里**高**及以上不可商量。

### 🔴 高 — 真实 bug / 架构违背

#### H1. `ASYNCIFY_IMPORTS` 引用已删除函数 → wasm 协程挂起配置失效
**位置**：`wink-micro-os/CMakeLists.txt:33`

```cmake
"-s" "ASYNCIFY_IMPORTS=['js_sim_get_ultrasonic_distance']"
```

**问题**：代码早已从 L1 直通（`js_sim_get_ultrasonic_distance`）演进为 ADR-0003 决策2 的细粒度旁路（`js_sim_trigger_ultrasonic` + `js_sim_measure_echo_pulse_us`）。`js_sim_get_ultrasonic_distance` 在整个 C 代码库与 `wasm_bridge.h` 中**已不存在**，仅残留在 `simulation.md §2` 文档范例（见 M5）。

更关键的是：真正需要 Asyncify 挂起的是 `pal_delay_ms → js_pal_delay_ms`（`targets/wasm/pal_osal_wasm.c:9-11` 注释明写"Asyncify 挂起，由 JS 唤醒"），它**未**出现在 IMPORTS 列表里。`ASYNCIFY_IMPORTS` 是"只对 these import 插桩挂起点"的白名单——当前白名单指向一个死符号：

- 主循环 `wink_runtime_run(cb, 0)` 是无限循环（`runtime/src/wink_runtime.c:21`、`targets/wasm/wasm_entry.c:33`），`wink_app_delay_ms → pal_delay_ms` 内部由于 `js_pal_delay_ms` 未被 `ASYNCIFY_IMPORTS` 白名单覆盖，导致 Wasm 无法插桩保存/恢复调用栈，运行时挂起会触发 `Asyncify.StateError` 异常崩溃，或独占 JS 事件循环导致浏览器卡死；
- 同时，仅把已删除符号写进 `ASYNCIFY_IMPORTS` 并不等于强制链接该 import：因为若 C 代码未调用该函数，死符号会在链接期被 DCE（死代码消除）移除，Wasm 不会产生该 import 需求，JS 侧不提供也不会导致必然的链接失败。真正挂起点在未被插桩的 `js_pal_delay_ms`。

**为何未被暴露**：本项目当前只跑 host 测试，wasm 路径（`CMakeLists.txt:25` 的 `if(TARGET_PLATFORM STREQUAL "wasm" AND EMSCRIPTEN)`）从未真正执行。这正是 pitfalls.md 陷阱3「SSOT 未强制」的活样本，但比文档描述更严重——它直接破坏构建产物。

**依据**：c-code.md §3（双 target 同源）、ADR-0002、pitfalls 陷阱3。

**建议**：把列表改为实际需要挂起的 import——`ASYNCIFY_IMPORTS=['js_pal_delay_ms','js_pal_delay_us']`（或按 Emscripten 最新写法用 `-sASYNCIFY` 交由 emcc 自动分析），并在 emcc 环境实测一次确认主循环能正确让出。同步删除 `simulation.md §2` 残留的 `js_sim_get_ultrasonic_distance` 范例（与 M5 联动）。

---

#### H2. `dal_ultrasonic_read` 真机分支阻塞 polling，破坏 10ms tick 调度，且与 grilling Q5 自相矛盾
**位置**：`wink-micro-os/dal/src/dal_ultrasonic.c:38-59`

真机分支用两段 `while` 忙等测 echo：等上升沿（超时 30ms）+ 测高电平宽度（超时 30ms），**最坏阻塞 ≈ 60ms**。而 `wink_runtime_run` 的 tick 是 `WINK_RUNTIME_TICK_MS = 10`（`runtime/include/wink_runtime.h:19`）。BAL 示例 `samples/avoidance_car/app_main.c:21` 正是**在 `app_loop` 里同步调用** `dal_ultrasonic_read`——每 tick 可能阻塞达 60ms，是 tick 周期的 6 倍，真机上会饿死协作式调度、迟滞其他逻辑，并逼近看门狗阈值。

**三重问题**：

1. **契约虚标**：`dal/include/dal_ultrasonic.h:24` 写 `Blocking: Yes (MAX 30ms timeout)`，实际最坏 60ms。contracts.md 要求 blocking 必须如实标注最大值。
2. **违背项目自身原则**：skill 核心原则 4「零容忍阻塞」+ grilling 问题5 官方答案「PAL 阻塞必须由 DAL 封装成非阻塞 API，禁止把阻塞传播到 BAL」。当前实现把阻塞原样暴露给 BAL。
3. **host 掩盖了问题**：`pal_hal_host.c:36-54` 的协作式虚拟时间推进让这段 polling 在 host 下不真正阻塞 wall-clock，于是 host 测试无法发现真机实时性问题（`pal_hal_host.c:10-11` 注释已诚实标注这一耦合风险）。

**依据**：skill 核心原则 4、grilling 问题5、contracts.md、safety-checklist 阶段9（无出口死循环/看门狗）。

**建议**：
- **(A) 硬件中断/脉宽捕获方案（推荐，高精度且非阻塞）**：由于 HC-SR04 的脉宽是微秒级（1us 约对应 0.017cm，10ms Tick 相当于 170cm 的粗糙度），无法依靠 10ms 协作式 Tick 状态机进行准确采样。更好的方向是：PAL 层提供硬件脉冲捕获或 GPIO 双沿中断结合定时器捕获的能力（在中断中通过 `pal_get_us()` 记录脉宽），DAL 层仅保留读取缓存距离的非阻塞语义 API。这样既解决了阻塞 busy-wait 饿死调度的问题，又保证了微秒级的测距精度。
- (B) 线程队列方案（原 grilling Q5）：由 DAL 工作线程在后台进行阻塞轮询并将数据推入队列。但当前 `pal_osal.h` 仅有互斥锁，不支持任务创建，此方案需扩展 OSAL 接口，对于超声波这种简单的脉宽捕获存在轻微过度设计风险。

---

### 🟡 中 — 编码硬规则违规 / 架构缺口 / 文档漂移

#### M1. 单行 `if` 无大括号 —— 违反 BARR-C 第 1 条（安全关键硬规则）
**位置**：`dal/src/dal_servo.c:7,9,10,17,18`；`dal/src/dal_ultrasonic.c:21,28,39`

```c
if (dev == NULL) return WINK_ERR_INVALID_ARG;                              /* ❌ */
if (angle < 0.0f) angle = 0.0f;                                             /* ❌ */
if (!pal_pwm_init(dev->pwm_channel, SERVO_PWM_FREQ_HZ)) return WINK_ERR_IO; /* ❌ */
```

clean-code.md「BARR-C 安全编码四条」第 1 条 + c-code.md 强制：`if/for/while/do-while` 即使单行也必须加大括号（防 Apple `goto fail` 类漏洞）。这是安全关键项目的不可商量项。`-Wall -Wextra` 不抓，但项目计划引入的 clang-tidy `cert-*`/`misra-*` 子集会卡。

**建议**：全部补大括号。例如：
```c
if (dev == NULL) {
    return WINK_ERR_INVALID_ARG;
}
```

---

#### M2. DAL 驱动缺 `init` —— GPIO 方向未配置，真机行为未定义，且可能存在通道冲突
**位置**：`dal/include/dal_ultrasonic.h`、`dal/src/dal_ultrasonic.c:38-59`

真机分支直接 `pal_gpio_write(dev->trig_pin, …)` / `pal_gpio_read(dev->echo_pin)`，但**全代码库无任何处配置 trig=output / echo=input**（没有 `dal_ultrasonic_init`，也没有 `pal_gpio_init(trig, OUTPUT)`）。contracts.md 模板的 `Preconditions: dal_ultrasonic_init(dev) must be called` 在实际头文件里**没有对应函数**。lifecycle.md §3 要求 `dal_xxx_init` 幂等、§6 要求 `health` 状态机——两者皆缺。另外，舵机在每次设置角度时都重复初始化 PWM 通道，缺乏全局一次性 init 入口。

**后果**：物理 target 部署后，第一次 `read` 会读写**未配置方向的引脚**，导致数据读写失败或硬件行为未定义；且舵机频繁重置 PWM 可能产生电气毛刺并浪费时钟周期。真机下这比大括号问题实际得多。

**依据**：lifecycle §3/§6、contracts.md、c-code.md §2。

**建议**：
1. **提升该项至 P0 优先级**，在 DAL 层补充 `dal_ultrasonic_init` 与 `dal_servo_init` 显式完成引脚及通道初始化。
2. **结合静态资源冲突校验设计**：在 init 阶段引入静态引脚与 PWM 通道的注册占用机制，若发现不同设备声明并占用了相同的引脚或物理通道，则在初始化时返回 `WINK_ERR_COLLISION`。

---

#### M3. `wink_trace` 并发契约未声明 + `s_count++` 非原子（真机潜在数据竞争）
**位置**：`trace/src/wink_trace.c:8-19`、`trace/include/wink_trace.h`

`s_count++` 与 `s_head` 的读-改-写在单线程主循环下安全（grilling Q3 已声明仿真不证明并发）。但：
- `wink_trace.h` **未声明 thread-safety 契约**（contracts.md 要求每个公共 API 标注）；
- `app_on_fault` 的语义暗示 fault 可能从异步路径上报，而 `wink_trace_fault` 在 `app_loop` 错误路径、`app_on_fault`、未来 ISR 通知路径都会被调用。一旦真机引入多任务上报，`s_count++`/`s_head` 无保护 → 环形索引错乱、丢记录或越界。

concurrency.md「`volatile` ≠ 原子 ≠ 内存序」明确：单核 ISR 共享 RMW 需关中断/临界区。

**依据**：contracts.md、concurrency.md「volatile ≠ 原子」、safety-checklist 阶段4/5、grilling Q3。

**建议**：在 `wink_trace.h` 契约里显式标注 `Thread-safe: No（仅限 runtime 主循环单上下文调用）`；若将来放开多上下文，用关中断临界区保护 `fault` 写入路径（`s_count`/`s_head`）。

---

#### M4. PAL 失败型 API 全量返回 `bool`，丢失错误语义且违背全局一致性
**位置**：`pal/include/pal_hal.h:40,55,60,72,79,95`；`pal/include/pal_osal.h:57,62`

`pal_gpio_init`/`pal_pwm_init`/`pal_i2c_transfer`/`pal_mutex_lock` 等全部返回 `bool`。导致上层只能 `if (!pal_pwm_init(…)) return WINK_ERR_IO;`（`dal_servo.c:17-18`），把所有失败（INVALID_ARG / BUSY / 真实 IO）一律折叠成 `WINK_ERR_IO`，**丢失诊断信息**。

从 **API 一致性** 和 **AI 代码生成（AI Codegen）友好度** 考量，混用 `bool` 与 `wink_status_t` 会增加 AI 生成反向判定（如 `if (!pal_xxx)` vs `if (dal_xxx != WINK_OK)`) 的 bug 概率。如果全量返回 `wink_status_t`，则全栈均可使用 `wink_status_is_error(status)`，且 DAL 可以直接透传错误码，无缝传播诊断信息。考虑到当前 ESP32 适配层（`pal_hal_esp32.c`）仍处于 skeleton 骨架阶段，此时是重构 PAL 返回值签名的黄金窗口期，重构成本最低。

**依据**：error-codes.md「错误传播」、clean-code.md「BARR-C 第 4 条：返回值检查」、ADR-0001、一贯性规范。

**建议**：将所有可能失败的 PAL 接口返回值重构为 `wink_status_t`，但建议**分阶段迁移**以降低风险：
1. **第一阶段**：优先修改硬件 IO 与外设配置型 API（`pal_gpio_init`、`pal_pwm_init`、`pal_pwm_set_duty`、`pal_i2c_transfer`、`pal_gpio_enable_interrupt` 等）。这能以最小的重构面，让 DAL 能够直接向上透传底层外设的具体失败码（如 `WINK_ERR_BUSY`/`WINK_ERR_INVALID_ARG`），消除直接折叠成 `WINK_ERR_IO` 的弊端，且大幅提高 AI 生成的逻辑一致性。
2. **第二阶段**：再重构 OSAL 层的同步锁（`pal_mutex_lock`/`pal_mutex_unlock`），与硬件失败链隔离，避免一次性重构动摇过多单元测试及多 target 的骨架。

---

#### M5. `simulation.md` SSOT 滞后于代码（活文档不实）
**位置**：`.claude/skills/embedded-best-practice/references/static-dispatch/simulation.md:38-49`

文档「双模直通实现模板」仍展示旧 L1 直通形态 `js_sim_get_ultrasonic_distance(uint16_t trig_pin, float *distance_cm)`，并标注其为当前 `dal_ultrasonic.c`。代码早已演进。docs-adr.md「Decision Backporting (SSOT)」要求活文档代表最新真相。同一符号还在 H1 的 CMakeLists 里造成真实 bug。

**依据**：docs-adr.md §2、pitfalls 陷阱3。

**建议**：更新 `simulation.md §2` 范例为当前的 `js_sim_trigger_ultrasonic` + `js_sim_measure_echo_pulse_us` 两段式旁路，与代码、`wasm_bridge.h`、pitfalls 陷阱3 对齐。

---

### 🟢 低 — 效率 / 一致性 / 风格

- **L1. `dal_servo_set_angle` 每次都 `pal_pwm_init`**（`dal/src/dal_servo.c:17`）：应一次性初始化（放进未来的 `dal_servo_init`）。频繁重配频率可能产生硬件毛刺、浪费周期。
- **L2. App 错误处理不对称**（`samples/avoidance_car/app_main.c:15-17,28-32,38-39`）：`app_loop` 对 ultrasonic 错误正确 `wink_trace_fault`，但 servo 的返回值一律 `(void)r;` 吞掉（含 `app_init` 的 set_angle 失败）。`(void)` 虽算"显式标注"，但 init 失败无 fault 处理，违反 AI 禁令3 的精神（错误码应向 App 传播做降级）。
- **L3. 魔法数字**（`dal/src/dal_servo.c:10,15`）：`180.0f`、`20.0f`（周期）、`0.0f` 裸用，建议提宏 `SERVO_MAX_ANGLE_DEG`、`SERVO_PERIOD_MS`。
- **L4. 整数↔函数指针 cast**（`targets/wasm/wasm_entry.c:25`、`targets/wasm/pal_hal_wasm.c:24`）：`(pal_gpio_isr_t)(uintptr_t)callback_index` 把 JS 传回的 uint32 当函数指针调用——wasm32 下可行，但 wasm64 会截断；`if (isr != NULL)` 不防"错误非零索引"。属仿真沙箱信任边界，可接受但应注释风险，理想方案改用 Emscripten `addFunction` 的真实 function table 索引。
- **L5. 契约字段不全**（`dal/include/dal_servo.h:24-31`）：缺 contracts.md 模板要求的 `Preconditions`（min/max_pulse_ms 有效、dev 已 init）与 `Callback-context`。
- **L6. esp32 PAL 全骨架**（`targets/esp32/pal_hal_esp32.c`、`pal_osal_esp32.c`）：标 `@status ROADMAP`，预期内，移植时填充。

---

## 四、Safety Review（按 `embedded-best-practice` 编辑后安全审查协议输出）

```text
Safety review:
- Risk level: 中-高（含 1 处 wasm 运行期配置失效 + 1 处真机实时性违背；二者被 host 测试现状掩盖）
- Checklist phases run: 1（编译/语法）、2（逻辑正确性）、3（内存安全）、
  4（线程安全）、7（资源生命周期）、8（硬件交互）、9（鲁棒性）、
  10（代码质量）、11（SOLID/Clean Code）、12（影响分析）
- Findings:
  · 致命: 0
  · 高:   H1（ASYNCIFY_IMPORTS 引用已删除符号 + 缺 delay 挂起导致运行时挂起异常）、
          H2（ultrasonic 阻塞 60ms 破坏 10ms tick，10ms 协作式 Tick 状态机精度不足，推荐采用 PAL 层硬件中断捕获重构）
  · 中:   M1（单行 if 无大括号/BARR-C）、M2（DAL 缺 init/GPIO 方向未配，建议提至 P0 并加入资源冲突校验）、
          M3（trace 并发契约未声明 + s_count++ 非原子）、M4（PAL 返回 bool 丢语义，建议分阶段迁移重构为 status）、
          M5（simulation.md SSOT 滞后）
  · 低:   L1-L6（效率/错误对称性/魔法数/契约完整性/仿真边界 cast/esp32 骨架）
- Fixed: 无（本次为只读评审，未改动代码）
- Assumptions:
  · esp32 target 为 ROADMAP 骨架，真机功能缺口（M2）待移植时一并处理
  · H1 的 emcc 精确行为建议在 Emscripten 环境实测确认（host 测试不可达该路径）
  · wink_trace 当前仅主循环单上下文调用，M3 为前瞻性风险
- Commands run: 全程只读（Glob/Read），未执行构建/测试
```

---

## 五、建议优先动作（Roadmap）

| 优先级 | 项 | 理由 | 预估工作量 |
|--------|----|----|-----------|
| **P0** | **H1** 修 `ASYNCIFY_IMPORTS` | 解决 `js_pal_delay_ms` 未插桩导致的运行时挂起崩溃，并删除死符号引用 | 小（需 emcc 环境） |
| **P0** | **H2** 重构 ultrasonic 阻塞为**硬件中断/脉宽捕获** | 彻底解决真机阻塞，且利用 PAL 层中断捕获维持微秒级测距精度，避免 10ms Tick 导致精度崩溃 | 中 |
| **P0** | **M2** 补 `dal_xxx_init` 并引入资源冲突校验 | 解决真机部署引脚方向未配的致命问题；在初始化阶段静态校验 GPIO 与 PWM 通道冲突 | 小 |
| **P1** | **M4** 重构 PAL 失败型 API（分阶段） | 确立全套 status 范式提升 AI 生成成功率，分阶段先改硬件 IO API 以降低集成测试回归风险 | 中 |
| **P1** | **M1** 补大括号 | 安全关键硬规则，机械修复，低风险 | 极小 |
| **P1** | **M5** 同步 `simulation.md` | 纠正 SSOT 文档漂移，与 H1 联动 | 极小 |
| **P2** | **M3** trace 契约标注 thread-safety | 文档级标注，防未来并发踩坑 | 极小 |
| **P2** | **L2–L6** | 包含错误对称性校验、消除魔数、Wasm 指针强转风险标注、esp32 移植等 | 小-中 |

> H1、H2、M2、M4 是本次评审中影响“物理 target 运行”、“同源编译与执行”以及“AI 代码生成一致性”的关键架构基石。按项目 Workflow 规则，复杂重构（如 H2 中断捕获与 M4 分阶段错误码重构）应先产出 Implementation Plan 获得确认再动手。

---

## 六、整改跟踪

| 评审发现 | 整改方式 | 落地位置 | 状态 |
|---|---|---|---|
| H1 — ASYNCIFY_IMPORTS 配置失效与死符号 | 修正白名单为 `js_pal_delay_ms/us` + emcc 验证运行期挂起 | `wink-micro-os/CMakeLists.txt:33` | ⬜ 未开始 |
| H2 — ultrasonic 阻塞 polling | 重构为 PAL 中断/捕获结合 DAL 非阻塞读取缓存模式 | `dal/src/dal_ultrasonic.c:38-59`、`dal/include/dal_ultrasonic.h:24` | ⬜ 未开始 |
| M1 — 单行 if 无大括号 | 补 BARR-C 大括号 | `dal/src/dal_servo.c`、`dal/src/dal_ultrasonic.c` | ⬜ 未开始 |
| M2 — DAL 缺 init 与资源占用冲突 | 补 `dal_xxx_init` 并引入静态通道/引脚占用表校验（检测 `WINK_ERR_COLLISION`） | `dal/include/*.h`、`dal/src/*.c` | ⬜ 未开始 |
| M3 — trace 并发契约未声明 | 头文件标注 thread-safety 限制 | `trace/include/wink_trace.h` | ⬜ 未开始 |
| M4 — PAL 返回 bool 丢语义且影响一致性 | 分阶段重构：先改硬件 IO API，再改 OSAL mutex，统一为 `wink_status_t` | `pal/include/pal_hal.h`、`pal_osal.h` | ⬜ 未开始 |
| M5 — simulation.md SSOT 滞后 | 更新范例为两段式旁路 + 删死符号 | `…/embedded-best-practice/references/static-dispatch/simulation.md §2` | ⬜ 未开始 |
| L1–L6 | 见第三节低优先项逐条，L1 与 M2 整合，L2 进行错误对称性校验 | 散落多处 | ⬜ 未开始 |

---

## 七、分模块评分矩阵

| 模块 | 架构合规 | 规范符合 | 安全/实时 | 仿真同源 | 综合 |
|---|---|---|---|---|---|
| PAL（HAL+OSAL 契约） | 8.5 | **6.5**（bool 丢语义） | 假锁已知 | — | 7.2 |
| DAL（servo/ultrasonic） | 8.5 | 7.0（缺 init/health） | **H2 阻塞违背** | 9.0（决策2 优秀） | 7.5 |
| runtime（App 调度） | 8.0 | 7.5 | 协作式 WCET 不可静态界定（H2 放大） | — | 7.5 |
| trace | 8.0 | 7.0（契约缺并发） | 单线程安全/多任务隐患（M3） | — | 7.5 |
| targets/wasm | 8.0 | **6.0**（H1 ASYNCIFY 失效） | 主循环挂起失效（H1） | 7.0 | 6.5 |
| targets/host | 9.0 | 8.5 | 虚拟时间推进设计精良 | 9.0（真机分支可测） | 8.8 |
| targets/esp32 | — | — | ROADMAP 骨架 | — | N/A |
| samples（App 示例） | 8.5 | 7.5（错误处理不对称 L2） | — | — | 8.0 |
| build/test（CMake+Unity） | 8.0 | 8.5（-Werror + sim/真机分测） | — | H1 配置漂移 | 7.5 |
| **加权综合** | **8.4** | **7.3** | — | **8.0** | **8.0** |

> 规范符合度（7.3）是最低分，集中在 PAL bool 语义、wasm ASYNCIFY 配置、DAL init 缺口；仿真同源（8.0）与架构合规（8.4）是强项。

---

*评审人立场：本报告为 2026-06-24 时点对 `wink-micro-os/` 代码实现的判断快照。归档后按 reviews 约定只读；后续整改结论回写至对应 ADR / 设计规范 / 代码后，本报告中的相关表述不再代表当前事实。*

