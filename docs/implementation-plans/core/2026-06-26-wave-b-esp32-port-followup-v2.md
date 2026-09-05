# Wave B ESP32 Port Follow-up — Revised Implementation Plan (v2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Before editing any ESP32 PAL/HAL C file, load the `embedded-best-practice` skill.

**Goal:** Remove the Wave B P0 crash risks, fix ESP32 PAL correctness gaps, and land the LEDC timer-allocation fix with full host CI coverage — leaving design truth updated before hardware acceptance.

**Architecture:** Fix low-level ESP32 runtime hazards first (NULL spinlock, RMT timeout reset), then extract a **target-agnostic `pal_pwm_router`** so the LEDC timer-slot / frequency / ref-count logic is shared identically by host/wasm/esp32 and unit-tested on host; the three targets become thin shells. Then move PWM GPIO routing into a board config, clean up naming/sdkconfig/docs, and run the verification gate.

**Tech Stack:** C99, ESP-IDF v5.1.3 (target `esp32`, dual-core Xtensa), FreeRTOS SMP, ESP32 RMT/LEDC/I2C drivers, CMake + Unity host tests, `idf.py build`.

**Target chip (confirmed):** `CONFIG_IDF_TARGET="esp32"` (classic, dual-core). Task 1's `taskENTER_CRITICAL(NULL)` crash and `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` both apply.

## Supersedes

This plan replaces `2026-06-26-wave-b-esp32-port-followup-action-plan.md`. Key differences from v1: (1) PWM allocation is extracted into a shared `pal_pwm_router` so host tests cover the LEDC fix (v1's host tests were unimplementable); (2) PWM concurrency is a documented non-concurrent contract with **no lock** (v1 proposed a two-lock scheme); (3) PWM pin routing lives in a dedicated `board_config.c` with weak/strong symbol resolution; (4) Task 8 adds runtime smokes for the two P0 fixes.

## Source Inputs

- External review: `C:/Users/77174/.gemini/antigravity-ide/brain/f6b9efd6-665a-484e-88f1-65a54911f640/review_feedback.md`
- Original Wave B report: `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`
- Code inspected: `wink-micro-os/targets/esp32/{pal_resource_esp32,pal_hal_esp32,pal_hal_esp32_rmt}.c`, `wink-micro-os/pal/include/{pal_hal,pal_resource,wink_status}.h`, `wink-micro-os/targets/{host,wasm}/pal_hal_*.c`, `wink-micro-os/test/test_{host_pal,pal_contract,dal_servo}.c`, `wink-micro-os/samples/avoidance_car/{device_tree,CMakeLists}.*`, `wink-micro-os/{CMakeLists.txt,python wink-tools/wink.py test}`, `esp32_firmware/{sdkconfig.defaults,main/CMakeLists.txt}`.

## Global Constraints

- `WINK_OK == 0`; errors are negative; check with `wink_status_is_error(status)` or `status < 0`. Key codes: `WINK_ERR_INVALID_ARG=-1`, `WINK_ERR_BUSY=-6`, `WINK_ERR_RESOURCE_EXHAUSTED=-10`, `WINK_ERR_HARDWARE=-12`, `WINK_ERR_TIMEOUT=-2`.
- Preserve static dispatch: no device `ops`, vtables, or `container_of` for DAL/PAL device abstraction.
- No heap allocation in PAL/DAL realtime paths; use fixed static tables. (Init-time heap such as `xSemaphoreCreateBinary` is allowed.)
- Keep ESP32-only logic under `#if defined(ESP_PLATFORM)` in target-layer files.
- Do not widen `#ifdef SIMULATION`.
- Docs use repo-root-relative paths with `wink-micro-os/` prefixes where applicable.
- **Every code task ends by running `python wink-tools/wink.py test` (host) and, when ESP-IDF is available, `idf.py -C esp32_firmware build`.**
- **Each Task is one independent commit** (English message, Conventional Commits) so any regression can be `git revert`-ed precisely. End every commit message with the trailer:
  ```
  Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
  ```
- **Doc-edit ordering:** Task 2 and Task 7 both edit `2026-06-26-wave-b-esp32-port-compilation-review.md`. Execute Task 2 before Task 7 to avoid edit conflicts.

## File Structure

New files:
- `wink-micro-os/pal/include/pal_pwm_router.h` — target-agnostic PWM allocation contract (channels, timers, acquire/release/ready).
- `wink-micro-os/pal/src/pal_pwm_router.c` — pure state machine: timer slots + ref counts + per-channel freq. No HW, no lock. Compiled by host + wasm + esp32.
- `wink-micro-os/test/test_pal_pwm_router.c` — host unit tests for the router (the LEDC fix's CI coverage).
- `wink-micro-os/samples/avoidance_car/board_config.c` — physical PWM channel→GPIO routing (strong `pal_pwm_pin_map`), firmware-only.
- `esp32_firmware/sdkconfig.defaults.dev` — dev/debug-only safety defaults (heap poisoning), kept out of the production baseline.

Modified files (per task below): `pal_resource_esp32.c`, `pal_hal_esp32_rmt.c`, `pal_hal_esp32.c`, `pal_hal.h`, `pal_hal_host.c`, `pal_hal_wasm.c`, both `CMakeLists.txt` (root + targets), `test/CMakeLists.txt`, `test_host_pal.c`, `test_dal_servo.c`, `samples/avoidance_car/{CMakeLists.txt,app_main.c→app_callbacks.c}`, `esp32_firmware/main/CMakeLists.txt`, `esp32_firmware/sdkconfig.defaults`, and three design docs.

Rationale: PAL is currently an INTERFACE (headers-only) library with implementations per-target. The router is the first **shared PAL implementation**, so it establishes `pal/src/` as the home for target-agnostic PAL logic (vs `targets/<plat>/` for hardware glue). This is what makes ADR-0002 (dual-target same-source) real for PWM.

## Priority Roadmap

| Order | Priority | Task | Outcome |
|---:|---|---|---|
| 1 | P0 | Replace `taskENTER_CRITICAL(NULL)` | Avoid ESP-IDF v5.x SMP null-spinlock crash |
| 2 | P0 | RMT timeout: `rmt_disable`+`rmt_enable` reset | Avoid invalid `rmt_receive(NULL)` cancel |
| 3 | P1 | GPIO ISR `uintptr_t` unwrap + `@verified` headers | Remove portability warning + doc-truth drift |
| 4 | P0 | Extract `pal_pwm_router` + 3 target shells + `pal_pwm_deinit` | LEDC timer fix with host CI coverage |
| 5 | P1 | PWM GPIO routing → `board_config.c` | Remove board hardcode from ESP32 PAL |
| 6 | P2 | Rename sample callback source | Remove dual `app_main.c` confusion |
| 7 | P1/P2 | sdkconfig split + docs + I2C roadmap | Keep design and code aligned |
| 8 | Gate | Final verification (incl. 2 P0 runtime smokes) | Ready for hardware acceptance |

---

## Task 1: Replace ESP32 resource critical-section NULL spinlock

**Files:**
- Modify: `wink-micro-os/targets/esp32/pal_resource_esp32.c`

**Interfaces:**
- Produces: a process-wide `static portMUX_TYPE s_resource_mux` guarding `s_claims[]`/`s_count`. No API change.

- [ ] **Step 1: Add the spinlock declaration**

In `wink-micro-os/targets/esp32/pal_resource_esp32.c`, after the `static uint32_t s_count = 0;` line and **before any function definition**, add:

```c
#if defined(ESP_PLATFORM)
/* SMP 临界区自旋锁：v5.x 下 taskENTER_CRITICAL(NULL) 会触发 spinlock_acquire(NULL)
 * 断言/解引用 → panic 复位。portMUX_INITIALIZER_UNLOCKED 为编译期初始化，
 * 保证在任何 taskENTER_CRITICAL 调用前已就绪。*/
static portMUX_TYPE s_resource_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
```

Keep `static uint32_t s_count = 0;` **non-volatile** (concurrency is provided by the critical section, not `volatile`, which gives neither atomicity nor a memory barrier across cores).

- [ ] **Step 2: Replace all NULL critical-section calls**

Replace every `taskENTER_CRITICAL(NULL)` with `taskENTER_CRITICAL(&s_resource_mux)` and every `taskEXIT_CRITICAL(NULL)` with `taskEXIT_CRITICAL(&s_resource_mux)`. There are 8 occurrences across `pal_resource_reset`, `pal_resource_claim`, and `pal_resource_release`.

- [ ] **Step 3: Verify none remain**

Run: `pwsh -c "Select-String -Path wink-micro-os/targets/esp32/pal_resource_esp32.c -Pattern 'CRITICAL\(NULL\)'"`
Expected: no output (no matches).

- [ ] **Step 4: Build ESP32 firmware**

Run: `idf.py -C esp32_firmware build`
Expected: zero errors, zero warnings.

- [ ] **Step 5: Commit**

```bash
git add wink-micro-os/targets/esp32/pal_resource_esp32.c
git commit -m "fix(esp32-pal): replace NULL spinlock with dedicated portMUX_TYPE

taskENTER_CRITICAL(NULL) dereferences/asserts the spinlock on ESP-IDF v5.x
SMP (classic esp32 dual-core), causing a panic. Add s_resource_mux and use
it for all claim/release/reset critical sections."
```

---

## Task 2: Replace invalid RMT timeout cancellation

**Files:**
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`
- Modify: `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`

**Interfaces:** none changed.

- [ ] **Step 1: Verify the IDF v5.1 RMT RX cancellation contract**

Confirm by reading `components/driver/rmt_rx.c` in the IDF v5.1.3 tree (or the RMT docs) that `rmt_receive(channel, NULL, 0, NULL)` is not a documented cancellation and that the supported reset path is `rmt_disable()` + `rmt_enable()`. Record the finding in the commit message. (If IDF source shows an explicit NULL assertion, cite it; the fix is correct either way.)

- [ ] **Step 2: Replace the timeout branch**

In `pal_hal_esp32_rmt.c`, in `pal_rmt_ultrasonic_measure`, replace the timeout block:

```c
    BaseType_t ok = xSemaphoreTake(s_rx_done_sem, pdMS_TO_TICKS((timeout_us + 999) / 1000 + 1));
    if (ok != pdPASS) {
        /* 超时恢复：disable → enable 复位 RMT RX 状态机。
         * 信号量残留：超时后 s_rx_done_sem 可能有残留 Give，但下一次 measure 入口的
         * xSemaphoreTake(s_rx_done_sem, 0) 会清空，故此处不必额外 Take。
         * 旧实现误用 rmt_receive(NULL,...) 取消——违反 v5.x RX 契约，改为状态机复位。*/
        rmt_disable(s_rmt_rx_chan);
        esp_err_t start_err = rmt_enable(s_rmt_rx_chan);
        if (start_err != ESP_OK) {
            return WINK_ERR_HARDWARE;
        }
        return WINK_ERR_TIMEOUT;
    }
```

(Note: `rmt_disable` returns `esp_err_t` but is best-effort here; the gating error is `rmt_enable`. Keep `rmt_disable` unchecked as above, consistent with `pal_rmt_ultrasonic_deinit`.)

- [ ] **Step 3: Add the RMT checklist row to the Wave B report**

In `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`, §6 "RMT 硬件验收前置检查清单" table, add one row:

```markdown
| 超时复位路径用 disable/enable | ✅ | `targets/esp32/pal_hal_esp32_rmt.c` `pal_rmt_ultrasonic_measure` 超时分支 |
```

- [ ] **Step 4: Verify no NULL-buffer receive remains**

Run: `pwsh -c "Select-String -Path wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c -Pattern 'rmt_receive\('"`
Expected: exactly one match — the legitimate `rmt_receive(s_rmt_rx_chan, s_rx_buf, sizeof(s_rx_buf), &recv_cfg)`. No `NULL` buffer call.

- [ ] **Step 5: Build ESP32 firmware**

Run: `idf.py -C esp32_firmware build`
Expected: zero errors, zero warnings.

- [ ] **Step 6: Commit**

```bash
git add wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md
git commit -m "fix(esp32-rmt): reset RX channel via disable/enable on timeout

rmt_receive(chan,NULL,0,NULL) is not a valid cancellation under the IDF v5.x
RMT RX contract. On timeout, disable then re-enable the channel to reset the
state machine so the next measure arms cleanly."
```

---

## Task 3: Fix GPIO ISR pointer round-trip and verification headers

**Files:**
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32.c`
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`

**Interfaces:** none changed.

- [ ] **Step 1: Fix the ISR unwrap in `pal_hal_esp32.c`**

In `gpio_isr_wrapper`, replace:

```c
    uint32_t pin = (uint32_t)arg;
```

with the symmetric round-trip (the call site at `pal_gpio_enable_interrupt` already uses `(void *)(uintptr_t)pin`):

```c
    uint32_t pin = (uint32_t)(uintptr_t)arg;
```

No change needed at the call site — only the ISR-side unwrap was asymmetric.

- [ ] **Step 2: Update the `@verified` header in `pal_hal_esp32.c`**

Change the file header line `⚠️ @verified: NO` block's lead to:

```c
 * ⚠️ @verified: COMPILED -- ESP-IDF v5.1.3 idf.py build compile verification passed.
 *    Hardware behavior remains pending Wave B board validation.
```

Retain the rest of the header (MVP notes, FIXME references).

- [ ] **Step 3: Update the `@verified` header in `pal_hal_esp32_rmt.c`**

Change the lead `⚠️ @verified: NO` to:

```c
 * ⚠️ @verified: COMPILED -- ESP-IDF v5.1.3 idf.py build compile verification passed.
 *    Hardware validation still required: oscilloscope ISR latency < 10us and HC-SR04 accuracy.
```

- [ ] **Step 4: Build ESP32 firmware**

Run: `idf.py -C esp32_firmware build`
Expected: zero errors, zero warnings.

- [ ] **Step 5: Commit**

```bash
git add wink-micro-os/targets/esp32/pal_hal_esp32.c wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c
git commit -m "fix(esp32-pal): symmetric uintptr_t ISR unwrap + mark headers COMPILED

gpio_isr_wrapper now mirrors the (void*)(uintptr_t)pin call site, clearing
the int-to-pointer-cast warning. Bump @verified to COMPILED (hardware still
pending Wave B board validation)."
```

---

## Task 4: Extract `pal_pwm_router` and harden all three target PWM shells

This is the centerpiece. It (a) introduces the platform-agnostic allocator with **no lock** (non-concurrent contract), (b) adds `pal_pwm_deinit`, (c) replaces the hardcoded `LEDC_TIMER_0` with router-allocated timers, (d) renames local `PWM_CHANNELS` → `PAL_PWM_CHANNELS`, and (e) makes it all host-testable.

**Files:**
- Create: `wink-micro-os/pal/include/pal_pwm_router.h`
- Create: `wink-micro-os/pal/src/pal_pwm_router.c`
- Create: `wink-micro-os/test/test_pal_pwm_router.c`
- Modify: `wink-micro-os/pal/include/pal_hal.h`
- Modify: `wink-micro-os/targets/host/CMakeLists.txt`
- Modify: `wink-micro-os/targets/host/pal_hal_host.c`
- Modify: `wink-micro-os/targets/wasm/pal_hal_wasm.c`
- Modify: `wink-micro-os/CMakeLists.txt`
- Modify: `wink-micro-os/targets/esp32/CMakeLists.txt`
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32.c`
- Modify: `wink-micro-os/test/CMakeLists.txt`
- Modify: `wink-micro-os/test/test_host_pal.c`
- Modify: `wink-micro-os/test/test_dal_servo.c`

**Interfaces:**
- Produces: `pal_pwm_router_acquire/release/channel_ready/channel_timer/reset`, `PAL_PWM_CHANNELS`, `PAL_PWM_TIMERS`, `void pal_pwm_deinit(uint8_t channel)`.
- Consumes: `wink_status_t`, `pal_resource_claim/release` (host + esp32 shells), `ledc_*` (esp32 shell only).

### Step group A — write the failing test first (TDD)

- [ ] **Step A1: Add `PAL_PWM_CHANNELS` to `pal_hal.h`**

In `wink-micro-os/pal/include/pal_hal.h`, in the "2. PWM 抽象" section before `pal_pwm_init`, add:

```c
/** @brief 平台无关 PWM 逻辑通道上限（各 target 与 router 统一引用）。*/
#define PAL_PWM_CHANNELS 8
```

- [ ] **Step A2: Create `pal_pwm_router.h`**

Create `wink-micro-os/pal/include/pal_pwm_router.h`:

```c
/**
 * @file pal_pwm_router.h
 * @brief target 无关的 PWM 定时器分配状态机（LEDC timer 槽 + 引用计数）。
 *
 * 由 host/wasm/esp32 三个 target 共享链接，使 LEDC timer 分配逻辑在 host CI 可测。
 * 纯逻辑：无硬件调用、无锁、无堆。
 *
 * 非并发契约：仅从 PAL 初始化任务调用（wink_runtime_run 先 init 后 loop，init 完成才
 * 进入 tick；set_duty 只读 channel_ready，不与 init 交叠）。故内部不持锁——锁本身是
 * hazard，本路径不存在并发。
 */
#ifndef PAL_PWM_ROUTER_H
#define PAL_PWM_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"   /* PAL_PWM_CHANNELS */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief LEDC 低速定时器上限（经典 ESP32 / S3 / C3 均为 4）。*/
#define PAL_PWM_TIMERS 4

/**
 * @brief 为 channel 预约频率，输出应绑定的 timer 编号。
 * @param channel   逻辑 PWM 通道 [0, PAL_PWM_CHANNELS)
 * @param freq_hz   频率 (Hz)，须 > 0
 * @param out_timer_num [out] 分配/复用的 timer 编号 [0, PAL_PWM_TIMERS)
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_BUSY / WINK_ERR_RESOURCE_EXHAUSTED
 *
 * 语义：
 *  - channel 越界 / freq_hz==0 / out==NULL  → WINK_ERR_INVALID_ARG（状态不变）
 *  - channel 已初始化且同 freq              → WINK_OK（幂等，out 为原 timer）
 *  - channel 已初始化且异 freq              → WINK_ERR_BUSY（状态不变）
 *  - 新频率且 4 个 timer 全占                → WINK_ERR_RESOURCE_EXHAUSTED
 *  - 否则复用同频 timer 或分配空闲 timer      → WINK_OK
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(uint8_t channel, uint32_t freq_hz,
                                     uint8_t *out_timer_num);

/**
 * @brief 释放 channel：递减其 timer 引用计数，归零则回收 timer 槽。
 *        未初始化 channel 为 no-op。调用方应在调用前完成硬件 stop。
 */
void pal_pwm_router_release(uint8_t channel);

/** @brief channel 是否已就绪（set_duty 守卫）。*/
bool pal_pwm_router_channel_ready(uint8_t channel);

/** @brief channel 当前绑定的 timer；未就绪返回 0xFF。*/
uint8_t pal_pwm_router_channel_timer(uint8_t channel);

/** @brief 清空所有表（测试隔离 / 启动重置）。BSS 零初始化已满足真机启动，主要供 host 测试。*/
void pal_pwm_router_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_PWM_ROUTER_H */
```

- [ ] **Step A3: Create the router test file**

Create `wink-micro-os/test/test_pal_pwm_router.c`:

```c
#include "unity.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"

void setUp(void) {
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

static uint8_t acquire_ok(uint8_t ch, uint32_t freq) {
    uint8_t t = 0xFF;
    TEST_ASSERT_EQUAL_INT_MESSAGE(WINK_OK, pal_pwm_router_acquire(ch, freq, &t), "acquire should succeed");
    return t;
}

void test_router_acquire_release_basic(void) {
    uint8_t t = acquire_ok(0, 1000);
    TEST_ASSERT_TRUE(t < PAL_PWM_TIMERS);
    TEST_ASSERT_TRUE(pal_pwm_router_channel_ready(0));
    TEST_ASSERT_EQUAL_UINT8(t, pal_pwm_router_channel_timer(0));

    pal_pwm_router_release(0);
    TEST_ASSERT_FALSE(pal_pwm_router_channel_ready(0));
    TEST_ASSERT_EQUAL_UINT8(0xFF, pal_pwm_router_channel_timer(0));
}

void test_router_same_freq_reuses_timer(void) {
    uint8_t t0 = acquire_ok(0, 50);
    uint8_t t1 = acquire_ok(1, 50);
    TEST_ASSERT_EQUAL_UINT8(t0, t1);   /* same freq → same timer */
}

void test_router_diff_freq_diff_timer(void) {
    uint8_t t0 = acquire_ok(0, 50);
    uint8_t t1 = acquire_ok(1, 1000);
    TEST_ASSERT_NOT_EQUAL(t0, t1);
}

void test_router_idempotent_and_busy(void) {
    uint8_t t = acquire_ok(0, 50);
    uint8_t t2 = 0xFF;
    /* same channel, same freq → idempotent, same timer */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_router_acquire(0, 50, &t2));
    TEST_ASSERT_EQUAL_UINT8(t, t2);
    /* same channel, different freq → BUSY, state unchanged */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_pwm_router_acquire(0, 1000, &t2));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_router_acquire(0, 50, &t2));
    TEST_ASSERT_EQUAL_UINT8(t, t2);
}

void test_router_exhausted_after_four_distinct_freqs(void) {
    uint8_t t;
    uint32_t freqs[PAL_PWM_TIMERS] = {50, 200, 1000, 5000};
    for (uint8_t i = 0; i < PAL_PWM_TIMERS; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_router_acquire(i, freqs[i], &t));
    }
    /* 5th distinct frequency, no free timer */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, pal_pwm_router_acquire(4, 25000, &t));
    /* but reusing an existing freq still OK (channel 4 shares timer 0's 50Hz) */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_router_acquire(4, 50, &t));
}

void test_router_release_recycles_timer(void) {
    uint8_t ta = acquire_ok(0, 50);
    (void)acquire_ok(1, 50);            /* ref=2 */
    pal_pwm_router_release(0);          /* ref=1 */
    pal_pwm_router_release(1);          /* ref=0 → slot recycled */
    /* re-allocating 50Hz succeeds on a fresh slot */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_router_acquire(2, 50, &ta));
}

void test_router_invalid_args(void) {
    uint8_t t;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_pwm_router_acquire(PAL_PWM_CHANNELS, 50, &t));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_pwm_router_acquire(0, 0, &t));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_pwm_router_acquire(0, 50, NULL));
    /* release of OOR/uninit channel is a safe no-op */
    pal_pwm_router_release(PAL_PWM_CHANNELS);
    pal_pwm_router_release(0);
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_router_acquire_release_basic);
    RUN_TEST(test_router_same_freq_reuses_timer);
    RUN_TEST(test_router_diff_freq_diff_timer);
    RUN_TEST(test_router_idempotent_and_busy);
    RUN_TEST(test_router_exhausted_after_four_distinct_freqs);
    RUN_TEST(test_router_release_recycles_timer);
    RUN_TEST(test_router_invalid_args);
    return UNITY_END();
}
```

- [ ] **Step A4: Wire the router source into the host OBJECT lib and register the test**

In `wink-micro-os/targets/host/CMakeLists.txt`, add the shared router source to the `pal_host` OBJECT library:

```cmake
add_library(pal_host OBJECT
    pal_hal_host.c
    pal_osal_host.c
    pal_resource_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/src/pal_pwm_router.c)
```

In `wink-micro-os/test/CMakeLists.txt`, next to the `test_pal_contract` line, register the new test (it links `${HOST_PAL_OBJECT}`, which now includes the router):

```cmake
add_wink_host_test(test_pal_pwm_router test_pal_pwm_router.c)
```

Create a **stub** `wink-micro-os/pal/src/pal_pwm_router.c` (so CMake configures; the test will fail at runtime until the real impl lands):

```c
#include "pal_pwm_router.h"

void pal_pwm_router_reset(void) {}
wink_status_t pal_pwm_router_acquire(uint8_t channel, uint32_t freq_hz, uint8_t *out_timer_num) {
    (void)channel; (void)freq_hz; (void)out_timer_num;
    return WINK_ERR_UNSUPPORTED;
}
void pal_pwm_router_release(uint8_t channel) { (void)channel; }
bool pal_pwm_router_channel_ready(uint8_t channel) { (void)channel; return false; }
uint8_t pal_pwm_router_channel_timer(uint8_t channel) { (void)channel; return 0xFF; }
```

- [ ] **Step A5: Run the test and verify it FAILS**

Run: `python wink-tools/wink.py test`
Expected: `test_pal_pwm_router` FAILS (acquire returns `WINK_ERR_UNSUPPORTED`, not `WINK_OK`).

### Step group B — implement the router (make tests pass)

- [ ] **Step B1: Write the real router implementation**

Replace the **entire** contents of `wink-micro-os/pal/src/pal_pwm_router.c` with:

```c
/**
 * @file pal_pwm_router.c
 * @brief target 无关 PWM 定时器分配状态机实现（纯逻辑，无锁，无硬件）。
 *
 * 非并发契约见 pal_pwm_router.h。host/wasm/esp32 三 target 共享链接。
 */
#include "pal_pwm_router.h"

typedef struct {
    uint32_t freq_hz;
    uint8_t  ref_count;
} pwm_timer_slot_t;

static pwm_timer_slot_t s_timer_slots[PAL_PWM_TIMERS];
static bool    s_channel_init[PAL_PWM_CHANNELS];
static uint32_t s_channel_freq[PAL_PWM_CHANNELS];
static uint8_t s_channel_timer[PAL_PWM_CHANNELS];

void pal_pwm_router_reset(void) {
    for (uint8_t t = 0; t < PAL_PWM_TIMERS; t++) {
        s_timer_slots[t].freq_hz = 0;
        s_timer_slots[t].ref_count = 0;
    }
    for (uint8_t c = 0; c < PAL_PWM_CHANNELS; c++) {
        s_channel_init[c] = false;
        s_channel_freq[c] = 0;
        s_channel_timer[c] = 0xFF;
    }
}

/* 同频 timer 优先复用；否则首个空闲槽；皆无返回 -1。*/
static int8_t pwm_router_find_slot(uint32_t freq_hz) {
    int8_t free_slot = -1;
    for (uint8_t t = 0; t < PAL_PWM_TIMERS; t++) {
        if (s_timer_slots[t].ref_count > 0 && s_timer_slots[t].freq_hz == freq_hz) {
            return (int8_t)t;
        }
        if (s_timer_slots[t].ref_count == 0 && free_slot < 0) {
            free_slot = (int8_t)t;
        }
    }
    return free_slot;
}

wink_status_t pal_pwm_router_acquire(uint8_t channel, uint32_t freq_hz,
                                     uint8_t *out_timer_num) {
    if (channel >= PAL_PWM_CHANNELS || freq_hz == 0 || out_timer_num == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_channel_init[channel]) {
        if (s_channel_freq[channel] == freq_hz) {
            *out_timer_num = s_channel_timer[channel];
            return WINK_OK;                 /* idempotent */
        }
        return WINK_ERR_BUSY;               /* state unchanged */
    }

    int8_t slot = pwm_router_find_slot(freq_hz);
    if (slot < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (s_timer_slots[slot].ref_count == 0) {
        s_timer_slots[slot].freq_hz = freq_hz;
    }
    s_timer_slots[slot].ref_count++;

    s_channel_init[channel] = true;
    s_channel_freq[channel] = freq_hz;
    s_channel_timer[channel] = (uint8_t)slot;
    *out_timer_num = (uint8_t)slot;
    return WINK_OK;
}

void pal_pwm_router_release(uint8_t channel) {
    if (channel >= PAL_PWM_CHANNELS || !s_channel_init[channel]) {
        return;                             /* no-op */
    }
    uint8_t t = s_channel_timer[channel];
    if (t < PAL_PWM_TIMERS && s_timer_slots[t].ref_count > 0) {
        s_timer_slots[t].ref_count--;
        if (s_timer_slots[t].ref_count == 0) {
            s_timer_slots[t].freq_hz = 0;   /* recycle */
        }
    }
    s_channel_init[channel] = false;
    s_channel_freq[channel] = 0;
    s_channel_timer[channel] = 0xFF;
}

bool pal_pwm_router_channel_ready(uint8_t channel) {
    return channel < PAL_PWM_CHANNELS && s_channel_init[channel];
}

uint8_t pal_pwm_router_channel_timer(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) {
        return 0xFF;
    }
    return s_channel_timer[channel];
}
```

- [ ] **Step B2: Run the router tests and verify they PASS**

Run: `python wink-tools/wink.py test`
Expected: all `test_pal_pwm_router` cases PASS. (Other suites still pass — router not yet wired into shells.)

### Step group C — wire the router into the host shell + `pal_pwm_deinit`

- [ ] **Step C1: Rewrite the host PWM shell; add `pal_pwm_deinit`**

In `wink-micro-os/targets/host/pal_hal_host.c`, add to the includes:

```c
#include "pal_pwm_router.h"
```

Delete the line `#define PWM_CHANNELS 8`. Replace the three PWM functions (`pal_pwm_init`, `pal_pwm_set_duty`, and add `pal_pwm_deinit`) with:

```c
wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq) {
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, freq, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }
    rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, channel, "pal_hal_host");
    if (wink_status_is_error(rs)) {
        pal_pwm_router_release(channel);   /* roll back router reservation */
        return rs;
    }
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    host_record_pwm(channel, duty);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
    (void)pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, "pal_hal_host");
    pal_pwm_router_release(channel);
}
```

Note the behavior change: `pal_pwm_set_duty` now requires the channel to be initialized (matches ESP32). The existing `test_pwm_duty_recorded` is updated in Step C4.

- [ ] **Step C2: Update `test_host_pal.c` — reset + init-first + new scenarios**

In `wink-micro-os/test/test_host_pal.c`, add includes and strengthen `setUp`:

```c
#include "pal_pwm_router.h"
#include "pal_resource.h"

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
    pal_resource_reset();
}
```

Replace `test_pwm_duty_recorded` so it inits the channel first:

```c
void test_pwm_duty_recorded(void) {
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern wink_status_t pal_pwm_set_duty(uint8_t, float);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(2, 50));
    wink_status_t st = pal_pwm_set_duty(2, 7.5f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(2));
}
```

Append these three tests (and register them in `main`):

```c
void test_pwm_deinit_then_reinit(void) {
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern wink_status_t pal_pwm_set_duty(uint8_t, float);
    extern void pal_pwm_deinit(uint8_t);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(3, 1000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_set_duty(3, 50.0f));
    pal_pwm_deinit(3);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(3, 2000));   /* different freq OK after deinit */
    pal_pwm_deinit(3);
}

void test_pwm_reinit_different_freq_returns_busy(void) {
    extern wink_status_t pal_pwm_init(uint8_t, uint32_t);
    extern void pal_pwm_deinit(uint8_t);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(0, 50));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_pwm_init(0, 1000));  /* not deinit'd → BUSY */
    pal_pwm_deinit(0);
}

void test_pwm_deinit_uninit_is_noop(void) {
    extern void pal_pwm_deinit(uint8_t);
    pal_pwm_deinit(5);   /* uninitialized: must not crash */
    TEST_PASS();
}
```

In `main`, add before `UNITY_END()`:

```c
    RUN_TEST(test_pwm_deinit_then_reinit);
    RUN_TEST(test_pwm_reinit_different_freq_returns_busy);
    RUN_TEST(test_pwm_deinit_uninit_is_noop);
```

- [ ] **Step C3: Isolate `test_dal_servo.c` against router state**

`dal_servo_init` calls `pal_pwm_init`, so router tables must be clean per test. In `wink-micro-os/test/test_dal_servo.c`, add the include and reset in `setUp`:

```c
#include "pal_pwm_router.h"

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
}
```

(`pal_resource_claim` is idempotent by owner, so resource state across these tests is non-fatal; the router is the frequency-sensitive table that must be reset.)

- [ ] **Step C4: Run host tests and verify PASS**

Run: `python wink-tools/wink.py test`
Expected: ALL suites pass, including `test_host_pal`, `test_dal_servo`, `test_pal_pwm_router`.

### Step group D — wire the router into the wasm shell

- [ ] **Step D1: Rewrite the wasm PWM shell; add `pal_pwm_deinit`**

In `wink-micro-os/targets/wasm/pal_hal_wasm.c`, add the include:

```c
#include "pal_pwm_router.h"
```

Replace the two PWM functions (and add `pal_pwm_deinit`):

```c
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz) {
    uint8_t timer_num = 0;
    /* wasm 无资源表/硬件，但 router 提供通道/频率校验与槽位记账，保持与 host/esp32 一致。*/
    return pal_pwm_router_acquire(channel, frequency_hz, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    pal_pwm_router_release(channel);   /* no-op if uninitialized */
}
```

- [ ] **Step D2: Add the router source to the wasm simulator build**

In `wink-micro-os/CMakeLists.txt`, add the router to the `wink_simulator` sources:

```cmake
    add_executable(wink_simulator
        targets/wasm/pal_hal_wasm.c
        targets/wasm/pal_osal_wasm.c
        targets/wasm/pal_resource_wasm.c
        targets/wasm/wasm_entry.c
        pal/src/pal_pwm_router.c)
```

### Step group E — wire the router into the ESP32 shell (the LEDC fix)

- [ ] **Step E1: Add the router source to the ESP32 component**

In `wink-micro-os/targets/esp32/CMakeLists.txt`, add it to both the IDF `SRCS` list and the `ESP32_PAL_SOURCES` list:

```cmake
        pal_hal_esp32_rmt.c
        ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
        # 核心共享源文件（来自 core_sources.cmake）
        ${WINK_CORE_SOURCES}
```

(and the same `${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c` line in the `elseif` `ESP32_PAL_SOURCES` block).

- [ ] **Step E2: Declare `pal_pwm_deinit` in `pal_hal.h`**

In `wink-micro-os/pal/include/pal_hal.h`, after the `pal_pwm_set_duty` declaration, add:

```c
/**
 * @brief 释放指定 PWM 通道（清零占空比、释放通道占用、递减 timer 引用计数）。
 * @note void 返回：deinit 为 best-effort，不应失败；未初始化 channel 调用为 no-op。
 */
void pal_pwm_deinit(uint8_t channel);
```

- [ ] **Step E3: Rewrite the ESP32 PWM shell; allocate timers via the router**

In `wink-micro-os/targets/esp32/pal_hal_esp32.c`, add includes:

```c
#include "pal_pwm_router.h"
```

Delete `#define PWM_CHANNELS 8` and `static bool s_pwm_initialized[PWM_CHANNELS] = {false};` (the router now owns readiness). Replace the entire PWM section (`pal_pwm_init` + `pal_pwm_set_duty`, plus the new `pal_pwm_deinit`) with:

```c
/* owner 字符串常量：claim/release 必须逐字一致，否则 release 静默 no-op。*/
static const char *const PWM_OWNER = "pal_hal_esp32";

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz) {
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, freq_hz, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }

    rs = pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    if (wink_status_is_error(rs)) {
        pal_pwm_router_release(channel);
        return rs;
    }

#if defined(ESP_PLATFORM)
    /* router 分配 timer，不再写死 LEDC_TIMER_0：同频复用、异频隔离。*/
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t)timer_num,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        (void)pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        return WINK_ERR_HARDWARE;
    }

    ledc_channel_config_t ch_cfg = {
        .gpio_num = pwm_gpio_map[channel],   /* Task 5 swaps to pal_pwm_pin_map[channel] */
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer_num,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        (void)pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
        return WINK_ERR_HARDWARE;
    }
#else
    (void)freq_hz;
#endif
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty_percent < 0.0f) { duty_percent = 0.0f; }
    if (duty_percent > 100.0f) { duty_percent = 100.0f; }

#if defined(ESP_PLATFORM)
    uint32_t duty = (uint32_t)(duty_percent / 100.0f * 8191.0f); /* 13-bit = 8192 */
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
#else
    (void)duty_percent;
#endif
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }   /* no-op if uninitialized */
#if defined(ESP_PLATFORM)
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
#endif
    (void)pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, channel, PWM_OWNER);
    pal_pwm_router_release(channel);
}
```

Keep the existing `static const int pwm_gpio_map[PWM_CHANNELS]` lookup table for now — **Task 5 replaces it with `pal_pwm_pin_map`**. Since `PWM_CHANNELS` was deleted, update that table's declaration to use `PAL_PWM_CHANNELS`:

```c
    static const int pwm_gpio_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};
```

- [ ] **Step E4: Build all targets**

Run: `python wink-tools/wink.py test`
Expected: all host tests PASS (router now wired into all shells).

Run: `idf.py -C esp32_firmware build`
Expected: zero errors, zero warnings.

- [ ] **Step E5: Commit**

```bash
git add wink-micro-os/pal/include/pal_pwm_router.h \
        wink-micro-os/pal/src/pal_pwm_router.c \
        wink-micro-os/pal/include/pal_hal.h \
        wink-micro-os/targets/host/CMakeLists.txt \
        wink-micro-os/targets/host/pal_hal_host.c \
        wink-micro-os/targets/wasm/pal_hal_wasm.c \
        wink-micro-os/CMakeLists.txt \
        wink-micro-os/targets/esp32/CMakeLists.txt \
        wink-micro-os/targets/esp32/pal_hal_esp32.c \
        wink-micro-os/test/CMakeLists.txt \
        wink-micro-os/test/test_pal_pwm_router.c \
        wink-micro-os/test/test_host_pal.c \
        wink-micro-os/test/test_dal_servo.c
git commit -m "feat(pal): shared pal_pwm_router + pal_pwm_deinit, fix LEDC timer sharing

Extract LEDC timer-slot allocation into a target-agnostic pal_pwm_router
(shared by host/wasm/esp32) so the timer-sharing fix is unit-tested on host
instead of ESP32-only. Allocate per-frequency timers via ref counts instead
of forcing all channels onto LEDC_TIMER_0. Add pal_pwm_deinit. PWM shells
become thin wrappers; rename local PWM_CHANNELS -> PAL_PWM_CHANNELS. Router
is non-concurrent by contract (PAL init path), so it carries no lock.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Move PWM GPIO routing into `board_config.c`

**Files:**
- Modify: `wink-micro-os/pal/include/pal_hal.h`
- Modify: `wink-micro-os/targets/esp32/pal_hal_esp32.c`
- Create: `wink-micro-os/samples/avoidance_car/board_config.c`
- Modify: `esp32_firmware/main/CMakeLists.txt`
- Modify: `docs/design/07-platform-governance/01-device-model-registry.md`

**Interfaces:**
- Produces: `extern const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS]` (contract in `pal_hal.h`; weak default in ESP32 target; strong override in `board_config.c`).

**Routing boundary:** ESP32 target code must not include the sample's `device_tree.h`. The PAL contract declares the symbol; physical firmware links a strong `board_config.c`; the ESP32 target provides a weak default so the symbol always resolves.

- [ ] **Step 1: Declare the routing contract in `pal_hal.h`**

In `wink-micro-os/pal/include/pal_hal.h`, right after the `PAL_PWM_CHANNELS` definition, add:

```c
/**
 * @brief 板级 PWM 通道→GPIO 路由表。
 * @note 物理 target 由 board_config.c 提供强定义；esp32 target 提供弱默认。
 *       host/wasm 不引用本符号（不路由物理 GPIO）。
 */
extern const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS];
```

- [ ] **Step 2: Provide the weak default in the ESP32 target**

In `wink-micro-os/targets/esp32/pal_hal_esp32.c`, under `#if defined(ESP_PLATFORM)` near the top of the PWM section (after the includes, before `pal_pwm_init`), add:

```c
#if defined(ESP_PLATFORM)
/* 板级路由弱默认：无 board_config.c 覆盖时使用，避免链接缺符号。
 * 强定义由 samples/<app>/board_config.c 提供。*/
__attribute__((weak)) const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};
#endif
```

- [ ] **Step 3: Create the strong board routing for the avoidance_car sample**

Create `wink-micro-os/samples/avoidance_car/board_config.c`:

```c
/**
 * @file board_config.c
 * @brief 板级硬件路由（物理引脚映射）—— codegen 产物占位。
 *
 * 与 device_tree.c 分离：device_tree.c 描述逻辑设备实例（servo/ultrasonic），
 * 本文件描述 PWM channel→GPIO 的物理路由。仅物理 firmware 链接；host/wasm 不引用。
 */
#include "pal_hal.h"

/* 强定义，覆盖 esp32 target 的弱默认。*/
const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};
```

- [ ] **Step 4: Swap the ESP32 shell to use `pal_pwm_pin_map`**

In `wink-micro-os/targets/esp32/pal_hal_esp32.c`, delete the local lookup table:

```c
    static const int pwm_gpio_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};
```

and change the channel config line:

```c
        .gpio_num = pal_pwm_pin_map[channel],
```

- [ ] **Step 5: Link `board_config.c` into the physical firmware**

In `esp32_firmware/main/CMakeLists.txt`, add the strong-definition source:

```cmake
idf_component_register(
    SRCS
        "app_main.c"
        "../../wink-micro-os/samples/avoidance_car/app_main.c"
        "../../wink-micro-os/samples/avoidance_car/device_tree.c"
        "../../wink-micro-os/samples/avoidance_car/board_config.c"
    INCLUDE_DIRS
        "."
        "../../wink-micro-os/samples/avoidance_car"
    REQUIRES driver esp32
)
```

(Do **not** add `board_config.c` to the host sample build — host/wasm do not reference `pal_pwm_pin_map`.)

- [ ] **Step 6: Update the device model registry doc**

In `docs/design/07-platform-governance/01-device-model-registry.md` §7, add a bullet: generated `board_config.c` must provide PWM channel→GPIO routing for physical targets; host/wasm builds omit it.

- [ ] **Step 7: Build both targets**

Run: `python wink-tools/wink.py test` → all pass.
Run: `idf.py -C esp32_firmware build` → zero errors/warnings.

- [ ] **Step 8: Commit**

```bash
git add wink-micro-os/pal/include/pal_hal.h \
        wink-micro-os/targets/esp32/pal_hal_esp32.c \
        wink-micro-os/samples/avoidance_car/board_config.c \
        esp32_firmware/main/CMakeLists.txt \
        docs/design/07-platform-governance/01-device-model-registry.md
git commit -m "refactor(esp32-pal): move PWM GPIO routing to board_config.c

ESP32 PAL no longer hardcodes the channel->GPIO map. pal_hal.h declares the
routing contract; the ESP32 target provides a weak default and the physical
firmware links a strong board_config.c. Decouples the reusable PAL target
from one app's board."

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Rename the avoidance_car callback source

**Files:**
- Rename: `wink-micro-os/samples/avoidance_car/app_main.c` → `wink-micro-os/samples/avoidance_car/app_callbacks.c`
- Modify: `wink-micro-os/samples/avoidance_car/CMakeLists.txt`
- Modify: `esp32_firmware/main/CMakeLists.txt`

**Interfaces:** public function `wink_app_get_callbacks()` is unchanged (file rename only).

- [ ] **Step 1: Rename the file**

```bash
git mv wink-micro-os/samples/avoidance_car/app_main.c wink-micro-os/samples/avoidance_car/app_callbacks.c
```

- [ ] **Step 2: Update the host sample CMake**

In `wink-micro-os/samples/avoidance_car/CMakeLists.txt`, change the `APP_SOURCES` entry:

```cmake
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)
```

- [ ] **Step 3: Update the ESP32 firmware CMake**

In `esp32_firmware/main/CMakeLists.txt`, change the app source line:

```cmake
        "../../wink-micro-os/samples/avoidance_car/app_callbacks.c"
```

- [ ] **Step 4: Build both targets**

Run: `python wink-tools/wink.py test` → all pass.
Run: `idf.py -C esp32_firmware build` → zero errors/warnings.

- [ ] **Step 5: Commit**

```bash
git add wink-micro-os/samples/avoidance_car/app_callbacks.c \
        wink-micro-os/samples/avoidance_car/CMakeLists.txt \
        esp32_firmware/main/CMakeLists.txt
git commit -m "refactor(sample): rename avoidance_car app_main.c to app_callbacks.c

Disambiguates the sample callback source from the IDF app_main.c entry point.
wink_app_get_callbacks() symbol unchanged."

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: sdkconfig split + docs + I2C roadmap

**Files:**
- Modify: `esp32_firmware/sdkconfig.defaults`
- Create: `esp32_firmware/sdkconfig.defaults.dev`
- Modify: `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`
- Modify: `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`

- [ ] **Step 1: Verify Kconfig symbols exist in v5.1.3 before writing them**

Run (in the IDF environment): `idf.py -C esp32_firmware menuconfig` (or grep `components/*/Kconfig*`) to confirm each symbol below exists in v5.1.3. If any is absent, record the closest equivalent in the commit message and use that instead — do not guess.

Symbols to confirm: `CONFIG_FREERTOS_CHECK_STACKOVERFLOW`, `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0`, `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1`, `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`, `CONFIG_HEAP_POISONING_LIGHT`, `CONFIG_HEAP_POISONING_COMPREHENSIVE`.

- [ ] **Step 2: Add production-safe defaults to `sdkconfig.defaults`**

Append to `esp32_firmware/sdkconfig.defaults` (these are cheap enough for the production baseline):

```ini
# Runtime safety (cheap; safe for production baseline)
CONFIG_FREERTOS_CHECK_STACKOVERFLOW=2
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=y
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
```

(`CONFIG_ESP_TASK_WDT_INIT=y` and `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5` are already present.)

- [ ] **Step 3: Create dev/debug-only defaults**

Create `esp32_firmware/sdkconfig.defaults.dev`:

```ini
# Development/debug-only safety defaults. NOT for production.
# Load with: SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.dev" idf.py build
CONFIG_HEAP_POISONING_LIGHT=y
```

- [ ] **Step 4: Fix the Wave B report (paths, file count, scope, governance IDs, deprecations)**

In `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`:
- Fix shortened paths by adding `wink-micro-os/` prefixes where needed (e.g. `pal/include/wink_status.h` → `wink-micro-os/pal/include/wink_status.h`).
- §8 says "9 个文件" but lists 8 rows — change the count to 8 (or add the actual ninth file).
- Add a Wave B out-of-scope list: ADC/DAC, SPI, NVS, Wi-Fi/BLE, OTA, Flash partition planning.
- Assign fresh governance IDs to PWM routing and LEDC timer conflict (e.g. P1-6, P2-7) instead of reusing P1-3/P2-2; update the §5 matrix.
- Mark ESP32 `pal_gpio_pulse_in` busy-wait as a deprecated/WCET-violating fallback; RMT is the acceptance path.
- Add I2C migration note: `i2c_master_write_read_device` is legacy in v5.1.3; **Wave C (IDF v5.2+) must migrate** to `i2c_master_bus_add_device` + `i2c_master_transmit`/`i2c_master_receive`; the legacy API is removed in IDF v6.

- [ ] **Step 5: Update the PAL resource-governance spec**

In `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md` §4.1 resource table, add a row documenting LEDC timer-slot governance via `pal_pwm_router` (`s_timer_slots[PAL_PWM_TIMERS]` ref counts; non-concurrent PAL-init contract).

- [ ] **Step 6: Build ESP32 firmware after sdkconfig changes**

Run: `idf.py -C esp32_firmware reconfigure` then `idf.py -C esp32_firmware build`
Expected: zero errors/warnings; confirm the new symbols took effect in `build/config/sdkconfig.h`.

- [ ] **Step 7: Commit**

```bash
git add esp32_firmware/sdkconfig.defaults esp32_firmware/sdkconfig.defaults.dev \
        docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md \
        docs/design/02-wink-micro-os/02-pal-platform-abstraction.md
git commit -m "docs(esp32): sdkconfig safety split + Wave B report fixes + I2C roadmap

Move heap poisoning to a dev-only defaults file (production baseline keeps
only cheap safety: stack canary, idle-task WDT both cores, event stack).
Fix Wave B report paths/file-count/scope/governance IDs; deprecate busy-wait
pulse_in; record I2C legacy->new-API migration for Wave C. Document LEDC
timer-slot governance in PAL spec."

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: Final verification gate

- [ ] **Step 1: Host suite green**

Run: `python wink-tools/wink.py test`
Expected: `[PASS] All tests passed`, including `test_pal_pwm_router`, `test_host_pal`, `test_dal_servo`.

- [ ] **Step 2: ESP32 build green**

Run: `idf.py -C esp32_firmware build`
Expected: zero errors, zero warnings.

- [ ] **Step 3: Search checks**

All of these must return no matches:
- `pwsh -c "Select-String -Path wink-micro-os/targets/esp32/*.c -Pattern 'CRITICAL\(NULL\)'"`
- `pwsh -c "Select-String -Path wink-micro-os/targets/esp32/*.c -Pattern 'rmt_receive\([^,]+,\s*NULL'"`
- `pwsh -c "Select-String -Path wink-micro-os/targets/esp32/pal_hal_esp32.c -Pattern 'pwm_gpio_map'"` (must be gone — replaced by `pal_pwm_pin_map`)
- No `(uint32_t)arg` (without `uintptr_t`) in `gpio_isr_wrapper`.

- [ ] **Step 4: Hardware acceptance checklist** (oscilloscope method per Task 3 `@verified` headers)

| 验收项 | 通过标准 | 测量方法 |
|---|---|---|
| **Task 1 临界区 (new)** | 跨核并发 claim/release 60s 无 panic/断言 | CPU0/CPU1 各起任务循环 claim/release 不同 (type,id)，观察无复位 |
| **Task 2 RMT 复位 (new)** | 超时后立即复测返回有效距离 | 拔 ECHO 触发一次 measure 超时 → 接回 → 立即 measure 得合理脉宽 |
| RMT ISR latency | `< 10μs` | TRIG 翻转 + RMT ISR 翻转另一 GPIO，示波器测上升沿间隔 |
| 100 次测距最大偏差 | `< 15μs` | 连续 100 次 `pal_rmt_ultrasonic_measure`，统计 max−min |
| HC-SR04 距离精度 | 误差 `< 2cm`（1m 内） | 已知距离目标实测对比 |
| Watchdog reset path | 触发后系统复位 | 人为阻塞 task 超时，观察复位原因寄存器 |
| PWM mixed-frequency smoke | 50Hz + 1000Hz 同时运行无频率串扰 | 示波器同时监测两通道频率（验证 router 异频隔离） |

- [ ] **Step 5: Commit the verification record** (optional — record results in the Wave B report or a validation log)

---

## Self-Review

**1. Spec coverage (each v1 review issue → task):**
- Task 1 NULL spinlock → Task 1. ✓
- Task 2 invalid RMT cancel → Task 2. ✓
- GPIO ISR unwrap + headers → Task 3. ✓
- LEDC timer sharing (ISSUE-001) + host-test feasibility (the v1 Blocker) → Task 4 (shared router). ✓
- dual-lock over-engineering / reserve-commit interleaving → Task 4 (no lock, non-concurrent contract). ✓
- owner-string consistency on release → Task 4 (`PWM_OWNER` constant). ✓
- PWM pin routing to device tree → Task 5 (`board_config.c`). ✓
- weak/strong symbol resolution → Task 5 (weak default + strong override). ✓
- wasm channel-validation gap → Task 4 Step D1. ✓
- `PAL_PWM_CHANNELS` rename → Task 4 (introduced + propagated). ✓
- app_main rename → Task 6. ✓
- heap-poisoning-in-production-defaults → Task 7 (split dev defaults). ✓
- missing P0 runtime verification → Task 8 (two new smokes). ✓
- target-chip ambiguity → confirmed `esp32` in header; Task 7 records CPU1 applicability. ✓
- RESOURCE_EXHAUSTED coverage → Task 4 Step A3 `test_router_exhausted_after_four_distinct_freqs`. ✓
- RMT single-instance concurrency limit → out of MVP scope; noted as known limitation, not addressed here. (Explicit non-goal.)

**2. Placeholder scan:** No "TBD/TODO/handle edge cases". Every code step shows full code; every command shows expected output. Kconfig symbols are gated behind a verify step (Step 1 of Task 7) rather than guessed.

**3. Type/signature consistency:**
- `pal_pwm_deinit(uint8_t)` — declared in `pal_hal.h` (Task 4 E2), defined in host (C1), wasm (D1), esp32 (E3). ✓
- `pal_pwm_router_acquire(channel, freq_hz, &out_timer_num)` signature identical across header (A2), impl (B1), and all three shells (C1/D1/E3). ✓
- `PWM_OWNER` constant used identically in esp32 claim and release (E3). ✓
- `PAL_PWM_CHANNELS` defined once in `pal_hal.h` (Task 4 A1); local `PWM_CHANNELS` deleted from host (C1) and esp32 (E3); wasm never had one. ✓
- `pal_pwm_pin_map` declared `pal_hal.h` (Task 5 Step 1), weak-defined in esp32 (Step 2), strong-defined in `board_config.c` (Step 3), consumed in esp32 shell (Step 4). ✓

**4. Buildability invariant:** each task ends with both `python wink-tools/wink.py test` (host) and `idf.py build` (esp32) green. Task 4 is sequenced so the router exists and is host-tested (Step B2) before any shell depends on it; shells are wired one target at a time with a test/build gate after each group. ✓
