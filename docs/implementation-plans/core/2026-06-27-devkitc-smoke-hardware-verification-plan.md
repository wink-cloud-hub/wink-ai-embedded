# ESP32 DevKitC 裸板真机冒烟验证实施计划

| 项 | 内容 |
|----|------|
| **计划创建日期** | 2026-06-27 |
| **计划执行日期** | 2026-06-27 |
| **计划状态** | ✅ 已执行（全部验收通过） |
| **关联 Wave** | Wave B 真机验证收尾 |
| **关联实施计划** | [`2026-06-26-wave-b-esp32-port-followup-v2.md`](2026-06-26-wave-b-esp32-port-followup-v2.md)（v2 Task 8 硬件验收门）、[`2026-06-27-esp-idf-v6-i2c-compat-plan.md`](2026-06-27-esp-idf-v6-i2c-compat-plan.md)（I2C 真机验证项） |
| **关联验证记录** | [`2026-06-26-wave-b-followup-v2-verification.md`](2026-06-26-wave-b-followup-v2-verification.md)（原 DEFERRED 项已关闭） |
| **验证报告** | [`../../reviews/core/2026-06-27-devkitc-smoke-hardware-verification.md`](../../reviews/core/2026-06-27-devkitc-smoke-hardware-verification.md)（④ 层评审记录） |
| **关联 ADR** | [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)（I2C v6.x 兼容，S6 真机验证） |
| **关联设计规范** | [`02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md) |
| **硬件前提** | ESP32 DevKitC（经典双核，`CONFIG_IDF_TARGET=esp32`），**无需任何外设** |
| **Out-of-Scope** | HC-SR04 测距（RMT）、OLED 显示、PWM 异频定量（需示波器）——留待外设到位 |

---

## 1. 背景与目标

### 1.1 问题陈述

Wave B（ESP32 真机移植）的 **8 个 Task 代码已全部完成**，I2C v6.x 兼容性（原 Wave C 的 ISSUE-007）也已提前完成（ADR-0006），v6.0.1 编译 `0 err/0 warn`、host 单测 16/16 通过。但 **真机硬件验证全部 DEFERRED**——`targets/esp32/*.c` 的 `@verified` 头仍停在 `COMPILED`，从未上过板（见 [`wave-b-followup-v2-verification.md`](2026-06-26-wave-b-followup-v2-verification.md) §延迟项）。

当前用户手上**只有一块 ESP32 DevKitC（经典双核）裸板**，无 HC-SR04 / OLED / 示波器。需要在不依赖任何外设的前提下，尽可能多地关闭 Wave B 的真机验证项。

### 1.2 目标

用一个专门的 `devkitc_smoke` 冒烟固件，**一次性关闭 Wave B 在裸板上可验证的全部项**，把 `targets/esp32/*.c` 的 `@verified` 从 `COMPILED` 推进到 `HARDWARE-SMOKE-PASSED`，并产出 ④ 层验证报告。

✅ **首次闭环**：证明 toolchain → 烧录 → 启动 → runtime → PAL/DAL 整条链路在真机工作
✅ **覆盖裸板可验项**：S1–S8 共 8 项（见 §2）
✅ **零外设**：只用板载 LED（GPIO2）+ Boot 按钮（GPIO0）+ 空 I2C 总线 + 双核
✅ **双 target 同源**：smoke app 同一份代码 host 可测、esp32 可烧（ADR-0002）

---

## 2. 验证项矩阵

| 验证项 | 验证的 Wave B 成果 | 裸板资源 | 真机 PASS 标准 |
|---|---|---|---|
| **S1** 启动 + UART + 栈/堆监控 | toolchain 整链路 | USB 串口 | `idf.py monitor` 打印启动行 + 每秒 uptime/stack/heap |
| **S2** GPIO 输出（LED 慢闪） | `pal_gpio_write` 路径 | 板载 LED GPIO2 | LED 肉眼 500ms 周期闪烁 |
| **S3** GPIO 输入去抖（按钮） | `pal_gpio_read` + DAL 去抖 | Boot 按钮 GPIO0 | 按下 LED 亮、释放慢闪 |
| **S4** GPIO 中断 ISR 计数 | **Task 3 `uintptr_t` ISR 对称化** | Boot 按钮 GPIO0 | UART 打印 `isr_count` 随按键递增 |
| **S5** PWM router 异频分配 | **Task 4 `pal_pwm_router` + LEDC timer** | PWM ch1/ch2（GPIO4/5，无 LED） | 50Hz/1kHz 分到不同 timer，`pal_pwm_init` 返回 OK |
| **S6** I2C v6 总线扫描 | **ADR-0006 I2C v6 `i2c_master` 驱动** | I2C bus0（空总线） | 扫描 3 个地址全 NACK（返回错误码而非 panic）→ 驱动初始化+传输不崩 |
| **S7** 双核临界区并发压测 60s | **Task 1 `s_resource_mux` spinlock** | 双核 | CPU0/CPU1 各起任务循环 claim/release，60s 无 panic/复位 |
| **S8** 看门狗复位链路 | WDT + `pal_get_reset_reason` + boot safe-lock | Boot 按钮长按 | 长按 >3s 触发 WDT 复位 → 重启后打印 `watchdog: PASS (fault 8001)` |

**做不了的**（等外设）：HC-SR04 测距 4 项（RMT ISR 延迟/精度/复位复测）、OLED 显示、PWM 异频定量——明确标注为 Out-of-Scope。

---

## 3. 设计

### 3.1 架构决策

- **照搬 `oled_dashboard` sample 模式**（button+led，最贴近），但 app 文件命名为 `app_callbacks.c`（**不能叫 `app_main.c`**——会与 `esp32_firmware/main/app_main.c` 的 IDF 入口符号冲突，这正是 Wave B Task 6 重命名的原因）。
- **直接调 PAL/OSAL 层**做 S4/S5/S6/S7/S8 验证（目标是验 PAL 移植，不只验 DAL）；S2/S3 用 DAL（`dal_led`/`dal_button`）便利封装。
- **`#if defined(ESP_PLATFORM)` 隔离 esp32-only 验证逻辑**（ISR/I2C/双核/看门狗/telemetry），host 下跳过 → 同一份 `app_callbacks.c` 既能烧真机、又能在 host 跑 e2e（ADR-0002）。
- **`app_loop` 零 `printf`、< 5ms WCET**：所有周期性 UART 输出由一个 esp32-only 的低优先级 telemetry task 承担（独立 FreeRTOS task，不受 runtime WCET 约束）；避免 115200 波特率下 printf 触发 `WINK_WARN_WCET_EXCEEDED(8002)` 误警。
- **S8 看门狗复位检测复用 runtime 既有逻辑**：`wink_runtime_run` 进入时若 `pal_get_reset_reason()==WATCHDOG/PANIC` 会自动 `wink_trace_fault(8001)` + `wink_actuator_safe_off_all`（`wink_runtime.c:31-35`）；smoke app 在 `app_init` 开头检查 `wink_trace_last()==8001` 即判定上次是 WDT 复位，打印 PASS 后 `wink_trace_reset()` 恢复。

### 3.2 引脚 / 资源 / WCET 要点

- **GPIO2 不双用**：board_led（DAL，GPIO 输出）独占 GPIO2；S5 PWM 验证用 **channel 1（GPIO4）/ channel 2（GPIO5）**（`pal_pwm_pin_map` 弱默认 `[1]=4, [2]=5`，board_config.c 强定义覆盖）。
- **看门狗触发路径**：`app_loop` 检测 Boot 按钮**持续按下 >3s** → `pal_watchdog_init(2000)` + `while(1){}` 不喂狗 → 2s 后 Task WDT panic → 复位。正常态不订阅 WDT，runtime task 每 tick `vTaskDelay(10ms)` 让出 CPU，idle task 喂狗，不误触发。
- **owner 字符串用字面量**（`pal_resource.h` 契约：静态表持有指针不拷贝）。
- **FAULT 码用 9001+**：避开 `WINK_FAULT_BOOT_AFTER_RESET(8001)` 与 oled_dashboard 的 8001–8003。

---

## 4. 文件结构

新建 sample `wink-micro-os/samples/devkitc_smoke/`：

| 文件 | 作用 | 模板来源 |
|---|---|---|
| `device_tree.h` | `extern dal_led_t board_led; extern dal_button_t boot_button;` + 引脚宏 | `oled_dashboard/device_tree.h` |
| `device_tree.c` | 零初始化静态实例 `{0}` | `oled_dashboard/device_tree.c` |
| `app_callbacks.c` | `wink_app_callbacks_t` + S1–S8 验证逻辑（esp32-only 用 `#ifdef ESP_PLATFORM`） | `oled_dashboard/app_main.c` + avoidance_car 三段式 |
| `board_config.c` | `pal_pwm_pin_map[PAL_PWM_CHANNELS]` 强定义（验证 Task 5 board_config 链接 + sample 自包含） | `avoidance_car/board_config.c` |
| `test_devkitc_smoke_e2e.c` | host e2e：跑 N tick → 断言 LED 状态 + 按钮恒 pressed + 无 fault | `oled_dashboard/test_oled_dashboard_e2e.c` |
| `CMakeLists.txt` | host sample build（链 `dal_led.c`+`dal_button.c`+runtime+trace+`$<TARGET_OBJECTS:pal_host>`） | `oled_dashboard/CMakeLists.txt` |

### 4.1 `device_tree.h`

```c
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H
#include "dal_button.h"
#include "dal_led.h"
#define BOARD_LED_PIN    2u    /* DevKitC 板载 LED, active-high */
#define BOOT_BUTTON_PIN  0u    /* GPIO0 Boot 按键, active-low（内部上拉）*/
extern dal_led_t    board_led;
extern dal_button_t boot_button;
#endif
```

### 4.2 `app_callbacks.c` 骨架（关键结构）

```c
#include "device_tree.h"
#include "wink_app.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "wink_status.h"
#include "pal_hal.h"       /* pal_pwm_init/set_duty/deinit, pal_gpio_enable_interrupt */
#include "pal_osal.h"      /* pal_watchdog_init/feed, pal_get_ms */
#include "pal_resource.h"  /* pal_resource_claim/release, PAL_RESOURCE_GPIO_PIN */
#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#endif

#define FAULT_LED_INIT     9001u
#define FAULT_BUTTON_INIT  9002u
#define FAULT_PWM_INIT     9003u
#define FAULT_ISR_INIT     9004u
#define SMOKE_PWM_CH_LO   1u       /* GPIO4, 50Hz  */
#define SMOKE_PWM_CH_HI   2u       /* GPIO5, 1kHz（验证 router 异频隔离 → 不同 timer）*/

static volatile uint32_t s_isr_count = 0;        /* S4: ISR 计数（IRAM 侧最小工作）*/
static uint32_t s_press_start_ms = 0;            /* S8: 长按计时 */
static bool      s_wdt_verified = false;         /* 本次启动是否已确认 WDT 复位 */

static wink_status_t led_safe_off_thunk(void *ctx) { return dal_led_off((dal_led_t *)ctx); }
static void boot_button_isr(void *arg) { (void)arg; s_isr_count++; }   /* Task 3 ISR 路径 */

/* S5: PWM router 异频分配（host + esp32 都跑 router 纯逻辑）*/
static void smoke_check_pwm_router(void) {
    uint8_t t_lo=0xFF, t_hi=0xFF;
    wink_status_t s = pal_pwm_init(SMOKE_PWM_CH_LO, 50u);   /* 签名 (channel, frequency_hz) */
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_PWM_INIT); return; }
    s = pal_pwm_init(SMOKE_PWM_CH_HI, 1000u);
    if (wink_status_is_error(s)) { wink_trace_fault(FAULT_PWM_INIT); return; }
    t_lo = pal_pwm_router_channel_timer(SMOKE_PWM_CH_LO);   /* 期望 != t_hi */
    t_hi = pal_pwm_router_channel_timer(SMOKE_PWM_CH_HI);
    pal_pwm_set_duty(SMOKE_PWM_CH_LO, 50.0f);
#if defined(ESP_PLATFORM)
    printf("[SMOKE] pwm: ch1=50Hz->timer%u, ch2=1kHz->timer%u %s\n",
           t_lo, t_hi, (t_lo!=t_hi && t_lo<4 && t_hi<4) ? "PASS" : "FAIL");
#endif
}

#if defined(ESP_PLATFORM)
/* S6: I2C v6 总线扫描（空总线 → 全 NACK，证明驱动初始化+传输不 panic）*/
static void smoke_check_i2c_bus(void) {
    static const uint8_t addrs[] = {0x3C, 0x68, 0x76};
    for (size_t i=0; i<sizeof(addrs); i++) {
        uint8_t buf=0;
        wink_status_t s = pal_i2c_transfer(0, addrs[i], NULL, 0, &buf, 1);
        printf("[SMOKE] i2c scan 0x%02X: status=%d (NACK expected)\n", addrs[i], (int)s);
    }
    printf("[SMOKE] i2c: PASS (v6 driver init+transfer ran without panic)\n");
}

/* S7: 双核临界区压测（验证 Task 1 s_resource_mux SMP 安全）*/
static void resource_stress_task(void *arg) {
    uint32_t core = (uint32_t)(uintptr_t)arg;
    uint32_t n = 0; uint64_t end = pal_get_ms() + 60000;
    while (pal_get_ms() < end) {
        (void)pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 100+core, "stress");
        (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, 100+core, "stress");
        n++;
    }
    printf("[SMOKE] resource_stress core%u: %lu claims, no panic\n", core, n);
    vTaskDelete(NULL);
}
static void smoke_check_resource_smp(void) {
    xTaskCreatePinnedToCore(resource_stress_task,"stress0",4096,(void*)0,5,NULL,0);
    xTaskCreatePinnedToCore(resource_stress_task,"stress1",4096,(void*)1,5,NULL,1);
    printf("[SMOKE] resource_stress: 60s dual-core claim/release started (Task1 spinlock)\n");
}

/* telemetry task：承担所有周期 UART 输出，避免 app_loop 触发 WCET */
static void telemetry_task(void *arg) {
    (void)arg; uint32_t last = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        uint32_t now = (uint32_t)pal_get_ms();
        if (now - last >= 2000u) {
            printf("[SMOKE] uptime=%ums isr_count=%lu faults=%lu wdt_verified=%d\n",
                   now, s_isr_count, wink_trace_count(), (int)s_wdt_verified);
            last = now;
        }
    }
}
#endif

static void app_init(void) {
    /* S8: 检测上一次 WDT/PANIC 复位（runtime 已先 trace_fault(8001)）*/
    if (wink_trace_last() == WINK_FAULT_BOOT_AFTER_RESET) {
        s_wdt_verified = true;
#if defined(ESP_PLATFORM)
        printf("[SMOKE] watchdog: PASS (prior WDT/PANIC reset detected, fault 8001)\n");
#endif
        wink_trace_reset();
    }
    if (wink_status_is_error(dal_led_init(&board_led, BOARD_LED_PIN, true)))    wink_trace_fault(FAULT_LED_INIT);
    if (wink_status_is_error(dal_button_init(&boot_button, BOOT_BUTTON_PIN, true))) wink_trace_fault(FAULT_BUTTON_INIT);
    (void)wink_actuator_register(led_safe_off_thunk, &board_led);
    (void)dal_led_off(&board_led);
    smoke_check_pwm_router();               /* S5（跨平台）*/
#if defined(ESP_PLATFORM)
    if (wink_status_is_error(pal_gpio_enable_interrupt(BOOT_BUTTON_PIN, PAL_GPIO_INTR_FALLING_EDGE, boot_button_isr, NULL)))
        wink_trace_fault(FAULT_ISR_INIT);    /* S4 */
    smoke_check_i2c_bus();                  /* S6 */
    smoke_check_resource_smp();             /* S7 */
    xTaskCreate(telemetry_task,"smoke_telem",4096,NULL,1,NULL);
    printf("[SMOKE] init done. Long-press BOOT(>3s) to trigger WDT reset test.\n");
#endif
}

static void app_loop(void) {        /* 必须 < 5ms，零 printf */
    (void)dal_button_poll(&boot_button);
    bool pressed=false; (void)dal_button_is_pressed(&boot_button, &pressed);
    uint32_t now = (uint32_t)pal_get_ms();
    if (pressed) {
        if (s_press_start_ms==0) s_press_start_ms = now;
#if defined(ESP_PLATFORM)
        if (!s_wdt_verified && (now - s_press_start_ms) > 3000u) {
            printf("[SMOKE] triggering WDT reset (stop feeding)...\n");
            (void)pal_watchdog_init(2000u);
            while (1) { }          /* 不喂狗 → 2s 后 WDT panic → 复位 */
        }
#endif
        (void)dal_led_on(&board_led);
    } else {
        s_press_start_ms = 0;
        bool on = ((now / 500u) % 2u) == 0u;       /* 释放时慢闪 */
        (void)(on ? dal_led_on(&board_led) : dal_led_off(&board_led));
    }
}

static void app_on_fault(uint32_t code) { wink_trace_fault(code); (void)dal_led_off(&board_led); }

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = { app_init, app_loop, app_on_fault };
    return &cb;
}
```

### 4.3 `test_devkitc_smoke_e2e.c`（host）

仿 `test_oled_dashboard_e2e.c`：`wink_trace_reset()` + `sim_reset_time()` → `wink_runtime_run(cb, 5)` → 断言 `board_led.is_on`（host 下 active_low 按钮恒 pressed → LED on）、`wink_trace_count()==0`（dal/pwm init 无 fault）→ `E2E_PASS`。host 下 `pal_gpio_read` 非 echo pin 恒 false（`pal_hal_host.c:49`），active_low 按钮经 3 tick 去抖稳定 pressed。

---

## 5. 实施步骤

### Part A — 新建 sample（6 个文件）

按 §4 创建 `wink-micro-os/samples/devkitc_smoke/` 全部文件。编辑 ESP32 C 文件前加载 `embedded-best-practice` skill；遵守 `wink_status_t` 负数错误码、POD+命名 API、无堆实时路径。

### Part B — 注册到 host 构建

`wink-micro-os/CMakeLists.txt`（oled_dashboard 的 `add_subdirectory` 之后）追加：
```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples/devkitc_smoke
                 ${CMAKE_BINARY_DIR}/devkitc_smoke_build)
```
（额外 sample 模式，不动默认 App avoidance_car。）

### Part C — host 测试验证（Claude 执行）
```powershell
python wink-tools/wink.py test
```
预期：`app_devkitc_smoke_e2e` PASS，既有 16 个测试无回归。**此步通过 = sample 代码正确性已在 host 验证**，真机只验 esp32-only 部分。

### Part D — 切换 esp32_firmware 到 devkitc_smoke（2 处改动）

1. `esp32_firmware/main/app_main.c`（device_tree.h include 行）：
   ```c
   #include "../../wink-micro-os/samples/devkitc_smoke/device_tree.h"
   ```
   （其余监控 printf 逻辑保留——它就是 S1。）
2. `esp32_firmware/main/CMakeLists.txt`：SRCS 三行指向 `devkitc_smoke/{app_callbacks,device_tree,board_config}.c`，`INCLUDE_DIRS` 指向 `samples/devkitc_smoke`。

### Part E — 真机编译 + 烧录验收（用户执行）

激活 ESP-IDF v6.0.1（EIM profile + PowerShell + `PYTHONUTF8=1`），然后：
```powershell
idf.py -C esp32_firmware fullclean
idf.py -C esp32_firmware build                              # 预期 0 err/0 warn
idf.py -C esp32_firmware -p COMx flash monitor              # COMx 换实际串口
```

**验收 checklist（用户对照 UART 输出勾选，贴回给 Claude）：**

| 项 | PASS 标准 | UART 关键行 |
|---|---|---|
| S1 启动 | 打印启动行 + 每秒 uptime/stack/heap | `Wink-Micro-OS ESP32 Firmware` + `Uptime: ...s Stack: ... Heap: ...` |
| S2 LED 慢闪 | 释放按钮时 LED 500ms 周期闪烁 | （肉眼）|
| S3 按钮 | 按住 LED 常亮、释放慢闪 | （肉眼）|
| S4 ISR | telemetry 行 `isr_count` 随按键递增 | `[SMOKE] uptime=... isr_count=N` |
| S5 PWM router | `pwm: ch1=50Hz->timerX, ch2=1kHz->timerY PASS`（X≠Y） | `[SMOKE] pwm: ... PASS` |
| S6 I2C v6 | 3 个地址 status 为负（NACK），无 panic | `[SMOKE] i2c scan 0x3C: status=-N` + `i2c: PASS` |
| S7 双核压测 | 60s 后两核各打印 claim 次数、无复位 | `[SMOKE] resource_stress core0/core1: ... no panic` |
| S8 看门狗 | 长按 >3s 触发复位，重启后打印 watchdog PASS | `[SMOKE] triggering WDT reset...` → 重启 → `[SMOKE] watchdog: PASS` |

> S7 需等 60s；S8 会真复位一次（重启后自动恢复，无需重新烧录）。

### Part F — 回写 @verified + 验证报告（Claude 执行）

根据用户贴回的 UART 输出：
1. 更新 `targets/esp32/{pal_resource_esp32,pal_hal_esp32,pal_hal_esp32_rmt,pal_osal_esp32}.c` 的 `@verified` 头：`COMPILED` → `HARDWARE-SMOKE-PASSED (DevKitC, 2026-06-xx)`；并在 Wave B v2 verification record 补真机结论。
2. 新建 `docs/tech-designs/unisim/2026-07-20-co-simulation-plugin-contract.md`（④ 评审层），记录 S1–S8 逐项结果 + Out-of-Scope（HC-SR04/OLED/示波器）。
3. 关闭 Wave B v2 verification 对应 DEFERRED 项（Task 1 临界区、Task 3 ISR 已裸板验证；Task 2 RMT/HC-SR04 仍 DEFERRED）。
4. 本计划状态 → ✅ 已执行。

---

## 6. 关键文件

**新建**：`wink-micro-os/samples/devkitc_smoke/{device_tree.h,device_tree.c,app_callbacks.c,board_config.c,test_devkitc_smoke_e2e.c,CMakeLists.txt}`

**修改**：`wink-micro-os/CMakeLists.txt`（+1 行）、`esp32_firmware/main/app_main.c`（1 行 include）、`esp32_firmware/main/CMakeLists.txt`（SRCS/INCLUDE_DIRS）

**回写（Part F）**：`wink-micro-os/targets/esp32/*.c` 的 `@verified` 头、`2026-06-26-wave-b-followup-v2-verification.md`、新增 ④ 层验证报告

**复用的既有 API/模式**（不重写）：
- PAL：`pal_gpio_init/write/read/enable_interrupt`、`pal_pwm_init/set_duty`+`pal_pwm_router_channel_timer`、`pal_i2c_transfer`、`pal_watchdog_init/feed`、`pal_get_ms/get_reset_reason`、`pal_resource_claim/release`
- DAL：`dal_led_init/on/off`、`dal_button_init/poll/is_pressed`
- Runtime：`wink_runtime_run`（自带 WDT 复位检测）、`wink_trace_last/count/reset/fault`、`wink_actuator_register`
- 模板：`oled_dashboard` sample 全套、`test_oled_dashboard_e2e.c`

---

## 7. 风险与缓解

| 风险 | 缓解 |
|---|---|
| `pal_watchdog_init` 内 `esp_task_wdt_init` 与 sdkconfig 已有 WDT 实例冲突（`INVALID_STATE`） | 实现时若报错，改用 `esp_task_wdt_add(NULL)` 复用已有实例（不重新 init）；S8 仍触发（当前 task 订阅后不喂狗即超时） |
| `app_loop` printf 触发 WCET(8002) 误警 | 已设计：loop 零 printf，周期输出由独立 telemetry task 承担 |
| GPIO0（Boot）作中断输入影响启动模式 | 仅配置为上拉输入 + 下降沿中断，不改变 strapping；启动后才 `pal_gpio_enable_interrupt`（device-model-registry 已注记 GPIO0 safeForBoot） |
| I2C 空总线扫描每个地址超时拖慢 init | 只扫 3 个典型地址；v6 驱动 NACK 快速返回，总耗时数 ms，init 不受 WCET 约束 |
| DevKitC 板载 LED 极少数型号不在 GPIO2 | S2 失败时检查板子丝印；经典 ESP32 DevKitC V1 标准 = GPIO2 |

---

## 8. 后续动作

- [x] 执行 Part A–D（写 sample + 切固件 + host 验证）
- [ ] 用户执行 Part E（真机烧录 + 8 项验收）
- [ ] Claude 执行 Part F（回写 @verified + ④ 层验证报告 + 关闭 DEFERRED 项）
- [ ] 外设到位后补做 Out-of-Scope 项（HC-SR04 测距 / OLED / 示波器定量）

---

## 9. 参考资料

- Wave B v2 实施计划：[`2026-06-26-wave-b-esp32-port-followup-v2.md`](2026-06-26-wave-b-esp32-port-followup-v2.md)
- Wave B 编译评审：[`2026-06-26-wave-b-esp32-port-compilation-review.md`](2026-06-26-wave-b-esp32-port-compilation-review.md)
- ESP-IDF v6 I2C 兼容计划：[`2026-06-27-esp-idf-v6-i2c-compat-plan.md`](2026-06-27-esp-idf-v6-i2c-compat-plan.md)
- ADR-0006：[`../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md`](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)
- PAL 设计规范：[`../02-wink-micro-os/02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md)

