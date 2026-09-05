# 实施计划：Boot safe-lock 连续复位计数与恢复策略

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260628-SAFELOCK-RECOVERY` |
| **创建日期** | 2026-06-28 |
| **目标平台/SoC** | `host` / `wasm` / `ESP32`（双 target 同源，ADR-0002） |
| **工具链/SDK版本** | GCC host + ESP-IDF v6.0.1 |
| **计划状态** | ✅ 已完成（代码/测试/spec 早已落地，2026-06-28 事后回填 checkbox） |
| **优先级** | 🟡 P1（重要，改善安全态可用性 + 修冒烟验收口径） |
| **关联 ADR** | [`ADR-0010`](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)（修订 ADR-0007 safe-lock） |
| **关联设计规范** | [`02-wink-micro-os/04-runtime-and-trace.md`](../../design/02-wink-micro-os/04-runtime-and-trace.md) §3.4 |
| **关联评审记录** | [`2026-06-27-devkitc-smoke-hardware-verification.md`](../../reviews/core/2026-06-27-devkitc-smoke-hardware-verification.md) S8 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标

### 2.1 问题陈述
ESP32 真机实测：长按 BOOT >3s 触发 WDT 复位测试（smoke S8）后，设备被 ADR-0007 boot safe-lock 永久锁死（LED 常亮、不闪烁、按键不计数）。根因是 safe-lock「一次异常复位即锁死」无法区分「真死循环」与「单次/测试复位」。详见 [ADR-0010 背景](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md#背景context)。

### 2.2 目标
- ✅ 单次/偶发异常复位后设备**自动恢复**正常运行（init/loop 照跑）。
- ✅ 真死循环（每次启动都崩、跑不到稳定态）**连续 3 次**才锁死，防护不削弱。
- ✅ 修复 8001 三连 trace（锁死路径 trace 一次）。
- ✅ 双 target 同源：PAL 新接口 host/wasm/baremetal/esp32 四态自适应。
- ✅ smoke S8 验收口径与实现一致。

### 2.3 成功指标
| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| host 单测 | 100% 通过（含恢复/里程碑/阈值锁死/去重 trace） | `python wink-tools/wink.py test` |
| ESP32 构建 | 0 error, 0 warning | `idf.py -C esp32_firmware build` |
| 真机：单次 WDT | 复位后自动恢复闪烁 | 长按 BOOT>3s → 重启 → LED 恢复闪烁、按键计数恢复 |
| 真机：连续崩溃 | 第 3 次锁死、trace 8001 一次 | 人为 init 崩溃 3 次 |

---

## 3. 变更范围

### 3.1 文件变更清单
| 文件路径 | 变更类型 | 说明 |
|----------|----------|------|
| `wink-micro-os/pal/include/pal_osal.h` | ✏️ 修改 | +`pal_get/set_abnormal_boot_count` 声明 |
| `wink-micro-os/targets/esp32/pal_osal_esp32.c` | ✏️ 修改 | RTC_NOIT 计数 + magic；+`#include "esp_attr.h"` |
| `wink-micro-os/targets/host/pal_osal_host.c` | ✏️ 修改 | 可注入静态 + impl + `sim_reset_time` 清零 |
| `wink-micro-os/targets/wasm/pal_osal_wasm.c` | ✏️ 修改 | stub（get 返回 0 / set no-op） |
| `wink-micro-os/targets/baremetal/pal_osal_bare.c` | ✏️ 修改 | stub |
| `wink-micro-os/runtime/include/wink_runtime.h` | ✏️ 修改 | +`WINK_BOOT_LOCK_THRESHOLD`/`WINK_BOOT_HEALTHY_TICKS` 常量 |
| `wink-micro-os/runtime/src/wink_runtime.c` | ✏️ 修改 | safe-lock 状态机重写 + 里程碑清零 + 8001 去重 |
| `wink-micro-os/test/test_runtime.c` | ✏️ 修改 | 改写锁死测试（注入 count）+ 新增恢复/里程碑测试 |
| `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | ✏️ 修改 | `app_on_fault` 去重 trace |

### 3.2 接口影响
| 接口层 | 破坏性 | 备注 |
|--------|--------|------|
| PAL 公开 API | ⚠️ 新增（非破坏） | 新增 2 个 boot-count 接口；旧 API 不变 |
| 应用层 | ❌ 否 | safe-lock 行为变化对 App 透明（除非依赖「一次即锁」） |

### 3.3 架构红线
1. 严禁削弱对真死循环的防护：连续异常复位达阈值**必须**锁死。
2. 双 target 同源（ADR-0002）：四份 PAL 实现必须同时编译通过。
3. 计数持久化仅在 ESP32；host/wasm/baremetal 不引入持久化假象。

### 3.4 资源约束
| 维度 | 变化 | 缓解 |
|------|------|------|
| RAM (ESP32 RTC) | +8 字节（count + magic，RTC_NOIT） | 可忽略 |
| ROM/Flash | <100 字节代码 | 可忽略 |
| 栈深度 | 不变（safe-lock 路径无新增深调用） | — |

---

## 6. 详细任务拆分

### Task 1：PAL boot-count 接口（header + 4 target 实现） `[ ✅ 已完成 ]`
**修改文件**：`pal_osal.h`、`targets/{esp32,host,wasm,baremetal}/pal_osal_*.c`

- [x] **pal_osal.h**（line 95 `pal_get_reset_reason` 声明后）新增：
  ```c
  /**
   * @brief 读取连续异常复位计数（boot safe-lock 恢复策略，ADR-0010）。
   * @note esp32: 读 RTC_NOIT(+magic)，跨复位保留、断电/无效返回 0；
   *       host: 可注入静态（供单测）；wasm/baremetal: 恒 0。
   */
  uint32_t pal_get_abnormal_boot_count(void);
  /** @brief 写连续异常复位计数。esp32 写 RTC；host 写静态；wasm/baremetal no-op。 */
  void pal_set_abnormal_boot_count(uint32_t count);
  ```
- [x] **esp32**：文件头加 `#include "esp_attr.h"`；reset reason 段落新增：
  ```c
  #define WINK_BOOT_COUNT_MAGIC 0xB007C0DEu
  static RTC_NOINIT_ATTR uint32_t s_abnormal_count;
  static RTC_NOINIT_ATTR uint32_t s_abnormal_count_magic;

  uint32_t pal_get_abnormal_boot_count(void) {
      return (s_abnormal_count_magic == WINK_BOOT_COUNT_MAGIC) ? s_abnormal_count : 0u;
  }
  void pal_set_abnormal_boot_count(uint32_t count) {
      s_abnormal_count = count;
      s_abnormal_count_magic = WINK_BOOT_COUNT_MAGIC;
  }
  ```
- [x] **host**：加 `static uint32_t s_abnormal_count = 0;`；`sim_reset_time()` 内加 `s_abnormal_count = 0;`；实现 get/set 读写该静态。
- [x] **wasm/baremetal**：`pal_get_abnormal_boot_count` 返回 0；`pal_set_abnormal_boot_count` 空 body `(void)count;`。

**验证**：host 构建编译通过（接口齐全）；esp32 构建零警告。

---

### Task 2：runtime safe-lock 状态机重写 + 里程碑清零 + 去重 `[ ✅ 已完成 ]`
**修改文件**：`wink_runtime.h`、`wink_runtime.c`

- [x] **wink_runtime.h**（line 36 后）新增常量：
  ```c
  /** @brief 连续异常复位锁死阈值（ADR-0010）：达此值才锁死，单次/偶发自动恢复 */
  #define WINK_BOOT_LOCK_THRESHOLD   3u
  /** @brief 健康里程碑 tick 数（≈2s @10ms tick）：init 成功 + 跑满则清零计数 */
  #define WINK_BOOT_HEALTHY_TICKS    200u
  ```
- [x] **wink_runtime.c:94-101** 替换为计数状态机（见 [ADR-0010 决策](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md#决策结论decision)）：POWERON 清零 / WDT·PANIC 计数+1 / ≥阈值锁死（仅 `wink_runtime_fault` 一次调用，trace 一次）/ <阈值放行。
- [x] tick 循环内（`tick++` 前）加：
  ```c
  if (tick == WINK_BOOT_HEALTHY_TICKS) {
      pal_set_abnormal_boot_count(0);
  }
  ```

**验证**：host 单测（Task 3）。

> ⚠️ 注意：锁死路径原先 safe-lock 先 `trace+safe_off` 再调 `wink_runtime_fault`（再 trace+safe_off+on_fault），致 8001 三连 trace。改为仅调 `wink_runtime_fault` 一次 → runtime 侧 trace 一次、safe_off 一次。host 单测 `s_safe_off_calls` 由 2→1。

---

### Task 3：host 单测改写与新增 + 构建验证 `[ ✅ 已完成 ]`
**修改文件**：`test/test_runtime.c`

- [x] **改写** `test_boot_safe_lock_on_watchdog_reset`：注入 `pal_set_abnormal_boot_count(WINK_BOOT_LOCK_THRESHOLD - 1)` (=2) + `sim_set_reset_reason(WATCHDOG)` → 第 3 次锁死；断言 `WINK_ERR_LOCKED`、`s_init_calls==0`、`s_safe_off_calls==1`、`wink_trace_count()==1`（去重）、`wink_trace_last()==8001`。
- [x] **新增** `test_boot_single_watchdog_recovers`：count=0 + WATCHDOG → 放行；断言 `WINK_OK`、`s_init_calls==1`、`s_safe_off_calls==0`、`pal_get_abnormal_boot_count()>=1`。
- [x] **新增** `test_boot_count_clears_after_healthy_milestone`：count=1 + WATCHDOG，`wink_runtime_run(&cb, WINK_BOOT_HEALTHY_TICKS + 5)` → 断言返回 OK 且 `pal_get_abnormal_boot_count()==0`。
- [x] **保留** `test_boot_no_safe_lock_on_power_on_reset`（POWERON 清零放行，仍通过）。

**验证命令**：`python wink-tools/wink.py test`；预期全部通过（host `pal_delay_ms` 虚拟时间，200 tick 无实际耗时）。

---

### Task 4：样例 on_fault 去重 trace `[ ✅ 已完成 ]`
**修改文件**：`samples/devkitc_smoke/app_callbacks.c:306-312`

- [x] `app_on_fault` 移除 `wink_trace_fault(fault_code);`（runtime 已 trace，通知回调不重复）。

**验证**：真机 Faults 计数由 3→1（runtime 一次）。

---

### Task 5（Acceptance 后）：回写 spec + S8 + ADR 标 Accepted `[ ✅ 已完成 ]`
- [x] `04-runtime-and-trace.md §3.4` 更新 safe-lock 语义为「连续 N 次 + 健康里程碑」。
- [x] smoke S8 验收口径更新（评审记录）。
- [x] ADR-0010 标 Accepted + 变更记录补「真机验证通过」。

---

## 7. 测试策略

### L0 编译门禁
- [x] host：`python wink-tools/wink.py test` 全绿
- [x] esp32：`idf.py -C esp32_firmware build` 零错误零警告

### L1 单测
- [x] 恢复路径（count=0→1 放行）
- [x] 里程碑清零时序（跑满 200 tick 清零）
- [x] 阈值锁死（count=2→3 锁）
- [x] 8001 单次 trace（去重）
- [x] POWERON 清零放行

### L2 真机集成（用户侧）
| 场景 | 验收标准 |
|------|----------|
| 长按 BOOT>3s 1 次 | 复位后 LED 恢复闪烁、按键计数恢复、栈 telemetry 正常 |
| 人为 init 崩溃 3 次 | 第 3 次锁死、串口见 8001 一次 |

---

## 8. 回滚方案

### 方案 1：Git 还原
```bash
git revert <本次 commit>
```
恢复「一次异常复位即锁死」旧语义。

### 方案 2：阈值调 1
将 `WINK_BOOT_LOCK_THRESHOLD` 改回 `1u` 重编译 → 等价旧行为（最快降级，不需 revert）。

---

## 9. 参考资料
- [ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)
- [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)（被修订）
- [04-runtime-and-trace.md §3.4](../../design/02-wink-micro-os/04-runtime-and-trace.md)

---

## 10. 事后回填说明（2026-06-28）

本计划代码、单测、spec 回写实际在计划创建同期已全部落地（commits：`e05909c` Task1 PAL boot-count、`6b00ca7` Task2/3 runtime 状态机+单测、`084e585` Task4 on_fault 去重、`19b2f94` Task5 spec 回写 + ADR-0010 Accepted）。本次仅事后回填 Task/测试 checkbox 与计划状态为 ✅，**未改动任何代码**。实际完成情况以代码 + [ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md) + 评审记录为准（与 `cooperative-loop` 计划同样的滞后回填情况）。

Task 5「smoke S8 验收口径更新（评审记录）」说明：评审记录属 Layer ④ 只读快照不可改，新验收口径（恢复检测改查 `pal_get_abnormal_boot_count`）已记录于 ADR-0010 §遵循与后续 follow-up。

