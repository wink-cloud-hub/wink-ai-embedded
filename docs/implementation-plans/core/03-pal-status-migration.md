# Phase 3: PAL 失败型 API 迁移为 wink_status_t

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `pal_hal.h`、`pal_osal.h`、`targets/host/pal_hal_host.c`、`targets/wasm/pal_hal_wasm.c` 逐项确认。
>
> **执行序位置（见 00-README）：** `0/1 → 3 → 2 → 4 → 5 → 6`。**本阶段是 Phase 2 的硬前置**——Phase 2 资源冲突治理需要 status 返回才能表达 `WINK_ERR_BUSY`，故必须先完成本阶段。

**Goal:** 修复 P1-1：PAL 失败型 API 返回 `bool` 导致错误语义丢失。分两波迁移：先硬件 IO / 外设配置（HAL），再 OSAL mutex。

**Architecture:**
- HAL 失败型 API（init / enable_interrupt / disable_interrupt / pwm_init / pwm_set_duty / i2c_transfer）由 `bool` 改 `wink_status_t`。
- 读取型不可失败 API（`pal_gpio_write` void、`pal_gpio_read` bool）**保持现状**——它们无有意义的失败语义，强行 status 化只会污染调用点。
- 全部新 status API 加 `WINK_WARN_UNUSED_RESULT`。
- 一次性迁移完毕，**不留 bool/wink_status_t 混合态**（混合态是 AI CodeGen 判错不一致的最大来源）。

**Tech Stack:** C99, CMake, Unity, host/wasm targets

## Global Constraints
- `0 = WINK_OK`，负数 = 错误；判定统一用 `wink_status_is_error(status)` 或 `status < 0`，**禁 `if (status)`**
- 所有新 `wink_status_t` PAL API 加 `WINK_WARN_UNUSED_RESULT`
- 签名迁移是 ABI 破坏：波及全部 targets 与全部 DAL 调用点，**单阶段闭环**，不得跨阶段留半成品

## Sequencing
- 前置：Phase 0/1（无文件冲突，但建议 Phase 0 先合入以减少并发 diff）
- **后续硬约束**：Phase 2 资源占用治理依赖本阶段 status 签名——本阶段未完成则 Phase 2 无法落地
- Task 内部：3-1（头）→ 3-2（targets 实现）→ 3-3（DAL 调用点）→ 3-4（OSAL mutex）→ 3-5（测试/文档），严格串行（签名一旦改，下游全断）

---

### Task 3-1: `pal_hal.h` 状态化

**Files:**
- Modify: `wink-micro-os/pal/include/pal_hal.h`

**Source-of-truth check:** 已确认当前全部为 `bool` 返回：`pal_gpio_init`(L40)、`pal_gpio_enable_interrupt`(L55)、`pal_gpio_disable_interrupt`(L60)、`pal_pwm_init`(L72)、`pal_pwm_set_duty`(L79)、`pal_i2c_transfer`(L95)。保持不变：`pal_gpio_write`(L45, void)、`pal_gpio_read`(L50, bool)。

**Change signatures（加 `WINK_WARN_UNUSED_RESULT`）：**
```c
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t intr_type, pal_gpio_isr_t callback, void *arg);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_disable_interrupt(uint16_t pin);
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);
WINK_WARN_UNUSED_RESULT wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr, const uint8_t *write_buf, uint32_t write_len, uint8_t *read_buf, uint32_t read_len);
```
**Keep unchanged:** `void pal_gpio_write(uint16_t pin, bool level);` / `bool pal_gpio_read(uint16_t pin);`

> ⚠️ **补 channel 校验契约**：当前 host `pal_pwm_init`（`pal_hal_host.c:61`）**无 channel 校验**（恒 true），仅 `pal_pwm_set_duty`(L62-66) 校验 `channel >= PWM_CHANNELS(8)`。迁移时 `pal_pwm_init` 也应校验非法 channel 并返回 `WINK_ERR_INVALID_ARG`——否则 Phase 2 资源占用表对非法 channel 无防御。

---

### Task 3-2: host/wasm target 实现同步

**Files:**
- Modify: `wink-micro-os/targets/host/pal_hal_host.c`
- Modify: `wink-micro-os/targets/wasm/pal_hal_wasm.c`
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32.c`（若存在）

**Source-of-truth check:** `pal_hal_wasm.c` 各函数现状：`pal_gpio_init`/`pwm_init`/`enable_interrupt`/`disable_interrupt` 恒 true；`pal_pwm_set_duty` 调 js 后恒 true；`pal_i2c_transfer` 直返 js bool。`pal_hal_host.c`：`pal_pwm_set_duty` 已有 channel 校验（→ 现返 false，应改 `WINK_ERR_INVALID_ARG`）。

**Host rules:**
- 成功 → `WINK_OK`
- 非法 PWM channel（`channel >= PWM_CHANNELS`）→ `WINK_ERR_INVALID_ARG`（含 `pal_pwm_init`，补齐）
- 资源冲突 → 返回 Phase 2 guard 的精确 status（本阶段先留调用点，guard 在 Phase 2 接入）

**Wasm rules:**
- JS `false`（`pal_i2c_transfer`）→ `WINK_ERR_IO`
- 不支持的中断路径 → `WINK_ERR_UNSUPPORTED`

---

### Task 3-3: DAL 调用点迁移

**Files:**
- Modify: `wink-micro-os/dal/src/dal_servo.c`
- Modify: `wink-micro-os/dal/src/dal_ultrasonic.c`

**Source-of-truth check:** `dal_servo.c` 现为 `if (!pal_pwm_init(...)) return WINK_ERR_IO` / `if (!pal_pwm_set_duty(...)) return WINK_ERR_IO`（Phase 0 已补 `{}`）。`dal_ultrasonic.c` 真机分支不直接调失败型 PAL init（GPIO init 在 Phase 2 补），仅 `pal_gpio_write/read` + `pal_delay_*`（均非失败型，无需改）。

**Pattern:**
```c
wink_status_t status = pal_pwm_set_duty(dev->pwm_channel, duty_percent);
if (wink_status_is_error(status)) {
    return status;   /* 透传精确 PAL 错误，不再折叠成 WINK_ERR_IO */
}
```

> ⚠️ **与 Phase 2 的衔接**：`dal_servo.c` 的 `pal_pwm_init` 调用将在 Phase 2 Task 2-1 移入 `dal_servo_init`。本阶段先把现有调用点 status 化；Phase 2 再搬迁。DAL 公共契约的 Error-codes 列表须更新为透传集：`WINK_ERR_INVALID_ARG` / `WINK_ERR_IO` / `WINK_ERR_BUSY` / `WINK_ERR_RESOURCE_EXHAUSTED` / `WINK_ERR_NOT_INITIALIZED`。

---

### Task 3-4: `pal_osal.h` mutex 状态化

**Files:**
- Modify: `wink-micro-os/pal/include/pal_osal.h`
- Modify: `wink-micro-os/targets/host/pal_osal_host.c`
- Modify: `wink-micro-os/targets/wasm/pal_osal_wasm.c`
- Modify: `wink-micro-os/targets/esp32/pal_osal_esp32.c`（若存在）

**Source-of-truth check:** `pal_osal.h` 现状：`pal_mutex_lock`(L57)、`pal_mutex_unlock`(L62) 均 `bool`。`pal_osal_wasm.c` mutex 为无竞争 stub（恒 true）—— blast radius 小，但签名仍须一致迁移。

**Change:**
```c
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_lock(pal_mutex_t mutex, uint32_t timeout_ms);
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_unlock(pal_mutex_t mutex);
```
**Rules:** NULL mutex → `WINK_ERR_INVALID_ARG`；timeout → `WINK_ERR_TIMEOUT`；不支持 target → `WINK_ERR_UNSUPPORTED`。

---

### Task 3-5: tests and docs

**Files:**
- Modify: `wink-micro-os/test/test_host_pal.c`
- Modify: `wink-micro-os/test/test_dal_servo.c`
- Modify: `wink-micro-os/test/test_dal_ultrasonic.c`
- Modify: `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`
- Modify: `docs/design/07-platform-governance/02-error-fault-model.md`

**Verification Gate:**
```powershell
cd wink-micro-os
python wink-tools/wink.py test --clean          # 全绿，无 -Werror 失败
```
**Search gate（混合态零容忍）：**
```powershell
rg "bool pal_(gpio_init|gpio_enable_interrupt|gpio_disable_interrupt|pwm_init|pwm_set_duty|i2c_transfer|mutex_lock|mutex_unlock)" wink-micro-os
```
→ **0 命中**（证明无残留 bool 签名）。

## 出口验收
- [ ] `python wink-tools/wink.py test --clean` 全绿
- [ ] search gate 0 命中
- [ ] DAL 公共契约 Error-codes 已更新为透传集
- [ ] 整改跟踪表 P1-1 标"完成"——本阶段是 Phase 2 的前置，完成后方可启动 Phase 2
