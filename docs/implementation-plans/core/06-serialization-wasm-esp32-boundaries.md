# Phase 6: 序列化边界、Wasm 回调边界与 ESP32 路线收口

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `wink_bridge.h`、`targets/wasm/pal_hal_wasm.c`、`wink_status.h` 确认。
>
> **执行序位置（见 00-README）：** 文档/规范为主，可与多数阶段并行；grep 门禁与 ESP32 清单建议在前序 API 稳定后收尾。

**Goal:** 收口 P1-5 / P2-4 / P2-5 / P2-6：运行时结构体禁 `packed`、wire/flash 序列化隔离；Wasm 回调索引边界明确；DAL 头契约补全；ESP32 PAL ROADMAP 转为可执行移植清单。

**Architecture:** 以**制度化与边界防线**为主，防止后续 AI CodeGen 或驱动扩展误把 runtime POD 当 wire layout 直接 `memcpy`，或把 Wasm function index 当 C 函数指针裸 cast 扩散。

**Tech Stack:** C99, docs/design, wasm bridge, ESP-IDF roadmap

## Global Constraints
- 运行时结构体自然对齐，**禁** `__attribute__((packed))`
- 传输/持久化结构独立定义，带 version / endianness / CRC
- Wasm function pointer/index **不做裸 cast 扩散**

## Sequencing
- 可与 Phase 2 起的任意阶段并行（文档/规范为主，不碰已稳定的 C 公共契约）
- Task 6-3（wasm 回调边界）与 Phase 1 的 `wasm_bridge.h` 同文件 → 须 Phase 1 之后

---

### Task 6-1: packed 禁令与序列化规范

**Files:**
- Modify: `docs/design/02-wink-micro-os/01-dal-device-abstraction.md`
- Modify: `docs/design/07-platform-governance/03-security-sandbox.md`
- Modify: `.claude/rules/c-code.md`

**Add rules:**
- DAL/runtime POD 禁 `packed`（ARM/Xtensa 未对齐访问 → 性能下降甚至 Alignment Fault/HardFault）
- 成员按对齐需求**降序**排列（`uint32_t/float` 优先，`uint16_t` 次之，`bool/uint8_t` 末尾）
- wire/flash struct 须独立命名：`xxx_wire_t` / `xxx_flash_record_t`
- **禁** `memcpy` runtime struct 到 wire/flash buffer
- 须经 `serialize`/`deserialize` 转换，校验 version / endianness / CRC

---

### Task 6-2: CI grep 门禁建议

**Files:**
- Modify: `docs/design/06-build-toolchain/01-toolchain-deployment.md`

**Add gate:**
```powershell
rg "__attribute__\s*\(\(packed\)\)|#pragma\s+pack" wink-micro-os/dal wink-micro-os/runtime
```
→ runtime/DAL POD 路径 **0 命中**。
**未来 AST linter 规则：** `packed` 仅允许在 `protocol/`、`storage/` 或显式白名单 wire header 下（注：这些目录尚未建立，本门禁为预防性）。

---

### Task 6-3: Wasm 回调索引边界

**Files:**
- Modify: `wink-micro-os/targets/wasm/wasm_bridge.h`
- Modify: `wink-micro-os/targets/wasm/wasm_entry.c`
- Modify: `docs/design/07-platform-governance/03-security-sandbox.md`

**Source-of-truth check:** `pal_hal_wasm.c:24` 现状 `uint32_t callback_index = (uint32_t)(uintptr_t)callback;`（C 函数指针 → Table 索引）——正是 P2-4 所指 wasm32 可用 / wasm64 截断风险点。`wink_bridge.h:29` `js_pal_register_interrupt(uint16_t pin, uint32_t callback_index, void *arg)` 已用 index 语义，边界契约须补。

**Rules:**
- `uint32_t callback_index` 是**不透明 JS function table 索引**，不是 C 函数指针
- 禁在单一 wasm adapter 边界之外 cast 为 `pal_gpio_isr_t`
- 分发前校验 index 在已注册范围内（`isr != NULL` 不能防错误非零索引）
- 长期用 Emscripten `addFunction` / function table registry
- **中断桥安全分两维（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：**索引安全**（本 Task：callback_index 不透明、禁裸 cast、wasm64 截断）与**时序安全**（Phase 1 Task 1-5：Asyncify sleeping 窗口禁重入 `_trigger_wasm_interrupt`）互补。索引正确但时序错误仍会崩——ESP32 移植与前端 JS 实现须**同时**满足两维。

**Documented contract:**
```c
/* callback_index 是不透明 JS 表索引。禁在 wasm target adapter 之外把任意非零
 * 整数 cast 成函数指针。wasm64 下裸 (uint32_t)(uintptr_t) cast 会截断。 */
```

---

### Task 6-4: DAL header contract completion

**Files:**
- Modify: `wink-micro-os/dal/include/dal_servo.h`
- Modify: `wink-micro-os/dal/include/dal_ultrasonic.h`

**Source-of-truth check:** `dal_servo.h:24-31` 已有较完整契约（Preconditions/Blocking/ISR-safe/Thread-safe/Error-codes/Postconditions），`dal_ultrasonic.h:23-29` 同。本 Task 统一所有公共 API 契约字段齐全。

**Each public API must include:** Preconditions / Blocking / ISR-safe / Thread-safe / Callback-context（若有回调）/ Error-codes 精确列表 / Postconditions。

**Example:**
```c
 * @note API Contract:
 *   - Preconditions: dev != NULL; dal_servo_init() 已成功。
 *   - Blocking: No.
 *   - Thread-safe: No; 多任务访问需外部互斥。
 *   - ISR-safe: No.
 *   - Callback-context: None.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误。
 *   - Postconditions: WINK_OK 时 dev->current_angle 为钳位后的目标角度。
```

---

### Task 6-5: ESP32 PAL implementation checklist

**Files:**
- Modify: `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`
- Modify: `docs/design/06-build-toolchain/01-toolchain-deployment.md`
- Modify: `docs/design/07-platform-governance/01-device-model-registry.md`

**Checklist:**
- GPIO init/read/write via ESP-IDF driver
- PWM via LEDC，channel/timer 分配由 Phase 2 resource guard 追踪
- I2C transfer 带 timeout + 精确错误映射
- OSAL delays/time via FreeRTOS / esp_timer
- watchdog via ESP-IDF task watchdog 或 RTC watchdog
- reset reason via `esp_reset_reason()` → `pal_reset_reason_t` 映射
- ultrasonic capture via RMT 或 GPIO ISR + timer，**禁** runtime tick 内 busy-wait（呼应 Phase 4）
- boot fail-safe：每执行器板级 pull-down / 电源门控 / 使能脚默认关断须文档化

**Verification Gate:**
- host 测试仍全绿
- docs 无残留 `js_sim_get_ultrasonic_distance`
- `rg "packed|#pragma pack" wink-micro-os/dal wink-micro-os/runtime` → 0 命中

## 出口验收
- [ ] host 全绿
- [ ] docs 死符号清零；packed grep 0 命中
- [ ] DAL 公共 API 契约字段齐全
- [ ] wasm 回调 index 边界契约写入 `wasm_bridge.h`
- [ ] 整改跟踪表 P1-5 / P2-4 / P2-5 / P2-6 标"规范/清单完成；esp32 实现随移植推进"

