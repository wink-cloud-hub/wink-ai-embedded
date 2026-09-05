# Wave B ESP32 Port Follow-up Action Plan

> **For agentic workers:** Use `embedded-best-practice` before editing ESP32 PAL/HAL code. Execute in order; each task must leave the repo buildable.

**Goal:** Combine the Wave B compilation report and external review into an executable plan that removes new P0 crash risks, fixes ESP32 PAL correctness gaps, and updates design truth before hardware acceptance.

**Architecture:** Fix low-level ESP32 runtime hazards first (`pal_resource_esp32.c`, RMT timeout path), then fix PWM correctness (LEDC timer allocation plus device-tree routing), then clean up naming, sdkconfig, and documentation drift.

**Tech Stack:** C99, ESP-IDF v5.1.3, FreeRTOS SMP, ESP32 RMT/LEDC/I2C drivers, CMake, Unity host tests, `idf.py build`.

## Source Inputs

- External review: `C:/Users/77174/.gemini/antigravity-ide/brain/f6b9efd6-665a-484e-88f1-65a54911f640/review_feedback.md`
- Original document: `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`
- Main code inspected: `wink-micro-os/targets/esp32/pal_resource_esp32.c`, `wink-micro-os/targets/esp32/pal_hal_esp32.c`, `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`, `wink-micro-os/samples/avoidance_car/device_tree.*`, `esp32_firmware/main/CMakeLists.txt`, `esp32_firmware/sdkconfig.defaults`.

## Global Constraints

- `WINK_OK == 0`; errors are negative; check with `wink_status_is_error(status)` or `status < 0`.
- Preserve static dispatch: no device `ops`, vtables, or `container_of` for DAL/PAL device abstraction.
- No heap allocation in PAL/DAL realtime paths; use fixed static tables.
- Keep ESP32-only logic under `#if defined(ESP_PLATFORM)` in target-layer files.
- Do not widen `#ifdef SIMULATION`.
- Docs must use repo-root-relative paths with `wink-micro-os/` prefixes where applicable.
- Every code task must run host tests and, when ESP-IDF is available, `idf.py -C esp32_firmware build`.
- **每个 Task 完成后独立 commit**，便于 `git revert` 回滚到任意已验证状态。若后续 Task 引入回归，可精确回退而不影响先前修复。
- **文档编辑归属**：Task 2 仅在 Wave B 文档中添加 RMT checklist row；Task 7 负责路径修复和其余文档更新。两者修改同一文件时按顺序执行（Task 2 先，Task 7 后），避免冲突。

## Priority Roadmap

| Order | Priority | Task | Outcome |
|---:|---|---|---|
| 1 | P0 | Replace `taskENTER_CRITICAL(NULL)` | Avoid ESP-IDF v5.x SMP null spinlock crash |
| 2 | P0 | Replace RMT timeout cancel via NULL receive | Avoid RMT assertion/undefined behavior |
| 3 | P1 | Fix GPIO ISR `uintptr_t` unwrap and `@verified` headers | Remove portability and doc-truth drift |
| 4 | P0/P1 | Implement LEDC timer slots with ref counts | Prevent PWM frequency corruption |
| 5 | P1 | Move PWM GPIO routing to device tree | Remove board routing hardcode from ESP32 PAL |
| 6 | P2 | Rename sample callback source | Remove dual `app_main.c` confusion |
| 7 | P1/P2 | Update docs, sdkconfig, I2C roadmap | Keep design and code aligned |
| 8 | Gate | Run final verification | Ready for hardware acceptance |

---

## Task 1: Replace ESP32 resource critical-section NULL spinlock

**Files:** `wink-micro-os/targets/esp32/pal_resource_esp32.c`

- [ ] Keep `static uint32_t s_count = 0;` non-volatile. 并发安全由临界区提供；`volatile` 不能替代互斥，也不提供跨核原子性。仅 `pal_resource_reset` / `pal_resource_claim` / `pal_resource_release` 可读写该变量。
- [ ] Add spinlock declaration **before all function definitions**（确保 `portMUX_INITIALIZER_UNLOCKED` 在任何 `taskENTER_CRITICAL` 调用前完成初始化）:

```c
#if defined(ESP_PLATFORM)
static portMUX_TYPE s_resource_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
```

- [ ] Replace every `taskENTER_CRITICAL(NULL)` / `taskEXIT_CRITICAL(NULL)` with `taskENTER_CRITICAL(&s_resource_mux)` / `taskEXIT_CRITICAL(&s_resource_mux)`.
- [ ] Verify no NULL critical-section calls remain.
- [ ] Run `python wink-tools/wink.py test` and `idf.py -C esp32_firmware build`.

## Task 2: Replace invalid RMT timeout cancellation

**Files:** `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`, `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`

- [ ] Replace timeout branch in `pal_rmt_ultrasonic_measure`:

```c
if (ok != pdPASS) {
    /* 超时恢复：disable → enable 复位 RMT 状态机。
     * 信号量残留处理：本次 timeout 后 s_rx_done_sem 可能仍有残留 Give，
     * 但下次 measure 入口的 xSemaphoreTake(s_rx_done_sem, 0) 会清空，
     * 无需在此额外 Take。注释说明此约定即可。 */
    esp_err_t stop_err = rmt_disable(s_rmt_rx_chan);
    esp_err_t start_err = rmt_enable(s_rmt_rx_chan);
    if (stop_err != ESP_OK || start_err != ESP_OK) {
        return WINK_ERR_HARDWARE;
    }
    return WINK_ERR_TIMEOUT;
}
```

- [ ] Add RMT checklist row to the Wave B document: timeout reset path uses `rmt_disable()` plus `rmt_enable()` at `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`.
- [ ] Verify no `rmt_receive` call with a NULL buffer remains.
- [ ] Run `idf.py -C esp32_firmware build`.

## Task 3: Fix GPIO ISR pointer round-trip and verification headers

**Files:** `wink-micro-os/targets/esp32/pal_hal_esp32.c`, `wink-micro-os/targets/esp32/pal_hal_esp32_rmt.c`

- [ ] In `gpio_isr_wrapper`, replace `uint32_t pin = (uint32_t)arg;` with `uint32_t pin = (uint32_t)(uintptr_t)arg;`。
- [ ] 确认调用侧（`pal_gpio_enable_interrupt` 中 `gpio_isr_handler_add` 的第四个参数）已使用 `(void *)(uintptr_t)pin`（`pal_hal_esp32.c:142`），无需修改——仅 ISR 侧解包需对称修复。
- [ ] Update both file headers from `@verified: NO` to `@verified: COMPILED`.
- [ ] Required wording: `@verified: COMPILED -- ESP-IDF v5.1.3 idf.py build compile verification passed. Hardware behavior remains pending Wave B board validation.`
- [ ] For RMT, keep caveat: oscilloscope validation still required for ISR latency `<10us` and HC-SR04 accuracy.
- [ ] Run host tests and ESP32 build.

## Task 4: Implement LEDC timer allocation with reference counts

**Files:** `wink-micro-os/pal/include/pal_hal.h`, `wink-micro-os/targets/esp32/pal_hal_esp32.c`, `wink-micro-os/targets/host/pal_hal_host.c`, `wink-micro-os/targets/wasm/pal_hal_wasm.c`, `wink-micro-os/test/test_pal_contract.c`, `wink-micro-os/test/test_host_pal.c`

**Design:** add `void pal_pwm_deinit(uint8_t channel)`. ESP32 has four LEDC low-speed timers. `pal_pwm_init(channel, freq_hz)` reuses an existing same-frequency timer or allocates a free timer; no free timer returns `WINK_ERR_RESOURCE_EXHAUSTED`. Re-initializing an already initialized channel is allowed only with the same frequency and is a no-op returning `WINK_OK`; re-initializing the same channel with a different frequency returns `WINK_ERR_BUSY` and must not alter timer ref counts. Frequency `0` returns `WINK_ERR_INVALID_ARG`. `pal_pwm_deinit` sets duty zero, releases channel claim, decrements timer ref count, and clears slot at zero.

**并发安全约束：** `pwm_timer_acquire` / `pwm_timer_release` 操作 `s_timer_slots[]`、`s_pwm_timer_by_channel[]`、`s_pwm_channel_freq_hz[]` 和 `s_pwm_initialized[]` 静态表。ESP32 HAL 不得复用 `pal_resource_esp32.c` 中的 `static s_resource_mux`（跨翻译单元不可见，也会制造锁边界耦合）；应在 `pal_hal_esp32.c` 内新增独立 `static portMUX_TYPE s_pwm_mux = portMUX_INITIALIZER_UNLOCKED;`，仅保护 PWM 私有状态表。不要在持有 `s_pwm_mux` 时调用 `ledc_timer_config` / `ledc_channel_config` / `pal_resource_claim` / `pal_resource_release`，避免驱动调用阻塞或锁顺序反转；helper 只做表状态变更并返回要配置/释放的 timer 信息。

**`pal_pwm_deinit` 错误码语义：** `void` 返回——deinit 操作（清 duty、释放 channel claim、递减 timer ref count）不应失败。若 channel 未初始化，deinit 为 no-op。

- [ ] Add `void pal_pwm_deinit(uint8_t channel);` after `pal_pwm_set_duty` in `pal_hal.h`，并补充 `@note`：deinit 为 void 返回，未初始化 channel 调用为 no-op。
- [ ] Add host implementation that releases `PAL_RESOURCE_PWM_CHANNEL` for valid channels; wasm no-ops.
- [ ] In ESP32 HAL, replace PWM state with a `ledc_timer_slot_t { uint32_t freq_hz; uint8_t ref_count; }`, `s_pwm_timer_by_channel[PAL_PWM_CHANNELS]`, `s_pwm_channel_freq_hz[PAL_PWM_CHANNELS]`, `s_pwm_initialized[PAL_PWM_CHANNELS]`, and `s_timer_slots[4]`.
- [ ] Add helpers `pwm_timer_acquire(freq_hz, &timer_num)` and `pwm_timer_release(timer_num)`; both protect only PWM private state with `taskENTER_CRITICAL(&s_pwm_mux)` / `taskEXIT_CRITICAL(&s_pwm_mux)`. Add rollback helper for failed hardware config so ref counts cannot leak.
- [ ] In `pal_pwm_init`, reject `freq_hz == 0`; if channel is already initialized with the same frequency return `WINK_OK`; if initialized with a different frequency return `WINK_ERR_BUSY`. For a new channel, acquire timer before `ledc_timer_config`; use `timer_num` for `.timer_num` and `.timer_sel`.
- [ ] On IDF error, release both timer and PWM channel claim before returning `WINK_ERR_HARDWARE`.
- [ ] Add ESP32 `pal_pwm_deinit` as described above.
- [ ] Add tests:
  - `test_pal_contract.c`: 调用 `pal_pwm_deinit(0)` 确认不崩溃（void 返回）。
  - `test_host_pal.c` 场景 1: init(channel=3, 1000Hz) → set_duty(50%) → deinit(3) → re-init(3, 2000Hz)，两次 init 均返回 `WINK_OK`。
  - `test_host_pal.c` 场景 2: init(channel=0, 50Hz) → init(channel=1, 50Hz)（同频复用 timer）→ init(channel=0, 50Hz)（同 channel 同频幂等）→ init(channel=0, 1000Hz) 期望 `WINK_ERR_BUSY` → deinit(0) → deinit(1)，确认 timer ref count 正确归零。
  - `test_host_pal.c` 场景 3: init(invalid_channel=PWM_CHANNELS) → 期望 `WINK_ERR_INVALID_ARG`。
- [ ] Run host tests and ESP32 build.

## Task 5: Move PWM GPIO routing into device tree

**Files:** `wink-micro-os/pal/include/pal_hal.h`, `wink-micro-os/targets/esp32/pal_hal_esp32.c`, `wink-micro-os/samples/avoidance_car/device_tree.h`, `wink-micro-os/samples/avoidance_car/device_tree.c`, `esp32_firmware/main/CMakeLists.txt`, `docs/design/07-platform-governance/01-device-model-registry.md`

**Routing boundary:** ESP32 target code must not include the sample-specific `wink-micro-os/samples/avoidance_car/device_tree.h`, otherwise the reusable PAL target becomes coupled to one app. The PAL contract will expose a weak/default board routing symbol in `pal_hal.h`; the physical firmware links a generated/sample `device_tree.c` that provides the strong definition. Host/wasm targets remain independent of physical GPIO routing.

**`PAL_PWM_CHANNELS` 与 `PWM_CHANNELS` 关系：** `PAL_PWM_CHANNELS` 是**平台无关契约常量**，定义在 `pal_hal.h`，表示 PAL 接口层支持的最大逻辑 PWM 通道数。各 target 的本地 `PWM_CHANNELS` 应统一替换为 `PAL_PWM_CHANNELS`（ESP32 HAL 当前 `#define PWM_CHANNELS 8`，host/wasm target 也应对齐）。`pal_hal.h` 中 `pal_pwm_init` 的 `@note` 应引用 `PAL_PWM_CHANNELS` 作为通道上限。

- [ ] 在 `pal_hal.h` 中定义 `#define PAL_PWM_CHANNELS 8`（平台无关契约），并更新 `pal_pwm_init` 的 `@note` 引用此常量。
- [ ] ESP32 HAL：删除 `#define PWM_CHANNELS 8`，改用 `PAL_PWM_CHANNELS`（需 `#include "pal_hal.h"`，已有）。
- [ ] host/wasm target：确认 `PWM_CHANNELS` 对齐为 `PAL_PWM_CHANNELS` 或直接引用。
- [ ] Add `extern const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS];` to `pal_hal.h` as the platform routing contract. Do not include sample `device_tree.h` from `pal_hal_esp32.c`.
- [ ] Add the generated/sample strong definition `const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};` to `device_tree.c`; keep `device_tree.h` focused on app device instances unless codegen later needs to expose routing to app code.
- [ ] In ESP32 HAL, remove local `pwm_gpio_map` and use `pal_pwm_pin_map[channel]` declared by `pal_hal.h`.
- [ ] Ensure `esp32_firmware/main/CMakeLists.txt` links the generated/sample `device_tree.c` so the strong `pal_pwm_pin_map` definition is present in physical firmware. Host/wasm builds must not need this symbol unless they link ESP32 HAL.
- [ ] Update device model registry §7: generated `device_tree.c` must include PWM channel-to-GPIO routing for physical targets。
- [ ] Run host tests and ESP32 build。

## Task 6: Rename avoidance car callback source

**Files:** rename `wink-micro-os/samples/avoidance_car/app_main.c` to `wink-micro-os/samples/avoidance_car/app_callbacks.c`; modify `wink-micro-os/samples/avoidance_car/CMakeLists.txt`, `esp32_firmware/main/CMakeLists.txt`

- [ ] Rename file.
- [ ] Update sample CMake `APP_SOURCES`.
- [ ] Update ESP32 firmware CMake source list.
- [ ] Keep public function `wink_app_get_callbacks()` unchanged.
- [ ] Run host tests and ESP32 build.

## Task 7: Update docs, sdkconfig defaults, and roadmap items

**Files:** `docs/implementation-plans/core/2026-06-26-wave-b-esp32-port-compilation-review.md`, `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`, `docs/design/07-platform-governance/01-device-model-registry.md`, `esp32_firmware/sdkconfig.defaults`

- [ ] Fix shortened paths in the original Wave B document by adding `wink-micro-os/` where needed。
- [ ] Fix the "9 files" mismatch: list the actual ninth file or change the count to 8。
- [ ] Add Wave B out-of-scope list: ADC/DAC, SPI, NVS, Wi-Fi/BLE, OTA, Flash partition planning。
- [ ] Record governance ID cleanup: assign new IDs for PWM routing and LEDC timer conflict instead of reusing P1-3/P2-2。
- [ ] Mark ESP32 `pal_gpio_pulse_in` busy-wait as deprecated/WCET-violating fallback; RMT is the acceptance path。
- [ ] Add I2C legacy API migration note: `i2c_master_write_read_device` 在 IDF v5.1.3 仍可用但已标记 legacy；**Wave C（IDF v5.2+）必须迁移到 `i2c_master_bus_add_device` 族**（`i2c_master_bus_add_device` + `i2c_master_transmit` / `i2c_master_receive`），IDF v6 将移除旧 API。迁移边界写入 roadmap。
- [ ] Add sdkconfig development safety defaults（先查 ESP-IDF v5.1.3 Kconfig 确认符号可用性）：

| 配置项 | 目的 | IDF v5.1.3 符号 | 默认值 |
|---|---|---|---|
| Stack overflow check | 栈溢出检测 | `CONFIG_FREERTOS_CHECK_STACKOVERFLOW` | `2`（canary） |
| Task watchdog timeout | 任务看门狗超时 | `CONFIG_ESP_TASK_WDT_TIMEOUT_S` | `5` |
| CPU0 idle WDT | 空闲任务看门狗 | `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` | `y` |
| CPU1 idle WDT | SMP 双核空闲任务看门狗 | `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` | `y`（若目标为单核或符号不存在则记录原因） |
| System event task stack | 系统事件栈大小 | `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` | `4096` |
| Heap poisoning | 开发阶段堆破坏检测 | 查 v5.1.3 Kconfig 后选择 `CONFIG_HEAP_POISONING_LIGHT` / `CONFIG_HEAP_POISONING_COMPREHENSIVE` / 等价符号 | 仅 dev/debug 默认启用；不得无条件进入量产默认配置 |

如果某个符号在 v5.1.3 不可用，记录最近的等价符号，不要猜测。`sdkconfig.defaults` 只放开发期安全且不会显著改变量产行为的默认项；heap poisoning 若会影响性能/内存，仅写入单独 dev defaults 文件或在文档中列为开发构建建议。

- [ ] Update `02-pal-platform-abstraction.md` §4.1 资源占用治理表格：补充 LEDC timer slot 治理说明（引用 Task 4 的 `s_timer_slots` 设计）。
- [ ] Run ESP32 build after sdkconfig changes。

## Task 8: Final verification gate

- [ ] `python wink-tools/wink.py test` passes.
- [ ] `idf.py -C esp32_firmware build` passes with zero errors/warnings.
- [ ] Search checks pass: no `taskENTER_CRITICAL(NULL)`, no `taskEXIT_CRITICAL(NULL)`, no `rmt_receive(... NULL ...)`, no shortened paths in edited Wave B doc.
- [ ] Hardware checklist is ready（测量方法统一见 Task 3 `@verified` 头部的示波器方法）：

| 验收项 | 通过标准 | 测量方法 |
|---|---|---|
| RMT ISR latency | `< 10μs` | TRIG 引脚翻转 + RMT ISR 翻转另一 GPIO，示波器测量上升沿间隔 |
| 100 次测距最大偏差 | `< 15μs` | 连续 100 次 `pal_rmt_ultrasonic_measure`，统计 max - min |
| HC-SR04 距离精度 | 误差 `< 2cm`（1m 内） | 已知距离目标实测对比 |
| Watchdog reset path | 触发后系统复位 | 人为阻塞 task 超时，观察复位原因寄存器 |
| PWM mixed-frequency smoke | 50Hz + 1000Hz 同时运行无频率串扰 | 示波器同时监测两通道频率 |

## Safety Review Scope for Execution

- Risk level: High.
- Full checklist required because tasks touch ISR, FreeRTOS critical sections, RMT, PWM timers, and hardware routing.
- Key findings to re-check after implementation: no NULL spinlock, no invalid RMT cancel, timer refs cannot leak after deinit, no PAL hardcoded board route, host and ESP32 builds both pass.
