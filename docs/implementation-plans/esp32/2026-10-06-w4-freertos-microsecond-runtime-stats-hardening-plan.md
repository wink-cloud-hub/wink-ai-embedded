<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：W-4 (S-4) FreeRTOS 真实微秒运行计费与底座时钟模型彻底重构

> **前置决策与规范依据**：
> - [ADR-0001 负数错误码](../../../docs/design/decisions/0001-error-code-sign-convention.md)
> - [ADR-0004 编译期静态分发优于运行时函数指针](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)
> - [ADR-0012 契约诚实优于静默降级](../../../docs/design/decisions/0012-contract-honesty-over-silent-degradation.md)
> - [ADR-0042 虚拟时钟单一 Gate 规则](../../../docs/design/decisions/0042-sim-execution-modes.md)
> - [ADR-0053 虚拟时间因果同刻总序仲裁模型](../../../docs/design/decisions/0053-virtual-time-irq-total-order.md)
> - [2026-10-05-esp-idf-completed-items-review.md (S-4 评审定性)](../../reviews/esp32/2026-10-05-esp-idf-completed-items-review.md)

---

## 1. 背景与现存“假修复”缺陷诊断

### 1.1 缺陷实证（S-4 现状）
在 2026-10-05 历史提交 `74b62b5d` 中，针对 S-4 计费倒错仅做了表面替换：
```diff
--- a/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c
+++ b/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c
@@ -236,7 +236,9 @@ void vTaskDelay(const TickType_t xTicksToDelay) {
-        s_tcb[self].runtime_counter += (uint32_t)xTicksToDelay * portTICK_PERIOD_MS;
+        /* S-4 fix: Never credit delay sleep duration to task runtime counter!
+         * Only account for active dispatch execution before entering blocked state. */
+        s_tcb[self].runtime_counter += 1;
```
并且在 `uxTaskGetSystemState` 中维持了毫秒除法：
```c
*pulTotalRunTime = (uint32_t)(pal_os_get_us() / 1000ULL);
```

### 1.2 假修复产生的四大致命矛盾
1. **工作量脱钩**：`runtime_counter` 实际变成了“调用延时函数的次数”。计算循环 `SPIN_ITER` 无论设为 500,000 次、0 次还是 10,000,000 次，`spin0` 依然恒定输出 `11`；
2. **量纲分裂**：分子 `ulRunTimeCounter` 单位是“调用次数”，分母 `*pulTotalRunTime` 单位是“系统毫秒”，量纲完全不匹配；
3. **变异逃逸**：将工作任务挂起或清空计算循环，无法被任何变异断言击杀；
4. **计划标记漂移**：主计划第 362 行过早标记 `[x] W-4`，与第 188 行和第 354 行的“尚未满足验收标准”存在真实性矛盾。

---

## 2. 彻底重构技术方案（Zero-Fake-Fix Architecture）

```
                     【FreeRTOS 真实微秒计费拓扑】

  ┌──────────────────────────────────────────────────────────────┐
  │ 主调度器循环 (pal_osal_wasm.c / pal_osal_host.c)              │
  │                                                              │
  │  1. 选中任务 next: sim_scheduler_set_current(next)           │
  │  2. wall_start_us = wall_clock_us()                          │
  │  3. sim_ctx_switch(s_main_ctx, t->ctx)  ───► 纤程执行代码     │
  │  4. 纤程挂起/让出切回主循环                                  │
  │  5. delta_us = wall_clock_us() - wall_start_us (min 1µs)     │
  │  6. sim_scheduler_accumulate_runtime(next, delta_us)         │
  └──────────────────────────────┬───────────────────────────────┘
                                 │
                                 ▼
  ┌──────────────────────────────────────────────────────────────┐
  │ 调度器任务账本 (wink_sim_scheduler.c)                        │
  │  s_tasks[next].runtime_us += delta_us                        │
  └──────────────────────────────┬───────────────────────────────┘
                                 │
                                 ▼
  ┌──────────────────────────────────────────────────────────────┐
  │ FreeRTOS 门面层 (freertos_task.c)                            │
  │  uxTaskGetSystemState:                                       │
  │    - s->ulRunTimeCounter = sim_scheduler_get_runtime(slot)    │
  │    - *pulTotalRunTime = (uint32_t)pal_os_get_us()            │
  │      (同频微秒 µs 量纲，彻底废除 / 1000ULL 毫秒除法)         │
  └──────────────────────────────────────────────────────────────┘
```

### 2.1 核心设计准则
1. **纯微秒量纲统一**：`ulRunTimeCounter` 与 `pulTotalRunTime` 严格统一为微秒（$\mu s$），对齐 ESP-IDF 物理规范；
2. **彻底消灭延时累加**：`vTaskDelay`、`vTaskDelayUntil`、`vTaskSuspend` 等阻塞入口**绝对禁止**手动累加任何时间；
3. **真实纤程切入切出积分**：在 `pal_sim_scheduler_run` 的 `sim_ctx_switch` 前后测量真实的微秒消耗，自动计入该任务账本；
4. **对齐静态分发与内存契约**：`sim_task_t` 保持紧凑 POD 结构（严格满足 `sizeof(sim_task_t) <= 96` 静态断言）；
5. **抗假绿红绿成对变异验证**：在变异测试中将 `SPIN_ITER` 归零，断言必须能够敏感捕捉到执行时间急剧下降并引发 RED 失败。

---

## 3. 分阶段实施工作包 (Work Packages)

### WP 1: 计划与看板状态校准 (Plan Reconciliation)
- [ ] **Task 1.1**: 更新 `docs/implementation-plans/esp32/2026-10-05-comprehensive-adversarial-red-green-testing-plan.md`，将第 362 行标记纠正为待重构状态，消除进度漂移。

### WP 2: 核心调度器任务微秒账本 (Core Scheduler Ledger)
- [ ] **Task 2.1**: 修改 `wink-micro-os/targets/common/include/wink_sim_scheduler.h`：
  - 在 `sim_task_t` 结构体中新增 `uint64_t runtime_us;` 字段；
  - 校验 `_Static_assert(sizeof(sim_task_t) <= 96, "...")` 依然成立；
  - 声明对外 API：
    - `void sim_scheduler_accumulate_runtime(uint32_t task_id, uint64_t delta_us);`
    - `uint64_t sim_scheduler_get_runtime_us(uint32_t task_id);`
    - `void sim_scheduler_reset_runtime(void);`
- [ ] **Task 2.2**: 修改 `wink-micro-os/targets/common/src/wink_sim_scheduler.c`：
  - 实现上述三个运行时记账函数；
  - 在 `sim_scheduler_reset()` 与 `sim_scheduler_register()` 中确保 `runtime_us = 0` 正确初始化。

### WP 3: 宿主与 Wasm 纤程切换计费桥接 (Fiber Switch Bridge)
- [ ] **Task 3.1**: 修改 `wink-micro-os/osal/wasm/pal_osal_wasm.c`：
  - 在 `pal_sim_scheduler_run()` 的 `sim_ctx_switch` 之后，将实测的 `duration_us`（保底 1$\mu s$）通过 `sim_scheduler_accumulate_runtime(next, duration_us)` 累加至当前任务。
- [ ] **Task 3.2**: 修改 `wink-micro-os/osal/host/pal_osal_host.c`：
  - 对齐 Wasm Target，在 Host 纤程切出后同步调用 `sim_scheduler_accumulate_runtime(next, duration_us)`。

### WP 4: FreeRTOS 门面层时钟与量纲彻底修复 (FreeRTOS Facade Fix)
- [ ] **Task 4.1**: 修改 `wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c`：
  - 彻底删除 `vTaskDelay` 内的所有伪记账代码（`s_tcb[self].runtime_counter += 1`）；
  - `xTaskCreatePinnedToCore` 初始 `runtime_counter = 0`；
  - `uxTaskGetSystemState` 读取真实的调度器微秒账本：
    `s->ulRunTimeCounter = (uint32_t)sim_scheduler_get_runtime_us(s_tcb[i].sim_id);`
  - `pulTotalRunTime` 统一为纯微秒时基：
    `*pulTotalRunTime = (uint32_t)pal_os_get_us();`（彻底废除 `/ 1000ULL` 毫秒除法）。

### WP 5: 单测、变异与端到端复验 (Verification & Anti-False-Green Proof)
- [ ] **Task 5.1**: 在 Host 单元测试中验证 `sim_scheduler_accumulate_runtime` 与 `uxTaskGetSystemState` 的微秒单调性与非阻塞排他性。
- [ ] **Task 5.2**: 执行 `run_esp32_headless_evidence.ps1 -App freertos_real_time_stats`，验证真实微秒输出（`task_elapsed_time` 为真实物理微秒，而非固定的 11）。
- [ ] **Task 5.3**: 变异反例验证：将 `SPIN_ITER` 设为 0，验证运行微秒数显著跌落并可被测试精确识别。
- [ ] **Task 5.4**: 运行全量静态门禁：`python .governance/gates/run_gates.py --gate 1`、`winkcli lint --pack layering --pack api` 及 `check_license_map.py`。
