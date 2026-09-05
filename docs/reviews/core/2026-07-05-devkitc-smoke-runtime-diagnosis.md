---
title: DevKitC Smoke 固件 ESP32 运行时故障诊断
date: 2026-07-05
type: diagnosis
scope: samples/devkitc_smoke + ESP32 target PAL/DAL
status: root-cause-analyzed
---

# DevKitC Smoke 固件 ESP32 运行时故障诊断

## 1. 背景与症状

烧录 commit `71d87c9`（"refactor(test): decouple devkitc_smoke from esp32 platform, add OSAL semaphores, fix same-pin loopback and watchdog yields"）后，固件可正常启动但存在多个运行时故障。串口日志核心片段：

```
I (319) main_task: Calling app_main()
=== Wink-Micro-OS ESP32 Firmware ===
Wink-Micro-OS ESP32 Runtime started
  Reset reason: 1
pwm: ch1=50Hz->timer0, ch2=1kHz->timer1 PASSS9: RMT hardware loopback test starting (unwired)...W (439) ledc: GPIO 4 is not usable, maybe conflict with others
Runtime task created (stack=8192 bytes, handle=0x3ffb91c0)
Heap baseline: 286120 bytes
S9: RMT capture failed: -2W (559) ledc: GPIO 4 is not usable, maybe conflict with others
i2c scan 0x3C: status=-12 (NACK expected)i2c scan 0x68: status=-12 (NACK expected)i2c scan 0x76: status=-12 (NACK expected)i2c: PASS (v6 driver init+transfer ran without panic)init done. Long-press BOOT (>3s) to trigger WDT reset test.
Uptime: 1s  Stack: used=1404B free=6788B  Heap: 271624B (delta-14496)  Faults: 6
uptime=2311ms isr_count=0 faults=10 wdt_verified=0 sonar_st=-2 dist=-1.00cmWARNING: Possible heap leak! delta=-14496B
uptime=5311ms isr_count=0 faults=22 wdt_verified=0 sonar_st=-2 dist=-1.00cmUptime: 6s  Stack: used=1404B free=6788B  Heap: 271624B (delta-14496)  Faults: 26
WARNING: Possible heap leak! delta=-14496B
uptime=7311ms isr_count=0 faults=30 wdt_verified=0 sonar_st=-2 dist=-1.00cmUptime: 8s  Stack: used=1404B free=6788B  Heap: 271624B (delta-14496)  Faults: 32
```

症状汇总：

| # | 症状 | 关键数值 |
|---|------|---------|
| S1 | `W ledc: GPIO 4 is not usable, maybe conflict with others` 出现 2 次 | line 439 / 559 |
| S2 | S9 RMT 自环测试失败：`S9: RMT capture failed: -2` | err = WINK_ERR_TIMEOUT |
| S3 | S10 超声波持续失败：`sonar_st=-2 dist=-1.00cm` | err = -2 持续 |
| S4 | `Faults` 计数约 +4/s 持续增长 | 6 → 10 → 22 → 26 → 30 → 32 |
| S5 | `isr_count=0` 始终为零（用户按住 BOOT 仍零） | 待确认是否真按键 |
| S6 | `WARNING: Possible heap leak! delta=-14496B` | 固定 14496B，不增长 |
| S7 | `wdt_verified=0` | 未触发 WDT（非故障，等用户长按） |
| —  | PWM 异频 PASS、I2C 空总线 PASS、栈剩余健康 (6788B) | ✅ 正常 |

**关键判断**：固件可启动、核心 PAL（task/gpio/pwm/i2c/heap/栈）运行正常；故障集中在 **S9 RMT 同-pin 自环**、**S10 超声波 TRIG→ECHO 影子任务**、以及**告警/故障计数混计**三个方面。

---

## 2. 根因分析（按优先级）

### 2.1 [P0] S9 RMT 同-pin 回环永远超时（`S9: RMT capture failed: -2`）

**涉及文件**：
- `wink-micro-os/samples/devkitc_smoke/app_callbacks.c:281-363` (`smoke_check_rmt_loopback`)
- `wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c:75-81` (`pal_pwm_deinit`)
- `wink-micro-os/targets/esp32/pal_hal_gpio_esp32.c:459-487` (`pal_test_enable_hardware_loopback`)
- `wink-micro-os/targets/esp32/pal_rmt_esp32.c` (`pal_rmt_pulse_capture_init/wait`)

**根因**：三处缺陷叠加，导致 RMT RX 永远收不到 PWM 脉冲。

**(a) `pal_pwm_deinit()` 未真正释放 GPIO matrix 绑定**

`pal_hal_pwm_esp32.c:75-81` 现有实现：
```c
void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    /* Track A（M1）：PAL 不再持 PWM claim；release 归 DAL 层未来 deinit。 */
    pal_pwm_router_release(channel);
}
```

只做了两件事：
1. duty 设 0（停占空比输出，但 LE DC 通道仍然 bind 在 GPIO4 上）；
2. 释放 router 引用计数。

缺失：
- 没有调用 `ledc_stop(LEDC_LOW_SPEED_MODE, channel, 0)`；
- 没有通过 `esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, ...)` 或 `gpio_reset_pin(pin)` 断开 LEDC 与 GPIO pad 的 matrix 路由；
- GPIO4 仍然"属于"LEDC 外设（IDF 内部 pin reservation 未释放）。

后果：S5 `pal_pwm_init(SMOKE_PWM_CH_LO=1, 50Hz)` 通过 `ledc_channel_config({.gpio_num=4})` 已经把 LEDC ch1 永久 route 到 GPIO4；S9 调用 `pal_pwm_deinit(1)` 后 LEDC 仍然挂在 GPIO4 上。

**(b) S9 初始化顺序错误：先 RMT 后 PWM**

`app_callbacks.c:297-314` 顺序：
```c
st = pal_rmt_pulse_capture_init(rmt_pin=4, PAL_RMT_EDGE_RISING);  // 步骤 2
// ...
pal_pwm_deinit(SMOKE_PWM_CH_LO);                                  // 步骤 3（在 RMT 之后）
st = pal_pwm_init(SMOKE_PWM_CH_LO, 50u);                          // 步骤 3
// ...
st = pal_pwm_set_duty(SMOKE_PWM_CH_LO, 0.5f);                     // 步骤 4
st = pal_test_enable_hardware_loopback(pwm_pin=4, rmt_pin=4);    // 步骤 5
```

- 步骤 2 调用 `rmt_new_rx_channel({.gpio_num=4})`，此时 GPIO4 还被 LEDC 持有 → ESP-IDF LEDC 驱动检测到冲突，打印 **第一次** `W ledc: GPIO 4 is not usable`；
- 步骤 3 `pal_pwm_deinit`（无效，见 a）+ `pal_pwm_init` 再次 `ledc_channel_config` 又抢回 GPIO4 → 打印 **第二次** 警告；
- 最终两个外设都"以为"自己占有 GPIO4，ESP-IDF 内部 pin 状态不确定（LEDC 驱动输出警告但不阻止 RMT 绑定，反之亦然）。

**(c) 同-pin loopback 只开 IE 位，未做 GPIO matrix 回环**

`pal_hal_gpio_esp32.c:465-468` 同-pin 分支：
```c
if (pin_out == pin_in) {
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin_out]);
    return WINK_OK;
}
```

这是本次失败的最根本 bug：

- `PIN_INPUT_ENABLE` 仅设置 IOMUX 寄存器的 IE 位，**让 pad 的输入缓冲可以感知外部引脚上的电平**；
- 但裸板上 GPIO4 没有任何外部连线，外部永远是浮空/弱拉状态；
- ESP32 上，一个 pin 被配置为外设输出（LEDC）时，其 **输出信号走 GPIO matrix 从外设到 pad**，但这个信号**不会自动回灌到该 pad 的输入通道**。输入通道只看 pad 上的物理电平，不看本芯片驱动的输出。
- 因此即便 LEDC 在 GPIO4 输出 100µs 脉冲，RMT RX 通过 GPIO matrix 监听 GPIO4 的**输入**也看不到这个脉冲——输入缓冲看的是物理 pad 上的外部电平，永远是低。

**正确做法**（二选一）：
- **方案 A（软脉冲，推荐）**：不要用 PWM 产生 100µs 脉冲，直接 `pal_gpio_write(pin, 1); pal_os_busy_wait_us(100); pal_gpio_write(pin, 0);`。此时 pin 被配置为 GPIO 输出+输入（回环），RMT 能捕捉到；需要在 GPIO 模式下配合 `esp_rom_gpio_connect_in_signal`/`PIN_INPUT_ENABLE` 做真正的自环。
- **方案 B（异-pin 硬件回环）**：用两个不同 GPIO（如 pin_out=4 接 PWM，pin_in=5 接 RMT），走现有的异-pin 分支（已经正确实现 `esp_rom_gpio_connect_out_signal`），但需要物理短接或依赖 GPIO matrix 跨-pin 回环；
- **方案 C（正确的同-pin 回环）**：在 pin 已被 LEDC 驱动时，额外调用 `esp_rom_gpio_connect_in_signal(pin, RMT_SIG_IN0_IDX, ...)` 之类的 matrix-in 配置，让 RMT 的输入信号直接来自 LEDC 输出信号（或 GPIO out 信号），绕过 pad 输入缓冲。这需要根据 ESP32 TRM 验证信号索引。

**结论**：当前同-pin 路径只做了 `PIN_INPUT_ENABLE`，无法把"自己驱动的内部输出"送回"自己的内部输入"，RMT 永远等不到边沿 → 30ms 超时返回 `WINK_ERR_TIMEOUT(-2)`，对应日志 `S9: RMT capture failed: -2`。

**附：S9 清理路径虽有 fault trace 但未提前 return**

S9 在 capture 失败后（`app_callbacks.c:342-343`）调用 `wink_trace_fault(FAULT_ISR_INIT=9004)`，但**没有 `return`**，代码继续执行 step 7 清理（deinit PWM、重新 init PWM 为 S5 50% 占空比、deinit RMT、disable loopback、release 资源）。这意味着：
- RMT 被 deinit 后，GPIO4 的 matrix 状态处于"LEDC 重新绑定"状态；
- 下一次 ultrasonic 调用 `pal_gpio_pulse_in(ECHO=19)` 时，`pal_gpio_pulse_in`（`pal_hal_gpio_esp32.c:439-449`）检查 `pal_rmt_pulse_capture_is_active()`（此时 false，因为 S9 调了 deinit），重新 init RMT 到 GPIO19 上——这个是正确的（从 GPIO4 切换到 GPIO19）。

---

### 2.2 [P0] S10 超声波持续返回 -2（`sonar_st=-2 dist=-1.00cm`）

**涉及文件**：
- `wink-micro-os/samples/devkitc_smoke/app_callbacks.c:69-75, 253-278, 408-439, 520-530`
- `wink-micro-os/dal/src/sensor/dal_ultrasonic.c` (`dal_ultrasonic_init/request_measurement`)
- `wink-micro-os/targets/esp32/pal_hal_gpio_esp32.c:169-206, 429-457, 459-487`

**设计意图**（从代码推断）：
- TRIG=GPIO18, ECHO=GPIO19，无外部传感器；
- `app_init` 调用 `pal_test_enable_hardware_loopback(TRIG=18, TRIG=18)` + `pal_test_enable_hardware_loopback(TRIG=18, ECHO=19)`，想让 TRIG 的输出通过 GPIO matrix 内部环回到 ECHO 的输入；
- `trig_gpio_isr` 挂在 TRIG 的上升沿，触发后 give `s_trig_sem`；
- `mock_sensor_task` 等信号量后，`busy_wait_us(100)` → 拉 ECHO 高 → `busy_wait_us(2940)` → 拉 ECHO 低，产生一个 2940µs 的回波脉冲（对应距离 ≈ 50cm）；
- `app_loop` 每 500ms 调 `dal_ultrasonic_request_measurement`，驱动 TRIG 发 10µs 脉冲，然后用 RMT 捕捉 ECHO 高电平脉宽。

**根因**：两处实现缺陷叠加，整个回环链路断裂。

**(a) TRIG 被配置为 `PAL_GPIO_OUTPUT_PUSH_PULL`（纯输出模式），其 ISR 永远不会触发**

`dal_ultrasonic_init`（`dal_ultrasonic.c:71-78`）调用：
```c
status = pal_gpio_init(cfg->trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL);
```

`pal_gpio_init`（`pal_hal_gpio_esp32.c:193-195`）映射到 `cfg.mode = GPIO_MODE_OUTPUT`。

ESP32 `GPIO_MODE_OUTPUT` 不设置 `GPIO_MODE_INPUT` 位，也不 `PIN_INPUT_ENABLE`：**pad 的输入缓冲被关闭**，GPIO 中断检测逻辑无法感知引脚上的电平变化。因此 `pal_gpio_enable_interrupt(TRIG, RISING_EDGE, trig_gpio_isr, NULL)` 虽然注册了 ISR 到 GPIO 分发表，但永远不会有中断触发 → `trig_gpio_isr` 永不执行 → `s_trig_sem` 永远不 give → `mock_sensor_task` 永远阻塞在 `pal_os_sem_take(s_trig_sem, FOREVER)`。

**(b) `pal_test_enable_hardware_loopback(TRIG, ECHO)` 对普通 GPIO 输出的路由也存在问题**

异-pin 分支（`pal_hal_gpio_esp32.c:480-485`）：
```c
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uint32_t sig = GPIO.func_out_sel_cfg[pin_out].func_sel;
#else
    uint32_t sig = GPIO.func_out[pin_out].func;
#endif
    esp_rom_gpio_connect_out_signal((gpio_num_t)pin_in, sig, false, false);
```

这里读取 `pin_out=TRIG(18)` 当前选定的 **外设输出信号索引**。当 pin 被配为 `GPIO_MODE_OUTPUT` 且由 `gpio_set_level` 位操作驱动时，`func_sel` 应当是 `SIG_GPIO_OUT_IDX=256`（即"从 GPIO.out 寄存器位驱动"的信号源）。这一步理论上能把 GPIO 输出信号 route 到 ECHO pin 的输入通道——但前提是 pin_in 被配置为可以接收这个信号。异-pin 分支第一步 `gpio_config(pin_in, GPIO_MODE_INPUT_OUTPUT)`（`pal_hal_gpio_esp32.c:470-478`）确实做了这个配置。

但 `pal_test_enable_hardware_loopback(TRIG, TRIG)`（同-pin）只 `PIN_INPUT_ENABLE(GPIO18)`，并未把 GPIO18 配成 `GPIO_MODE_INPUT_OUTPUT`——而 TRIG 是 OUTPUT 模式，此时 `PIN_INPUT_ENABLE` 对 TRIG 的作用：输入缓冲开了，但 GPIO 中断检测是否能看到自己驱动的输出？

在 ESP32 上：`PIN_INPUT_ENABLE` 只打开 pad 输入缓冲（看外部电平）；要让"自己驱动的输出信号"被自己的输入通道看到，需要走 GPIO matrix 的 loopback 路径或者配置成 INPUT_OUTPUT 模式 + 用 `esp_rom_gpio_connect_in_signal`/`esp_rom_gpio_connect_out_signal` 做内部信号环回。

综上，即使没有 mock_sensor_task 介入，RMT 在 ECHO(19) 上也等不到任何信号，30ms 超时返回 `WINK_ERR_TIMEOUT(-2)` → DAL 设置 `state=ERROR, last_status=-2` → `get_cached_distance` 返回 `dev->last_status=-2` → 日志打印 `sonar_st=-2 dist=-1.00cm`。

---

### 2.3 [P1] `Faults` 计数 ~4/s 持续增长（实为 WCET/超时告警误计为故障）

**涉及文件**：
- `wink-micro-os/runtime/src/wink_runtime.c:63-79, 190-217`
- `wink-micro-os/runtime/include/wink_runtime.h`（fault 码 8002/8003）
- `wink-micro-os/dal/src/sensor/dal_ultrasonic.c:18, 97-142`

**机制**：

`wink_runtime_run` 主循环里：
```c
wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");  // >5ms → 8002
tick_elapsed_us = pal_os_get_us() - tick_start_us;
if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {         // >10ms → 8003
    wink_trace_fault(WINK_WARN_TICK_OVERRUN);
}
```

两个阈值（`WINK_RUNTIME_TICK_MS=10`）：
- **每个 app_loop 执行 >5ms** → trace `WINK_WARN_WCET_EXCEEDED (8002)`；
- **整个 tick（含 sleep）>10ms** → trace `WINK_WARN_TICK_OVERRUN (8003)`。

两个都是 `WINK_WARN_*`，但都用 `wink_trace_fault()` 累加进同一个 `wink_trace_count()` 计数器，与真正 fault（9001..9004、8001、70xx）无区别。

**触发源**：

`app_loop` 每 500ms 调用一次 `dal_ultrasonic_request_measurement`（`app_callbacks.c:520-530`）。这个函数名为"非阻塞 request"，但在 ESP32 上实际是**全阻塞实现**：发 TRIG 后直接调用 `pal_gpio_pulse_in(..., ULTRASONIC_TIMEOUT_US=30000, ...)`，阻塞最长 30ms 等 ECHO。由于 Bug 2.2 导致 ECHO 永远超时，**每 500ms 就有一次 loop 阻塞 30ms**。

30ms > 5ms（WCET 阈值） → 触发 8002；
30ms > 10ms（tick overrun 阈值） → 触发 8003；
每 500ms 触发 2 次 trace → 2 / 0.5s = **4 次/秒**。

与日志完全吻合：
| Uptime | Faults | Δ / 实际时间 | 速率 |
|--------|--------|------------|------|
| 0s → 1s | 0 → 6 | +6 / 1s（含 init 阶段 S9 fault、按钮 init fault 等 9004 一次性贡献） | ~6 init faults |
| 1s → 2.3s | 6 → 10 | +4 / 1.3s ≈ 3/s（第一次 500ms tick 触发） | |
| 2.3s → 5.3s | 10 → 22 | +12 / 3s = 4/s | 符合 |
| 5.3s → 6s | 22 → 26 | +4 / 0.7s ≈ 6/s（日志行时间戳取整） | |
| 6s → 7.3s | 26 → 30 | +4 / 1.3s ≈ 3/s | |
| 7.3s → 8s | 30 → 32 | +2 / 0.7s（uptime=8s 行没打 faults？实际是 32） | |

**结论**：
- 这不是"真正的故障风暴"，是设计 API 名不副实（`request_measurement` 实际 block 30ms）+ trace 系统把 warn 当 fault 数两个因素叠加的**误导性告警**；
- 文件头注释（`dal_ultrasonic.c:92-95`）自己也写着"当前实现仍阻塞，未来非阻塞 RMT 后端（`dal_ultrasonic_start`+`dal_ultrasonic_poll`）落地后严格模式才可保留"——已知债。

---

### 2.4 [P2] `WARNING: Possible heap leak! delta=-14496B` 是假阳性

**涉及文件**：`esp32_firmware/main/app_main.c:97-135`

Heap baseline 在 `app_main` 创建 runtime 任务后**立即**采集（line 97-98）：
```c
const uint32_t heap_free_base = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
```

此时 `wink_runtime_task` 刚刚创建，还未执行 `app_init`。之后 `app_init` 内部会分配：
- RMT channel (`rmt_new_rx_channel`，内部 DMA/ringbuf 等)；
- RMT 完成信号量（`xSemaphoreCreateBinary`）；
- `s_trig_sem = pal_os_sem_create()`；
- `stress_0`、`stress_1`、`mock_sensor`、`smoke_telem` 四个任务（每个任务 FreeRTOS 要分配 TCB + 栈：2048+2048+4096+4096 = 12288 字节，加上 TCB 若干）；
- I2C 驱动内部内存（如果有）；
- 其他 IDF 内部 lazy-init 的驱动结构。

合计约 14KB 稳定下降，不再增长（日志里 `delta=-14496` 从 1s 到 8s 完全不变）。这是**初始化期间的正常一次性分配**，不是泄漏。

阈值 `-512B` 过于敏感：对真实泄漏（持续增长）和一次性 init 分配没有区分。

**修复方向**：把 baseline 采集推迟到 init 完成后（例如通过一个 init-done 回调或事件组），或改为检测"稳定运行后持续下降速率 > X B/s"。

---

### 2.5 [P2] 重复出现的 `W ledc: GPIO 4 is not usable` 警告

这是 Bug 2.1 (a)(b) 的副作用。修复 2.1（正确 deinit + 调整顺序）后这两个警告会自然消失，不需要单独处理。

---

### 2.6 [P3] `isr_count=0` 是否故障待确认

从代码路径看，`pal_gpio_enable_interrupt(BOOT_BUTTON_PIN=0, FALLING_EDGE, boot_button_isr, NULL)` 位于 `app_callbacks.c:445-449`，在 S9→S10→S6→S7 之后、telemetry task 之前：
- S9、S10 fault trace 只调用 `wink_trace_fault`，不 return，继续执行；
- 日志出现了 `init done. Long-press BOOT...` 证明 init 跑到了末尾（line 469），按钮 ISR 注册代码行也应该已执行；
- `pal_gpio_enable_interrupt` 路径（`pal_hal_gpio_esp32.c:234-342`）若失败会 fault trace，但 faults 计数里 init 结束时是 6（S9 超时 1 次 trace 9004，加 init 早期其他 fault），符合 init 完成后 ISR 注册成功。

**因此**：uptime=8s 期间 `isr_count=0` **可能是正常**（用户没有物理按 BOOT 按钮）。需要用户长按 BOOT >3s 验证：
- 若 `isr_count` 随按压递增、且长按 3s 后 WDT 复位 → 正常；
- 若按键无反应 → 需进一步排查 GPIO0 引脚（BOOT button 在 ESP32 上 strap 引脚，上拉/中断配置需特别注意）。

**建议**：用户本次复测时特别验证按一下 BOOT 是否能让 `isr_count` 递增。

---

### 2.7 [正常] I2C 空总线 `status=-12` 符合预期

三个地址 0x3C/0x68/0x76 空总线 NACK，返回的 `-12` 是 ESP-IDF 驱动层面的 NACK/timeout 错误码映射到 WINK 码（需确认映射，但 S6 的 PASS 判定只看"未 panic"，不看每个地址的 status，这是设计意图：空总线扫描 → 所有地址 NACK 是正确行为）。

**i2c: PASS** 打印正常。

---

### 2.8 [正常] `wdt_verified=0`

未长按 BOOT 3s，故未触发 WDT 复位链路。这是正确的初始状态，需要用户物理操作触发。

---

### 2.9 [正常] 栈使用 1404B / 8192B

Runtime 任务栈 8192B，水位 6788B 剩余，远 >1024B 安全阈值，栈健康。

---

## 3. 故障传播图

```
app_init() 调用顺序：
  S2 LED init        ──→ pal_gpio_init(GPIO2, OUTPUT)               ✅ ok
  S3 Button init     ──→ pal_gpio_init(GPIO0, INPUT_PULLUP)         ✅ ok
  S5 PWM router      ──→ pal_pwm_init(ch1/GPIO4, 50Hz)              ✅ PASS
                        pal_pwm_init(ch2/GPIO5, 1kHz)               ✅ PASS
                        pal_pwm_set_duty(ch1, 50%)                  ✅ ok
  S9 RMT loopback    ──→ pal_rmt_pulse_capture_init(4)   ⚠️ GPIO4 还在 LEDC 手里 → W #1
                        pal_pwm_deinit(ch1)              ❌ 未释放 GPIO matrix
                        pal_pwm_init(ch1, 50Hz)         ⚠️ LEDC 抢回 GPIO4 → W #2
                        pal_pwm_set_duty(ch1, 0.5%)     ⚠️ 但同-pin loopback 只 PIN_INPUT_ENABLE
                        pal_test_enable_loopback(4,4)   ❌ 没有把 LEDC out route 到 RMT in
                        pal_rmt_pulse_capture_wait(30ms) ❌ RMT 看不到脉冲 → -2 → fault 9004
                        （清理继续，不 return）
  S10 Ultrasonic     ──→ pal_gpio_init(TRIG=18, OUTPUT)  ❌ OUTPUT 模式 IE 关
                        pal_gpio_init(ECHO=19, INPUT)
                        enable_loopback(18,18)           ❌ 同-pin 只 PIN_INPUT_ENABLE，GPIO 中断不可见
                        enable_loopback(18,19)           ⚠️ func_sel 读取正确（SIG_GPIO_OUT=256）
                        enable_interrupt(TRIG, RISING, isr) ⚠️ TRIG isr 永远不触发（IE 关）
                        task_create(mock_sensor, prio=6, core1) 阻塞等 sem，永不返回
  S4 ISR button      ──→ enable_interrupt(GPIO0)         ✅ 注册成功（待按键验证）
  S6 I2C scan        ──→ 全 NACK                          ✅ PASS
  S7 SMP stress      ──→ 两个 stress 任务启动（core0/1）  ✅ ok
  telemetry task     ──→ 每 2s 打印状态                   ✅ ok

app_loop() 每 10ms 一次：
  button_poll / is_pressed                                 ✅ 微秒级
  led_on/off                                               ✅ 微秒级
  每 500ms：
    dal_ultrasonic_request_measurement()
      → pal_gpio_write(TRIG, 1/0)                         ✅ TRIG 脉冲产生了
      → pal_gpio_pulse_in(ECHO=19, high, 30ms)
         → RMT 在 GPIO19 上 wait 30ms                      ❌ 等不到脉冲（mock_sensor 被堵）
         → WINK_ERR_TIMEOUT(-2)                            → state=ERROR
      → 总耗时 ~30ms
    runtime WCET 检测: 30ms > 5ms → trace 8002
    runtime overrun: 30ms > 10ms → trace 8003
  → 每 500ms +2 faults，即 +4/s                            ⚠️ warn 混进 fault 计数
```

---

## 4. 修复建议（按优先级）

### P0（必须修复，否则 S9/S10 测试失效）

#### 修复 4.1：S9 RMT 自环 — 改方案为"软脉冲 + 真实 GPIO 回环"

最稳健的做法：放弃用 PWM 产生 100µs 脉冲的方案，直接在 S9 里：

1. 在开始前 `pal_pwm_deinit(SMOKE_PWM_CH_LO)`，然后**真正释放 GPIO4**：通过新的 PAL API（或直接在 loopback test 里）把 GPIO4 从 LEDC 断开，配成 `GPIO_MODE_INPUT_OUTPUT`；
2. `pal_rmt_pulse_capture_init(4, RISING)`；
3. `pal_test_enable_hardware_loopback(4, 4)` — 同-pin 路径需升级：
   - 设置 pin 为 `GPIO_MODE_INPUT_OUTPUT`（保证输出驱动器和输入缓冲同时使能）；
   - 调用 `esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false)` 确保 pin mux 选择 GPIO 输出信号而非 LEDC；
   - `PIN_INPUT_ENABLE`；
   - （视需要）`esp_rom_gpio_connect_in_signal(pin, <GPIO-in-matrix-signal>, false)` 或依赖 INPUT_OUTPUT 模式的"自听"路径；
4. 用 `pal_gpio_write(4, 1); pal_os_busy_wait_us(100); pal_gpio_write(4, 0);` 产生精确 100µs 软脉冲；
5. `pal_rmt_pulse_capture_wait(30000, &pulse_us)` 捕捉；
6. 清理时把 GPIO4 重新交还给 PWM（`pal_pwm_init(SMOKE_PWM_CH_LO, 50Hz); pal_pwm_set_duty(ch1, 50%)`），并恢复 pin 模式。

**需要先验证的技术点**：在 ESP32 上，`GPIO_MODE_INPUT_OUTPUT` 模式下写入 `GPIO.out_w1ts/w1tc` 后，输入寄存器 `GPIO.in` 能否在下一个 APB 时钟周期读到相同电平？如果可以，则"自听"路径成立，RMT 通过 GPIO matrix 输入信号能捕获到自己 soft-pulse 产生的上升/下降沿。若不能，则需要显式 `esp_rom_gpio_connect_in_signal(pin, GPIOX_IN_IDX, false)` 之类。

**替代方案**：直接把 S9 改成异-pin 回环（GPIO4→GPIO5 经 GPIO matrix），省掉同-pin 自环的技术不确定性；或者在板级跳线上留一个 pad 用于物理短接。

#### 修复 4.2：`pal_pwm_deinit` 真正释放 pin

在 `pal_hal_pwm_esp32.c:pal_pwm_deinit` 里追加：
```c
#include "driver/gpio.h"
// ...
ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
wink_pin_t pin = pal_pwm_pin_map[channel];
esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
gpio_reset_pin(pin);
```

注意：Track A/M1 说"DAL 持有资源，PAL 不 claim"，所以 deinit 时**也不应该 release 资源**（让 DAL 管），但是硬件层面 pin matrix 必须真的断开，否则别的外设没法用该 pin。

#### 修复 4.3：S10 TRIG ISR 触发路径

`dal_ultrasonic_init` 把 TRIG 配成 `PAL_GPIO_OUTPUT_PUSH_PULL` 是正确的（DAL 语义：TRIG 是输出）；要让 TRIG 上的中断可见，`pal_gpio_enable_interrupt` 需要在内部把 pin 重新配置为 `GPIO_MODE_INPUT_OUTPUT`（或在中断路径上 `PIN_INPUT_ENABLE`）。

更简单的做法：改 S10 的 mock 方案 — **不要依赖 TRIG 上的 GPIO ISR**。可以让 `mock_sensor_task` 以固定频率模拟 ECHO 脉冲（比如每 60ms 产生一个 2940µs 脉冲），不与 TRIG 同步；或者把 mock 做成 RMT TX 风格的信号发生器，直接驱动 ECHO 引脚。

或者保留 TRIG-sync 方案，但让 `enable_interrupt` 对 output pin 自动打开 IE 位 + 设置 `GPIO_MODE_INPUT_OUTPUT`（需评估对其他 DAL 用例的影响）。

#### 修复 4.4：`pal_test_enable_hardware_loopback` 同-pin/异-pin 路径规范化

按 4.1/4.3 的要求重构：
- 同-pin：设置 `GPIO_MODE_INPUT_OUTPUT` + 保证 pin mux 路由到 GPIO 信号（非外设） + `PIN_INPUT_ENABLE`；
- 异-pin：已经比较合理，需要确认对 GPIO-out（`func_sel=SIG_GPIO_OUT_IDX`）场景工作正常。

---

### P1（修复"告警风暴"误导）

#### 修复 4.5：分离 warn 和 fault 计数

把 `wink_trace_fault` 拆成两个 API（或加一个 level 参数）：
- `wink_trace_fault(fault_code)`：严重故障，入 `fault_count`；
- `wink_trace_warn(warn_code)`：性能/保真度告警，入 `warn_count`；
- `wink_trace_count()` 返回 fault 数，新增 `wink_warn_count()` 返回 warn 数；
- 日志里 `Faults:` 与 `Warns:` 分开打印。

8002/8003 改为 `wink_trace_warn`。

#### 修复 4.6（长期/结构性）：`dal_ultrasonic_request_measurement` 真正非阻塞

如 `dal_ultrasonic.c:92-95` 注释所述，落地 `dal_ultrasonic_start() + dal_ultrasonic_poll()` RMT 异步后端：
- `start`：发 TRIG 脉冲，启动 RMT RX 异步接收，立即返回；
- `poll`：非阻塞查询 RMT 是否完成，完成则计算距离缓存；
- `request_measurement` 改为调用 `start`，`app_loop` 里在非 sonar 周期内调用 `poll`，彻底消除 30ms 阻塞。

这是 ADR-0017 层 2 的计划内工作。在非阻塞 RMT 落地前，短期**可以**接受 30ms 阻塞，但建议：
- 在 app_loop 里降低 sonar 采样频率（例如改到 2s 一次）以降低 warn 速率；
- 或者在 smoke 测试里标记"测试模式：允许 WCET 超限"。

---

### P2（修好不准确性/误导）

#### 修复 4.7：Heap baseline 延后到 init 完成

在 `app_main.c` 里，让 runtime 任务在 `app_init` 完成后通过一个 event-group / queue / 直接回调通知 app_main 任务"init 完成，可以采 baseline"。或在 `wink_runtime_run` 里暴露一个 init-done 钩子。

简单方案：把 `heap_free_base` 采集放到 `for(;;)` 循环第一次迭代时（即 init 已经完成、task 进入稳态 loop 后），用"第一次循环的 heap 值"当 baseline。

#### 修复 4.8：I2C status=-12 的码值核对

快速确认下 `pal_i2c_transfer` 对 NACK 映射的 WINK 码是否就是预期的 NACK/timeout 码；日志里标注"NACK expected"是对的，但码值 `-12` 含义建议在 `wink_status.h` 里有明确命名。

---

### P3（非紧急）

- 验证 BOOT 按键 ISR 可触发（用户实测）；
- mock_sensor_task 在信号量永远不 give 时应加超时 + 错误计数，避免 task 永久死等（当前是永久阻塞）；
- `stress_0/stress_1` 双核压测 60s 期间会持续占 CPU，可能影响其他 task 的 WCET，建议在 smoke 测试里加标记让 runtime WCET 监控在 stress 期间降级；
- `pal_pwm_pin_map[0]=2` 是 GPIO2（板载 LED 引脚），如果 DAL LED 也用 GPIO2 做 GPIO 输出，PWM ch0 若被其他用例 init 就会和 LED 冲突——当前 devkitc_smoke 只用 ch1/ch2 所以没撞到，但需要文档/ASSERT 防护。

---

## 5. 验证步骤（修复后）

重新烧录后预期：

1. **S9 应打印**：`S9: PASS (pulse captured: ~100 us)`（90~110µs 范围内）；
2. **无 `ledc: GPIO 4 is not usable` 警告**；
3. **S10 telemetry**：`sonar_st=0 dist≈50.00cm`（mock_sensor 产生 2940µs 脉冲 → 2940 × 0.017 ≈ 49.98cm）；
4. **isr_count 按键后递增**（按一下 BOOT 看是否 +1）；
5. **Faults 稳定**：init 完成后应该是个小的常数（比如 0 或 1，代表正常启动），不再 +4/s 增长；
6. **Warns（新增）**：在 sonar 采样 tick 可能 +2/s，可接受；
7. **Heap delta 稳定**：在稳态 baseline 之上 delta 应 < 500B 长期运行；
8. **长按 BOOT 3s** 触发 WDT 复位，重启后打印 `watchdog: PASS (recovered after abnormal reset, count=1)` 且 `wdt_verified=1`；
9. **Stack 水位**：仍应剩余 >3000B；
10. **I2C 三地址** 仍然 NACK，`i2c: PASS`。

---

## 6. 附：关键文件索引

| 关注点 | 文件 | 行号 |
|--------|------|------|
| S9 主体 | `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | 281-363 |
| S10 主体 | `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | 253-278, 408-439, 520-530 |
| app_init 顺序 | `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | 365-470 |
| app_loop 10ms tick | `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | 477-531 |
| telemetry task | `wink-micro-os/samples/devkitc_smoke/app_callbacks.c` | 220-248 |
| pal_pwm_deinit（错） | `wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c` | 75-81 |
| pal_test_enable_hardware_loopback（同-pin 错） | `wink-micro-os/targets/esp32/pal_hal_gpio_esp32.c` | 459-487 |
| pal_gpio_pulse_in（RMT 路由） | `wink-micro-os/targets/esp32/pal_hal_gpio_esp32.c` | 429-457 |
| pal_gpio_init（OUTPUT 不开 IE） | `wink-micro-os/targets/esp32/pal_hal_gpio_esp32.c` | 169-206 |
| pal_rmt_pulse_capture_init/wait | `wink-micro-os/targets/esp32/pal_rmt_esp32.c` | 74-223 |
| dal_ultrasonic_request_measurement（阻塞） | `wink-micro-os/dal/src/sensor/dal_ultrasonic.c` | 97-142 |
| dal_ultrasonic_init（TRIG OUTPUT 模式） | `wink-micro-os/dal/src/sensor/dal_ultrasonic.c` | 44-90 |
| runtime WCET/tick overrun 监测 | `wink-micro-os/runtime/src/wink_runtime.c` | 63-79, 190-217 |
| app_main heap baseline 位置（早） | `esp32_firmware/main/app_main.c` | 97-98 |
| PWM pin map (ch0=GPIO2=LED, ch1=GPIO4, ch2=GPIO5) | `wink-micro-os/samples/devkitc_smoke/board_config.c` | 11 |
| 错误码 WINK_ERR_TIMEOUT=-2 | `wink-micro-os/pal/include/wink_status.h` | 72 |
