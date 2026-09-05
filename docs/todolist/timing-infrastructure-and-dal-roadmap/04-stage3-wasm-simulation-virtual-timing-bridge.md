# 阶段 3 计划：在 UniSim 3.0 桥接上增量接入 Stage 0/2 PAL 通道（ADR-0003）

| 元数据项 | 说明 |
| :--- | :--- |
| **阶段编号** | STAGE-3-WASM-BRIDGE |
| **所属模块** | `wink-micro-os/targets/wasm/`、`embedded-frontend` (UniSim / Wokwi Bridge) |
| **解决时序类别** | 全时序类别的 WebAssembly 沙箱同源仿真支持 |
| **依赖 ADR** | [ADR-0002 双 Target 编译](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0003 仿真可信度边界](../../decisions/unisim/0003-simulation-fidelity-boundary.md)、[ADR-0012 合约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0047 FOC ISR 分层与 pal_hwtimer](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) |
| **前置阶段** | [Stage 0](01-stage0-pal-hardware-acceleration-engines.md)、[Stage 1](02-stage1-timing-safety-and-critical-section.md)、[Stage 2](03-stage2-fast-slow-loop-and-hwtimer.md) |
| **基线** | **UniSim 3.0 Active（2026-08-02）**；虚拟时钟 2026-06-29 已 Landed；DAL 零 `#ifdef SIMULATION` 已达成；`wasm_bridge.h` 387 行，ABI hash `0x50333036`；本阶段只做增量，不重建底座 |
| **状态** | **Ready for Implementation** |

---

## 1. 阶段目标与边界

### 1.1 已交付（不重复立项）

| 能力 | 落地时间 / 证据 |
|---|---|
| UniSim 3.0 四层结构（overview / mechanisms / axes / assurance） | 2026-08-02 Active；`docs/design/04-wasm-simulation-3.0/00-README.md` |
| 64 位虚拟时钟 `s_virtual_us` + 唯一 Gate + Bigint 跨语言契约 | 2026-06-29；`osal/wasm/pal_osal_wasm.c:33,40,47`；`wink_sim_js.js:20,46` |
| DAL 零 `#ifdef SIMULATION`（旁路下沉至 PAL Wasm + Plugin） | ADR-0003 合规日志 2026-08-02 |
| `wasm_bridge.h` Axis A–F 组织 + ABI hash 纪律 | `targets/wasm/wasm_bridge.h`（387 行） |
| UART async RX 推流（`pal_wasm_push_uart_rx_byte/error`、`pal_wasm_get_uart_rx_available`） | `wasm_bridge.h:332`、`targets/wasm/pal_wasm_ch2_uart.c:42` |
| ADC 归一化 + RC + 噪声 | `js_pal_adc_read_norm`、`targets/wasm/pal_wasm_ch3_adc.c` |
| WS2812 像素透传 | `js_pal_ws2812_write`、`pal_wasm_ch4_ws2812.c` |
| SPI JS 侧导入声明（Phase 4 T5 stub） | `wasm_bridge.h:97 js_pal_spi_transfer` |
| 超声波脉冲环回（CH1 pin-event） | `targets/wasm/pal_wasm_waveform.c`（进行中，Partial） |

### 1.2 本阶段真正要做的事

1. **补齐 C PAL 层 SPI 通道**：把 `js_pal_spi_transfer` 接到新增的 `pal_spi`（Stage 0 T0.3），新增 `targets/wasm/pal_wasm_ch2_spi.c`。
2. **CH4 异步完成拉模型契约**：禁止 DMA 同步回调（Axis E 重入红线）；统一 `pal_wasm_schedule_complete_us(delta_us, cb, arg)`，用虚拟时钟给 SPI/RMT/UART 建硬件延迟模型。
3. **Stage 2 新模块软步进通道**：`pal_hwtimer` 由虚拟时钟驱动；`pal_mcpwm`/`pal_pcnt` 维护状态 + 软边沿时间戳，不做电气级仿真。
4. **ABI 与 lint 同步**：每新增跨边界符号都 bump `PAL_WASM_ABI_HASH` 并同步 TS `WasmImports/WasmExports`；新头加 ADR-0043 allowlist。
5. **修文档漂移**：`01-architecture.md:117` 引用不存在的 `pal_hal_wasm.c`；`08-channel-routing.md:102` 把 SPI 标 "Landed" 但无 C PAL 实现；`05-memory-and-faults.md:35` ADR-0045 固定堆 triple flags 未入 CMake。

### 1.3 明确不做

- ❌ 不新建虚拟时钟协议（已 Landed）。
- ❌ 不在 DAL 加 `#ifdef SIMULATION`（ADR-0003 已禁）。
- ❌ 不新增 `js_pal_rmt_*` 符号：RMT 捕获是 CH1 pin-event 路径的职责，未实现的 variant 按 ADR-0012 返回 `WINK_ERR_UNSUPPORTED`。
- ❌ 不做电气级 MCPWM dead time / brake 仿真（host/wasm 只维护语义状态，真机量化由 Nightly Gate 负责）。
- ❌ 不为已有符号 bump ABI hash。

---

## 2. 详细任务

### T3.1 `pal_wasm_ch2_spi.c`：C PAL SPI 转发到已有 JS 导入

**现状**：`js_pal_spi_transfer` 已在 `wasm_bridge.h:97` 声明，但无 C PAL 调用者；Stage 0 T0.3 的 `pal_spi_transfer_dma` 在 wasm target 没有实现。

**交付**：新建 `wink-micro-os/targets/wasm/pal_wasm_ch2_spi.c`

```c
/* SPDX-License-Identifier: Apache-2.0 */
#include "hal/pal_spi.h"
#include "wasm_bridge.h"
#include "pal_wasm_completion.h"   /* T3.2 新增 */

wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx, uint8_t *rx,
                                   size_t len,
                                   pal_spi_dma_callback_t cb, void *arg) {
    if (dev == NULL || (tx == NULL && rx == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 1. 同步把数据交给 JS（Axis C CH2），JS 立即拷贝到前端缓冲。
     *    不在此路径触发回调——Axis E 禁止 C→JS→C 重入。 */
    js_pal_spi_transfer(dev->port, (uint8_t)dev->cs_pin,
                        (const uint8_t *)tx, rx, len);

    /* 2. 按波特率建模硬件完成延迟：len 字节 × 8 bit / baud，向上取整到 µs。
     *    若 cb 非空，入完成队列，由 Phase 0 / unlock 排空。
     *    队列满（32 项）必须把 WINK_ERR_NO_RESOURCES 向上返回——
     *    禁止静默丢通知（评审 P6）。DAL 三层状态机的 request 路径把该
     *    错误映射为 WINK_ERR_BUSY，App 在下一 10 ms tick 重试。 */
    if (cb != NULL) {
        uint32_t delta_us = (uint32_t)((len * 8 * 1000000ULL + dev->clock_hz - 1)
                                       / dev->clock_hz);
        wink_status_t st = pal_wasm_schedule_complete_us(
            delta_us, (pal_wasm_completion_cb_t)cb, arg);
        if (st != WINK_OK) {
            /* 数据已交到 JS，但完成通知未入队——回滚策略由 DAL 决定：
             * 1) OLED 等纯 TX 设备：DAL poll 直接返回 OK（数据确实发出）；
             * 2) SD-Card 等需读回设备：DAL 返回 WINK_ERR_BUSY 让 App 重试，
             *    禁止假装成功。具体映射在 DAL 层（Stage 4）。 */
            return st;
        }
    }
    return WINK_OK;
}

wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx, uint8_t *rx, size_t len) {
    /* 轮询路径仅 WINK_BLOCKING 使用：同步调用 + 立即推进虚拟时钟到完成。 */
    wink_status_t st = pal_spi_transfer_dma(dev, tx, rx, len, NULL, NULL);
    if (st != WINK_OK) return st;
    uint32_t delta_us = (uint32_t)((len * 8 * 1000000ULL + dev->clock_hz - 1)
                                   / dev->clock_hz);
    pal_wasm_advance_virtual_clock(delta_us);  /* 已存在；HEADLESS 合法调用者之一 */
    return WINK_OK;
}
```

**约束**：
- `js_pal_spi_transfer` 签名已存在，**不 bump ABI hash**；若需要扩展（如加 mode/clock_hz），新增 `js_pal_spi_transfer_v2` 并 bump。
- 头文件在 Stage 0 T0.3 已加 Doxygen 注释 "may run in ISR context"；wasm 软中断上下文对齐该约束。
- **Host 测试对齐（评审 S4）**：host target 的 `pal_spi_transfer_dma` 实现**禁止** naive TX→RX loopback。Stage 0 T0.3 已交付 `stub_spi_inject_rx` / `stub_spi_get_last_tx` / `stub_spi_force_failure` 三个 hook；wasm target 的 JS 侧 `js_pal_spi_transfer` 同样应支持前端 plugin 预设响应序列（Wokwi SPI plugin 已具备该能力），让 `test_dal_sdcard` 等协议测试能在 wasm 与 host 上跑到真实协议分支。

**验收**：
- [ ] OLED 刷屏 app 在 wasm 上 1024 B @ 40 MHz 不阻塞主循环（callback 在下一 Phase 0 触发）；
- [ ] Wasm Headless 契约测试：spi transfer 序列与真机字节一致；
- [ ] TS `WasmImports/WasmExports` 类型已含 `js_pal_spi_transfer`（之前 stub 已存在，仅补测试）；
- [ ] 完成队列溢出注入测试：手动占满 32 项后再发起 SPI，`pal_spi_transfer_dma` 返回 `WINK_ERR_NO_RESOURCES`，DAL 层 `request` 返回 `WINK_ERR_BUSY`，App 下一 tick 可重试成功。

### T3.2 CH4 异步完成拉模型（统一调度器）

**问题**：旧文档示例
```c
js_pal_spi_flush_frame(...);
if (callback) callback(arg, WINK_OK);   /* ❌ 同步重入 */
```
违反 Axis E（`wasm_bridge.h:223-231`）、ABI #6（Asyncify 休眠态 host 调用）、ADR-0047 R-009。

**交付**：新增 `targets/wasm/pal_wasm_completion.h` / `.c`

```c
typedef void (*pal_wasm_completion_cb_t)(void *arg, wink_status_t result);

wink_status_t pal_wasm_schedule_complete_us(uint32_t delta_us,
                                            pal_wasm_completion_cb_t cb,
                                            void *arg);

/* 由 pal_wasm_advance_virtual_clock 在推进结束时调用；
 * 排空所有 deadline <= s_virtual_us 的完成项。 */
void pal_wasm_drain_completions(void);
```

**实现要点**：
- 完成项按 deadline 排序的小顶堆或 ring（最大并发 `PAL_WASM_MAX_PENDING_COMPLETIONS=32`，溢出返回 `WINK_ERR_NO_RESOURCES`）；
- **溢出策略（评审 P6）**：固定 **reject**，不做 drop-oldest。原因：
  1. 完成项代表真实硬件事件；drop-oldest 会让 DAL 永久等待已丢失的事务完成；
  2. 32 项对当前所有 DAL 并发模型（协作 10 ms tick + 每 tick 最多几次 DMA）有 ≥4× 裕量；
  3. reject 路径是显式错误，CI 可测；drop-oldest 是隐式数据丢失，无法在测试里稳定复现。
  - 若将来某 DAL 实测需要 >32 并发，走 ADR 流程调大常量并更新 assurance 文档，禁止在代码里静默覆盖。
  - DAL 三层状态机（Stage 1 T1.4）的 `request_*` 必须处理 `WINK_ERR_NO_RESOURCES`：纯 TX 类（OLED/WS2812）可降级为同步完成，需 RX 类（SDCard/SPI 闪存）返回 `WINK_ERR_BUSY` 让 App 下一 tick 重试。
- `pal_wasm_schedule_complete_us` 可在 ISR 软中断 / Asyncify 路径调用，仅入队，不调用 JS；
- `pal_wasm_drain_completions` 在 `wink_vclock_advance_internal`（`pal_osal_wasm.c:40`）末尾、`HEADLESS` idle-jump（:551）之后各调用一次；
- 延迟模型：
  - SPI: `ceil(len * 8 * 1e6 / baud_hz)` µs
  - RMT TX: `symbol_count * max(duration0, duration1) / resolution_hz`
  - UART DMA TX: `ceil(len * 10 * 1e6 / baud_hz)` µs（含起始/停止位）
  - ADC continuous: 按 PWM 周期（Stage 2 T2.4）
  - HX711 若升级 RMT/SPI 也走此路径

**硬红线**：
- ❌ **严禁**在 `js_pal_*` 导入函数内同步调用用户 callback；
- ❌ **严禁**在 Asyncify 休眠态（`wink_sim_asyncify_state != AWAKE`）调用任何 host 函数；
- ✅ 所有完成由 C 主循环拉取（pull model），与真机 ISR→任务回调形态一致。

**验收**：
- [ ] Host 单测：插入 3 个不同 delta 的完成项，drain 按虚拟时间顺序触发；
- [ ] Wasm Headless 确定性：同一种子两次运行，callback 序列完全一致；
- [ ] `grep -n "callback(.*WINK_OK" targets/wasm/` 零命中。

### T3.3 `pal_hwtimer` 软步进通道

**目标**：Stage 2 T2.2 的 `pal_hwtimer_fire_soft` 在 wasm 由虚拟时钟驱动。

**实现**：
- `targets/wasm/pal_wasm_hwtimer.c` 维护 `next_fire_us[timer_id]`；
- `pal_hwtimer_init` 计算 `next_fire_us = s_virtual_us + period_us`；
- 在 `pal_wasm_drain_completions`（T3.2）同一排空点扫描到期 timer，调用回调后推进 `next_fire_us += period_us`（oneshot 不再重载）；
- 绑核 / FPU 字段在 wasm 无操作，但结构体布局必须与真机一致（`_Static_assert` ABI 尺寸）；
- `pal_hwtimer_fire_soft` 在 wasm 直接调用回调（供测试手动触发）。

**验收**：
- [ ] 20 kHz 配置下，虚拟时钟推进 1 s 触发 20000 次回调（无累积漂移）；
- [ ] `change_period` 下一周期生效；
- [ ] Headless 确定性。

### T3.4 `pal_mcpwm` / `pal_pcnt` 语义状态通道

**`pal_mcpwm`**：
- wasm target 维护 `duty_ticks / freq_hz / deadtime / fault_state`；
- `set_duty_ticks` 即时更新；`new_fault` 把 `fault_state=1` 并调度 brake 事件（通过 T3.2 立即触发，delta=0）；
- capture 通道：由前端 plugin 通过现有 CH1 pin-event 机制投递软边沿，wasm target 打上 `s_virtual_us*1000` ns 时间戳后调用 `on_capture_isr`；
- **不**模拟 dead time 电气行为。

**`pal_pcnt`**：
- wasm target 维护 64 位累加（与真机 `watch_threshold` ISR 累加同构）；
- plugin 通过 `pal_wasm_push_pcnt_edge(unit, delta_count)` 注入（**新 bridge 符号，需 bump ABI hash**）；
- `get_count` 返回 64 位值；`clear_count` 原子清零。

**新 bridge 符号（bump hash 到 `0x50333037`）**：
```c
/* wasm_bridge.h 增到 CH5 软边沿分组 */
extern void     js_pal_pcnt_edge(uint8_t unit, int32_t delta);
extern void     js_pal_mcpwm_capture_edge(uint8_t cap_channel, uint32_t ts_ns, bool rising);
extern uint32_t js_pal_mcpwm_get_duty_ticks(uint8_t mcpwm_unit, uint8_t cmp_id);
```

同步：
- `targets/wasm/pal_wasm_degradation.c:80` 更新 `PAL_WASM_ABI_HASH 0x50333037u`；
- `embedded-frontend` 中 `WasmImports` 类型表加三符号；
- `wink_sim_stub.js` 加 stub；
- ADR-0047 仿真条款回写。

**验收**：
- [ ] host Unity：pcnt 累加与真机 64 位语义一致；
- [ ] wasm Headless：mcpwm duty 写入 → 前端读到 duty 镜像；
- [ ] ABI hash bump 在 PR 描述中显式列出。

### T3.5 修复 UniSim 3.0 文档漂移

| 文件 | 问题 | 修复 |
|---|---|---|
| `docs/design/04-wasm-simulation-3.0/axes/01-architecture.md:117` | 引用不存在的 `pal_hal_wasm.c` | 改为 `pal_wasm_ch2_*.c` 系列文件名 |
| `docs/design/04-wasm-simulation-3.0/axes/08-channel-routing.md:102` | SPI 标 "Landed" 但无 C PAL | 改为 "Partial（JS import Landed，C PAL 由 Stage 3 T3.1 补齐）" |
| `docs/design/04-wasm-simulation-3.0/mechanisms/05-memory-and-faults.md:35` | ADR-0045 固定堆 triple flags 未入 CMake | 开 issue / 加 Stage 0 CMake 任务：`-DCONFIG_SPIRAM_USE_MALLOC=n` 等 triple flags |
| `docs/design/04-wasm-simulation-3.0/axes/09-timer-and-pwm-semantics.md` | 未覆盖 `pal_hwtimer` 软步进契约 | T3.3 完成后追加章节：虚拟时钟驱动、无累积漂移、oneshot/period |

> 按 CLAUDE.md 文档流转规则，本阶段新增/变更的跨边界契约必须回写到 UniSim 3.0 活文档（① 设计规范），不能只留在 Stage 3 计划里。

### T3.6 ADR-0043 lint allowlist 同步

- Stage 0/2 新增头（`pal_spi.h`、`pal_pcnt.h`、`pal_hwtimer.h`、`pal_mcpwm.h`、`pal_atomic.h`、`pal_wasm_completion.h`；`pal_rmt.h` 重写、`pal_uart.h` 扩写——无 `pal_uart_ex.h`，按 PLAN-PRE-STAGE0-PAL-NAMING v2 该头已取消）登记到 layering allowlist；
- api 规则：`js_pal_*` 导入必须出现在 `wasm_bridge.h` Axis 注释分组内；`pal_wasm_*` 导出必须附 ABI hash 变更记录；
- `python wink-tools/wink.py lint --pack layering --pack api --pack dal` 零错误。

---

## 3. 跨边界符号与 ABI hash 纪律

| 变更 | hash bump |
|---|---|
| 已有 `js_pal_spi_transfer` 首次接入 C PAL | ❌ 不 bump（签名不变） |
| 已有 `pal_wasm_push_uart_rx_byte` 等无变更 | ❌ 不 bump |
| 新增 `js_pal_pcnt_edge` / `js_pal_mcpwm_capture_edge` / `js_pal_mcpwm_get_duty_ticks` | ✅ bump 到 `0x50333037` |
| 后续若新增 hwtimer 状态查询或 fault 状态导出 | ✅ 每次新增跨边界符号 bump |

**PR 模板检查项**：
- [ ] 若改了 `wasm_bridge.h`，已 bump `PAL_WASM_ABI_HASH`；
- [ ] TS `WasmImports/WasmExports` 已同步；
- [ ] `wink_sim_stub.js` 已加 stub；
- [ ] ADR 或 UniSim 3.0 活文档已回写；
- [ ] 任何 `js_pal_*` 内不触发用户 callback。

---

## 4. 验收门槛（进入 Stage 4 前置）

- [ ] `pal_wasm_ch2_spi.c` 合入，OLED/SDCard 等 SPI DAL 在 wasm 跑通；
- [ ] `pal_wasm_schedule_complete_us` 统一所有异步完成路径；`grep` 确认无同步 callback；
- [ ] `pal_hwtimer` 软步进 1 s / 20000 次无漂移；
- [ ] `pal_mcpwm` / `pal_pcnt` 语义通道 + ABI hash bump；
- [ ] UniSim 3.0 文档漂移全部修复；
- [ ] Wasm Headless 契约测试：同一种子两次回放 bit-exact；
- [ ] `python wink-tools/wink.py lint --pack layering --pack api --pack dal` 零错误；
- [ ] Stage 3 新增/变更契约已回写 UniSim 3.0 活文档与相关 ADR follow-up。
