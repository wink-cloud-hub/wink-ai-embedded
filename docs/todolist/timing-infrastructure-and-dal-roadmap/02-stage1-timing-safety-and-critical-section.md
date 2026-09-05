# 阶段 1 计划：微秒级时序安全、微临界区加固与 HX711 掉电修复

| 元数据项 | 说明 |
| :--- | :--- |
| **阶段编号** | STAGE-1-TIMING-SAFETY |
| **所属模块** | `wink-micro-os/pal/include/`（`pal_irq.h` / `pal_osal.h`）、`wink-micro-os/dal/src/sensor/dal_load_cell.c`、全 DAL |
| **解决时序类别** | Class 2（单总线握手）/ Class 3（慢速同步串行移位） |
| **依赖 ADR** | [ADR-0001 负数错误码](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0007 协作循环](../../decisions/core/0007-cooperative-loop-execution-model.md)、[ADR-0012 合约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0017 阻塞硬隔离](../../decisions/core/0017-blocking-api-hard-isolation.md) |
| **前置阶段** | [Stage 0](01-stage0-pal-hardware-acceleration-engines.md)（HX711 若实测超标需升级 RMT/SPI 硬件移位） |
| **基线** | 以 [00-master-plan.md §5](00-master-plan.md#5-stage--1-现状审计基线v2-新增) 为准；64 位时钟 API 已存在，本阶段不重建 |
| **状态** | **Ready for Implementation** |

---

## 1. 阶段目标与解决痛点

- 修复 **HX711 24-bit 移位循环无任何临界区保护**（`dal_load_cell.c:167-199`）的掉电复位 Bug。FreeRTOS 1 ms Tick 抢占即可让 SCK 高电平 $>60$ µs，触发芯片掉电。
- 明确两种临界区机制（`PAL_CRITICAL_SECTION` 本核中断屏蔽 vs `pal_os_critical_enter` portMUX 自旋锁）的选用规则；双核场景下前者**不是**跨核互斥。
- 审计全 DAL 的时间戳用法（API 已是 `uint64_t pal_os_get_us()`，无需重建），统一减法范式。
- 把非阻塞三段式状态机（request/poll/get_cached）落到所有耗时 > 1 ms 的 DAL 驱动上。

> **事实校正**：理论指南 Phase 1 里程碑将 HX711 标为 `[x]`，但代码 `dal_load_cell.c:169-199` 未包临界区、未 include `pal_irq.h`。以代码为准，本阶段重新立项。

---

## 2. 任务清单

### T1.1 HX711 修复（实测驱动，非纸面估算）

#### ① 字段对齐实际代码

实际结构体（以 `dal_load_cell.c:208` 为准）使用：

- `dev->config.zero_offset`（非 `tare_offset_raw`）
- `dev->config.calibration_factor`（**除法**：`(raw - zero_offset) / calibration_factor`）
- `dev->last_weight_g`（非 `last_grams`）
- **无** `last_sample_ms` 字段

补丁必须按上述字段写，不得引用不存在的成员。

#### ② 临界区方案

**双核语义**：
- `PAL_CRITICAL_SECTION`（`pal_irq.h:192`）展开为 `pal_irq_save_rtos_safe` / `pal_irq_restore_rtos_safe`，**仅屏蔽本核中断**，不阻止 Core 0 / Core 1 并发。
- `pal_os_critical_enter/exit`（`pal_osal.h:180-198`）基于 FreeRTOS portMUX，是**跨核自旋锁**。

HX711 的 SCK/DT 引脚可能被另一核的任务或 ISR 误触；因此二选一：

1. **方案 A（推荐）**：`pal_os_critical_enter/exit` 跨核保护整段 24+extra 移位（典型 portMUX 持锁 < 100 µs）。
2. **方案 B**：驱动实例级 `pal_spinlock_t`（Stage 0 T0.1a 交付）。在 ESP32 FreeRTOS SMP 上，`pal_spinlock_lock` 展开为 `taskENTER_CRITICAL`，**同时完成跨核自旋 + 本核中断屏蔽**，无需再嵌套 `PAL_CRITICAL_SECTION`。host/wasm 上退化为编译屏障，与单线程协作模型一致。

   ```c
   #include "pal_spinlock.h"
   typedef struct {
       pal_spinlock_t lock;
       /* ... existing fields ... */
   } dal_load_cell_t;

   /* init 中 */
   dev->lock = PAL_SPINLOCK_INITIALIZER;
   pal_spinlock_init(&dev->lock);

   /* request_read 中 */
   pal_spinlock_lock(&dev->lock);
   /* ... 24+extra bitbang ... */
   pal_spinlock_unlock(&dev->lock);
   ```

   > ISR 侧若需抢同一把锁，改用 `pal_spinlock_lock_isr/unlock_isr`（`taskENTER_CRITICAL_ISR`）。HX711 读路径只在任务上下文，用普通版本即可。

优先方案 A，除非实测 portMUX 在 Wi-Fi 吞吐下持锁延迟超标。

#### ③ 实测红线（**必须用 ccount，不接受纸面估算**）

在 ESP32 上用 `xthal_get_ccount()` 测量整段移位耗时：

```c
#include "xtensa/core-macros.h"   /* ESP_PLATFORM only */

uint32_t c0 = xthal_get_ccount();
/* ... critical section with 24 + extra pulses ... */
uint32_t c1 = xthal_get_ccount();
uint32_t cycles = c1 - c0;
uint32_t us = cycles / 240;       /* 240 MHz */
```

- 验收：`us < 100`（微临界区红线）；SCK 高电平单步 $<50$ µs（HX711 60 µs 掉电红线留 10 µs 裕量）。
- **不接受** "24 次循环 × `pal_os_busy_wait_us(1)` ≈ 48~54 µs" 这种纸面估算；GPIO 函数调用开销 + cache miss + FreeRTOS portMUX 开销实测往往显著更大。
- 若实测超标，**立即升级**：24+3 脉冲改由 Stage 0 交付的 RMT TX 通道硬件移位，或用 SPI 主机在 mode-1 下半双工以 1 MHz 时钟移位；DAL 层改为启动 RMT/SPI 事务 + 完成回调，CPU 不参与位级时序。升级路径在本任务内完成，不留 TODO。

#### ④ 参考补丁骨架（方案 A）

```c
#include "pal_osal.h"      /* pal_os_critical_enter/exit, pal_os_get_us */
                             /* 不要 include 不存在的 last_sample_ms 字段 */

wink_status_t dal_load_cell_request_read(dal_load_cell_t *dev) {
    /* ... 参数与 DRDY 检查同现状 ... */

    uint32_t raw24 = 0;
    int extra_pulses = 1;
    if (dev->pending_gain == DAL_LOAD_CELL_GAIN_32)      extra_pulses = 2;
    else if (dev->pending_gain == DAL_LOAD_CELL_GAIN_64) extra_pulses = 3;

    pal_os_critical_enter();   /* 跨核自旋锁；本核仍允许 ISR？见下 */
    for (int i = 0; i < 24; i++) {
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_HIGH);
        pal_os_busy_wait_us(1);
        pal_gpio_level_t bit_level = PAL_GPIO_LEVEL_LOW;
        pal_gpio_read(dev->config.dt_pin, &bit_level);
        raw24 = (raw24 << 1) | (bit_level == PAL_GPIO_LEVEL_HIGH ? 1u : 0u);
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_LOW);
        pal_os_busy_wait_us(1);
    }
    for (int i = 0; i < extra_pulses; i++) {
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_HIGH);
        pal_os_busy_wait_us(1);
        pal_gpio_write(dev->config.sck_pin, PAL_GPIO_LEVEL_LOW);
        pal_os_busy_wait_us(1);
    }
    pal_os_critical_exit();

    int32_t signed_raw = (int32_t)raw24;
    if (raw24 & 0x800000u) signed_raw |= (int32_t)0xFF000000u;

    dev->last_raw = signed_raw;
    dev->last_weight_g =
        (float)(signed_raw - dev->config.zero_offset) / dev->config.calibration_factor;
    return WINK_OK;
}
```

> 若 `pal_os_critical_enter` 不屏蔽本核中断且系统 tick 为 1 kHz，tick ISR 本身耗时可能让 SCK 高电平超标。此时改用方案 B（portMUX + `PAL_CRITICAL_SECTION` 嵌套）或直接走 RMT/SPI 硬件移位。

#### ⑤ 验收

- [ ] ESP32 ccount 实测：整段移位 < 100 µs，单步高电平 < 50 µs；
- [ ] 开启 Wi-Fi 高频连接 + 双任务背景下连续 10000 次读取，零乱码、零掉电、跳变 < 0.1%；
- [ ] 逻辑分析仪抓取 SCK 波形确认；
- [ ] 若升级为 RMT/SPI，回归同样的 10000 次压测，CPU 占用 < 5%。

### T1.2 微临界区选用规约

在 `.claude/rules/c-code.md` 或新建 `docs/design/02-wink-micro-os/micro-critical-section-policy.md` 落地以下规约：

| 场景 | 机制 | 理由 |
|---|---|---|
| 仅本核 ISR 与任务共享数据（如 GPIO IRQ 回调累加） | `PAL_CRITICAL_SECTION` | 只屏蔽本核中断，开销低 |
| 双核共享数据 / 多核任务间互斥 | `pal_os_critical_enter` + `portMUX` | 跨核自旋锁 |
| ISR 与任务跨核共享 | portMUX（FromISR 后缀）+ 必要时本核中断屏蔽 | portMUX 自旋本身不屏蔽本核中断 |
| 临界时间可能 > 100 µs | **禁止关中断**；改状态机 / 硬件引擎 | 红线 2 |

**三禁止（任一临界区）**：
1. ❌ `pal_log*`（可能触碰 Flash 字符串 + 互斥锁）
2. ❌ `malloc/free`
3. ❌ `pal_delay_ms` 或任何可能让出 CPU 的 API

**数据后处理移到临界区外**：CRC、浮点换算、滤波、状态机转移全部在退出临界区后执行。

**Host/Wasm 断言**：
- Host target 下 `PAL_CRITICAL_SECTION` 展开为朴素的信号量锁定或空（若不模拟抢占），并在调试编译中 `assert(!pal_log_locked_from_isr())` 之类的契约检查；
- Wasm target 为单线程协作循环，`pal_os_critical_enter` 展开为编译屏障 `__atomic_signal_fence(__ATOMIC_ACQ_REL)`；
- 三禁止在 host 编译期通过 `-Werror=deprecated-declarations` 或 wrapper 宏捕获（例如把 `malloc` 包成 `pal_malloc` 并在临界区内禁止）。

**ISR 路径编译期强制（红线 8，master §7）**：
- `PAL_ISR` / `PAL_DEFINE_ISR` 已存在于 `pal_irq.h:83-113`。本任务增加编译期检查：
  1. `PAL_ISR` 修饰的函数在 Debug 构建中通过 GCC `__attribute__((error("pal_log called from PAL_ISR")))` 包装的 `pal_log*` inline 拦截（或 `-Werror=deprecated-declarations` 配合 `__attribute__((deprecated))` 在 ISR TU 中启用）；
  2. CI 脚本 `scripts/check_isr_no_log.sh` 对 `targets/esp32/*.o` 做 `xtensa-esp32-elf-nm -u`，扫描 `PAL_ISR` 函数体可达的 `pal_log*` / `malloc` / `vprintf` / `xQueueSend`（非 FromISR 变体）符号，违规 PR 失败；
  3. 对 `PAL_CRITICAL_SECTION({...})` 块做同型扫描（静态或基于编译器 plugin；第一阶段用正则 + code review 兜底）。
- 实现成本评估：若 GCC `error` 属性方案过脆（LTO 会内联消除），改用 link-time wrapper：把 `pal_log` 重命名为 `pal_log__forbidden_in_isr`，在 ISR TU 中 `#define pal_log pal_log__forbidden_in_isr`，链接时未解析符号即失败。方案由实现者选，PR 中说明取舍。

### T1.3 全 DAL 64 位时钟减法范式审计（API 已存在，不重建）

**事实**：`pal_os_get_us()` 已返回 `uint64_t`（`pal_osal.h:48`），底层 `esp_timer_get_time()`，不存在 32 位 µs 时间戳。旧文档"标准化 64 位时钟"任务方向正确但范围写错。

**本任务改为审计**：

- 全仓 `grep -rn "pal_os_get_us\|pal_os_get_ms\|esp_timer_get_time" wink-micro-os/dal/`：
  - 禁止直接比较 `now > start + timeout`（虽然 uint64 不会在 71.5 分钟翻转，但减法范式更稳健且与代码库统一）；
  - 统一写法：`if (pal_os_get_us() - start_us > timeout_us) return WINK_ERR_TIMEOUT;`
  - 禁止缓存 `uint32_t now = ...` 截断 64 位时间戳；
  - 禁止裸 `esp_timer_get_time()`（必须走 PAL）。
- `pal_os_get_ms()` 同样返回 `uint64_t`（`pal_osal.h:42`），减法范式同上。
- Host/Wasm 的 `pal_os_get_us()` 必须由 UniSim 虚拟时钟驱动（`pal_osal_wasm.c` 已是），不得用宿主 `clock_gettime`。

**验收**：
- [ ] `grep -rnE "uint32_t[[:space:]]+\w*(us|ms|tick)\w*[[:space:]]*=[[:space:]]*pal_os_get" wink-micro-os/dal/` 零命中；
- [ ] 所有超时判定为减法形式；
- [ ] 新增 host Unity 测试：模拟 71.5 分钟边界（`wink_vclock_advance_internal` 推过 $2^{32}$ µs），所有 DAL 超时逻辑无死锁。

### T1.4 非阻塞三段式状态机（ADR-0017）

对所有耗时 > 1 ms 的 DAL（EEPROM 写、超声波测距、DHT 采集、HX711 RMT/SPI 升级路径、SD 卡、GPS 定位）统一接口：

```
dal_xxx_request_read(&dev)              -> WINK_OK / WINK_ERR_BUSY
dal_xxx_poll(&dev)                      -> WINK_OK / WINK_ERR_BUSY / WINK_ERR_TIMEOUT
dal_xxx_get_cached(&dev, &out_physical) -> WINK_OK
```

- `request` 仅触发硬件开始（DMA / RMT / 定时器），立即返回；
- `poll` 在 10 ms 协作 tick 里被 App 调用，检查 DRDY/完成队列/软中断，**不阻塞**；
- `get_cached` 零硬件访问，返回最近一次成功采样。

**`WINK_STRICT_NONBLOCKING` 编译期开关**：定义后任何 `pal_os_busy_wait_us(>5)` / `pal_delay_ms` 出现在 DAL 同步路径即编译失败。`WINK_BLOCKING` 路径（CLI 工具、一次性校准）显式 opt-in。

**验收**：
- [ ] 所有 Class 2/3 DAL 提供 request/poll/get_cached；
- [ ] `WINK_STRICT_NONBLOCKING` 下 DAL 编译无 `pal_delay_ms` / 长 `busy_wait`；
- [ ] App loop 单 tick WCET < 2 ms（红线 1）。

---

## 3. 验收门槛（进入 Stage 2 前置）

- [ ] HX711 ccount 实测达标或已升级 RMT/SPI；
- [ ] 微临界区规约文档落地并被 `.claude/rules/c-code.md` 引用；
- [ ] DAL 全量时间戳审计通过；
- [ ] 三段式状态机在所有 Class 2/3 驱动落地；
- [ ] Host Unity + Wasm 契约测试通过；
- [ ] `python wink-tools/wink.py lint --pack layering --pack api --pack dal` 零错误；
- [ ] PR Gate 红线 1/2 静态检查项接入 CI。
