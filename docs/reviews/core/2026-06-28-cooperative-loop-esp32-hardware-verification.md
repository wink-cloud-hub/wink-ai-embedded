# ADR-0007 协作式执行模型 ESP32 真机闭环验证报告

| 项 | 内容 |
|---|---|
| **验证日期** | 2026-06-28 |
| **硬件平台** | ESP32 DevKitC（双核，WROOM-32） |
| **ESP-IDF 版本** | v6.0.1（EIM 安装） |
| **固件来源** | `esp32_firmware/` + `wink-micro-os/`（runtime 完整编译进 ESP32 IDF 组件） |
| **关联 ADR** | [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)（协作式执行模型，本次 Accepted） |
| **关联实施计划** | [`implementation-plans/2026-06-27-cooperative-loop-execution-model-plan.md`](../../implementation-plans/core/2026-06-27-cooperative-loop-execution-model-plan.md) |
| **前置冒烟报告** | [`2026-06-27-devkitc-smoke-hardware-verification.md`](2026-06-27-devkitc-smoke-hardware-verification.md)（PAL 层 S1–S8） |
| **回写设计规范** | [`02-wink-micro-os/04-runtime-and-trace.md`](../../design/02-wink-micro-os/04-runtime-and-trace.md) §3.5 / §4 |
| **验证人** | 用户实机验证 |

---

## 1. 验证范围

本报告记录 [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)「底座软定时器调度器 + 双核非对称物理隔离 + 跨核逃生舱通信」三要素在 ESP32 真机的闭环验证。区别于 [2026-06-27 PAL 冒烟](2026-06-27-devkitc-smoke-hardware-verification.md)（验 PAL/DAL 硬件抽象），本次验证的是**runtime 执行模型本身**——首次将完整 runtime（含软定时器、跨核 ringbuf、钉核 task）编译进 ESP32 IDF 组件并在真机运行。

**ADR-0007 闭环验证三要素**：
- **V1** 软定时器调度器（`wink_soft_timer`）经 `wink_runtime_run` tick 循环调度。
- **V2** 多核物理隔离：runtime 主循环钉 Core 1（`pal_task_create` 钉核）。
- **V3** 跨核逃生舱：`pal_ringbuf_*` 非阻塞环形缓冲跨核通信。

### Out-of-Scope（follow-up）
- `WINK_PT_*` 无栈协程宏（Protothreads 语法封装）——footgun 检查器已就位，宏本身未实现（ADR-0007 唯一剩余 backlog）。
- 高速硬实时闭环（FOC / Fast PID）——协作式 + 软定时器适用边界外，需硬实时化。
- 跨核 ringbuf 高负载压测的定量吞吐/延迟——需专用测试固件，留待后续。

---

## 2. 逐项验证结果

### V1: 软定时器调度器（tick 循环调度）

| 检查项 | 结果 | 备注 |
|---|---|---|
| `wink_soft_timer_init()` 在 `wink_runtime_run()` 前成功初始化 | ✅ PASS | 静态槽分配，零动态分配 |
| ONESHOT 定时器到期触发一次后自动停止 | ✅ PASS | 行为符合 `wink_soft_timer.h` 契约 |
| PERIODIC 定时器周期性触发、Tick 对齐 | ✅ PASS | `period_ms` 为 `WINK_RUNTIME_TICK_MS` 整数倍 |
| `wink_soft_timer_dispatch()` 由 runtime 每 Tick 末尾调用 | ✅ PASS | `wink_runtime.c` tick 循环集成 |
| per-callback WCET 监控不触发 8002 误警 | ✅ PASS | 多速率任务独立计时 |

**结论**：ADR-0007 软定时器调度器在 ESP32 真机协作式 tick 循环中正确调度，多速率任务经统一分发。

---

### V2: 多核物理隔离（runtime 钉 Core 1）

| 检查项 | 结果 | 备注 |
|---|---|---|
| `pal_task_create(..., PAL_CORE_1, ...)` 将 runtime 主循环钉到 Core 1 | ✅ PASS | 控制环物理隔离 |
| `PAL_CORE_0` / `PAL_CORE_ANY`（`tskNO_AFFINITY`）映射正确 | ✅ PASS | `pal_osal_esp32.c` switch 分支 |
| Core 0 系统负载（协议栈/后台）不抖动 Core 1 控制环 | ✅ PASS | 非对称隔离拓扑成立 |

**结论**：ADR-0007 双核非对称物理隔离在 ESP32 真机生效，runtime 控制环独占 Core 1。

---

### V3: 跨核逃生舱通信（`pal_ringbuf`）

| 检查项 | 结果 | 备注 |
|---|---|---|
| `pal_ringbuf_create(size)` 创建 bytebuf（size 为 2 的幂） | ✅ PASS | 基于 FreeRTOS `xRingbufferCreate` |
| `pal_ringbuf_push` 非阻塞、满返回 `WINK_ERR_FULL` | ✅ PASS | tick=0，task 上下文 |
| `pal_ringbuf_pop` 非阻塞、空返回 `WINK_ERR_EMPTY`、大小不匹配返回 `WINK_ERR_INVALID_STATE` | ✅ PASS | `vRingbufferReturnItem` 归还语义正确 |
| Core 0↔Core 1 经 ringbuf 无锁轮询通信、无 panic/复位 | ✅ PASS | 符合 ADR-0007「禁止跨核阻塞同步」约束 |

**结论**：ADR-0007 跨核逃生舱在 ESP32 真机可用，`pal_ringbuf` 提供非阻塞线程安全跨核数据交换。

---

## 3. 构建链路解锁（本次验证前置）

ADR-0007 runtime 首次进 ESP32 IDF 构建时暴露并修复了 4 类「host/wasm 验证过、ESP32 未验证」的同源问题（ADR-0002 双 target 同源编译的典型漏网）：

| 问题 | 根因 | 修复 |
|---|---|---|
| `freertos/ringbuf.h` 找不到 | `esp_ringbuf` 组件未在 REQUIRES 声明 | `targets/esp32/CMakeLists.txt` 加 `esp_ringbuf` |
| `memcpy`/`malloc` 隐式声明 | 缺 `<string.h>`/`<stdlib.h>`，靠传递包含侥幸过 host | `pal_osal_esp32.c` 显式 include |
| `wink_config.h` 找不到 | ESP32 IDF 组件从无 codegen 集成 | `CMakeLists.txt` 加两阶段 codegen（register 后 `add_custom_command` + `file(MAKE_DIRECTORY)`） |
| `wink_soft_timer_*` 链接未定义 | `core_sources.cmake` 漏 `wink_soft_timer.c` | 补入 `WINK_RUNTIME_SOURCES` |

> 教训：单一 target 验证不等于双 target 同源通过。ADR-0007 的新 runtime 代码此前只过 host/wasm，ESP32 真机构建逐一暴露依赖/包含/codegen/源清单漂移。

---

## 4. 结论

**ADR-0007 协作式执行模型（软定时器调度器 + 双核物理隔离 + 跨核逃生舱）在 ESP32 DevKitC（ESP-IDF v6.0.1）真机闭环验证通过。**

- ADR-0007 状态：Proposed → **Accepted**（2026-06-28）。
- 决策已回写至 [04-runtime-and-trace.md](../../design/02-wink-micro-os/04-runtime-and-trace.md) §3.5（软定时器）/ §4（多核隔离 + 跨核 ringbuf）。
- `@verified` 已更新：`wink_soft_timer.c`、`pal_osal_esp32.c`（ringbuf + 钉核段落）。
- 唯一剩余 follow-up：`WINK_PT_*` 无栈协程宏。

---

## 5. 关联文档

- 决策：[ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)
- 设计规范：[02-wink-micro-os/04-runtime-and-trace.md](../../design/02-wink-micro-os/04-runtime-and-trace.md)
- 实施计划：[2026-06-27-cooperative-loop-execution-model-plan.md](../../implementation-plans/core/2026-06-27-cooperative-loop-execution-model-plan.md)
- 前置冒烟：[2026-06-27-devkitc-smoke-hardware-verification.md](2026-06-27-devkitc-smoke-hardware-verification.md)

