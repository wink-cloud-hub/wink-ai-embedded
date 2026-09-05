# WinkMicroOS 代码优化执行计划（2026 Q3 P0/P1 综合整改）

## 1. 元数据表

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3` |
| **创建日期** | 2026-07-01 |
| **目标平台/SoC** | `host` / `wasm` / `ESP32`（三 target 同步；PAL/DAL 层跨切） |
| **工具链/SDK 版本** | ESP-IDF v6.0.1（EIM profile）、GCC 15+（host / MinGW WinLibs）、Emscripten 6.0.1、CMake ≥ 3.20 |
| **计划状态** | ✅ 已完成（2026-07-02 结项） |
| **优先级** | 🔴 P0（Track A：`pal_resource` 空壳）+ 🟡 P1（Track B/C/D/E：AI Codegen 硬化） |
| **计划版本** | v1.0 |
| **关联评审记录** | [`../../reviews/core/2026-07-01-external-comprehensive-review-critique.md`](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md)（本计划的直接输入） |
| **关联外部报告（次级输入）** | `C:\Users\77174\.gemini\antigravity-ide\brain\6bd79a7f-0291-4da5-8560-c34aa047271f\wink_micro_os_critique_review_report.md`（Gemini 二次核验） |
| **关联设计规范** | [`../02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md)、[`../02-wink-micro-os/03-device-abstraction-layer.md`](../02-wink-micro-os/03-device-abstraction-layer.md) |
| **关联 ADR（既有）** | [ADR-0001 错误码](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0004 静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0007 协作式执行](../../decisions/core/0007-cooperative-loop-execution-model.md)、[ADR-0009 物理仿真](../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)、[ADR-0012 契约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) |
| **关联 ADR（本计划新增）** | ADR-0015 PAL GPIO 状态传播接口重构、ADR-0016 临界区上下文感知、ADR-0017 阻塞 API 硬隔离与 `WINK_BLOCKING` 属性 |
| **前置依赖计划** | [`./2026-07-01-sim-cooperative-scheduler-plan.md`](../unisim/2026-07-01-sim-cooperative-scheduler-plan.md)（Track E 需其 T5 fiber 落地）、[`./2026-07-01-pal-target-p1-maintainability-plan.md`](./2026-07-01-pal-target-p1-maintainability-plan.md)（✅ 已完成，本计划部分 Task 会碰同一片文件） |
| **替代/废弃** | 无 |
| **计划负责人** | wink-ai PAL/DAL/Runtime 组 |
| **所需子代理技能** | `embedded-best-practice` + `test-driven-development` + `systematic-debugging` |

---

## 2. 背景与目标

### 2.1 问题陈述

2026-07-01 对外部 Antigravity 综合评审报告进行批判性核验（见关联评审记录）后，识别出**5 条真正需要执行的整改事项**（另有 7 条报告论断已被撤回，2 条被降级/取消）。本计划把这 5 条整合为一个可执行的 Q3 优化包：

| Track | 事项 | 优先级 | 触发原因 |
|-------|------|--------|---------|
| **A** | `pal_resource` 空壳接线闭环 | 🔴 P0 | 核验证据：7 个 DAL 驱动仅 `dal_ssd1306` 调用过 `pal_resource_claim`，其余 6 个（`dal_button/dal_led/dal_servo/dal_ultrasonic/dal_gps/dal_eeprom`）完全未接线；`pal_resource` 框架**是空壳** |
| **B** | `pal_gpio_read/write` 返回值升 `wink_status_t` + out-param | 🟡 P1 | `pal_hal.h:61,63` 现况：`write` 返 `void`（`WINK_WARN_UNUSED_RESULT` 无法覆盖），`read` 返 `bool`（读失败无法上报）；与已升级的 `pal_pwm_*` 不对称 |
| **C** | `wink_status_t` 错误码目录（AI Codegen 语义手册） | 🟡 P1 | 现有 17 个负数码分散在 `pal_status.h` 且无统一语义/恢复策略手册；直接影响 AI 生成代码的错误处理鲁棒性 |
| **D** | `pal_os_critical_enter` 任务/ISR 双入口契约锁死 | 🟡 P1 | 核验证据：ESP32 版 `pal_os_critical_enter` 使用 `portENTER_CRITICAL`（task-only），`wink_trace_fault` 若从 ISR 调用会触发 Xtensa 级别异常；协作式调度器上线前必须先敲定 |
| **E** | 阻塞 DAL API 编译期硬隔离（`WINK_BLOCKING` 属性） | 🟡 P1 | 现况：`dal_ultrasonic_read` 只有 doxygen `@deprecated` 注释，无编译期告警；协作式调度器上线后（`sim-cooperative-scheduler-plan.md` 落地时机），AI 从旧样例 grep 复用此 API 会直接饿死真机 WDT |
| **F**（附属） | `pal_irq_direct_connect` API 契约改名 | 🟢 P2 | 当前实际走 `generic_isr_wrapper` 软件派发，"零延迟"契约虚标；应改名为 `pal_irq_iram_bind` 或降级 |

### 2.2 为什么现在必须解决

三条**时间敏感**理由（决定 Track A/D/E 必须在协作式调度器落地前完成）：

1. **调度器落地窗口冲突**：`2026-07-01-sim-cooperative-scheduler-plan.md`（v1.3）已在推进；一旦单虚拟核协作式调度上线：
   - Track E（阻塞 API 硬隔离）**必须**先落地，否则 AI 生成的样例代码会用 60ms 忙等 → WDT 饿死；
   - Track D（临界区 ISR 契约）**必须**先落地，否则 fault trace 从 ISR 调用会崩；
2. **API 变更成本呈指数**：Track B（`pal_gpio_read/write`）是跨 target + 跨 DAL 的公共 API 变更。当前只有 2 个 DAL 直接调用点（`dal_button.c:28`、`dal_led.c:28`）+ 若干 target 内部/sample 调用点；一旦更多 AI 生成 App 积攒，重构成本**每个 sprint 呈翻倍增长**；
3. **`pal_resource` 空壳直接影响仿真保真度**：Track A 不做，两端同源仿真的引脚冲突检测**完全失效**——用户在 host/wasm 上不会看到冲突，一到真机就电气冲突。这是仿真"保真度骗局"，与 ADR-0009 物理仿真的立项目标直接矛盾。

### 2.3 技术/业务目标

- ✅ **T1（Track A）**：6 个尚未接线的 DAL init 全部调用 `pal_resource_claim`；新增 `samples/resource_conflict/` 反例样本；`test/test_pal_resource_wire.c` 覆盖每个 DAL 的冲突路径。
- ✅ **T2（Track B）**：`pal_hal.h` 中 `pal_gpio_read/write` 签名重构为 `wink_status_t + out-param`；三 target 实现同步；DAL 层与 sample 全量迁移。
- ✅ **T3（Track C）**：`docs/design/01-system-overall/error-code-catalog.md` 成文；每个 `wink_status_t` 值有语义 / 触发场景 / 恢复策略 / 是否可作 `WINK_PT_EXIT` 条件四栏描述；接入 codegen prompt few-shot 存储位置。
- ✅ **T4（Track D）**：`pal_osal.h` 新增 `pal_os_critical_enter_isr / exit_isr` 双入口；三 target 实现分流；`wink_trace_fault` 分裂为 task 版与 ISR 版（或引入 context-aware 单一入口，见 ADR-0016 决策）。
- ✅ **T5（Track E）**：新增 `WINK_BLOCKING` / `WINK_DEPRECATED_MSG` 属性宏；`dal_ultrasonic_read` 挂上 `WINK_BLOCKING` 并在 `WINK_STRICT_NONBLOCKING` 编译选项下从符号表移除；协作式调度器构建默认开启该选项。
- ✅ **T6（Track F）**：`pal_irq_direct_connect` 改名 `pal_irq_iram_bind` 或降级；doxygen 契约删除"零延迟分发"字样。
- ✅ **T7（跨 Track 通用）**：零回归 —— host `python wink-tools/wink.py test` 全绿、wasm smoke 通过、ESP32 `idf.py build` 零 error 零 warning。
- ✅ **T8（跨 Track 通用）**：新增 3 份 ADR（0015/0016/0017），全部标 `Accepted` 并回写至 `01~07` 设计规范。

### 2.4 成功指标（验收出口）

| 指标 | 通过标准 | 验证方法 |
|------|---------|---------|
| Track A：DAL claim 覆盖率 | 6/6 DAL 在 init 阶段调用 `pal_resource_claim` | `grep pal_resource_claim wink-micro-os/dal/src/**/*.c` 命中 ≥ 7 处（含 `dal_ssd1306`） |
| Track A：冲突反例样本 | `samples/resource_conflict/` 存在且 host/wasm 均通过 | `python wink-tools/wink.py test` 全绿 + wasm smoke |
| Track A：冲突单测 | `test_pal_resource_wire.c` 覆盖每个 DAL 类型的 GPIO / PWM / I2C claim 冲突 | `python wink-tools/wink.py test` 新增用例通过 |
| Track B：API 迁移完整性 | `grep -rn "bool pal_gpio_read\|void pal_gpio_write" wink-micro-os/` **零命中** | ripgrep |
| Track B：DAL 调用点迁移 | `dal_button.c` / `dal_led.c` / `pal_hal_ultrasonic.c` / `pal_hal_esp32_gpio.c` 全部使用新签名 | `grep -rn "pal_gpio_read(" wink-micro-os/ \| grep -v "&"` 命中数 = 0（新签名必带 `&out_level`） |
| Track C：错误码手册 | `docs/design/01-system-overall/error-code-catalog.md` 存在且 17+ 条码覆盖率 100% | 手册中枚举 vs `wink_status.h` 定义 diff |
| Track D：ISR 契约 API | `pal_osal.h` 声明 `pal_os_critical_enter_isr / exit_isr` 或 context-aware 变体 | 头文件 grep |
| Track D：`wink_trace` ISR 安全 | `wink_trace_fault_from_isr` 存在或 `wink_trace_fault` doxygen 明确禁止 ISR 调用 | 头文件 grep |
| Track E：`WINK_BLOCKING` 属性 | `WINK_BLOCKING` 宏定义在 `wink_status.h`；`dal_ultrasonic_read` 挂载 | `grep WINK_BLOCKING wink-micro-os/dal/include/**/*.h` |
| Track E：严格模式编译剔除 | `-DWINK_STRICT_NONBLOCKING=1` 编译时 `dal_ultrasonic_read` 符号不可见 | `nm` 或链接失败测试 |
| Track F：`pal_irq_direct_connect` 契约 | 头文件删除"零延迟分发"字样，或函数改名 | `grep "直接连接\|zero-latency\|direct connect" wink-micro-os/pal/include/hal/pal_irq.h` 命中为 0 |
| 跨 Track：host 测试 | `python wink-tools/wink.py test` 100% 通过 | 命令行 |
| 跨 Track：wasm smoke | `node wink-micro-os/targets/wasm/wink_sim_stub.js` PASS | 命令行 |
| 跨 Track：ESP32 构建 | `idf.py -C esp32_firmware build` 0 error / 0 warning | 命令行 |
| 跨 Track：ADR 归档 | 3 份新 ADR（0015/0016/0017）Accepted 状态 | `python docs/decisions/scripts/list_adrs.py -s Accepted` 命中 ADR-0015/0016/0017 |
| 跨 Track：SSOT 回写 | 设计规范 §02 / §03 同步更新 | git diff review |

---

## 3. 变更范围与影响分析

### 3.1 文件变更清单

#### 3.1.1 Track A：`pal_resource` 接线闭环（P0）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `wink-micro-os/dal/src/actuator/dal_servo.c` | ✏️ 修改 | `dal_servo_init` 追加 `pal_resource_claim(PWM_CHANNEL, cfg->channel, owner)` |
| `wink-micro-os/dal/src/input/dal_button.c` | ✏️ 修改 | `dal_button_init` 追加 `pal_resource_claim(GPIO_PIN, cfg->pin, owner)` |
| `wink-micro-os/dal/src/output/dal_led.c` | ✏️ 修改 | `dal_led_init` 追加 `pal_resource_claim(GPIO_PIN, cfg->pin, owner)` |
| `wink-micro-os/dal/src/sensor/dal_ultrasonic.c` | ✏️ 修改 | `dal_ultrasonic_init` 追加 `pal_resource_claim` GPIO×2 (trig + echo) |
| `wink-micro-os/dal/src/communication/dal_gps.c` | ✏️ 修改 | `dal_gps_init` 追加 `pal_resource_claim(UART_PORT + GPIO×2)` |
| `wink-micro-os/dal/src/storage/dal_eeprom.c` | ✏️ 修改 | `dal_eeprom_init` 追加 `pal_resource_claim(I2C_ADDR)`（若挂 I2C）；纯 SPIFFS 版本可豁免 |
| `wink-micro-os/pal/include/hal/pal_resource.h` | ✏️ 修改（附加） | 补 `pal_resource_type_t` 缺失枚举（若 UART 未定义则新增 `PAL_RESOURCE_UART_PORT`） |
| `wink-micro-os/pal/include/hal/pal_resource.h` | ✏️ 修改（附加） | doxygen：明确 owner 字符串生命周期（rodata 只读、DAL 传 `__func__` 允许） |
| `wink-micro-os/samples/resource_conflict/CMakeLists.txt` | 🆕 新增 | 反例样本构建 |
| `wink-micro-os/samples/resource_conflict/app_main.c` | 🆕 新增 | 两个 DAL 实例故意抢同一 GPIO/PWM，验证第二个 init 返 `WINK_ERR_BUSY` |
| `wink-micro-os/samples/resource_conflict/README.md` | 🆕 新增 | 反例样本说明 |
| `wink-micro-os/test/test_pal_resource_wire.c` | 🆕 新增 | 6 个 DAL 类型的 claim 冲突单测（每 DAL 至少 1 组冲突用例） |
| `wink-micro-os/test/CMakeLists.txt` | ✏️ 修改 | 追加 `test_pal_resource_wire.c` |

#### 3.1.2 Track B：`pal_gpio_read/write` 返回值重构（P1）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `wink-micro-os/pal/include/hal/pal_hal.h` | ✏️ 修改 | 第 61/63 行签名重构，返 `wink_status_t + out-param`，加 `WINK_WARN_UNUSED_RESULT` |
| `wink-micro-os/targets/host/pal_hal_host.c` | ✏️ 修改 | 第 56/58 行实现同步；未 claim 引脚返 `WINK_ERR_INVALID_STATE` |
| `wink-micro-os/targets/wasm/pal_hal_wasm.c` | ✏️ 修改 | 第 36/40 行实现同步 |
| `wink-micro-os/targets/esp32/pal_hal_esp32_gpio.c` | ✏️ 修改 | 第 191/197 行实现同步；`pal_gpio_wait_level` 第 352/359 行内部调用同步（关键）；ESP_PLATFORM=0 stub 第 374/375 行同步 |
| `wink-micro-os/targets/esp32/pal_hal_ultrasonic.c` | ✏️ 修改 | 第 37/39 行调用点同步 |
| `wink-micro-os/dal/src/input/dal_button.c` | ✏️ 修改 | 第 28 行 `bool raw = pal_gpio_read(...)` 改为 out-param 形式；错误路径按 §4.3 决策 |
| `wink-micro-os/dal/src/output/dal_led.c` | ✏️ 修改 | 第 28 行 `pal_gpio_write(...)` 改为返回值传播 |
| `wink-micro-os/pal/include/hal/pal_hal_rmt.h` | ✏️ 修改 | 第 33/35 行 doxygen 示例代码同步 |
| `wink-micro-os/samples/oled_dashboard/app_main.c` | ✏️ 修改 | 第 10 行注释同步（host 语义仍不变，但接口变了） |
| `wink-micro-os/samples/devkitc_smoke/test_devkitc_smoke_e2e.c` | ✏️ 修改 | 第 6 行注释同步 |
| `wink-micro-os/test/test_pal_gpio.c` | 🆕 新增（或已有则修改） | 覆盖新签名的：正常 / 未 claim / 越界引脚三条路径 |

#### 3.1.3 Track C：错误码目录手册（P1）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `docs/design/01-system-overall/error-code-catalog.md` | 🆕 新增 | 17+ 条 `wink_status_t` 值的语义手册 |
| `docs/implementation-plans/scripts/README.md` | ✏️ 修改 | 增加对新文档的索引 |
| `wink-micro-os/pal/include/wink_status.h` | ✏️ 修改 | 在每个 `WINK_ERR_*` 定义旁增加简短 doxygen `@brief`，与手册同步（≤ 1 行/枚举值） |

#### 3.1.4 Track D：临界区 ISR 双入口（P1）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `wink-micro-os/pal/include/osal/pal_osal.h` | ✏️ 修改 | 第 131-137 行新增 `pal_os_critical_enter_isr / exit_isr`；旧 `pal_os_critical_enter` doxygen 补 "task-only, ISR 调用行为未定义" |
| `wink-micro-os/targets/esp32/pal_osal_esp32.c` | ✏️ 修改 | 实现 ISR 版：使用 `portENTER_CRITICAL_ISR(&s_global_mux)` |
| `wink-micro-os/targets/host/pal_osal_host.c` | ✏️ 修改 | 实现 ISR 版：单线程可退化为 no-op 或与 task 版共用（documented） |
| `wink-micro-os/targets/wasm/pal_osal_wasm.c` | ✏️ 修改 | 实现 ISR 版：wasm 无真实 ISR，同 host 退化 |
| `wink-micro-os/targets/baremetal/pal_osal_bare.c` | ✏️ 修改 | 实现 ISR 版：裸机上关中断即可 |
| `wink-micro-os/trace/include/wink_trace.h` | ✏️ 修改 | 新增 `wink_trace_fault_from_isr()` 声明 |
| `wink-micro-os/trace/src/wink_trace.c` | ✏️ 修改 | 拆分双入口实现；旧 `wink_trace_fault` doxygen 明标 "task-only" |
| `wink-micro-os/test/test_wink_trace.c` | ✏️ 修改（或新增） | 新增 ISR 上下文 fault 记录路径的等价性测试 |

#### 3.1.5 Track E：阻塞 API 硬隔离（P1）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `wink-micro-os/pal/include/wink_status.h` | ✏️ 修改 | 新增 `WINK_BLOCKING` / `WINK_DEPRECATED_MSG(msg)` / `WINK_NONBLOCKING_ONLY` 属性宏 |
| `wink-micro-os/dal/include/sensor/dal_ultrasonic.h` | ✏️ 修改 | `dal_ultrasonic_read` 挂 `WINK_BLOCKING` + `WINK_DEPRECATED_MSG(...)`；用 `#ifndef WINK_STRICT_NONBLOCKING` 包围 |
| `wink-micro-os/dal/src/sensor/dal_ultrasonic.c` | ✏️ 修改 | 同上包围实现 |
| `wink-micro-os/samples/*/CMakeLists.txt`（协作式调度样例） | ✏️ 修改 | 追加 `-DWINK_STRICT_NONBLOCKING=1` |
| `python wink-tools/wink.py test` | ✏️ 修改 | 新增 L1 lint：确认协作式调度器构建下 `dal_ultrasonic_read` 符号不可见 |

#### 3.1.6 Track F：PAL 中断 API 收窄与安全隔离（P2）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `wink-micro-os/pal/include/pal_irq.h` | ✏️ 修改 | 收窄优先级枚举值（3 级）；统一回调签名；将高级 API 移出此文件 |
| `wink-micro-os/pal/include/pal_irq_advanced.h` | 🆕 新增 | 新置高级/系统级中断 API（含 `pal_irq_synchronize` 等）并配置宏门控 |
| `wink-micro-os/targets/common/include/pal_shared_chain.h` | ❌ 删除 | 移除共享中断责任链数据结构定义 |
| `wink-micro-os/targets/common/src/pal_shared_chain.c` | ❌ 删除 | 移除共享中断 RCU 链算法实现文件 |
| `wink-micro-os/targets/esp32/pal_irq_esp32.c` | ✏️ 修改 | 移除 `direct_trampoline`、`shared_chain` 分发和 SMP 忙等逻辑；优先级映射为 3 级 |
| `wink-micro-os/targets/wasm/pal_hal_wasm.c` | ✏️ 修改 | 移除 `shared_chain` 和 `direct` 相关桩代码，优先级映射收窄 |
| `wink-micro-os/targets/host/pal_hal_host.c` | ✏️ 修改 | 同上，收窄优先级并删除共享中断桩实现 |
| `wink-micro-os/pal/include/hal/pal_hal.h` | ✏️ 修改 | GPIO 接口 `pal_gpio_enable_interrupt_ex` 的优先级对齐为 3 级 |

#### 3.1.7 ADR 新增

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `docs/decisions/core/0015-pal-gpio-safe-error-propagation.md` | 🆕 新增 | Track B 决策文档 |
| `docs/decisions/core/0016-pal-critical-section-task-isr-dual-entry.md` | 🆕 新增 | Track D 决策文档 |
| `docs/decisions/core/0017-blocking-api-hard-isolation.md` | 🆕 新增 | Track E 决策文档 |
| `docs/decisions/core/0018-pal-irq-api-narrowing.md` | 🆕 新增 | Track F 决策文档 |

#### 3.1.8 设计规范回写（ADR Accepted 后必须）

| 文件路径 | 变更类型 | 说明 |
|---------|---------|------|
| `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md` | ✏️ 修改 | 章节回写：GPIO API 新签名、临界区双入口 |
| `docs/design/02-wink-micro-os/03-device-abstraction-layer.md` | ✏️ 修改 | 章节回写：DAL init 必须 claim 资源 |
| `docs/design/07-platform-governance/*.md`（若有 API 稳定性章节） | ✏️ 修改 | `WINK_BLOCKING` 属性纳入 API 稳定性约束 |
| `docs/reviews/core/2026-07-01-external-comprehensive-review-critique.md` | ✏️ 追加尾部 | 增加"本 review 已被 PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 落地"标记 |

### 3.2 接口影响分析

| 接口层 | 是否破坏性 | 影响范围 | 备注 |
|--------|-----------|---------|------|
| PAL 公开 API (`pal_hal.h`) | ⚠️ **是**（Track B） | `pal_gpio_read/write` 签名变；所有直接/间接调用点必须迁移 | 通过 §4.3 迁移策略降低成本 |
| PAL 公开 API (`pal_osal.h`) | ⚠️ **附加**（Track D） | 新增 `_isr` 变体；旧 API 语义收紧（明标 task-only） | 非破坏，但需 CI lint 提醒未来使用 |
| PAL 公开 API (`pal_irq.h`) | ⚠️ **附加**（Track F） | 函数改名或加 deprecated 别名 | 非破坏（保留旧名映射） |
| DAL 公开 API | ⚠️ **附加**（Track A） | 6 个 `dal_*_init` 新增可能返回 `WINK_ERR_BUSY`（此前只可能 `WINK_ERR_INVALID_ARG`） | doxygen 补充新错误码 |
| DAL 公开 API | ⚠️ **收窄**（Track E） | `dal_ultrasonic_read` 在 `-DWINK_STRICT_NONBLOCKING` 下**符号不可见** | 属"编译期功能开关"，非签名破坏 |
| 应用层 (`samples/`) | ⚠️ 是 | 使用 `pal_gpio_read/write` 的 sample 需要迁移；受影响 sample 集小 | 见 §3.1.2 |
| 构建系统 (CMake) | ⚠️ 局部 | 三处：`resource_conflict` sample、`test_pal_resource_wire`、`WINK_STRICT_NONBLOCKING` 宏 | 无组件级重排 |
| 工具链 | ❌ 否 | 无 | |
| 文档 | ⚠️ 大量 | 3 份新 ADR + 3 份 SSOT 回写 + 1 份错误码手册 | Track C 独立成文 |
| 内部头文件 | ⚠️ 是 | `wink_status.h` 增加 `WINK_BLOCKING` 属性；`wink_trace.h` 增加 `_from_isr` 变体 | 见 §3.1 |

### 3.3 架构红线（⚠️ 违反即拒绝合入）

> 🚨 **R-1（Track A · Claim 完备性红线）**：Track A 完成后，`grep pal_resource_claim wink-micro-os/dal/src/` 命中的 DAL 文件数**必须 = 7**（含既有 `dal_ssd1306`）。任何一个 DAL 遗漏 `claim` = "空壳未接线"复发，直接拒绝合入。**样本代码验证**：`samples/resource_conflict/` 必须故意让第二个 DAL init 失败并断言，若断言不通过 = 接线失败。

> 🚨 **R-2（Track B · 兼容性无骑墙红线）**：`pal_gpio_read/write` **不允许**同时保留"新签名 + 旧签名 deprecated alias"两套。要么全量迁移，要么不做。理由：本项目北极星是 AI Codegen 友好，双 API 并存 = AI 陷阱（见 Track E 教训）。此红线由 §7 L4 架构 CR 卡口。

> 🚨 **R-3（Track C · 手册与代码同步红线）**：错误码手册中每一条 `WINK_ERR_*` 条目必须与 `wink_status.h` 中的定义**bit-for-bit** 一致（值、名称、注释）。CI lint 加入 `docs vs code` diff 检查（parse 手册表格 vs enum 定义）。

> 🚨 **R-4（Track D · ISR 契约无二义性红线）**：`pal_os_critical_enter` 在 ISR 中调用的行为必须是**明确**的三选一：(a) 定义为 UB 并 doxygen 标注禁用；(b) 内部 detect ISR 上下文并分流；(c) 保持 task-only 语义并要求 ISR 调用者显式使用 `_isr` 版本。ADR-0016 必须锁定选择哪一种。**默认建议**：(c) —— 最显式、最贴合 ESP-IDF 惯例、编译期即可 lint 命中误用。

> 🚨 **R-5（Track E · 硬隔离而非软提示红线）**：`WINK_BLOCKING` 属性**必须**同时挂载三层保护：
> 1. `__attribute__((deprecated))` 编译警告；
> 2. `#ifndef WINK_STRICT_NONBLOCKING` 符号级隔离（严格模式下从声明中消失）；
> 3. Runtime assert（若 blocking API 被 protothread 上下文调用，`WINK_PT_DEBUG` 下 panic）。
> 缺一即视为"软提示"，仍会被 AI Codegen 绕过。

> 🚨 **R-6（跨 Track · ADR 状态同步红线）**：**任何一个 ADR 转 Accepted 前不得动主分支代码**。三个 ADR（0015/0016/0017）必须先经用户确认 Proposed → Accepted 后再动手实施。ADR 与代码在同一 commit 或**至多相邻 2 commits** 内落地。违反 = 撤销合入。

> 🚨 **R-7（跨 Track · 单 PR 单 Track 红线）**：Track A/B/C/D/E/F **各自独立 PR**。不允许一个 PR 混合多 Track。理由：回滚粒度需要匹配风险等级——Track A/B 高风险需独立回滚，Track C 是纯文档不应阻塞代码 PR review。

> 🚨 **R-8（Track E · WDT 防线红线）**：`WINK_STRICT_NONBLOCKING` 编译标志必须默认在**所有 `runtime_cooperative_*` 相关 target build 中开启**。若某 build 未开启，构建配置 CI 直接失败（lint 检查 CMakeLists 中的编译定义）。

### 3.4 系统资源与并发约束评估

| 资源/安全维度 | 预计变化/开销 | 风险与限制 | 缓解/应对策略 |
|--------------|-------------|-----------|---------------|
| **ROM / Flash** | Track A：+2~3KB（DAL claim 调用 + strdup owner）；Track B：+0.5KB（out-param 展开）；Track C：0；Track D：+0.5KB（新增 ISR 版原语）；Track E：0（严格模式下减少符号）；总计 Δ ≈ +3~4KB | ESP32 flash 容量充裕（4MB+），不构成瓶颈 | 若 CI 检测总量增长 > 8KB → 触发 code review 排查 |
| **RAM (静态/全局)** | Track A：`pal_resource` 静态表已存在（`PAL_RESOURCE_MAX_CLAIMS`），Δ = 0；Track D：ESP32 需 `portMUX_TYPE` ISR-safe 版共享同一个 `s_global_mux`，Δ = 0 | 见前置 | - |
| **栈深度** | Track B：out-param 展开可能多 1 层，Δ ≤ +8 bytes/frame；Track A：DAL init 增加 claim 调用 ≤ +32 bytes/frame | ISR 上下文栈紧张的场景需关注 | Track D 的 ISR trace 路径专项检查 |
| **堆内存** | Δ = 0（`pal_resource` 表已静态） | - | - |
| **硬件通道/IO** | Track A：**这正是本 Track 要修的漏洞**——真正把物理硬件资源占用登记进 `pal_resource` | 若 Track A 前有 DAL 实例存在 owner 冲突，此次落地会**首次暴露**这些 bug | Task A-3 中提供迁移期"warn only"模式，Task A-5 转为 hard-fail |
| **并发与中断安全** | Track D：**本 Track 的核心风险区**。ISR 版临界区 API 上线前，`wink_trace_fault` 若被 ISR 调用会崩 | 见 R-4 红线；Track D 优先级仅次于 Track A | 迁移期在 `wink_trace_fault` 头 doxygen 增加显式警告，落地后立即拆双入口 |

---

## 4. 依赖与风险

### 4.1 前置依赖

| 依赖ID | 依赖内容 | 是否阻塞 | 验证状态 | 备注 |
|--------|---------|---------|---------|------|
| D-001 | ESP-IDF v6.0.1 EIM profile 可激活 | ✅ 是 | ✅ 已完成 | 记忆 `esp-idf-install-state` |
| D-002 | Host 工具链（WinLibs gcc/cmake）已装 | ✅ 是 | ✅ 已完成 | 记忆 `host-c-toolchain` |
| D-003 | `pal_resource.h` 框架已存在 | ✅ 是 | ✅ 已完成 | 核验证据：三 target 实现在位 |
| D-004 | `WINK_WARN_UNUSED_RESULT` 宏已存在 | ✅ 是 | ✅ 已完成 | `wink_status.h:11` |
| D-005 | 三份新 ADR（0015/0016/0017）经用户确认 Proposed → Accepted | ⚠️ **是**（R-6 红线） | ⏳ 待做 | Track 0 前置 |

### 4.2 外部依赖（非本项目可控）

无。全部改动在 wink-micro-os 内部。

### 4.3 风险登记册

| 风险ID | 风险描述 | 概率 | 影响 | 严重度 | 缓解措施 | 责任人 | 触发条件 |
|--------|---------|-----|-----|-------|---------|-------|---------|
| R-001 | Track A 一次性接线暴露既有引脚 owner 冲突（比如某 sample 早就同 pin 挂了两个 DAL 而没被发现） | 🟡 中 | 🟠 中 | 4 | Task A-3 阶段引入"warn only"过渡模式（log warn 但不 fail），修完存量冲突后转 hard-fail | DAL 组 | Task A-3 集成后 host smoke 失败次数 > 3 |
| R-002 | Track B 迁移期 sample/DAL 调用点遗漏，导致真机构建 warn/error | 🟡 中 | 🟠 中 | 4 | Task B-2 前置全量 grep 台账；Task B-4 在合入前跑一次 `-Werror` 编译 | PAL 组 | `idf.py build` 报未处理返回值 warn |
| R-003 | Track D 中 ISR 版 `wink_trace_fault_from_isr` 与 task 版行为漂移（比如时间戳源不一致） | 🟢 低 | 🔴 高 | 3 | Task D-3 提供等价性测试用例：同一 fault 在 task/ISR 两条路径下 buffer 状态一致 | Trace 组 | `test_wink_trace_isr_equivalence` 失败 |
| R-004 | Track E 严格模式下现有 host 单测使用 `dal_ultrasonic_read`（`@deprecated blocking`）而链接失败 | 🟡 中 | 🟡 低 | 2 | Task E-3 前置扫描所有 test 中的 blocking API 使用，迁移到非阻塞 API | DAL 组 | `python wink-tools/wink.py test` 链接 undefined symbol |
| R-005 | Track A 中 `dal_gps_init` 需要新增 `PAL_RESOURCE_UART_PORT` 枚举，但 `pal_resource_type_t` 变动需影响 wasm/host 实现 | 🟢 低 | 🟡 低 | 2 | Task A-1 前置检查 `pal_resource_type_t`；若无 UART 枚举则 Task A-1 独立扩展 | PAL 组 | UART owner claim 编译失败 |
| R-006 | 3 份 ADR 用户不同意 Accepted，导致 Track B/D/E 集体阻塞 | 🟡 中 | 🔴 高 | 4 | Task 0 阶段先出 3 份 ADR 草稿并合并回复，仅经过用户 review 通过才启动后续 Task | 计划负责人 | ADR review 反馈中出现 "reject" |
| R-007 | Track C 错误码手册与 codegen prompt 集成路径未知（本项目 codegen prompt 存储位置需另行确认） | 🟡 中 | 🟡 低 | 2 | Task C-4 前置向用户确认 codegen prompt 存储位置；若无则手册独立成文，后续项目接入 | 文档组 | Task C-4 无接入点 |
| R-008 | Track F 改名 `pal_irq_direct_connect → pal_irq_iram_bind` 有第三方样例引用旧名 | 🟢 低 | 🟢 低 | 1 | 保留 deprecated 别名 1 个 sprint，同步扫描所有引用点迁移 | PAL 组 | grep 命中未迁移引用 |
| R-009 | Track B 后 `pal_gpio_wait_level`（`pal_hal_esp32_gpio.c:352,359`）内部循环需处理 read 错误码，可能改变 wait 语义 | 🟡 中 | 🟠 中 | 4 | Task B-3 单独审阅 wait_level 语义：错误路径视为"wait 失败"提前退出并返回 `WINK_ERR_IO` | PAL 组 | `test_pal_gpio_wait_level` 行为漂移 |
| R-010 | 28-file 大 commit 跨 Track 修改时，本地与 CI 仅验证 host target 导致 WASM 等目标出现编译或符号漂移回归（例如 3cd7d21 回归） | 🟡 中 | 🟠 中 | 3 | 1. 本地 `python wink-tools/wink.py test` 脚本引入 `-WithWasm` 参数；2. CI 规划/新增独立 WASM build 校验任务拦截非 host 编译回归 | 架构组 | WASM 目标编译失败 |


### 4.4 跨团队/跨模块协调点

- **Codegen prompt few-shot 集成**（Track C）：需要确认存放位置与更新流程；建议放到 `docs/design/07-platform-governance/codegen-prompts/` 下，作为项目治理文档；
- **协作式调度器计划的 T5 阶段**（外部）：Track E 的严格模式默认开启必须与 `sim-cooperative-scheduler-plan.md` T5 落地同一 PR 或相邻 PR，避免调度器上线时 sample 编译失败。

---

## 5. 里程碑与时间线

假设起点 2026-07-02，标注**开始日**。每个 Track 内 Task 编号规则：`Track字母-任务序号`。

| 阶段 | 时间窗口 | 内容 | Gate 判据 |
|-----|---------|------|----------|
| **M0：ADR 草稿评审** | 07-02 ~ 07-03（2 天） | Task 0-1 起草 ADR 0015/0016/0017 三份草稿；Task 0-2 用户 review；Task 0-3 三份转 Accepted | 三份 ADR 状态从 Proposed → Accepted；`list_adrs.py -s Accepted` 出 |
| **M1：Track A（P0）** | 07-04 ~ 07-08（5 天） | Task A-1 → A-5 见 §6 | R-1 红线通过 + `resource_conflict` sample 通过 + `test_pal_resource_wire` 全绿 |
| **M2：Track D（P1，先做）** | 07-09 ~ 07-11（3 天） | Task D-1 → D-3 见 §6 | ISR 契约锁死；`wink_trace_fault_from_isr` 通过等价性测试 |
| **M3：Track E（P1，先做）** | 07-12 ~ 07-13（2 天） | Task E-1 → E-3 见 §6 | `WINK_STRICT_NONBLOCKING` 模式下 `dal_ultrasonic_read` 符号不可见 |
| **M4：Track B（P1，最大变更）** | 07-14 ~ 07-18（5 天） | Task B-1 → B-4 见 §6；跨 target + DAL + sample 迁移 | 全量 grep 零 legacy 签名；三 target 构建绿；host 单测全绿 |
| **M5：Track C（P1，纯文档）** | 07-14 ~ 07-16（3 天，可与 M4 并行） | Task C-1 → C-4 见 §6 | 错误码手册 17+ 条覆盖 100%；R-3 CI lint 通过 |
| **M6：Track F（P2，收尾）** | 07-02 ✅ **已完成** | Task F-1 → F-3 见 §6 | ADR-0018 Accepted；`pal_irq.h` 收窄到 3 级优先级 + 单一临界区宏；`pal_irq_advanced.h` 建立 `#error` 门控；smp_uaf_test + pal_shared_chain 物理删除；host 全测通过 |
| **M7：SSOT 回写 & 归档** | 07-20（1 天） | Task Z-1 → Z-3：3 份 SSOT 设计规范同步；本计划标 ✅ 已完成；review 文档追加落地标记 | 手工检查 |

**Buffer**：M1 后追加 1 天缓冲，若 M1 R-001 触发（存量 owner 冲突）则消耗 buffer 修 sample；总计 **20 个工作日**（4 周）。

---

## 6. 任务分解（含 Sub-agent 委托格式）

### Track 0 · M0 ADR 草稿评审

#### Task 0-1：起草 ADR-0015 PAL GPIO 安全错误传播

**输入**：本计划 §2.1 Track B 陈述、[ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、`pal_hal.h:61,63` 现况。

**动作**：
1. 复制 ADR 模板；决策标题："PAL GPIO Read/Write 从 bool/void 统一升级为 wink_status_t + out-param"；
2. `背景（Context）`：现状不对称、AI Codegen 陷阱、与 `pal_pwm_*` 一致性要求；
3. `方案比选（Options）`：
   - A. 双 API 并存（deprecated 别名）—— ❌ 拒绝，见 R-2
   - B. 硬切换（本方案）
   - C. 引入 result monad 结构体 —— ❌ 拒绝，与 `wink_status_t` 生态不兼容
4. `决策结论`：Option B；含 out-param + `WINK_WARN_UNUSED_RESULT`；
5. `后果与约束`：
   - 迁移工作量：4 个 target 实现 + 2 个 DAL 直接调用点 + 若干 sample；
   - AI Codegen 训练数据需要更新（`docs/design/07-platform-governance/codegen-prompts/` 若存在）；
6. `遵循与后续`：本计划 §6 Task B 系列；SSOT 回写 `02-pal-platform-abstraction.md`。

**产出**：`docs/decisions/core/0015-pal-gpio-safe-error-propagation.md`

**验收**：
- ADR 结构完整（含 Status Change Log）
- 4 个 Option 至少 3 个（A/B/C）被清晰对比
- 用户 review 通过并签字 Accepted

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "起草 ADR-0015 PAL GPIO 安全错误传播。参考 ADR-0001/0012 的风格，包含 5 个必备章节（背景/方案比选/决策结论/后果与约束/遵循与后续）+ 底部 Status Change Log。**必读**：docs/reviews/core/2026-07-01-external-comprehensive-review-critique.md §二.2 与 §四 P1-1。方案比选至少列出 3 个（含硬切换、deprecated 别名、result monad）。"
```

#### Task 0-2：起草 ADR-0016 临界区上下文感知

**输入**：本计划 §2.1 Track D 陈述、`pal_osal_esp32.c:169` `portENTER_CRITICAL` 现况、`wink_trace.c:15` ISR 潜在调用路径。

**动作**：同 Task 0-1 结构；决策标题："pal_os_critical_enter/exit 分裂为 task/ISR 双入口"；方案比选包括：
- A. Context-aware 单一入口（内部 detect）
- B. 双入口显式分流（推荐，见 R-4）
- C. 保持单一入口 + 严禁 ISR 调用（doxygen only）

**产出**：`docs/decisions/core/0016-pal-critical-section-task-isr-dual-entry.md`

**验收**：同 Task 0-1。

**Sub-agent 委托**：同 Task 0-1 格式，agent_type: general-purpose。

#### Task 0-3：起草 ADR-0017 阻塞 API 硬隔离

**输入**：本计划 §2.1 Track E 陈述、`dal_ultrasonic.h:104` 现况、`sim-cooperative-scheduler-plan.md`（可能触发 WDT 场景）。

**动作**：同 Task 0-1 结构；决策标题："`WINK_BLOCKING` / `WINK_STRICT_NONBLOCKING` 三层硬隔离阻塞 API"；方案比选包括：
- A. 仅 doxygen `@deprecated`（当前状态）—— ❌ 拒绝
- B. 三层硬隔离：属性宏 + 编译时符号剔除 + runtime PT panic assert
- C. 移除阻塞 API（激进）—— ❌ 拒绝，破坏兼容期

**产出**：`docs/decisions/core/0017-blocking-api-hard-isolation.md`

**验收**：同 Task 0-1。

**Sub-agent 委托**：同上。

#### Task 0-Gate：三份 ADR 转 Accepted

**动作**：用户 review 通过后，将三份 ADR 状态从 Proposed 改为 Accepted，追加 Status Change Log 时间戳。

**验收**：`python docs/decisions/scripts/list_adrs.py -s Accepted` 命中 ADR-0015/0016/0017。

---

### Track A · M1 `pal_resource` 接线闭环（P0）

#### Task A-1：`pal_resource_type_t` 补齐 UART 枚举（预检）

**输入**：`pal/include/hal/pal_resource.h`。

**动作**：
1. 检查是否存在 `PAL_RESOURCE_UART_PORT` 枚举值；
2. 若无：追加枚举值并在三 target 实现中处理（多为 no-op / pass-through）；
3. 若有：跳过。

**产出**：`pal_resource.h` 变动（可能）+ 三 target 实现同步。

**验收**：`grep PAL_RESOURCE_UART_PORT wink-micro-os/pal/include/` 命中 ≥ 1；host/wasm/esp32 三 target 均编译通过。

#### Task A-2：6 个 DAL init 接线 `pal_resource_claim`

**输入**：`dal_button.c` / `dal_led.c` / `dal_servo.c` / `dal_ultrasonic.c` / `dal_gps.c` / `dal_eeprom.c`。

**动作**（每 DAL 一次修改）：
1. 在 `dal_xxx_init(dev, cfg)` 内、参数校验之后，调用 `pal_resource_claim`；
2. owner 使用 `__func__` 或 `"dal_xxx"` 字符串常量；
3. 若 claim 失败（返 `WINK_ERR_BUSY` / `WINK_ERR_RESOURCE_EXHAUSTED`），**直接透传返回值**，不设 dev.state；
4. `dal_ultrasonic_init` 需 claim GPIO×2（trig+echo）；若第二次 claim 失败，需**回滚**第一次 claim（`pal_resource_release`）；
5. `dal_gps_init` 同上，UART + GPIO×2（rx/tx）；
6. `dal_eeprom_init` 视实现，若 I2C 挂载则 claim `PAL_RESOURCE_I2C_ADDR`。

**产出**：6 个 DAL 源文件修改。

**验收**：
- `grep pal_resource_claim wink-micro-os/dal/src/` 命中 DAL 源文件数 = 7；
- 每个 DAL 至少覆盖 1 种资源类型；
- `dal_ultrasonic_init` / `dal_gps_init` 的多资源 claim 有 rollback 逻辑。

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "为 6 个 DAL 驱动（dal_button/dal_led/dal_servo/dal_ultrasonic/dal_gps/dal_eeprom）在 init 阶段接线 pal_resource_claim。参考已接线的 dal/src/display/dal_ssd1306.c:77 风格。owner 用 __func__。多资源 claim 需 rollback。产出 6 个文件的修改差。**红线**：本计划 R-1，claim 命中 DAL 源文件数最终 = 7。"
```

#### Task A-3：`samples/resource_conflict/` 反例样本

**输入**：Task A-2 完成后的 DAL 状态。

**动作**：
1. 新建目录 `samples/resource_conflict/`；
2. `app_main.c`：初始化两个 `dal_led`（或 `dal_servo`）实例，故意配置**同一 GPIO 引脚**；
3. 断言：第一个 init 返 `WINK_OK`，第二个 init 返 `WINK_ERR_BUSY`；
4. 若断言失败，`wink_trace_fault(WINK_ERR_PANIC)` 并 assert；
5. `CMakeLists.txt`：加入 `test_devkitc_smoke` 同级组件；
6. `README.md`：说明反例目的、验证方法、预期输出。

**产出**：3 个新文件。

**验收**：
- host build 通过；wasm smoke 通过；ESP32 可选 flash（非必需）；
- 反例 sample 在 host run 时通过（断言路径正确触发 `WINK_ERR_BUSY`）。

**Sub-agent 委托**：同 Task A-2 格式。

#### Task A-4：`test_pal_resource_wire.c` 单测

**输入**：Task A-2 + A-3 完成。

**动作**：
1. `test/test_pal_resource_wire.c` 覆盖：
   - 每个 DAL 类型的 GPIO/PWM/I2C/UART claim 冲突路径（6 组）；
   - `dal_ultrasonic_init` 二次 claim 失败时的 rollback 正确性（trig claim 后 echo claim 失败，trig 应被 release，可通过再次 claim 检验）；
   - `pal_resource_release` 后 pin 可被再次 claim；
2. 使用现有测试框架（参考 `test_pal_irq.c` 风格）；
3. `test/CMakeLists.txt` 追加。

**产出**：`test_pal_resource_wire.c` + CMake。

**验收**：`python wink-tools/wink.py test` 中该测试全绿；覆盖率至少 6 个 DAL × 1 冲突场景。

#### Task A-5：Track A 集成回归

**动作**：
1. 运行 `python wink-tools/wink.py test` 全量；
2. 运行 `node wink-micro-os/targets/wasm/wink_sim_stub.js`；
3. 运行 `idf.py -C esp32_firmware build`；
4. 归档结果到 M1 Gate。

**验收**：三 target 全绿；R-1 红线通过。

---

### Track D · M2 临界区 ISR 双入口（P1，先做）

**理由为什么排在 Track B 之前**：Track D 修改面窄（`pal_osal.h` + 4 target + 2 trace 文件）、风险低但阻塞面广（`wink_trace` 已被广泛使用）。先落地 D，Track B 在 D 稳定基础上推进。

#### Task D-1：ADR-0016 决策落地为 `pal_osal.h` API

**输入**：ADR-0016 决策方案 B（双入口显式分流）。

**动作**：
1. `pal_osal.h:131-137` 位置：
   ```c
   /* @brief Enter critical section (task context only).
    * @warning Calling from ISR context has undefined behavior on some targets
    *          (e.g., ESP32 uses portENTER_CRITICAL which is task-only). */
   uint32_t pal_os_critical_enter(void);
   void pal_os_critical_exit(uint32_t key);

   /* @brief Enter critical section from ISR context.
    * @note On ESP32 uses portENTER_CRITICAL_ISR. On host/wasm degrades to no-op
    *       or task-equivalent (single-threaded). */
   uint32_t pal_os_critical_enter_isr(void);
   void pal_os_critical_exit_isr(uint32_t key);
   ```
2. 4 target 实现同步：
   - `pal_osal_esp32.c`：新增 `portENTER_CRITICAL_ISR(&s_global_mux)` 版本；
   - `pal_osal_host.c` / `pal_osal_wasm.c`：退化为 no-op 或与 task 版共用；
   - `pal_osal_bare.c`：使用关中断原语（如 `__disable_irq`）。

**产出**：1 头文件 + 4 target 源文件。

**验收**：三 target 编译通过；host 单测（无破坏性）全绿。

#### Task D-2：`wink_trace_fault_from_isr` 拆分

**输入**：Task D-1 完成；`wink_trace.h` / `wink_trace.c`。

**动作**：
1. `wink_trace.h` 追加：
   ```c
   /* Task-context version. Must not be called from ISR. */
   void wink_trace_fault(wink_status_t code);
   /* ISR-context version. Uses ISR-safe critical section. */
   void wink_trace_fault_from_isr(wink_status_t code);
   ```
2. `wink_trace.c`：
   - 保持 `wink_trace_fault` 现状（`pal_os_critical_enter`）；
   - 新增 `wink_trace_fault_from_isr`：使用 `pal_os_critical_enter_isr`；
   - 内部提取公共 `s_record_fault_locked(code)`。
3. 老 doxygen 明标 "**Not callable from ISR**"。

**产出**：`wink_trace.h` + `wink_trace.c` 修改。

**验收**：单测（新增）：在 host 侧用 mock ISR 上下文调用 `wink_trace_fault_from_isr`，验证 buffer state 与 task 路径等价。

#### Task D-3：Track D 集成回归

同 Task A-5 格式。

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "落地 ADR-0016：pal_os_critical_enter/exit 分裂 task/ISR 双入口。修改 pal_osal.h + 4 target 实现 + wink_trace 拆双入口。参考 pal_osal_esp32.c:169 现有 task 版实现。ESP32 ISR 版用 portENTER_CRITICAL_ISR。host/wasm 单线程退化。产出：全量 diff。**红线**：本计划 R-4，选方案 B 双入口显式分流。"
```

---

### Track E · M3 阻塞 API 硬隔离（P1，先做）

**理由为什么排在 Track B 之前**：Track E 修改面比 D 更窄（1 个属性宏 + 1 个 DAL 头/源 + 若干 sample CMake），且阻塞协作式调度器落地。

#### Task E-1：`WINK_BLOCKING` 属性宏与 `WINK_STRICT_NONBLOCKING` 编译选项

**输入**：ADR-0017 决策。

**动作**：`wink_status.h` 追加：
```c
/* Attribute for APIs that busy-wait or block > runtime tick.
 * Under -DWINK_STRICT_NONBLOCKING=1, these APIs are removed from declarations. */
#if defined(__GNUC__) || defined(__clang__)
    #define WINK_BLOCKING __attribute__((deprecated("Blocking API forbidden in cooperative runtime")))
    #define WINK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#else
    #define WINK_BLOCKING
    #define WINK_DEPRECATED_MSG(msg)
#endif
```

**产出**：`wink_status.h` 修改。

**验收**：三 target 编译通过；`grep WINK_BLOCKING wink-micro-os/` 定义命中 = 1。

#### Task E-2：`dal_ultrasonic_read` 三层硬隔离

**输入**：Task E-1 完成。

**动作**：
1. `dal_ultrasonic.h`：
   ```c
   #ifndef WINK_STRICT_NONBLOCKING
   /* @deprecated Blocking busy-wait ≈60ms. Use request_measurement + get_cached_distance. */
   WINK_BLOCKING WINK_WARN_UNUSED_RESULT
   wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
   #endif
   ```
2. `dal_ultrasonic.c`：实现同样 `#ifndef` 包围；
3. `WINK_PT_DEBUG` 下：在 `dal_ultrasonic_read` 入口处检测是否在 protothread 上下文，若是则 `wink_trace_fault(WINK_ERR_PANIC) + assert`（简化：可通过 TLS flag 或 runtime state）。
4. 协作式调度样例的 CMakeLists 追加 `-DWINK_STRICT_NONBLOCKING=1`（识别所有 `runtime_cooperative_*` samples）。

**产出**：`dal_ultrasonic.h/c` + 若干 CMakeLists。

**验收**：
- `-DWINK_STRICT_NONBLOCKING=1` 构建下，`nm` 不出现 `dal_ultrasonic_read`；
- 非严格模式构建下，`dal_ultrasonic_read` 使用触发编译警告；
- Runtime 保护：若在 PT 上下文调用，`WINK_PT_DEBUG` 下 panic（通过 `test_ultrasonic_blocking_in_pt.c` 验证）。

#### Task E-3：Track E 集成回归

同上。lint 步骤：`python wink-tools/wink.py test` 增加 L1 lint（构建两次，验证严格模式下符号消失）。

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "落地 ADR-0017：WINK_BLOCKING 属性 + WINK_STRICT_NONBLOCKING 编译选项 + dal_ultrasonic_read 三层硬隔离。修改 wink_status.h + dal_ultrasonic.h/c + 协作式 sample CMakeLists。**红线**：本计划 R-5，必须三层齐全（属性+符号剔除+runtime assert）。产出全量 diff。"
```

---

### Track B · M4 `pal_gpio_read/write` 返回值重构（P1，最大变更）

#### Task B-1：全量调用点台账

**动作**：`grep -rn "pal_gpio_read\|pal_gpio_write" wink-micro-os/ --include='*.c' --include='*.h'` 全量导出到 `docs/tech-designs/unisim/2026-07-20-co-simulation-plugin-contract.md`（临时台账）。

**产出**：台账表格；预期命中：
- `pal_hal.h`（声明）× 2
- `pal_hal_host.c` / `pal_hal_wasm.c` / `pal_hal_esp32_gpio.c` 实现 × 6
- `dal_button.c` / `dal_led.c` × 2
- `pal_hal_ultrasonic.c` × 2
- `pal_hal_esp32_gpio.c` 内部 `pal_gpio_wait_level` × 2
- `pal_hal_esp32_gpio.c` ESP_PLATFORM=0 stub × 2
- Sample 注释若干

**验收**：台账 100% 覆盖 §3.1.2 清单。

#### Task B-2：`pal_hal.h` 签名重构 + 三 target 同步

**动作**：
1. `pal_hal.h`：
   ```c
   WINK_WARN_UNUSED_RESULT
   wink_status_t pal_gpio_write(wink_pin_t pin, bool level);
   WINK_WARN_UNUSED_RESULT
   wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);
   ```
2. host/wasm/esp32 三 target 实现同步；
3. **错误路径策略**（预定义）：
   - `pin` 越界 → `WINK_ERR_INVALID_ARG`；
   - `out_level == NULL`（read） → `WINK_ERR_INVALID_ARG`；
   - `pin` 未 claim（若 target 支持检查）→ `WINK_ERR_INVALID_STATE`；
   - 硬件故障 → `WINK_ERR_IO`（真机）；host/wasm 通常返 `WINK_OK`。

**产出**：`pal_hal.h` + 3 个 target 实现文件。

**验收**：三 target 编译通过；旧签名 zero grep。

#### Task B-3：DAL/Sample/内部调用点迁移

**动作**（按台账逐个）：
- `dal_button.c:28`：
  ```c
  bool raw;
  wink_status_t s = pal_gpio_read(dev->config.pin, &raw);
  if (wink_status_is_error(s)) { return s; }
  /* ...使用 raw... */
  ```
- `dal_led.c:28`：
  ```c
  wink_status_t s = pal_gpio_write(dev->config.pin, level);
  if (wink_status_is_error(s)) { return s; }
  ```
- `pal_hal_ultrasonic.c:37,39`：同上，透传错误码；
- `pal_hal_esp32_gpio.c:352,359`（内部 `pal_gpio_wait_level`）：
  ```c
  bool cur;
  wink_status_t s;
  while (1) {
      s = pal_gpio_read(pin, &cur);
      if (wink_status_is_error(s)) { return WINK_ERR_IO; }
      if (cur == level) { break; }
      /* ... timeout check ... */
  }
  ```

**产出**：全量 DAL/sample/内部迁移。

**验收**：
- `grep pal_gpio_read(` **零命中** 无 `&`（新签名必带 out-param）；
- 三 target `-Werror` 编译通过。

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "根据 Task B-1 台账，全量迁移 pal_gpio_read/write 调用点到新签名（wink_status_t + out-param）。DAL 层错误码透传，pal_gpio_wait_level 内部循环 read 错误退出。**红线**：本计划 R-2，zero legacy 签名残留。产出全量 diff。"
```

#### Task B-4：Track B 集成回归 + `test_pal_gpio.c` 补齐

**动作**：
1. 新增/修改 `test/test_pal_gpio.c` 覆盖：
   - 正常 read/write 返 `WINK_OK`；
   - `out_level == NULL` 返 `WINK_ERR_INVALID_ARG`；
   - 未 claim 引脚返 `WINK_ERR_INVALID_STATE`（若 Track A 已完成接线，此路径可测试）；
   - 越界引脚返 `WINK_ERR_INVALID_ARG`；
2. `python wink-tools/wink.py test` 全绿；wasm smoke 通过；ESP32 build 通过。

**验收**：M4 Gate 判据。

---

### Track C · M5 错误码目录手册（P1，可与 M4 并行）

#### Task C-1：手册骨架成文

**输入**：`wink_status.h` 17 个负数码 + 现有代码 grep 语义。

**动作**：新增 `docs/design/01-system-overall/error-code-catalog.md`；表格 4 栏：

| 码值 | 名称 | 语义 | 典型触发场景 | 推荐恢复策略 | 可作 `WINK_PT_EXIT` 条件 |
|------|------|------|-------------|-------------|-----------------------|
| -1 | `WINK_ERR_INVALID_ARG` | 参数校验失败 | NULL/越界/非法枚举 | 修 caller 逻辑（编程 bug） | ❌ 否（是 bug 不是 recover） |
| -2 | `WINK_ERR_TIMEOUT` | 操作超时 | I2C ACK / GPIO wait / RMT | 重试 3 次；转 `WINK_ERR_IO` | ✅ 是（可用作 exit） |
| ... | ... | ... | ... | ... | ... |

**产出**：`error-code-catalog.md`；17+ 条码全部覆盖。

**验收**：
- 每一条与 `wink_status.h` bit-for-bit 一致（R-3）；
- CI lint 通过（如可能：脚本 parse markdown table vs enum）。

#### Task C-2：`wink_status.h` 内联 doxygen 补齐

**动作**：为每个 `WINK_ERR_*` 定义追加 `/**< brief... */`，与手册对齐。

**产出**：`wink_status.h` 修改。

**验收**：`doxygen` 生成不报警告。

#### Task C-3：CI Lint `docs vs code` diff

**动作**：`python wink-tools/wink.py test` L2 lint：解析 `error-code-catalog.md` 表格，与 `wink_status.h` enum 值集合比对；差异 = fail。

**产出**：`python wink-tools/wink.py test` 增补。

**验收**：diff = 0 通过；主动引入 1 条差异验证 lint 生效后回退。

#### Task C-4：Codegen prompt 接入

**动作**：向用户确认 codegen prompt 存储位置；若在 `docs/design/07-platform-governance/codegen-prompts/`，将 `error-code-catalog.md` 摘要（≤ 200 行）作为 few-shot 附加进去。

**产出**：`docs/design/07-platform-governance/codegen-prompts/error-code-few-shot.md`（若目录不存在则新建）。

**验收**：用户确认。

**Sub-agent 委托**：
```
subagent_type: general-purpose
prompt: "起草 error-code-catalog.md（17+ 条 wink_status_t 语义手册）。每条 4 栏：语义/触发场景/恢复策略/是否可作 WINK_PT_EXIT。参考 dal/include/**/*.h 中现有 doxygen '@Error-codes' 段落归纳典型场景。**红线**：本计划 R-3，与 wink_status.h enum bit-for-bit 一致。产出 markdown 表格。"
```

---

### Track F · M6 PAL 中断 API 收窄与安全隔离（P2，升级版）

#### Task F-1：ADR-0018 决策落地为 `pal_irq.h` / `pal_irq_advanced.h`
**动作**：
1. `pal_irq.h`：
   - 移除 `pal_direct_isr_t` 和 `pal_irq_shared_handler_t`，仅保留统一回调原型 `pal_isr_t`；
   - 将 `pal_irq_prio_t` 缩减为 3 级（`LOW`, `NORMAL`, `HIGH`）；
   - 将 `pal_irq_synchronize`、`pal_irq_save`、`PAL_CRITICAL_SECTION_STRICT` 等高级 API 移出此文件。
2. 新建 `pal_irq_advanced.h`：
   - 声明上述移出的高级 API，并在头部加入宏门控 `#ifndef WINK_ALLOW_ADVANCED_IRQ_APIS` 以进行物理隔离；
   - 规定在裸机（Baremetal）下 `pal_irq_save_rtos_safe` 降级为 `__disable_irq()` 关所有中断锁。
3. `pal_hal.h`：
   - 将 `pal_gpio_enable_interrupt_ex` 的优先级入参同步缩减为 3 级。

**产出**：`pal_irq.h` + `pal_irq_advanced.h` + `pal_hal.h` 修改。

**验收**：
- 头文件中不再包含 6 级优先级枚举及废弃的回调声明；
- 宏隔离机制符合预期。

#### Task F-2：Target 适配层重构与 Shared Chain 物理删除
**动作**：
1. 删除 `targets/common/include/pal_shared_chain.h` 及 `targets/common/src/pal_shared_chain.c` 文件，并移除编译系统（CMake）中的依赖；
2. 重构 `targets/esp32/pal_irq_esp32.c`：
   - 移除共享中断责任链（`pal_irq_shared_register`）及 direct connect 相关的静态槽位和 `direct_trampoline` 分发；
   - 缩减优先级映射逻辑（LOW/NORMAL/HIGH 映射至 LEVEL1/LEVEL2/LEVEL3）；
   - 对齐 `pal_gpio_enable_interrupt_ex` 优先级校验及锁定逻辑。
3. 同步重构 `targets/host/pal_hal_host.c` 和 `targets/wasm/pal_hal_wasm.c`：
   - 移除 `shared_register` 与 `direct_connect` 相关的存量桩代码，将优先级映射收窄。

**产出**：Wasm/Host/ESP32 适配层及 CMake 构建系统的文件重构（约删除 300 行代码）。

**验收**：
- 静态搜索 `pal_irq_shared_register` 及 `pal_shared_chain` 在 Target 代码中无残留；
- Wasm/Host/ESP32 平台编译通过。

#### Task F-3：测试用例重构与 regression 核验
**动作**：
1. 废弃并物理删除 `samples/smp_uaf_test/` 整个目录，并在根构建文件中移除其编译子目录；
2. 重构 `test/test_pal_irq.c`：
   - 删除针对已废弃 API（如共享中断、直接直连、`REALTIME` 等）的过时测试；
   - 在测试用例中将优先级统一为 `LOW`, `NORMAL`, `HIGH`；
   - 在测试临界区及同步的高级测试用例前定义 `WINK_ALLOW_ADVANCED_IRQ_APIS` 并包含 `pal_irq_advanced.h`。
3. 运行 `python wink-tools/wink.py test --with-wasm` 进行全回归测试。

**产出**：`test_pal_irq.c` 修正 + 废弃 sample 物理移除。

**验收**：
- 全项目测试全绿通过；
- Wasm smoke 通过。

---

### Track Z · M7 SSOT 回写 & 归档

#### Task Z-1：设计规范同步

**动作**：
1. `02-pal-platform-abstraction.md`：GPIO API 章节更新新签名；临界区章节增加 ISR 双入口；`WINK_BLOCKING` 章节；
2. `03-device-abstraction-layer.md`：DAL init 必须 claim 章节；
3. `07-platform-governance/*` 若有 API 稳定性章节，`WINK_BLOCKING` 属性纳入约束。

**产出**：3 份 SSOT 文档修改。

**验收**：SSOT 文档 diff 与新 ADR 结论一致；无未同步表述。

#### Task Z-2：本计划归档

**动作**：本计划状态改 ✅ 已完成；追加"实际完成日"到元数据表；追加"实际工时对比"到 §5。

#### Task Z-3：review 文档追加落地标记

**动作**：`docs/reviews/core/2026-07-01-external-comprehensive-review-critique.md` 尾部追加：
```
---
## 落地状态（2026-07-XX 追加）
本 review 已由 PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 全量落地，落地日期 2026-07-20。
```

---

## 7. 质量保障

### 7.1 静态分析与 Lint

- **L0（既有）**：`ESP_PLATFORM` guard 密度 lint（已有）；
- **L1（新增，Track E）**：验证 `-DWINK_STRICT_NONBLOCKING=1` 构建下 `dal_ultrasonic_read` 符号消失；
- **L2（新增，Track C）**：`error-code-catalog.md` 表格与 `wink_status.h` enum diff = 0；
- **L3（新增，Track A）**：`grep pal_resource_claim wink-micro-os/dal/src/` 命中 DAL 源文件数 = 7；
- **L4（架构 CR 卡口）**：`pal_gpio_read/write` 无 legacy 签名残留；`pal_irq.h` 无虚标契约文本。

### 7.2 单元测试

新增/修改的测试：
- `test_pal_resource_wire.c`（Track A，6 组冲突用例）；
- `test_pal_gpio.c`（Track B，正常/未 claim/越界/NULL out-param 四路径）；
- `test_wink_trace_isr_equivalence.c`（Track D，task/ISR 双路径 buffer 状态等价）；
- `test_ultrasonic_blocking_in_pt.c`（Track E，PT 上下文调用 blocking API panic）。

### 7.3 集成/回归测试

- **host `python wink-tools/wink.py test`**：M1/M2/M3/M4/M5 每阶段结束都必须通过；
- **wasm smoke** `node targets/wasm/wink_sim_stub.js`：M1/M4 结束通过；
- **ESP32** `idf.py -C esp32_firmware build`：M1/M2/M4 结束通过（0 error 0 warning）；
- **反例样本** `samples/resource_conflict/`：M1 结束通过。

### 7.4 硬件冒烟（可选）

如物理 ESP32 DevKitC 可用：`samples/devkitc_smoke` flash + 观察串口输出，验证 Track A 未破坏既有硬件路径。

---

## 8. 交付物清单

### 8.1 代码

- 6 个 DAL 源文件（Track A）
- 3 个新 sample 文件（`samples/resource_conflict/`）
- 4 个新单测文件（Track A/B/D/E）
- 12+ 个 PAL/target/HAL 修改文件（Track B/D/F）
- 3 个属性宏与 CMake 修改（Track E）

### 8.2 文档

- 3 份新 ADR：0015 / 0016 / 0017
- 1 份新手册：`error-code-catalog.md`
- 1 份 codegen few-shot（若接入点确定）：`error-code-few-shot.md`
- 3 份 SSOT 设计规范更新
- 1 份 review 归档追加

### 8.3 CI 增补

- `python wink-tools/wink.py test`：新增 L1/L2/L3 lint 步骤
- 现有 `python wink-tools/wink.py test` L0 保持

---

## 9. 回滚策略

按 R-7 红线，每个 Track 独立 PR，独立回滚粒度：

| Track | 回滚方式 | 影响 |
|-------|---------|------|
| A | `git revert` Track A 全部 commit | DAL 未 claim 状态，`pal_resource` 层继续"空壳"；不影响运行 |
| B | `git revert` Track B 全部 commit | 回到 `bool/void` 签名；调用者兼容 |
| C | `git revert` Track C 文档 commit | 手册消失；不影响代码 |
| D | `git revert` Track D 全部 commit | 回到单入口 `pal_os_critical_enter`；`wink_trace_fault_from_isr` 消失 |
| E | `git revert` Track E 全部 commit | `dal_ultrasonic_read` 恢复无属性状态 |
| F | `git revert` Track F 全部 commit | `pal_irq.h` 契约恢复 |

**ADR 回滚**：三份新 ADR 若集体回滚，状态改为 `Rejected` 并追加 Status Change Log；对应 Track 代码同 commit 一起回滚。

---

## 10. 计划确认与开工

**请用户确认**：

- [ ] §2.1 五 Tracks 优先级与顺序（A P0 → D/E P1 先做 → B/C P1 主变更 → F P2）
- [ ] §3.3 八条红线，特别是 R-2（禁止 API 骑墙）、R-5（阻塞 API 三层硬隔离）
- [ ] §5 时间线：起点 2026-07-02，20 个工作日总工期
- [ ] §6 Task 0（M0 ADR 起草）作为第一步启动
- [ ] R-7：每个 Track 独立 PR

确认后启动 Task 0-1：起草 ADR-0015。

---

*本实施计划为规划状态，未开始执行。任何变更请通过 Amendment Log 追加，不直接修改已确认段落。*

## Amendment Log

- 2026-07-01：v1.0 初稿，基于 review 文档 `2026-07-01-external-comprehensive-review-critique.md` 与 Gemini 二次核验报告合并整理。
- 2026-07-02：**Task A-2 衍生改动 — PAL 层撤 self-claim**（计划盲区补记）。落地 Task A-2 时发现 §3.1.1 未覆盖 PAL 层现有 self-claim（`pal_hal_host.c` GPIO/PWM、`pal_hal_esp32_gpio.c` GPIO、`pal_hal_esp32_pwm.c` PWM），若保留则与 DAL 层新增 claim 二次抢同一资源、恒返 `WINK_ERR_BUSY`（会导致 20+ 单测失败）。经评估此为 A-2 的**必然衍生**（非独立决策）：DAL 一旦成为语义 owner 的 SSOT，PAL 固定 owner 的 self-claim 即为死代码 + 恒冲突源。改动面：`targets/host/pal_hal_host.c`（撤 `pal_gpio_init` / `pal_pwm_init` 的 claim 与 `pal_pwm_deinit` 的 release）+ `targets/esp32/pal_hal_esp32_gpio.c` + `targets/esp32/pal_hal_esp32_pwm.c`（撤 init 中 claim + 错误路径 release + deinit release）。SSOT 边界表述同步回写至 [`docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md) §4.1。同步顺手补齐 `dal_ssd1306.c` 在 `pal_i2c_transfer` 失败时的 release 回滚（行为一致性），并预置 A-1 UART 枚举（`PAL_RESOURCE_UART_PORT = 5` in `pal_resource.h`）供 `dal_gps` 使用。落地 commit：`c1f20ca feat(dal): wire pal_resource_claim into DAL init and remove PAL self-claim`。纪律遵循：因单点决策，不足以独立起 ADR；按 CLAUDE.md 决策回写要求，同步更新 §01 设计规范。
- 2026-07-02：**Track A M1 完工**（Task A-1..A-5 全部落地）。落地明细：
  - A-1（`PAL_RESOURCE_UART_PORT` 枚举预检）：并入 A-2 落地，见 commit `c1f20ca`。
  - A-2（6 DAL init 接线 `pal_resource_claim` + rollback）：commit `c1f20ca`（含 A-2 衍生的 PAL 撤 self-claim，见上条 Amendment）。
  - A-3（`samples/resource_conflict/` 反例样本）：commit `2bdecc7`。4 组 host-only 断言样本（GPIO/PWM/UART/I2C 各一），加入 CTest；`sample_resource_conflict` exit 0 时冲突治理生效，否则 CI 红。
  - A-4（`test/test_pal_resource_wire.c` 冲突单测）：commit `81a9751`。**独立文件**（与 `test_pal_resource.c` 分层——PAL 单元 vs DAL↔PAL 集成），11 用例覆盖 6 DAL 冲突 + 跨 DAL 冲突 + `dal_ultrasonic` echo 冲突时 trig rollback 的可观测验证 + owner=NULL 契约 + release-then-reclaim。全绿。
  - A-5（三 target 集成回归）：
    - host：`powershell python wink-tools/wink.py test` 全绿（含 `test_pal_resource_wire` 11/11、`sample_resource_conflict` PASS、`test_pal_resource` 7/7）。
    - wasm：`emcmake` 构建 `wink_simulator.wasm+js` 成功；`node targets/wasm/wink_sim_stub.js` → "smoke PASS"。
    - esp32：`idf.py -C esp32_firmware build`（v6.0.1）成功，0 error / 0 warning；6 个改动过的 DAL 源文件在 ESP32 target 下全部编译通过。
  R-1 红线（DAL claim 覆盖率 = 7，含 `dal_ssd1306`）✅；`pal_resource` 由"空壳"转为真实生效的语义级冲突治理。Track A M1 目标全部达成，可交付。
- 2026-07-02：**Track A M1 尾巴收尾** — `test/test_dal_ssd1306.c` 补 owner=NULL 契约用例（与 A-4 `test_pal_resource_wire.c` 的 owner=NULL 用例对称覆盖，`dal_ssd1306.c:73` 已实施该校验，此前无 host 断言）。落地 commit：`254d19a test(dal-ssd1306): assert owner=NULL rejected as INVALID_ARG (A-4 symmetry)`。
- 2026-07-02：**Track D M2 完工**（Task D-1..D-3 全部落地）。落地明细：
  - **D-1**（`pal_osal` task/ISR 双入口）：commit `af5292a feat(pal-osal): split critical section into task/ISR dual entries (ADR-0016)`。
    - `pal_osal.h` 新增 `pal_os_critical_enter_isr / pal_os_critical_exit_isr`；旧入口 doxygen 明标 TASK-only；同时新增 `pal_os_set_sim_isr_context(bool)` / `pal_os_in_sim_isr_context()` 一对 sim-hook（ADR-0016 §4.2）。
    - 四 target 实现同步：
      - **ESP32** — 使用 `portENTER_CRITICAL_ISR(&s_global_mux)` 共享 task 版 mux（task/ISR 互斥保留）；sim-hook 为 no-op。
      - **host** / **wasm** — 单线程语义上退化为 no-op，但 4 个 critical 入口全部 `assert` `s_sim_in_isr` 与调用入口匹配 → Debug 构建下入口误用**立即命中 assert**。这就是 ADR-0016 §4.2 落地为可执行契约的手段。
      - **baremetal** — task/ISR 共用 `pal_bsp_irq_save/restore`（关中断已同时保护两类上下文）；契约诚实优于代码重复。
    - 三 target 验收：host `python wink-tools/wink.py test` 全绿；wasm smoke PASS；`idf.py build`（v6.0.1）0 error / 0 warning。
  - **D-2**（`wink_trace_fault_from_isr` 拆分 + 等价性硬门槛）：commit `bfe4ce9 feat(wink-trace): add wink_trace_fault_from_isr dual entry (ADR-0016 D-2)`。
    - `wink_trace.h` 拆双入口：`wink_trace_fault`（TASK）+ `wink_trace_fault_from_isr`（ISR），共享同一环形缓冲；`wink_trace_reset/count/last` 保持 TASK-only；旧的 "ISR-safe: Yes" 假承诺撤回。
    - `wink_trace.c` 抽 `static inline s_record_fault_locked(uint32_t)` 公共写入函数，保证 task/ISR 两路 bit-for-bit 等价的环形写入顺序；文件顶部 INVARIANT 注释同步升级。
    - 新增 `test/test_wink_trace_isr_equivalence.c` — 4 组用例通过 `pal_os_set_sim_isr_context()` 夹紧 ISR 边界，与纯 task 参考序列比对 `count/last`：单次 fault、交替 task/ISR 序列、环形回绕（CAPACITY+5）、task-only API 无 assert 干扰。全部 PASS（4/4）。CMakeLists 通过 `add_wink_host_test` 注册（自动链 `pal_host` OBJECT，供 sim-hook 符号可用）。
    - ADR-0016 addendum：在"未来演进路径"追加两条 —— host 单测多线程化时 `s_sim_in_isr` 需升 `_Thread_local`；嵌套模拟中断时 bool 需升嵌套计数器。作为 D-2 addendum 一并入库（Accepted ADR 的非规范补充）。
  - **D-3**（SSOT 回写 + Track D 集成收尾，doc-only 变更）：
    - `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md` §3 OSAL：临界区章节补 sim-hook API 声明；跨 target 行为矩阵 host/wasm 行升级为 `no-op + assert(!/s_sim_in_isr)`；增补"Host/Wasm sim-hook 落地为可执行契约"段与 `wink_trace` 双入口注解；演进路径追加 `_Thread_local` / 嵌套计数器备忘。
    - `docs/design/02-wink-micro-os/04-runtime-and-trace.md` §2.3/§2.4：wink_trace API 表更新为双入口 + 各行注明所属上下文；新增 §2.4 "并发契约（ADR-0016 task/ISR 双入口）"，说明 ISR 路径禁调 `wink_runtime_fault`（Safe-off 延迟到 TASK）与等价性硬门槛。
    - `.claude/skills/_embedded-shared/concurrency.md`：新增章节 "⭐ Task / ISR 双入口显式分流（ADR-0016）"，含选项对比表、落地范式、跨 target 矩阵、Host/Wasm sim-hook 强推、等价性测试硬门槛、演进路径。作为范式无关的工程纪律沉淀（未来 OSAL 原语扩展沿用）。
    - 三 target 集成回归复核：host `python wink-tools/wink.py test` 全绿；wasm smoke PASS；ESP32 build 与 D-2 同构（doc-only 变更不改构建产物）。R-4 红线（ISR 契约无二义性，锁定方案 B 双入口显式分流）✅；R-7 红线（Track D 独立 commit 组，`af5292a` / `bfe4ce9` / D-3 doc-only 三段可独立 revert）✅。
  Track D M2 目标全部达成；契约债务（`wink_trace.c:8-9` INVARIANT 声称 "ISR-safe" 而 ESP32 实现 task-only）清偿；ADR-0012 契约诚实原则在 OSAL 层再落一子。可交付。
- 2026-07-02：**Track E M3 完工**（Task E-1..E-3 全部落地）。落地明细：
  - **E-1**（`WINK_BLOCKING` / `WINK_DEPRECATED_MSG` / `WINK_ASSERT_NONBLOCKING` 属性宏与占位）：commit `4e8fdfc feat(wink-status): add WINK_BLOCKING / WINK_DEPRECATED_MSG / WINK_ASSERT_NONBLOCKING (ADR-0017 E-1)`。GCC/Clang + MSVC 三分支实现；`WINK_ASSERT_NONBLOCKING()` 为 T5 阶段 PT-context 检测宏保留 no-op 占位，届时替换宏体即可，无需再改 API 挂载点。
  - **E-2**（`dal_ultrasonic_read` 三层硬隔离首个应用点）：commit `cb74a50 feat(dal-ultrasonic): apply WINK_BLOCKING hard isolation to dal_ultrasonic_read (ADR-0017 E-2)`。头文件与实现均以 `#ifndef WINK_STRICT_NONBLOCKING` 包围；属性顺序 `WINK_BLOCKING WINK_WARN_UNUSED_RESULT`；函数体首行 `WINK_ASSERT_NONBLOCKING()`（T5 占位）；过渡期 `test/test_dal_ultrasonic{,_sim}.c` 顶部 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` 保 host `-Werror` 通过（ADR-0017 §Consequences 明列的过渡期例外）。
  - **E-3**（`python wink-tools/wink.py test` L1 lint + SSOT 回写）：commit `8ab8762 chore(adr-0017): add L1 lint for WINK_STRICT_NONBLOCKING + SSOT backport (E-3)`。L1 lint 独立于主构建——单文件编 `dal_ultrasonic.c` 加 `-DWINK_STRICT_NONBLOCKING=1`，`nm -g --defined-only` 断言 `dal_ultrasonic_read` 符号消失；反证跑过（临时去 guard → lint 立刻抓到并打完整符号表 → exit 1；恢复后 exit 0）。SSOT 回写：`07-platform-governance/01-device-model-registry.md` 增补 JSON registry 的 `blocking/attributes/strictBuildGuard` 三字段与摘要表注解；`02-wink-micro-os/01-dal-device-abstraction.md §3.0 契约 B` 已在 ADR-0017 Accepted 时同步。
  - **E-1 顺手**：`WINK_ASSERT_NONBLOCKING` 已挂占位宏，协作式调度器 T5 阶段仅需替换宏体、DAL 侧零改动。
  - **known follow-up（显式挂账，写入 E-2 commit message 防漏）**：
    1. `runtime_cooperative_*` sample 集尚未落地（前置 `2026-07-01-sim-cooperative-scheduler-plan` T5），因此 `target_compile_definitions(<t> PRIVATE WINK_STRICT_NONBLOCKING=1)` 挂载点是"预置状态"，交由调度器 T5 阶段接手（父计划 R-8）。
    2. `tools/app_codegen.py` 中两处 `dal_ultrasonic_read` 模板字符串仍在生成 blocking 调用；属父计划 Track C-4（codegen prompt few-shot 硬化）范围。
  - **三 target 集成回归**：host `python wink-tools/wink.py test --clean` 188/188 build + 全绿 + 双 lint（L0 ESP_PLATFORM + L1 ADR-0017）通过；wasm smoke PASS（本次在 R-010 修复上下文中验证）。ADR-0017 §决策落地规则 §1~§4 全部落地，第三层 PT-context 检测按 §5 授权延后到协作式调度器 T5 阶段。R-5 红线（三层硬隔离齐全）✅；R-7 红线（Track E 三段 commit 可独立 revert）✅。可交付。
- 2026-07-02：**Track B M4 完工**（并入 3cd7d21 大 commit，随 Track A 补锁 + pal_resource 双端同源重构一并落地；违反 R-7 单 PR 单 Track 红线，既成事实，回滚粒度粗化的代价已由 §Amendment Log 与 R-010 一并记录）。落地明细：
  - **B-1..B-2**（`pal_hal.h` 签名重构 + 三 target 同步）：`pal_gpio_read/write` 从 `bool/void` 升级为 `WINK_WARN_UNUSED_RESULT wink_status_t + out-param`；ADR-0015 §决策规则完整落地（ESP32 `GPIO_IS_VALID_GPIO` + `esp_err_t → wink_status_t` 显式转换；host/wasm 未 claim 引脚 `WINK_ERR_INVALID_STATE`；out-param 防御性初始化）。三 target 实现同步：`pal_hal_esp32_gpio.c`、`pal_hal_host.c`、`pal_hal_wasm.c`（wasm 侧 include 遗漏由 `5291a95 fix(pal-hal-wasm): add missing pal_resource.h include` 修补，见 R-010）。
  - **B-3**（DAL/Sample/内部调用点迁移）：`dal_button.c` / `dal_led.c` / `pal_hal_ultrasonic.c` / `pal_hal_esp32_gpio.c` 内部 `pal_gpio_wait_level` 全量迁移；sample 与 test（`test_button_debounce_e2e.c` / `test_host_pal.c` / `test_sim_physical.c` / wasm `test_debounce_middleware.c` / `test_button_debounce_e2e_wasm.c`）同步。R-9（wait_level 语义漂移风险）经 3cd7d21 内部处理——错误路径视为"wait 失败"提前退出并返 `WINK_ERR_IO`。
  - **B-4**（集成回归 + grep 台账）：Track B 收尾核验（本 commit 附）：`grep -rn "bool pal_gpio_read\|void pal_gpio_write" wink-micro-os/ --include='*.c' --include='*.h'` 零命中 ✅；新签名 declarations 集中在 `pal_hal.h:62,65` 各一处 ✅；host `python wink-tools/wink.py test --with-wasm` 三端绿（host 188/188 + wasm build + smoke） ✅。
  - **SSOT 回写**：`02-wink-micro-os/02-pal-platform-abstraction.md` §GPIO API 章节在 ADR-0015 Accepted 时已同步（doxygen 契约 + WASM 分支 + DAL 侧透传约定；多处 ADR-0015 v2.3 引用可 grep 佐证）。
  - **R-7 违反的复盘**：3cd7d21 一次性推 4 件（Track A ssd1306 补锁 + Track B GPIO 签名 + pal_resource 双端同源 + DAL 边界检查），导致 wasm 端 `pal_hal_wasm.c` include 疏漏未被 host CI 触发（回归发生窗口 = 提交 → 手动 wasm 重建 → 5291a95）。R-010 已新增到 §4.3 风险登记册 + `python wink-tools/wink.py test-WithWasm` 交付，见 `b969e73 chore(ci): add -WithWasm switch to python wink-tools/wink.py test + log R-010 (3cd7d21 lesson)`。
  Track B M4 目标全部达成；ADR-0015 §决策规则落地；`pal_gpio_read/write` 静默降级路径彻底消除。可交付。
- 2026-07-02：**Track F 升级决策落地**。将 Track F 从"改名"升级为"IRQ API 全面收窄"。ADR-0018 已起草并保存至 `docs/decisions/core/0018-pal-irq-api-narrowing.md`。任务分解 F-1、F-2、F-3 均已更新。
- 2026-07-02：**Track F M6 完工**（Task F-1..F-3 全部落地 + ADR-0018 Accepted）。落地明细：
  - **F-1 头文件收窄**：`pal_irq.h` 从 ~25 符号收窄到 10 个：删除 `pal_direct_isr_t / pal_irq_shared_handler_t`，合并到 `pal_isr_t`；`pal_irq_prio_t` 从 6 级收窄到 3 级（LOW/NORMAL/HIGH）；`pal_irq_synchronize / pal_irq_save / PAL_CRITICAL_SECTION_STRICT` 迁到新建的 `pal_irq_advanced.h`，加 `#ifndef WINK_ALLOW_ADVANCED_IRQ_APIS` `#error` 物理门控。落地 commit：`b75f8d8`（ADR + Q3 plan）+ `48fb60d`（全量重构，+294 / -2075）。
  - **F-2 Target 重构 + Shared Chain 物理删除**：`targets/common/{include,src}/pal_shared_chain.{h,c}` 整体删除；`targets/esp32/pal_irq_esp32.c` / `targets/host/pal_hal_host.c` / `targets/wasm/pal_hal_wasm.c` 移除 shared/direct 相关分发；优先级映射收窄。
  - **F-3 测试重构 + Sample 归档**：`samples/smp_uaf_test/` 整体删除；`test/test_pal_irq.c` 重构（-264 行）删除 shared/direct/REALTIME/STRICT/HIGHEST/LOWEST 相关用例，保留核心 enable/disable/pending/nesting/RAII/prio-lock 6 组测试；host `python wink-tools/wink.py test` 104/104 build + 全绿。
  - **审阅补丁**（用户手工 review 发现的实现遗漏，已在 F-1..F-3 commit 后修）：
    - B1: `pal_hal_esp32_gpio.c` 引用了已删枚举 `PAL_IRQ_PRIO_REALTIME/LOWEST/HIGHEST`，ESP32 target 无法编译（host 测试未覆盖）→ 修正为 3 级映射 + `prio <= 0` 边界检查。
    - B2~B7: `pal_irq.h` / `pal_irq_advanced.h` / `pal_irq_esp32.c` doxygen 中的 6 级 / REALTIME / synchronize / save 陈旧引用清理。
    - B8: `.claude/skills/burn-firmware-esp32/SKILL.md` 移除 smp_uaf_test 引用，添加 resource_conflict。
    - B9: `python wink-tools/wink.py test` 移除 -Optin pass（伴随 `WINK_HOST_ALLOW_REALTIME_FOR_TESTING` 无消费者退休）。
  - **SSOT 回写**：`02-pal-platform-abstraction.md §3.3` 新增"PAL IRQ 公开面收窄（ADR-0018）"章节；同文档 §4 `targets/common/` 表格删除 `pal_shared_chain` 条目 + 加历史备注；`tech-designs/pal-unified-interrupt-subsystem.md` 顶部加 v2.x 归档 banner；`03-directory-architecture.md` 删除 smp_uaf_test 引用。
  - **决策纪律**：ADR-0018 状态 Proposed → Accepted（同日）；触发理由完整链路 = [2026-07-02 收窄评审](../../reviews/core/2026-07-02-pal-irq-api-narrowing-review.md) → ADR-0018 → 代码 + SSOT 回写。R-7 单 Track 独立提交纪律遵守：`b75f8d8`（doc） + `48fb60d`（impl）两段可独立 revert；本次 B1..B9 clean-up 为一份 follow-up commit。
  Track F M6 目标全部达成；ADR-0018 落地闭环；`pal_irq.h` AI Codegen 组合空间从 ~36 → ~3；契约诚实原则（ADR-0012）在 IRQ 子系统 100% 落实。可交付。
- 2026-07-02：**Track C M5 部分完工 + Q3 计划结项**（Task C-1、C-2 落地；C-3、C-4 挂账；Z-1..Z-3 完成）。
  - **决策调整**（原计划外）：错误码 SSOT 位置从"新建 `docs/design/01-system-overall/error-code-catalog.md`"调整为"扩为 `docs/design/07-platform-governance/02-error-fault-model.md` §11"。理由：§07 已承载 §2 码段总览、§4 标准故障类型、§7 降级策略、§9 静态检查，是错误码 SSOT 的天然归属；若另起 §01 会形成双写，正是 R-3 想避免的漂移源。用户拍板确认。
  - **C-1 §07 §11 追加**：新增"AI Codegen 错误码语义详表"，四栏 × 22 行覆盖全部负值码：语义 / 典型触发场景 / 推荐恢复策略 / 是否可作 `WINK_PT_EXIT` 条件。补 §11.5 生成器约束段（含 `WINK_ERR_BUSY` 作为 PT yield 信号的特例）与"新增/调整枚举值必须双侧同步"义务。R-3 红线（手册 vs 代码 bit-for-bit 一致）以本节 SSOT 化收敛：`wink_status.h` 的内联 doxygen 与 §11 表首列必须同步更新，评审卡口。
  - **C-2 wink_status.h 内联 doxygen**：为每个 `WINK_ERR_*`（+ `WINK_OK`）追加 `/**< brief */`，与 §11 首行语义对齐；enum 顶部加块注释显式指向 §11 为 SSOT。host `python wink-tools/wink.py test` 155/155 build + 全 lint 通过确认头文件改动零回归。
  - **C-3 挂账（L2 lint 不做）**：CI lint `docs vs code` diff 未实施。理由：SSOT 化到 §11 后，双侧同步依赖评审卡口而非机械 diff；表格 markdown 与 enum 定义结构差异大，parser 维护成本 > 收益。若未来出现漂移事故再补 lint。
  - **C-4 挂账（codegen prompt 接入不做）**：`docs/design/07-platform-governance/codegen-prompts/` 目录当前不存在；`tools/app_codegen.py` 中还有两处 `dal_ultrasonic_read` 生成 blocking 调用（Track E 已挂账）。接入点未定，本次不预先创建目录；由后续 codegen 硬化专题统一处理。R-007 风险按"文档独立成文，后续项目接入"降级处理。
  - **Z-1 SSOT 回写差量核验**：各 Track Amendment 已同步完成 SSOT 回写（Track A 见 `02-pal §4.1` / Track B 见 `02-pal §GPIO` / Track D 见 `02-pal §3` OSAL + `04-runtime-and-trace §2.3-2.4` / Track E 见 `07-01 §JSON registry` / Track F 见 `02-pal §3.3 §4`）。Track C 新增 §11 后在 `02-pal §1 ADR-0001` 与 `01-dal §契约 A` 各加一条 §11 指针（读到 PAL/DAL 文档的人可直达 SSOT）。
  - **Z-2 Q3 计划归档**：元数据表状态改 ✅ 已完成，实际结项日 2026-07-02。**实际工时**：计划 20 个工作日（4 周），实际 1 日（M0 ADR 起草 + M1-M6 全部 Track + M7 归档）—— ADR 与代码同 commit 落地节省 M0 windows；Track F 从"改名"升级到"IRQ API 全面收窄"（+ADR-0018）反而缩短了 M6 的 debate 时间；user review 在同日多轮完成，未跨天等待。R-6（ADR 状态同步）纪律遵守：ADR-0015/0016/0017/0018 均按"Proposed → user review → Accepted → 代码"闭环。
  - **Z-3 review 落地标记**：`reviews/2026-07-01-external-comprehensive-review-critique.md` 尾部追加落地状态段。
  - **known follow-up（继承自各 Track，随本结项统一挂账）**：
    1. Track E R-8：`WINK_STRICT_NONBLOCKING=1` 在 `runtime_cooperative_*` sample CMake 挂载点是预置状态；由 `2026-07-01-sim-cooperative-scheduler-plan` T5 阶段接手。
    2. Track E：`WINK_ASSERT_NONBLOCKING()` 第三层 PT-context 检测是 no-op 占位；T5 阶段替换宏体即可，无需再改 API 挂载点。
    3. ✅ Track C：`tools/app_codegen.py` 中 2 处 `dal_ultrasonic_read` 模板生成 blocking 调用 —— **已修复（2026-07-02 后续）**：两处示例都改为 `dal_ultrasonic_request_measurement` + `dal_ultrasonic_get_cached_distance` 非阻塞模式。
    4. Track C：codegen prompt few-shot 接入路径待定；未来 codegen 硬化专题处理。
  Q3 优化包全部 5 条真正需要执行的整改事项落地；ADR-0015/0016/0017/0018 四份新决策 Accepted；错误码 SSOT 化到 §07 §11；`pal_resource` 从空壳转为真实生效的语义级冲突治理；`pal_gpio_read/write` 静默降级消除；OSAL 临界区 task/ISR 双入口显式化；阻塞 API 三层硬隔离；IRQ 子系统契约诚实（AI Codegen 组合空间 ~36 → ~3）。R-1..R-8 全部红线通过；R-7（单 PR 单 Track）在 3cd7d21 处违反已复盘并加 R-010 到风险登记册。计划状态 ✅ 已完成。

---

## Amendment Log（后续）

### 2026-07-02：修复 app_codegen.py 阻塞 API 模板
- **Track C follow-up**：`tools/app_codegen.py` 中两处示例代码（epilog 示例 + 完整示例）原本调用 `dal_ultrasonic_read`（阻塞 busy-wait），已全部改为非阻塞模式：
  - 请求：`dal_ultrasonic_request_measurement`
  - 获取：`dal_ultrasonic_get_cached_distance`
  - 错误处理：`WINK_ERR_BUSY` 分支显式化
- **影响**：未来 AI Codegen 从示例复制代码时，不会再生成阻塞调用，避免协程卡死。

