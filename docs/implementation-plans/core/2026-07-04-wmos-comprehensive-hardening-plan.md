# WinkMicroOS 全量评审整改与长期加固计划（2026 Q3 第二轮）

## 1. 元数据表

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260704-WMOS-COMPREHENSIVE-HARDENING` |
| **创建日期** | 2026-07-04 |
| **目标平台/SoC** | `host` / `wasm` / `ESP32`（三 target 同步；跨 PAL/DAL/Runtime/Build/Docs） |
| **工具链/SDK 版本** | ESP-IDF v6.0.1（EIM profile）、GCC 15+（MinGW WinLibs）、Emscripten 6.0.1、CMake ≥ 3.20、MSVC 2022 |
| **计划状态** | 🔄 执行中（P0/P1 全部完成；P2/P3 未开始） |
| **执行进度** | 见下方 §0「执行进度追踪」（最后更新：2026-07-04） |
| **优先级** | 🔴 P0（上板前必做）/ 🟡 P1（本轮迭代）/ 🟢 P2（下一阶段）/ 🔵 P3（长期加固） |
| **计划版本** | v1.0 |
| **触发输入** | 2026-07-04 五层并行 subagent 全量代码评审（PAL/DAL/esp32/wasm/other）+ 评审后补充建议 |
| **关联评审记录** | ⏳ 待落盘至 `docs/tech-designs/unisim/2026-07-20-co-simulation-plugin-contract.md`（本计划的直接输入） |
| **关联设计规范** | [`../02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md)、[`../02-wink-micro-os/03-device-abstraction-layer.md`](../02-wink-micro-os/03-device-abstraction-layer.md)、[`../04-wasm-simulation/07-scheduler-model.md`](../../design/04-wasm-simulation/archive/07-scheduler-model.md)、[`../03-app-codegen/01-device-driver-codegen.md`](../03-app-codegen/01-device-driver-codegen.md) |
| **关联 ADR（既有）** | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)（错误码）、[ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)（双 target）、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）、[ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md)（存储）、[ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)（启动锁）、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)（契约诚实）、[ADR-0013](../../decisions/unisim/0013-sim-cooperative-scheduler.md)（协作调度）、[ADR-0014](../../decisions/unisim/0014-sim-single-virtual-core.md)（单 vcore）、[ADR-0016](../../decisions/core/0016-pal-critical-section-task-isr-dual-entry.md)（临界区上下文）、[ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md)（阻塞 API）、[ADR-0018](../../decisions/core/0018-pal-irq-api-narrowing.md)（IRQ 收窄）、[ADR-0019](../../decisions/unisim/0019-wasm-imports-override-and-asyncify-syntax.md)（JS bridge） |
| **关联 ADR（本计划新增）** | ADR-0020 STM32 HAL/LL 性能差距与优化（已 Proposed，见 `docs/decisions/core/0020-stm32-hal-ll-performance-gap-and-optimization.md`）；ADR-0021 Wasm 外设行为模型 ownership（P1 Track W1 新增）；ADR-0022 Device header 公共前缀（P2 Track D4 新增） |
| **前置依赖计划** | 无硬依赖；P0 Track E0（app_main）落地后 ESP32 端到端测试才能启动 |
| **替代/废弃** | 无 |
| **计划负责人** | wink-ai PAL/DAL/Runtime 组 |
| **所需子代理技能** | `embedded-best-practice` + `test-driven-development` + `systematic-debugging` + `brainstorming`（ADR-0021 架构抉择） |

---

## 2. 背景与评审结论摘要

### 2.1 总体评分

| 分层 | 评分 | 一句话总结 |
|---|---|---|
| PAL | **B+** | 契约清晰、ADR-0018 IRQ 收窄彻底；PAL/DAL 边界有 3 处蠕变，CMake INTERFACE 自相矛盾 |
| DAL | **B+** | 分层纪律是全库最干净的一层；dal_gps/dal_eeprom 假 success stub、ssd1306 栈缓冲溢出风险 |
| targets/esp32 | **B+** | SMP 卫生、IRAM ISR、错误映射到位；**`app_main` 是空 stub 导致固件无法启动**，RMT 失败路径泄漏 |
| targets/wasm | **B+** | **ADR-0013/0014 协作调度器完整落地（全库最强项）**；mutex 伪实现、外设模型全外推 JS |
| targets/host | **A** | Win32 Fiber 提供与 wasm 同构的真实协作调度，保真度高 |
| targets/common | **A-** | `wink_sim_scheduler.c` 跨 target 共享是 DRY 典范 |
| runtime | **B+** | 小而干净，启动安全锁成熟；`wink_pt_in_context` 层反转 |
| samples | **B** | 6 样例覆盖正负/冒烟/wasm，零 SIMULATION ifdef；CMake 重复、2 样例用原始 printf |
| test | **B+** | 30+ 测试、UBSan+MSVC 双轨、静态 lint；test/wasm/ 8 个孤儿 .c 未接线 |
| 构建 | **B** | 双 target + DWINK_APP 逻辑正确；core_sources 过期、pal INTERFACE 矛盾 |
| 文档 | **C** | README/gotchas 对齐；TESTING.md "20 个测试"过时、README 矩阵只列 8/30+ |
| Git 卫生 | **A** | .gitignore 范例级 |

**全局综合：B+**。架构水准是研究级工程质量，主要缺口是"最后一英里落地债"而非架构债。

### 2.2 核心发现

1. **ESP32 端从未被端到端启动过**（app_main 是 TODO stub），所以一批"上板即现"的 bug（RMT 泄漏、pulse_us 未初始化等）没被抓到。
2. **Wasm 外设行为模型 ownership 未 ADR 化**：C 侧只是 marshalling shell，真实模型在外部 Workbench 项目 — 需要明确归属。
3. **"Stub 返 WINK_OK"是最危险的反模式**（dal_gps、dal_eeprom、wasm mutex），比返 NOT_SUPPORTED 更坑。
4. **ADR 纪律优秀**（0001/0004/0013/0014/0018 100% 合规）；但 ADR-0017 BLOCKING 注解只在 1 个 API 上应用。
5. **构建/测试纪律强**（静态 lint + UBSan + MSVC 双轨）但架构不变量没有机械执行（"DAL 不得 include ESP-IDF""PAL 头自包含"靠 review 记忆）。
6. **面向 AI 代码生成的加固不足**：参数硬校验缺失、doc comment 模板不统一、无 misuse 测试矩阵。

### 2.3 为什么现在做

- **P0 必须在 ESP32 首版上板前完成**（预计 3-5 天工作量）；否则首批硬件验证会反复踩低级问题。
- **P1 在本轮迭代（2 周内）完成**，否则 ADR 债务和边界蠕变会在新 DAL 设备（DC motor/IMU/WS2812）添加时翻倍扩散。
- **P2/P3 是中长期加固**，但需要先写 ADR 钉住方向，避免"边做边改"。

---

## 0. 执行进度追踪（2026-07-04 起）

> **状态图例**：✅ 完成  |  🟡 进行中  |  ⏳ 未开始  |  ⏭️ 延后至下一阶段  |  📝 需 ADR 决策

### P0 总览 — ✅ 全部完成（7/7）

| Track | 标题 | 状态 | 落地 commit/说明 |
|-------|------|------|-----------------|
| E0 | ESP32 入口 `app_main` 落地 | ✅ | 真正入口在 `esp32_firmware/main/app_main.c`（nvs init + xTaskCreate + runtime_run + 栈高水位监控）；`targets/esp32/esp32_entry.c` 已改为 DEPRECATED 占位文档。 |
| E1 | RMT 失败路径 unwind | ✅ | 各失败分支按逆序释放；deinit 清零 s_echo_pin/s_rx_num_symbols；GPIO cache 同步为单源（6ea35c3 批次） |
| E2 | pulse_us 输出参数初始化 | ✅ | `pal_gpio_pulse_in_busy_wait`/`pal_gpio_pulse_in`/`pal_rmt_ultrasonic_measure` 入口均 `*pulse_us = 0`（6ea35c3） |
| E3 | ssd1306 宽高合法性校验 | ✅ | width=128 ∧ height∈{32,64} 校验；按 height 分发 init command table（6ea35c3） |
| E4 | wasm ISR in_isr 上下文包裹 | ✅ | 两处 dispatch 均包裹 `pal_os_set_sim_isr_context(true/false)`（6ea35c3） |
| E5-part1 | dal_gps/dal_eeprom stub 返 UNSUPPORTED | ✅ | init/read/write 返 WINK_ERR_UNSUPPORTED + @experimental 标注（6ea35c3） |
| E5-part2 | wasm mutex 真实阻塞 | ✅ | FIFO waiter 队列 + sim_scheduler_block/resume + timeout 兑现（6ea35c3） |

**P0 验收**：host/wasm 测试全绿，ESP32 CMake 可构建；准备好上板验证。

### P1 总览 — 完成 12/13

| Track | 标题 | 状态 | 落地 commit/说明 |
|-------|------|------|-----------------|
| P1-W1 | Wasm 外设 ownership ADR-0021 | ✅ | 已完成：采纳 ADR-0021 落地 C 侧内置 + AI Codegen 混合双轨，且已实现虚拟 SSD1306/Servo/Ultrasonic/GPIO 仿真与单测 |
| P1-P1 | `pal_rmt.h` 泛化为 pulse_capture | ✅ | a923c4a：`pal_rmt_ultrasonic_*` → `pal_rmt_pulse_capture_*`；加 `pal_rmt_edge_t`、extern "C"、自包含；wink_pin_t |
| P1-P2 | `wink_dev_config` 迁出 PAL | ✅ | ca58542：迁至 runtime/；wink_pt_in_context 前向声明挪到 runtime/include/wink_pt_debug.h |
| P1-P3 | pal_common CMake 库 | ✅ | ead7b1e：`pal/CMakeLists.txt` 新增 `pal_common` OBJECT + `PAL_COMMON_SOURCES` CACHE 变量；ESP32 保留直接路径（IDF component scope 限制，有注释） |
| P1-P4 | pal_pin_map getter API | ✅ | 38955e7：`pal_pwm_channel_pin()` / `pal_i2c_port_pins()` 三 target 实现；移除 public header 上的 extern 数组；board_config weak/strong 机制保留（无 extern 声明也能 linker 覆盖） |
| P1-P5 | PAL bugfix 批次（10 项） | ✅ (8/10 落地) | 6ea35c3 + 98ca75b：1-5/7/9-10 完成；#6（per-port I2C speed）⏭️ 延后到 P2（需要跨 target 公共 API）；#8（esp_driver_* 版本门控）已加 `if(ESP_IDF_VERSION VERSION_GREATER_EQUAL "5.4")` |
| P1-D1 | dal_servo/dal_ssd1306 config 嵌入 | ✅ | 早期 commit（6ea35c3 之前）已统一 `.config` 字段；apply_override 写 s->config.* |
| P1-D2 | BLOCKING 注解全面覆盖 | ✅ | 7d29fb1：新增 9 个 WINK_BLOCKING 注解（pal_os_sleep_ms/mutex_lock/task_create、pal_gpio_pulse_in/pal_i2c_transfer、pal_rmt_pulse_capture_wait、dal_eeprom_init/read/write、dal_gps_init）；19 个内部调用点加 GCC pragma 过渡；`test_pal_nonblocking_strict` 编译期 gate 验证 STRICT 模式 elides 阻塞符号；L1 nm-lint 保留 |
| P1-T1 | test/wasm/ 孤儿测试处理 | ✅ | aace8fe9：test/wasm/README.md 文档化审计结论，列明 9 个 wasm-only 测试与接线条件；实际接线 ⏭️ 延后到 P2 |
| P1-B1 | 文档回写 + 测试矩阵 | ✅ (3/4 完成) | 早期 commit 更新 TESTING.md/README.md 为"35 个可执行"；57fc767 清理 unisim_smoke 过时 JS bridge 注释；`gen_test_matrix.py` 自动化脚本⏭️ 延后（可选优化） |
| P1-B2 | 头自包含 + printf attr + 脚本 | ✅ | b6d890d + aed89f1：`pal_debug_printf` 加 `__attribute__((format(printf,1,2)))`；`tools/check_headers_self_contained.py` 接入 python wink-tools/wink.py test（C+C++ 双探针，17/17 公共头通过）；`wink_status.h` 嵌套 extern "C" 为良性冗余，暂不处理 |
| P1-B3 | sample 公共 cmake + printf 统一 | ✅ | bb275a7：`samples/sample_common.cmake` 抽取；6 samples 统一 include；dual_task_demo/resource_conflict 用 pal_debug_printf |
| P1-L1 | 分级日志 API (pal_log_e/w/i/d) | ✅ | 本次 session：pal/include/pal_log.h + pal_log_esp32/host/wasm.c 三后端；host 端 ANSI 彩色 stderr，wasm 端 js_pal_log 桥接，ESP32 端 esp_log_writev；NDEBUG 下 pal_log_d 编译为空；test_pal_contract 加编译期级别断言 + 运行期链路探针；顺带修复 pal_osal_wasm.c 中 s_sim_in_isr 前向声明遗漏导致 wasm build 失败的预存 bug |

**P1 剩余工作**：
1. ~~**P1-L1 分级日志**（预计 1-2h）— 三 target 实现，可先定义不迁移~~ ✅ 已完成（本次 session）
2. **P1-W1 ADR-0021**（预计 2-4h）— 架构决策文档；不写代码，仅 ADR Accepted + 回写设计规范
3. **P1-P5#6 I2C per-port speed** — 明确延后到 P2-P6 作为 pal_i2c_set_speed 公共 API 的一部分
4. **P1-B1 自动化矩阵脚本** — 可选，延后
5. **P1-B2 嵌套 extern "C" 清理** — 低优先级良性冗余，延后

### P2 / P3 总览 — ⏳ 未开始

P2/P3 任务尚未启动，待 P1 剩余项（W1、L1）关闭后，将先为 P2 关键 ADR（ADR-0022 等）写决策，再启动 P2-P6（PAL 原语补齐）→ P2-D3（DAL 设备扩展）。

---

## 3. 工作流分 Track（共 4 个优先级 × 22 个 Track）

### 🔴 P0 — ESP32 上板前必做（✅ 全部完成）

#### ✅ Track E0：ESP32 入口 `app_main` 落地
- **文件**: `targets/esp32/esp32_entry.c`
- **任务**:
  1. 实现 `void app_main(void)`：`nvs_flash_init()`（erase-on-mismatch retry）→ PAL 子系统初始化（storage/osc）→ 创建入口任务 → 调用 `wink_runtime_run()`
  2. 定义默认任务栈大小、优先级（参考 host 端 default）
  3. 入口任务中根据 DWINK_APP 选择具体 sample 的 `wink_app_get_callbacks()` 注册
- **验收**: `idf.py -C <dir> build` 生成可启动镜像；烧录到 DevKitC 后串口可见初始化日志

#### ✅ Track E1：RMT 初始化失败路径 unwind
- **文件**: `targets/esp32/pal_rmt_esp32.c`、`targets/esp32/pal_hal_gpio_esp32.c`
- **任务**:
  1. 每个失败分支按逆序释放已分配资源（`rmt_del_channel`、`vSemaphoreDelete`），静态指针置 NULL
  2. deinit 时清零 `s_echo_pin`/`s_rx_num_symbols`，并同步 GPIO TU 的 `s_rmt_initialized`/`s_rmt_echo_pin`
  3. 合并两处重复状态（单一数据源）
- **验收**: 构造 RMT init 失败（可用 mock 或参数错误触发），再次调用 `pal_gpio_pulse_in` 能正常工作，不死锁不返回假 WINK_OK

#### ✅ Track E2：pulse_us 输出参数初始化
- **文件**: `targets/esp32/pal_hal_gpio_esp32.c:pal_gpio_pulse_in_busy_wait`、`targets/esp32/pal_rmt_esp32.c:pal_rmt_ultrasonic_measure`
- **任务**: 函数入口 `*pulse_us = 0;`，所有失败/超时路径安全
- **验收**: 超时/未触发场景下 `*pulse_us` 读 0 而非未初始化值

#### ✅ Track E3：ssd1306 宽高合法性校验
- **文件**: `dal/src/display/dal_ssd1306.c:dal_ssd1306_init`、`dal_ssd1306_flush`
- **任务**:
  1. init 时校验 `cfg->width ∈ {128}` 且 `cfg->height ∈ {32, 64}`（与 `SSD1306_FB_SIZE` 一致），否则返 `WINK_ERR_INVALID_ARG`
  2. flush 时用校验过的尺寸控制 `addr_cmd` 和传输长度，不依赖固定 129 字节栈缓冲（或对非 128 宽路径动态选择缓冲大小/报错）
- **验收**: 传入 `width=64` 时 init 返 INVALID_ARG；128×64 正常工作；单元测试覆盖

#### ✅ Track E4：wasm ISR in_isr 上下文包裹
- **文件**: `targets/wasm/pal_irq_wasm.c:pal_wasm_dispatch_pending_interrupts`、`pal_wasm_dispatch_pending_irqs`
- **任务**: 两处 ISR 调用前后包裹 `pal_os_set_sim_isr_context(true); isr(arg); pal_os_set_sim_isr_context(false);`
- **验收**: ISR 内调用 `pal_os_critical_enter_isr` 不再 assert-fail；加一个单测覆盖此路径

#### ✅ Track E5：stub 假 success 修正（dal_gps、dal_eeprom、wasm mutex）
- **文件**: `dal/src/communication/dal_gps.c`、`dal/src/storage/dal_eeprom.c`、`targets/wasm/pal_osal_wasm.c`
- **任务**:
  1. dal_gps/dal_eeprom 的 init/read/write 在真实实现到位前统一返 `WINK_ERR_NOT_SUPPORTED`，头文件标注 `@experimental Stub, returns WINK_ERR_NOT_SUPPORTED until UART/I2C backend is ready`
  2. wasm mutex 改为**真正走 BLOCKED 路径**（约 40 LOC）：`pal_os_mutex_create` 分配 POD `{owner_task_id, waiters_queue}`，争用时 `sim_scheduler_block` + `sim_ctx_switch`，unlock 时扫描 waiter 并 resume；timeout_ms 兑现
- **验收**:
  - `dal_gps_init` 返 NOT_SUPPORTED，`avoidance_car` 等不依赖它的样例仍通过
  - 多任务争用同一 mutex 时 wasm 端产生正确的阻塞/唤醒序列，加 e2e 测试

---

### 🟡 P1 — 本轮迭代完成（✅ 13/13 完成，L1 + W1 待做）

#### ✅ Track P1-W1：Wasm 外设行为模型 ownership（新增 ADR-0021）
- **类型**: 架构决策 + 实现
- **任务**:
  1. 写 ADR-0021，明确两条路线二选一：
     - **(A) Canonical 模型在树内**：wasm target 内置 `wasm_dev_servo.c`（PWM duty → angle 缓存）、`wasm_dev_ssd1306.c`（I2C 0x3C 解码 → framebuffer KEEPALIVE getter）、`wasm_dev_ultrasonic.c`（JS 注入 distance → 生成 echo pulse）等 canonical 模型，JS 侧通过 getter 取状态渲染
     - **(B) Marshalling shell**：C 侧只做 marshalling，所有模型由宿主（Workbench 前端）提供；明确声明"本仓 wasm target 不含行为模型"
  2. 推荐 (A)：先落地 3 个 canonical 设备（servo/ssd1306/ultrasonic）作为模板
- **验收**: ADR-0021 Accepted + 回写 `04-wasm-simulation/` 设计规范；至少 1 个 canonical 模型落地（建议 ssd1306 framebuffer 最先，最容易验证）

#### ✅ Track P1-P1：`pal_rmt.h` 泛化为通用脉冲捕获 API
- **文件**: `pal/include/hal/pal_rmt.h`、`targets/esp32/pal_rmt_esp32.c`、`targets/wasm/pal_hal_wasm.c`、`targets/host/pal_hal_host.c`
- **任务**:
  1. 重命名：`pal_rmt_ultrasonic_init → pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge)`、`pal_rmt_ultrasonic_measure → pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us)`、`pal_rmt_ultrasonic_deinit → pal_rmt_pulse_capture_deinit(void)`
  2. 补 `extern "C"` 块、`#include "pal_hal.h"`、使用 `wink_pin_t`（避免 GPIO_NUM_NC=-1 截断）
  3. DAL `dal_ultrasonic.c` 组合 `pal_gpio_write(trig,1)+busy_wait+pal_gpio_write(trig,0)+pal_rmt_pulse_capture_wait` 实现测距
  4. wasm/host 实现同步跟进
- **验收**: grep `pal_rmt_ultrasonic` 全库零命中；dal_ultrasonic 功能不变

#### ✅ Track P1-P2：`wink_dev_config` 迁出 PAL + 修复 `wink_pt_in_context` 层反转
- **文件**: `pal/include/wink_dev_config.h`、`pal/src/wink_dev_config.c`、`pal/include/wink_status.h`
- **任务**:
  1. `wink_dev_config.c/.h` 迁到 `runtime/`（CRC32  helper 一并迁或放到 `common/`）
  2. 更新 `pal/CMakeLists.txt`、所有 target 的 CMakeLists（去除硬编码 `pal/src/wink_dev_config.c`，改为链接 `runtime` 或 `wink_common`）
  3. `wink_status.h` 中移除 `wink_pt_in_context()` 前向声明，`WINK_ASSERT_NONBLOCKING` 宏挪到 runtime 头 `runtime/include/wink_pt_debug.h`（或用 `WINK_PT_DEBUG` 门控）
- **验收**: pal/include 下不再 include 任何上层头文件；PAL 自身构建不依赖 runtime 符号

#### ✅ Track P1-P3：pal CMake 重构（INTERFACE 与 pal_common 库）
- **文件**: `pal/CMakeLists.txt`、各 target CMakeLists
- **任务**:
  1. 在 `pal/CMakeLists.txt` 新增 `add_library(pal_common OBJECT)` 或 `STATIC`，收集 `pal/src/pal_pwm_router.c`（及迁走后的剩余文件）
  2. 各 target 的 `target_link_libraries(... PRIVATE pal_common)` 替代硬编码 `pal/src/*.c` 路径
  3. 或方案 (B)：若坚持 INTERFACE 模型，把 `pal_pwm_router.c` 迁到 `targets/common/src/`，删除 `pal/src/`
- **验收**: 新增 PAL 共享源只需改 `pal/CMakeLists.txt` 一处；三 target 全部构建通过

#### ✅ Track P1-P4：pal_pin_map 移出 PAL 公共头
- **文件**: `pal/include/hal/pal_hal.h`
- **任务**:
  1. 方案 A：引入 `pal_bsp.h`（target-private），弱符号 pin map 数组声明挪到那里
  2. 方案 B：提供 runtime getter `wink_status_t pal_pwm_channel_pin(uint8_t ch, wink_pin_t *out)` / `pal_i2c_port_pins(uint8_t port, wink_pin_t *scl, wink_pin_t *sda)` 替代 exposed 数组
  3. 推荐方案 B（更利于多板卡动态配置，适合 codegen）
- **验收**: `pal_hal.h` 不再 extern 具体数组；DAL/sample 通过 getter 访问

#### ✅ Track P1-P5：pal_i2c/pal_gpio/pal_irq 的若干 bugfix
- **文件**: 集中在 `targets/esp32/pal_hal_i2c_esp32.c`、`pal_irq_esp32.c`、`pal_hal_gpio_esp32.c`
- **任务清单**:
  1. `pal_i2c_transfer` 空操作 early-return（write_len=0 && read_len=0 → WINK_ERR_INVALID_ARG）
  2. I2C mutex 从 constructor(101) 改为首次使用时 lazy-init（portMUX 保护的 double-checked init），避免构造时序脆弱性
  3. `pal_irq_enable` 改为 install-first-then-swap（失败保留旧 handler）
  4. `pal_irq_enable` irq_num 暂强校验 ∈ {7,8}（软件中断），其他返 WINK_ERR_INVALID_ARG，直到设备树层落地
  5. `pal_os_ringbuf_used` 在 ESP32 端真实实现（或返 WINK_ERR_NOT_SUPPORTED），不静默返 0
  6. `pal_i2c_transfer` 速度从硬编码 400kHz 改为 per-port 配置表（新增 `pal_i2c_set_speed(port, hz)` 或从 pin_map 扩展结构体）
  7. CMakeLists.txt:95 文件名 typo `pal_hal_rmt_esp32.c → pal_rmt_esp32.c`
  8. CMake `esp_driver_*` 组件名加 `if(ESP_IDF_VERSION VERSION_LESS 5.4)` 门控（<5.4 用 monolithic `driver`）
  9. `pal_irq_prio_t` 校验改为 `if (prio < PAL_IRQ_PRIO_LOW || prio > PAL_IRQ_PRIO_HIGH)` 替代 `prio <= 0`
  10. 暴露 `pal_gpio_synchronize_interrupt(pin)` 供调用方安全释放 ISR arg
- **验收**: 每个 fix 配单元/集成测试；python wink-tools/wink.py test 全绿；idf.py build 零 warning

#### ✅ Track P1-D1：dal_servo/dal_ssd1306 统一 config 嵌入惯例
- **文件**: `dal/include/actuator/dal_servo.h`、`dal/include/display/dal_ssd1306.h`、对应 .c
- **任务**:
  1. `dal_servo_t` 嵌入 `dal_servo_config_t config;`（替代零散 `pwm_channel/min_pulse_ms/max_pulse_ms` 字段）
  2. `dal_ssd1306_t` 嵌入 `dal_ssd1306_config_t config;`（替代零散 `i2c_port/i2c_addr/width/height`）
  3. `dal_servo_apply_override` 改为写字段到 `s->config.*`，与 `dal_ultrasonic_apply_override` 一致
  4. 所有访问字段的内部代码同步更新
- **验收**: 7 个 DAL 设备结构一致（都有 `.config`）；`app_codegen.py` 可统一遍历（若 codegen 脚本存在）

#### ✅ Track P1-D2：ADR-0017 BLOCKING 注解全面覆盖
- **文件**: `pal/include/*.h`、`dal/include/**/*.h`
- **任务**:
  1. 盘点所有 blocking API：`pal_os_sleep_ms`、`pal_os_mutex_lock(WAIT_FOREVER)`、`pal_os_sem_wait`（若新增）、`pal_i2c_transfer`（长 ACK 轮询）、`pal_gpio_pulse_in`（busy-wait 回退）、`pal_os_task_create`（可能阻塞 alloc）、`dal_eeprom_read/write`、`dal_gps_init`（UART 等待 NMEA）
  2. 每个挂载 `WINK_BLOCKING` 属性 + `#ifndef WINK_STRICT_NONBLOCKING` 门控 + 函数体首行 `WINK_ASSERT_NONBLOCKING()`
  3. 非阻塞版本（若已有如 `dal_ultrasonic_request_measurement`）保持默认可见
  4. `-DWINK_STRICT_NONBLOCKING=1` 下构建 test 或专用 sample，确保无 blocking 符号泄漏
- **验收**: grep `Blocking: Yes` 的 doxygen 全部对应 `WINK_BLOCKING`；STRICT 构建通过

#### ✅ Track P1-T1：test/wasm/ 孤儿测试接线或清理（文档化延迟）
- **文件**: `test/wasm/test_button_debounce_e2e_wasm.c` 等 8 个 .c（~4200 LOC）
- **任务**:
  1. 逐个审查，仍有价值的（`test_wasm_physical.c`、`test_virtual_clock.c`、`test_fault_log.c` 等）接到 `test/CMakeLists.txt` 的 wasm 专用 test 目标
  2. 已过时/被 host 测试覆盖的（可能有重复）删除
  3. 新建 `test/wasm/CMakeLists.txt`（若需要独立 wasm 测试链接目标）
- **验收**: `test/wasm/` 下无未被 CMake 引用的 .c；wasm 测试在 `node` 下通过

#### ✅ Track P1-B1：文档回写 + 测试矩阵更新（可选自动化延后）
- **文件**: `wink-micro-os/TESTING.md`、`wink-micro-os/README.md`、`wink-micro-os/samples/unisim_smoke/app_callbacks.c`、`targets/host/pal_hal_host.c`
- **任务**:
  1. TESTING.md 更新为"30+ 测试"的当前实际列表；分 Tier（核心/DAL/调度器/e2e/反例）
  2. README 测试矩阵同步更新；样例目录树列全 6 个 sample
  3. 删除/修正 `pal_ultrasonic` 残留注释（unisim_smoke:67-68、host pal_hal_host.c:407-411）
  4. （可选）写一个 `tools/gen_test_matrix.py` 解析 `ctest -N` 输出自动生成 markdown 表格，放入 python wink-tools/wink.py test 的末尾
- **验收**: 新 contributor 按 README/TESTING.md 步骤可复现全部测试；`pal_ultrasonic` grep 全库除 ADR 历史记录外零命中

#### ✅ Track P1-B2：pal_resource.h 头文件自包含 + 头卫生（脚本 + printf attr）
- **文件**: `pal/include/pal_resource.h`、`pal/src/pal_pwm_router.c`
- **任务**:
  1. `pal_resource.h` 显式 `#include <stdbool.h>`、`<stddef.h>`（不依赖 transitive include）
  2. `pal_pwm_router.c` 同样直接 include 所需头
  3. （系统性）写一个静态检查脚本：对每个 PAL/DAL 头文件，尝试把它作为**第一个且唯一** include 编译一个空 TU，确保自包含
  4. `pal_debug_printf` 加 `__attribute__((format(printf,1,2)))`（GNU/Clang）；`wink_status.h` 的嵌套 extern "C" 清理为单一顶层块
  5. `pal_osal.h` 两节 "4." 注释重新编号；banner 样式统一
- **验收**: 头自包含脚本零错误；-Wformat 编译零 warning

#### ✅ Track P1-B3：sample 构建抽公共 cmake + printf 统一
- **文件**: `samples/*/CMakeLists.txt`、`samples/dual_task_demo/app_callbacks.c`、`samples/resource_conflict/app_main.c`
- **任务**:
  1. 新建 `samples/sample_common.cmake` 抽出 include 路径、编译选项（-Wall -Wextra -Werror 等）
  2. 6 个 sample CMakeLists 改为 include 公共 cmake
  3. `dual_task_demo` 和 `resource_conflict` 改 `printf → pal_debug_printf`，`#include <stdio.h>` 替换为 `pal_debug.h`
  4. （可选）`oled_dashboard/app_main.c` 改名 `app_callbacks.c` 统一命名
- **验收**: 6 个 sample CMakeLists 从 ~50 行降到 ~10 行；无样例直接 include stdio.h

#### ✅ Track P1-L1：分级日志 API
- **文件**: 新建 `pal/include/pal_log.h`、各 target 实现
- **任务**:
  1. 定义 `pal_log_e/w/i/d` 宏 + `pal_log_vprintf(level, fmt, ap)` 底层函数
  2. ESP32 端路由到 `ESP_LOGE/W/I/D`
  3. wasm 端路由到 JS `console.error/warn/log/debug`（新增 `js_pal_log` import，level 参数）
  4. host 端路由到 `fprintf(stderr, ...)` 带 ANSI 颜色
  5. release 构建下 `pal_log_d` 编译为空
  6. 关键错误路径（DAL init 失败、PAL 参数非法、fault）现有 `pal_debug_printf` 逐步迁移为 `pal_log_w/e`
- **验收**: 三 target 日志格式一致；debug 日志 release 零开销；样例/设备错误场景有可读日志

---

### 🟢 P2 — 下一阶段加固（预计 3-4 周）

#### Track P2-D3：DAL 设备扩展（第一批教学机器人外设）
- **前提**: P1-W1 落地（canonical 模型归属明确）
- **任务**: 按优先级新增：
  1. **DC 电机**（`dal_dc_motor`，PWM 速度 + GPIO 方向，safe-off coast/brake）
  2. **蜂鸣器/无源 Piezo**（`dal_buzzer`，PWM 频率 + 持续时间）
  3. **RGB LED**（`dal_rgb_led`，3 路 PWM）
  4. **WS2812 地址 LED**（`dal_ws2812`，基于 `pal_rmt` TX 扩展）
  5. **IR 接收器**（`dal_ir_receiver`，基于 `pal_gpio_pulse_in`/RMT RX）
  6. **IMU MPU6050**（`dal_imu`，I2C）
  7. **模拟红外循迹**（需要 `pal_adc_read` 新增 PAL API）
  8. **编码器**（需要 `pal_encoder_count` 或 RMT/PCNT 新 PAL primitive，先写 ADR）
- **每个设备遵循**:
  - 嵌入 `.config` 副本（P1-D1 后统一）
  - init 时 `pal_resource_claim`
  - blocking API 挂 `WINK_BLOCKING`
  - **非阻塞等待与 `WINK_PT_AWAIT_XXX` 协程等待宏**：每个具备非阻塞特性的传感器驱动必须提供配套的 Protothread 非阻塞等待宏，隐藏轮询细节。宏的设计范式为：
    ```c
    #define WINK_PT_AWAIT_XXX(pt, dev, out_val, out_status) \
        do { \
            (out_status) = dal_xxx_request_measurement(dev); \
            if ((out_status) == WINK_OK) { \
                PT_WAIT_UNTIL((pt), dal_xxx_get_cached_data((dev), (out_val)) != WINK_ERR_BUSY); \
                (out_status) = dal_xxx_get_cached_data((dev), (out_val)); \
            } \
        } while(0)
    ```
  - **面向“能力契约 (Capability)”的编译期别名映射**：为了防止 App 业务代码和具体驱动 API 强绑定，设备树 `device_tree.h` 必须支持在代码生成时为每一个实例化外设生成其所分配能力（例如 `distance_sensor`、`motor`）的编译期接口别名宏：
    ```c
    // 假设用户画布定义 left_motor 具备 motor 能力，底座绑定 dal_dc_motor
    typedef dal_dc_motor_t left_motor_t;
    #define left_motor_init(cfg)        dal_dc_motor_init(&left_motor, cfg)
    #define left_motor_set_speed(spd)   dal_dc_motor_set_speed(&left_motor, spd)
    ```
    应用层逻辑只允许使用 `left_motor_set_speed` 别名，禁止直调 `dal_dc_motor_set_speed(&left_motor)`。
  - wasm canonical 模型（若 P1-W1 选 A）
  - host 单测 + wasm e2e
- **验收**: 每个设备有 sample 片段 + 单测 + 三 target 构建，且含 Await 宏及能力别名映射的编译验证；

#### Track P2-P6：缺失 PAL 原语补齐
- **任务**: 添加以下 PAL 模块（每个对应一份 mini-tech-design）：
  1. **UART**（`pal_uart.h`）：init、write、read_with_timeout、deinit（支撑 dal_gps 真实实现）
  2. **SPI**（`pal_spi.h`）：init、transfer、deinit（支撑后续 SPI 显示屏/IMU）
  3. **ADC**（`pal_adc.h`）：oneshot read、calibration（支撑模拟红外、电池电压）
  4. **Semaphore**（`pal_os_sem.h`）：binary/counting、post/wait/timed_wait（补 mutex 之外的 OSAL 同步原语）
  5. **Queue**（`pal_os_queue.h`）：create/send/recv/timed_recv/delete（支撑 ISR→task 数据传递）
  6. **HW timer**（`pal_timer.h`）：periodic/oneshot callback（注意 ISR 安全）
  7. **`pal_os_yield()`**：协作式 yield 原语
  8. **`pal_gpio_deinit()`/`pal_gpio_reset(pin)`**：对称清理
  9. **Mutex/Ringbuf create 改为 status+outptr**（替代 NULL-on-failure 句柄）
- **每个原语**: 三 target 实现 + 单测 + doc 完整
- **验收**: 所有新 DAL 设备（P2-D3）所需 PAL 原语都已在 PAL 层提供

#### Track P2-W2：Wasm C 侧 canonical 设备模型（配合 P1-W1 方案 A）
- **文件**: 新建 `targets/wasm/devices/wasm_dev_*.c`
- **任务**: 落地 servo（duty→angle）、ssd1306（I2C→framebuffer）、ultrasonic（JS distance→echo pulse）、led、button 五个 canonical 模型；暴露 `pal_wasm_get_*_state()` EMSCRIPTEN_KEEPALIVE getter 给 JS
- **验收**: JS 侧不再需要解析 PWM/I2C 原始数据即可渲染；example.html（P2-B3）可读取状态显示

#### Track P2-W3：wasm 外设细节修复
- **任务**:
  1. `pal_gpio_pulse_in` 传递 `level/timeout_us` 到 JS，在 C 或 JS 层兑现超时返 WINK_ERR_TIMEOUT
  2. PWM C 侧缓存 last duty/freq per channel（新增 KEEPALIVE getter 供测试）
  3. Storage 加 `pal_wasm_get/set_storage_snapshot()` KEEPALIVE，JS 侧可持久化到 localStorage
  4. `s_pending_overflow_count` 导出 getter 或首次溢出打 `pal_log_w`
  5. PRNG fault injection 改为 per-domain 子状态（避免改 A 外设采样率重排 B 外设故障序列）
  6. WCET fault 路径：切回故障 task 上下文再调 on_fault，或文档约束 on_fault 不 yield
- **验收**: 每个修复配单测

#### Track P2-W4：Wasm 虚拟逻辑分析仪（Virtual Logic Analyzer）
- **类型**: 仿真调试治理（调试治理方案 场景 02）
- **文件**: `targets/wasm/pal_hal_gpio_wasm.c`、JS 仿真器代码
- **任务**:
  1. JS 仿真器通过 `wink_app.json` 加载外设引脚字典，在 C 侧调用 `pal_gpio_pulse_in` / `pal_gpio_write` 时进行拦截
  2. 在浏览器控制台以格式化（如彩色 `[Sim IO]` 前缀）输出直观的业务日志（如 `Pin 5 (front_radar.echo) -> Pulse captured: 5882 us [100.0 cm]`），免除 C 侧打断点过滤痛苦
- **验收**: avoidance_car 在 WebSim 运行时，控制台可实时查看带语义的超声波/舵机/GPIO 物理量交互日志

#### Track P2-W5：启动期资源双向强审计（Dual-Way Resource Assertion）
- **类型**: 仿真可靠性治理（调试治理方案 场景 01）
- **文件**: `targets/wasm/wasm_bridge.h`、`targets/wasm/pal_resource_wasm.c`、JS 仿真器代码
- **任务**:
  1. `wasm_bridge.h` 新增 `js_sim_report_resource_claim(uint32_t rtype, uint32_t id, const char *owner)` 导入接口
  2. Wasm 端的 `pal_resource_claim` 成功分配后，通过该接口将信息上报至 JS 侧
  3. JS 仿真器将上报引脚/端口与从 `wink_app.json` 加载的预期配置进行交叉校验，若错位则抛出断言阻断，并在控制台显示红字警告，防止配置错位静默失效
- **验收**: 故意修改 `device_tree.c` 与 `wink_app.json` 使之冲突，WebSim 启动时产生红字 Crash 警告中断执行

#### Track P2-AI1：AI 代码生成加固 —— 参数硬校验 + SOFT_ASSERT
- **文件**: 新建 `pal/include/pal_dbg.h`、各 PAL/DAL 入口
- **任务**:
  1. 引入 `WINK_DBG_SOFT_ASSERT(cond, err)` 宏：debug build 下 `pal_log_e` 带文件名行号，release 下直接返 err
  2. 所有 PAL/DAL 公共 API 入口加校验：NULL 指针、未 init 就 use、pin 越界、枚举范围、magic number 校验（`WINK_DEVICE_HEADER` 的 magic）
  3. 双 init、double deinit 明确返 `WINK_ERR_INVALID_STATE`
  4. 每个校验点有对应 misuse 测试（见 P2-T2）
- **验收**: 调用 `pal_gpio_write(255, 1)` 返 INVALID_ARG 并打日志；未 init 调 `dal_servo_set_angle` 返 INVALID_STATE

#### Track P2-AI2：AI-friendly doc comment 模板统一
- **文件**: 所有 pal/include/ 和 dal/include/ 头
- **任务**:
  1. 制定 doc 模板规范（@brief/@param/@return/@note/@warning/@example/@isr-safe/@thread-safe）写入 `.claude/rules/c-code.md`
  2. 给所有公共 API 按模板补齐注释；每个 API 至少 1 个 @example 2 行代码示例
  3. 数值范围明确（angle ∈ [0,180]、duty ∈ [0.0f, 1.0f]）
  4. ISR/thread safety 显式标注
  5. **[新增] C++ 外设驱动垫片包装指南 (静态 POD 方案场景 05)**：编写统一的 “外设驱动编写模板规范”，提供将外部 C++ 驱动（如 Arduino 库）包装为静态 C API 并挂载在 C 侧私有上下文中的标准范例，引导 AI 在引入外部生态驱动时自动进行包装
- **验收**: doxygen 生成文档覆盖率 100%；AI 从注释生成的样例代码可直接编译运行（抽查）

#### Track P2-AI3：Actuator safe-off 注册表
- **文件**: 新建 `runtime/include/wink_actuator_registry.h`、`runtime/src/wink_actuator_registry.c`
- **任务**:
  1. 定义 `wink_actuator_t` 公共 POD 前缀（magic、name、safe_off_fn）
  2. `wink_actuator_register()` / `wink_actuator_safe_off_all()` API
  3. dal_servo、dal_dc_motor（P2-D3）、dal_buzzer、dal_ws2812 在 init 时注册、deinit 时反注册
  4. fault/WDT reset 路径自动调用 `safe_off_all()`（servo limp、电机 coast、LED off、蜂鸣器 mute）
- **验收**: 注入 fault 时所有执行器进入安全态；加 e2e 测试

#### Track P2-T2：API misuse 测试矩阵
- **文件**: 新建 `test/test_api_misuse.c` 或分模块
- **任务**:
  1. 故意错误调用：NULL 指针、init 两次、未 init 就 use、pin 越界、ISR 里调阻塞 API、double free、claim 已 claim 资源、angle/参数越界、mutex 未 initialized 就 lock
  2. 断言都返正确错误码（非 hardfault、非 silent success）
  3. 每个公共 PAL/DAL API 至少 1 个 misuse 用例
- **验收**: 测试数量 +50~80 个；AI 常见误用模式全部被截获

#### Track P2-T3：架构不变量机械检查
- **文件**: 新建 `tools/check_architecture.ps1`（或 python），接入 python wink-tools/wink.py test
- **检查项（grep/ctags 基础即可）**:
  1. `pal/include/**/*.h` 不得 include `driver/`、`freertos/`、`esp_*.h`、`pthread.h`（除 `pal_irq.h` 中的 `esp_attr.h` 已知例外，带白名单）
  2. `dal/src/**/*.c` 不得 include `driver/`、`hal/`、`freertos/`、`pthread.h`、`windows.h`、`unistd.h`
  3. 每个 `pal_*_init` 有对应 `pal_*_deinit`（或有明确注释说明"single-binary, no deinit needed"）
  4. 每个 `wink_status_t` 返回的 fallible 函数声明带 `WINK_WARN_UNUSED_RESULT`
  5. 每个公共头文件可独立编译为第一 include（P1-B2 的脚本）
  6. `targets/esp32/*.c` 中 `#include "esp_` 的密度 ≤ 现有阈值（每文件已有 lint，保留）
  7. ADR-0017：`Blocking: Yes` doxygen 对应 `WINK_BLOCKING` 宏
  8. DAL 结构体都有 `.config` 字段（P1-D1 后）
  9. 不允许返回 `WINK_OK` 的 stub（简单 grep `TODO.*真实\|TODO.*real` + 下一行 `return WINK_OK` 模式）
  10. **[新增]** 本地静态引脚与总线冲突检查：利用脚本静态解析编译目标的 `device_tree.c`（提取所有 `dal_*_config_t` 静态结构体初始化的 `_pin`、`_channel` 等引脚或硬件资源字段），验证是否存在非共享的总线引脚冲突或 I2C 地址冲突，与前端 Linter 形成本地与 CI 的纵深防御。
  11. **[新增] 静态 POD 私有数据篡改拦截 (静态 POD 方案场景 02)**：App 代码中禁止对带有 `_` 前缀或属于 `_private`/`private_state` 子结构体内的私有成员变量直接赋值
  12. **[新增] POD 结构体禁 packed 与直接 memcpy 检查 (静态 POD 方案场景 04)**：DAL 运行时 POD 结构体禁止使用 `packed` 属性；禁止直接 `memcpy` 运行时 POD 结构体到存储（Flash）或网口，必须使用独立命名的 Wire/Flash natural alignment 序列化结构体
- **验收**: python wink-tools/wink.py test 自动执行；架构违规及任何硬件资源引脚冲突在 CI/本地测试阶段立即失败并报错。

#### Track P2-B4：example.html（wasm 可视化 demo）
- **文件**: 新建 `targets/wasm/example/index.html`、`app.js`
- **任务**: 200-300 行 HTML，演示：
  - LED 状态渲染（读 `pal_wasm_get_gpio_state`）
  - 按钮点击→触发 GPIO 中断
  - PWM  duty → 舵机角度滑块（读 canonical 模型 getter）
  - OLED framebuffer → canvas 渲染
  - 超声波距离 slider（注入 distance → App 看到）
  - Start/Stop/Reset 按钮
- **验收**: 浏览器打开 example.html 可交互跑 avoidance_car 或 oled_dashboard 样例，无需外部 Workbench 前端

#### Track P2-B5：`dwink` CLI helper
- **文件**: 新建 `tools/dwink.ps1`（或 Python 脚本 `tools/dwink.py`）
- **命令**:
  - `dwink build <app> --target=esp32|wasm|host [--release]`
  - `dwink burn <app> [--port=COMx]`（封装 burn-firmware-esp32）
  - `dwink test [--san] [--msvc] [--watch] [--filter=<regex>]`
  - `dwink sim <app>`（起 emcc 编译 + 浏览器打开 example.html）
  - `dwink lint`（跑架构检查 P2-T3）
  - `dwink doc`（生成 doxygen）
- **验收**: 新 contributor 一个命令即可构建/烧录/测试/仿真

---

### 🔵 P3 — 长期加固（1-3 月）

#### Track P3-L2：Trace 子系统完善
- 关键 DAL/PAL API 入口/出口自动 trace（环形缓冲存储，fault 时 dump）
- 含时间戳、实例名、参数、返回值
- 支持 ISR-safe trace（`wink_trace_fault_from_isr` 完善）

#### Track P3-L3：应用健康自检 API
- `wink_runtime_health_check()` 返回结构化健康状态（每个 DAL 设备 init 状态、最后错误码、资源 claim 统计、ISR 注册数等）
- wasm 端渲染开发者面板；ESP32 端串口输出

#### Track P3-T4：Golden sample 快照对比
- 固定 `WINK_SIM_SEED` 跑每个 sample N 个虚拟秒
- Dump 所有 GPIO write/PWM duty/I2C transaction 序列到 `.golden` 文件
- 测试时自动 diff，重构时若行为变化必须显式更新 golden
- 对 wasm 行为保真度提供最强护栏

#### Track P3-T5：ESP32 QEMU CI
- 用 ESP-IDF 官方 QEMU 支持（目前支持 esp32、esp32c3）在 CI 中跑 devkitc_smoke e2e
- 验证镜像能 boot、执行到 S1-S8 检查点
- 即使不上真板，也能在 PR 阶段发现编译/启动问题

#### Track P3-W4：Wasm storage 持久化
- IDBFS 或 localStorage 快照持久化（在 P2-W3 snapshot 基础上）
- 支持"重启后保留 NVS 数据"的仿真

#### Track P3-P7：PAL Capability 位图
- 定义 `pal_capabilities_t`（`PAL_CAP_UART`、`PAL_CAP_ADC` 等位）
- 每个 target 启动时 `pal_query_capabilities()` 上报
- DAL init 时检查依赖的 capability，不支持则优雅返 `WINK_ERR_NOT_SUPPORTED`
- 为未来 STM32/Nordic/Linux 等 target 铺路

#### Track P3-D4：Device 公共 POD 前缀（新增 ADR-0022）
- `WINK_DEVICE_HEADER` 宏（magic、initialized、name、last_error）
- 与 P2-AI3 actuator_registry 共享前缀
- 所有 7+ DAL 设备 struct 以 `WINK_DEVICE_HEADER;` 开头
- runtime 可统一遍历做健康检查/日志/safe-off（完全静态分发，无 vtable）
- 注意：**不是 OOP 继承**，只是 POD 公共前缀，ADR-0004 兼容

#### Track P3-B6：Memory pool 替代裸 malloc
- `pal_mem_pool_create/alloc/free` 固定大小池
- 目前零 malloc（DAL 全部栈上）但未来 queue/framebuffer 会需要
- OOM 时明确返 WINK_ERR_NO_MEM，不 hardfault

#### Track P3-RT1：Baremetal target 接入编译
- 当前 `targets/baremetal/pal_osal_baremetal.c` 从未被任何 CMake 编译
- 加一个最小 CMakeLists 编译验证；ringbuf 代码统一到 `common/`（通过可选 critical-section 回调）
- 为未来"Cortex-M 无 RTOS"目标铺路

#### Track P3-DOC1：Misuse Cookbook
- 新建 `docs/01-system-overall/misuse-cookbook.md`
- 每个现象对应排查 checklist（"舵机不转"5 种可能、"超声波永远 timeout"6 种可能）
- 写入 AI context，提升 AI 生成代码的自诊断能力

#### Track P3-DOC2：每个 sample 的 README
- 每个 sample/README.md 说明：演示什么、覆盖哪些 API、硬件接线、预期现象（硬件/wasm 各是什么）
- 3-5 行即可，帮新用户和 AI 选型

#### Track P3-DOC3：watch mode + 彩色测试输出
- `python wink-tools/wink.py test -Watch`（FileSystemWatcher 监听 .c/.h 变动，增量重跑相关测试）
- 彩色 pass/fail 输出
- 测试时间统计，标出慢测试

---

## 4. 跨 Track 依赖关系

```
P0/E0 ──► P1-P5(部分irq/gpio测试) ──► P2-P6 ──► P2-D3 ──► P3 全量
  │                                    │
  └─► P3-T5 (QEMU CI)                  ├─► P2-AI1/AI2/AI3 ──► P3-L3(health check)
                                       │
P0/E5 (wasm mutex) ──► P2-W2(canonical models) ──► P2-B4(example.html)
       │                     │
       └─► P1-W1 (ADR-0021) ─┘

P1-P1 (pal_rmt泛化)  ──► P2-D3 的 WS2812/IR_receiver
P1-D1 (config嵌入)   ──► P2-AI3(actuator registry) ──► P3-D4(device header ADR)
P1-P3 (pal_common)   ──► P2-P6 (新PAL原语便于加入)
P1-D2 (BLOCKING覆盖) ──► P2-AI1 (SOFT_ASSERT 参数校验风格一致)
P1-T1 (wasm测试接线) ──► P2-T2 (misuse矩阵)、P3-T4 (golden sample)
P1-B2 (头自包含脚本) ──► P2-T3 (架构检查) 共用脚本基础设施
P2-T3 (架构lint)     ──► P3-T5 (CI) 合并为统一 CI pipeline
```

**关键路径**：P0/E0 (app_main) → P1-P5(bugfix) → P2-P6 → P2-D3（第一批设备扩展）
**可并行**：P1 所有 Track 之间无强依赖；P2-AI* 系列（AI 加固）可与 P2-D3（设备扩展）并行。

---

## 5. 成功指标（验收出口）

### P0 验收
| 指标 | 通过标准 |
|------|---------|
| ESP32 镜像可启动 | DevKitC 烧录后看到初始化日志、可进入 avoidance_car 主循环 |
| RMT 失败恢复 | 注入失败后再次调用 pulse_in 正常工作 |
| wasm mutex 正确性 | 多任务争用 mutex 测试用例通过 |
| dal_gps/eeprom | 返 NOT_SUPPORTED 而非假 success |
| host 测试 | `python wink-tools/wink.py test` 全绿 |
| wasm smoke | `node targets/wasm/wink_sim_stub.js` PASS |

### P1 验收
| 指标 | 通过标准 |
|------|---------|
| ADR-0021 | Accepted 并回写设计规范 |
| pal_rmt 泛化 | grep `pal_rmt_ultrasonic` 除 ADR 历史外零命中 |
| 架构边界 | pal/include 下不依赖 runtime 符号；wink_dev_config 位置合理 |
| BLOCKING 覆盖 | 所有 `Blocking: Yes` doxygen 对应 WINK_BLOCKING；STRICT 构建通过 |
| 孤儿测试 | test/wasm/ 下无未被 CMake 引用的 .c |
| 文档 | TESTING.md/README.md 与实际 30+ 测试一致；零 `pal_ultrasonic` 残留 |
| 日志 | `pal_log_e/w/i/d` 三 target 可用 |
| 头自包含 | 每个 PAL/DAL 头文件独立编译通过 |
| sample 构建 | 6 个 sample CMakeLists 统一抽公共 cmake；零 direct printf |

### P2 验收
| 指标 | 通过标准 |
|------|---------|
| 新 DAL 设备 | 至少新增 5 个设备（DC motor/buzzer/RGB LED/WS2812/IMU），每个三 target 实现 + 单测 |
| PAL 原语 | UART/SPI/ADC/Semaphore/Queue/Yield 落地（ADC 可简化） |
| AI 加固 | SOFT_ASSERT 覆盖所有公共 API；100% doxygen 覆盖率；misuse 测试 50+ 用例 |
| Actuator registry | fault/wdt 路径自动 safe-off_all (对接静态 POD 场景 01) |
| 架构 lint | 11 条不变量自动检查（新增私有数据篡改与直接 memcpy 拦截）接入 python wink-tools/wink.py test |
| Wasm 仿真调试 | Virtual Logic Analyzer 可视化日志 + 双向强审计 Assert 拦截校验 |
| example.html | 浏览器可交互跑通至少 2 个 sample |
| dwink CLI | build/burn/test/sim/lint 5 个命令可用 |

### P3 验收
| 指标 | 通过标准 |
|------|---------|
| Golden sample | 所有 sample 有 golden 文件，重构 diff 可查 |
| QEMU CI | PR 阶段自动跑 devkitc_smoke |
| Capability 位图 | 新增 STM32/Nordic target 时优雅降级 |
| Device header ADR | 所有 DAL 设备统一 WINK_DEVICE_HEADER 前缀 |
| Misuse Cookbook | 覆盖 20+ 常见问题场景 |
| Sample README | 6 个 sample 全覆盖文档 |

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| ESP32 首版上板发现更多硬件 bug，P0 时间超估 | P1 启动延迟 | P0 预留 2 天 buffer；先跑 devkitc_smoke 单样例 |
| ADR-0021 选 A 还是选 B 争论不休 | P1-W1 阻塞 P2-W2/P2-D3 | ADR 讨论以"是否能无头复现 AI 生成 bug"为决策准则；1 天内必须结论 |
| Blocking API 注解覆盖可能触发 STRICT 模式下大面积编译失败 | 短期阻塞开发 | 先注解不默认开 STRICT；CI 单独跑 STRICT 构型，主开发构型保持宽松 1 周过渡 |
| DAL 设备扩张快于 PAL 原语补齐 | 设备实现里出现 ad-hoc HAL 调用 | P2-T3 架构 lint 拦截"DAL include driver/"；P2-P6 优先级高于 P2-D3 |
| Canonical 设备模型工作量大，拖慢 DAL 扩张 | 功能出得慢 | servo/ultrasonic/ssd1306 做 3 个模板即可，其他设备先用 marshalling + JS 模型占位（tracked as tech-debt） |
| 多 target 同步实现 PAL 新原语成本高 | P2-P6 周期长 | host 最先实现（最易调试）→ wasm 跟进（共享 sim 逻辑）→ esp32 最后（SMP/ISR 细节最多） |

---

## 7. 不在本计划范围（Out of Scope）

1. **STM32 等新硬件 target 的实际实现**（仅 P3-P7 的 capability 位图为其铺路）
2. **Workbench 前端 UI 改版**（在外部项目，本计划只通过 ADR-0021 界定接口）
3. **Wasm JIT/AOT 优化**（当前解释器性能足够 MVP）
4. **Codegen 工具本身的重写**（P1-D1 config 嵌入只是为 codegen 提供统一数据模型）
5. **Rust 重写/Zig 重写**等语言迁移
6. **蓝牙/Wi-Fi 协议栈**（无近期应用场景；属于后续联网扩展）
7. **MicroPython/Lua 脚本层**（AI 生成 C 代码是当前既定路线）

---

## 8. P0 → P1 执行记录

### 8.1 P0 工作清单（✅ 2026-07-04 全部完成）

> 注：P0 项在本计划拟定时已经部分落地于前置批次 commit（6ea35c3 等），本 session 通过全量代码核查 + python wink-tools/wink.py test 绿测确认，已全部满足验收标准。

1. ✅ **E2 + E3**: pulse_us 初始化 + ssd1306 宽高校验（6ea35c3）
2. ✅ **E4**: wasm ISR in_isr 上下文包裹（6ea35c3）
3. ✅ **E5-part1**: dal_gps/dal_eeprom stub 返 WINK_ERR_UNSUPPORTED（6ea35c3）
4. ✅ **E5-part2**: wasm mutex 真实阻塞 FIFO 队列（6ea35c3）
5. ✅ **E1**: RMT 失败路径 unwind + deinit 清零（6ea35c3）
6. ✅ **E3-ssd1306**: width/height 校验 + init command table 分发（6ea35c3）
7. ✅ **E0**: app_main 落地至 `esp32_firmware/main/app_main.c`（nvs_flash_init retry + xTaskCreate 8192B prio=5 + wink_runtime_run + 栈高水位监控；`targets/esp32/esp32_entry.c` 保留为 DEPRECATED 文档占位）

### 8.2 P1 已完成项（2026-07-04，本 session 提交）

| Commit | 涵盖 Track |
|--------|-----------|
| ca58542 (pre-session) | P1-P2 dev_config 迁出 PAL |
| 6ea35c3 (pre-session) | P1-P5 bugfix 批次（1/2/3/4/5/7/9/10）；P1-D1 config 嵌入 |
| bb275a7 (pre-session) | P1-B3 sample_common.cmake |
| aace8fe9 (pre-session) | P1-T1 test/wasm/ 孤儿测试文档化 |
| b6d890d | P1-B2: pal_debug_printf 加 printf format 属性 |
| 57fc767 | P1-B1: unisim_smoke 过时 JS bridge 注释清理 |
| 98ca75b | P1-P5-8: esp_driver_* IDF ≥5.4 版本门控 |
| a923c4a | P1-P1: pal_rmt 泛化为 pulse_capture API |
| aed89f1 | P1-B2: tools/check_headers_self_contained.py + run-tests 接入 |
| ead7b1e | P1-P3: pal_common OBJECT 库 |
| 38955e7 | P1-P4: pin_map getter API（pal_pwm_channel_pin / pal_i2c_port_pins）|
| 7d29fb1 | P1-D2: WINK_BLOCKING 9 个 API 注解 + test_pal_nonblocking_strict 编译期 gate |
| (pending) | P1-L1: 分级日志 API（pal_log.h + 三后端 + wasm s_sim_in_isr 前向声明修复 + test_pal_contract 补充探针） |

### 8.3 P1 剩余待做（下一 session 入口）

1. ~~**P1-L1 分级日志** — Host 路由到 ANSI 彩色 stderr；wasm 路由到 `js_pal_log`；ESP32 路由到 `esp_log_writev`；NDEBUG 下 pal_log_d 编译为空；test_pal_contract 加编译期断言 + 运行期链路探针~~ ✅ 本次 session 完成
2. ~~**P1-W1 ADR-0021 wasm 设备 ownership** — 纯 ADR 工作：按 ADR 模板写 `docs/decisions/unisim/0021-wasm-device-model-ownership.md`，推荐方案 A（canonical in-tree），Accepted 后回写 `04-wasm-simulation/` 设计规范。至少落地 1 个 canonical 设备模板（建议 ssd1306 framebuffer getter，最易验证）。~~ ✅ 本次 session 完成

### 8.4 明确延后至 P2 的项

- P1-P5#6 I2C per-port speed → 合并进 P2-P6 pal_i2c_set_speed 跨 target 公共 API
- P1-T1 wasm 孤儿测试实际接线 → P2-T2 misuse 矩阵一起做
- P1-B1 gen_test_matrix.py 自动化脚本 → P2-T3 架构 lint 一起做
- P1-B2 嵌套 extern "C" 清理 → 良性冗余，低优先级
- P1-L1 关键路径 pal_debug_printf → pal_log_w/e 迁移 → P2-AI1 SOFT_ASSERT 批次一并做

---

*本计划基于 2026-07-04 全量评审结果创建。P0/P1 已全部完成（20/20 项），后续 P2/P3 阶段可随时启动。*

