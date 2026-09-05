# Phase 4: 超声波非阻塞 / 硬件捕获架构

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `dal_ultrasonic.h/.c`、`pal_hal.h`、`targets/host/pal_hal_host.c` 确认。
>
> **执行序位置（见 00-README）：** `0/1 → 3 → 2 → 4 → 5 → 6`。**前置：Phase 2**（需 `dal_ultrasonic_init` + `initialized` 字段）。

**Goal:** 修复 P0-2：超声波真机 `dal_ultrasonic_read` 两段 busy-wait 最坏 ≈ 2×30ms + trigger，破坏 10ms runtime tick。建立硬件捕获 + 非阻塞 DAL 语义，短期以 PAL pulse capture 过渡。

**Architecture（含关键纠正）：**
- PAL 增底层脉宽捕获 `pal_gpio_pulse_in`（最终 ESP32 RMT / GPIO 双沿 ISR + timer）；DAL 不再在 `dal_ultrasonic_read` 内 busy-wait。
- DAL 新增 `dal_ultrasonic_request_measurement`（触发，立即返回）+ `dal_ultrasonic_get_cached_distance`（非阻塞读缓存/状态）。
- **host 协作推进孤儿问题（原计划遗漏，最大落地坑）**：`pal_hal_host.c:10-11` 作者自述"协作推进**强耦合** ultrasonic 真机分支的 while 轮询结构；若驱动改非阻塞，本实现须同步重构"。本阶段正是该触发点。新增 `pal_gpio_pulse_in` 直接读 `host_echo_rise_us/high_us`（L21-22 已确认存在），与 `pal_gpio_read`(L36-54) 的协作推进**重复掌握同一 echo 时序**。BAL 迁移到非阻塞后，协作推进无消费者。Task 4-3/4-6 必须同步重构 host。

**Tech Stack:** C99, PAL HAL, host 虚拟时间, Unity

## Global Constraints
- App 10ms tick **不得**调用 30ms/60ms blocking API
- 不用 10ms tick 采样 HC-SR04 微秒级 echo
- 仿真旁路只在最低物理信号层（ADR-0003 决策2）
- 无动态内存；状态机迁移用 host 虚拟时间（`host_sim_advance_to`），**禁止**真实墙钟阻塞

## Sequencing
- 前置：Phase 2（`dal_ultrasonic_init`）；间接依赖 Phase 3（`pal_gpio_pulse_in` 一开始即用 status 签名）
- 后续：Phase 6 长期目标——DAL 完全移除 `#ifdef SIMULATION`，旁路下沉到 PAL capture

---

### Task 4-1: 明确并冻结旧阻塞 API 风险

**Files:**
- Modify: `wink-micro-os/dal/include/dal_ultrasonic.h`
- Modify: `docs/design/02-wink-micro-os/01-dal-device-abstraction.md`

**Source-of-truth check:** 当前 `dal_ultrasonic.h:24` 契约写 "Blocking: Yes (MAX 30ms timeout)"——**与实际最坏值不符**（真机 `dal_ultrasonic.c:45-53` 两段超时各 30ms，最坏 ≈ 60ms + trigger）。review P0-2 已指出此文档漂移。

**Header contract update:**
```c
 * @deprecated Runtime/App 10ms tick 不得调用本 API；保留仅供过渡/单测，迁移完成后移除。
 * @note Blocking: Yes. Worst-case ≈ 2 * ULTRASONIC_TIMEOUT_US + trigger pulse (≈ 60ms+)。
 *       Not allowed in cooperative runtime loop.
```
**移除目标**：BAL 完全迁移到非阻塞 + Task 4-6 host 重构完成后，`dal_ultrasonic_read` 及 host 协作推进一并移除（登记为 follow-up，不在本阶段强删）。

---

### Task 4-2: 增加 PAL pulse capture 过渡 API

**Files:**
- Modify: `wink-micro-os/pal/include/pal_hal.h`
- Modify: `wink-micro-os/targets/host/pal_hal_host.c`
- Modify: `wink-micro-os/targets/wasm/pal_hal_wasm.c`
- Modify: `wink-micro-os/test/test_host_pal.c`

**Source-of-truth check:** host echo helper `host_echo_rise_us`/`host_echo_high_us` 已存在（`pal_hal_host.c:21-22` extern），可直接复用。

**Interfaces:**
```c
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level,
                                uint32_t timeout_us, uint32_t *pulse_us);
```
**Contract:** Blocking: target-defined，**禁**从 BAL/runtime tick 调用；ISR-safe: No；Thread-safe: target-defined；返回 `WINK_OK`/`WINK_ERR_INVALID_ARG`/`WINK_ERR_TIMEOUT`/`WINK_ERR_UNSUPPORTED`/`WINK_ERR_IO`。

**Host implementation:**
- 复用 `host_echo_rise_us`/`host_echo_high_us`；脉宽 = `host_echo_high_us()`
- 若 configured pulse 起始 > `timeout_us` → `WINK_ERR_TIMEOUT`
- 否则 `*pulse_us = host_echo_high_us()`，`WINK_OK`

**Wasm implementation:**
- 经 bridge 调 `js_sim_measure_echo_pulse_us(pin)`（仅当存在 pin 映射）；无映射 → `WINK_ERR_UNSUPPORTED`（直至 virtual registry routing 接入）
- ⚠️ **不挂起**：`js_sim_measure_echo_pulse_us` 是同步返回，**不得**列入 Phase 1 的 `ASYNCIFY_IMPORTS`（非挂起点）。Phase 1↔Phase 4 在此无冲突，但须显式声明以免误加。
- ⚠️ **echo 读取 vs 中断注入（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：本 Task 的 echo 脉宽**读取**是同步旁路（`js_sim_measure_echo_pulse_us` 直接返回，不挂起）。但若未来 echo 上升沿改用**中断注入**语义（JS 回调 `_trigger_wasm_interrupt`），则受 Phase 1 Task 1-5 的 Asyncify sleeping 窗口重入约束。**Phase 4 须确认采用 pulse-in 同步读取（本 Task 路径）而非中断注入**，否则 echo 中断路径依赖 Task 1-5 落地——即 00-README 横切红线所指的硬前置。

---

### Task 4-3: DAL 非阻塞缓存 API + host 状态机时序（虚拟时间驱动）

**Files:**
- Modify: `wink-micro-os/dal/include/dal_ultrasonic.h`
- Modify: `wink-micro-os/dal/src/dal_ultrasonic.c`
- Modify: `wink-micro-os/test/test_dal_ultrasonic.c`

**Source-of-truth check:** 当前 `dal_ultrasonic_t`（L11-15）无状态机字段，Phase 2 已加 `initialized`；本 Task 再扩展测量状态。

**Interfaces:**
```c
typedef enum { DAL_ULTRASONIC_IDLE=0, DAL_ULTRASONIC_MEASURING=1,
               DAL_ULTRASONIC_READY=2, DAL_ULTRASONIC_ERROR=3 } dal_ultrasonic_state_t;

WINK_WARN_UNUSED_RESULT wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);
WINK_WARN_UNUSED_RESULT wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *distance_cm);
```
**Struct extension:** `dal_ultrasonic_state_t state; wink_status_t last_status; uint32_t last_pulse_us;`

**Implementation（含原计划遗漏的 host 时序细节）：**
- `request_measurement`：触发 10us pulse，立即返回，置 `state=MEASURING`
- `get_cached_distance`：
  - `READY` → `WINK_OK` + 缓存距离
  - `MEASURING` → `WINK_ERR_BUSY`
  - `ERROR` → `last_status`
  - `!initialized` → `WINK_ERR_NOT_INITIALIZED`
- **host 测量完成迁移（关键，原计划含糊）**：host 上 `MEASURING → READY` 的迁移由 `request_measurement` 内部经 `pal_gpio_pulse_in` 一次性完成（host 无真实异步硬件，pulse_in 同步返回虚拟时间下的脉宽）。**即 host 的"非阻塞"对调用者表现为单 tick 内 ready**——这是可接受的仿真保真（host 测的是状态机契约正确性，不是真实 wall-clock 异步）。须在头文件契约注明 host 此行为，避免误以为 host 也在测真实异步。

---

### Task 4-4: avoidance_car App 改非阻塞状态机

**Files:**
- Modify: `wink-micro-os/samples/avoidance_car/app_main.c`
- Modify: `wink-micro-os/test/test_app_e2e.c`

**Pattern:**
```c
if (radar_state == NEED_TRIGGER) {
    status = dal_ultrasonic_request_measurement(&front_radar);
}
status = dal_ultrasonic_get_cached_distance(&front_radar, &distance_cm);
if (status == WINK_OK)            { /* normal avoidance */ }
else if (status == WINK_ERR_BUSY) { /* keep previous safe output */ }
else                              { /* safe stop + trace fault */ }
```
**Produces:** sample app `loop` 不再调 `dal_ultrasonic_read`。

---

### Task 4-5: Long-term ESP32 hardware capture design note

**Files:**
- Modify: `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`
- Modify: `docs/design/07-platform-governance/01-device-model-registry.md`

**Document:** ESP32 须用 RMT 或 GPIO 双沿 ISR + 硬件 timer 捕获；runtime tick 内无 polling 循环；`pal_gpio_pulse_in` 为过渡，最终目标 async capture/callback 或 driver-internal worker。

**架构红线（中断与任务解耦的“下半部”机制 / Deferred ISR）：**
在设计真实的 ESP32 硬件捕获 ISR 时，明确规定：**ISR 内部绝对禁止任何阻塞操作**。ISR 仅负责读取 timer/RMT 状态、清中断标志，并将捕获到的脉宽数据投递至无锁事件队列或置 Flag。由 `app_loop` 在正常的任务上下文中取出并处理（Bottom-Half），彻底杜绝中断嵌套与 RTOS 系统卡顿。

**wasm 侧对称约束（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md) / Phase 1 Task 1-5）**：wasm 下 JS 模拟的中断同样不得在 Asyncify sleeping 窗口直调 `_trigger_wasm_interrupt`，须经 JS 侧排队、在 tick 边界 flush。即 wasm 与 ESP32 共享同一「Deferred 中断」语义——仅投递时机不同（ESP32 = Bottom-Half 任务上下文，wasm = tick 边界）。这保证两 target 的中断处理模型同源，符合 ADR-0002 双 target 同源要求。

---

### Task 4-6: host 协作推进重构（消除孤儿，原计划遗漏）⚠️

**Files:**
- Modify: `wink-micro-os/targets/host/pal_hal_host.c`
- Modify: `wink-micro-os/test/test_dal_ultrasonic.c`（若旧测试依赖协作推进）

**Source-of-truth check:** `pal_hal_host.c:36-54` 的 `pal_gpio_read` 协作推进（向 echo 边沿推进虚拟时间）专为驱动旧 blocking `dal_ultrasonic_read` 的 while 轮询设计；L10-11 注释已预警本阶段需重构。

**Rationale:** Task 4-4 后 App 不再调 blocking read。此时 `pal_gpio_read` 的协作推进失去唯一消费者（除非单测仍用 blocking read）。若保留两者，则 host 有**两套**读 echo 时序的机制（协作 `pal_gpio_read` + `pal_gpio_pulse_in`），违反 SSOT，且 `pal_gpio_read` 推进逻辑会继续静默篡改进拟时间造成测试脆弱。

**Action（二选一，须明确决定并记录）：**
- **方案 A（推荐）**：`pal_gpio_read` 回归"纯读当前虚拟时间电平、不推进时间"的朴素语义；echo 时序唯一来源收敛到 `pal_gpio_pulse_in`。单测改用非阻塞 API。
- **方案 B**：保留 blocking `dal_ultrasonic_read` + 协作推进，仅作单测内部路径，公共契约仍标 deprecated。
- 无论选 A/B，须在 `pal_hal_host.c` 头注释更新（删除/修订 L10-11 的"强耦合"预警），并在文档记录决定。

**Verification Gate:**
```powershell
cd wink-micro-os
python wink-tools/wink.py test --clean
```
→ 全绿。**墙钟守卫测试**（强烈建议新增）：单 tick 超声波路径在 host 上的实际墙钟耗时 < 10ms（证明无真实阻塞泄漏到 tick）。

## 出口验收
- [ ] `python wink-tools/wink.py test --clean` 全绿
- [ ] App `loop` 不含 `dal_ultrasonic_read`（grep 验证）
- [ ] Task 4-6 host 重构方案已决定并落地，`pal_hal_host.c` 注释更新
- [ ] 墙钟守卫测试 < 10ms（若新增）
- [ ] 整改跟踪表 P0-2 标"host 非阻塞完成；真机 RMT 捕获随 P2-6"

