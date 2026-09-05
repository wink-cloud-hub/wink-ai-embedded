# Review：WinkMicroOS PAL HAL 最佳实践演进与解耦重构计划

| 项 | 内容 |
| :--- | :--- |
| **创建日期** | 2026-08-24 |
| **评审对象** | [`docs/implementation-plans/core/2026-08-24-pal-hal-best-practice-refactoring-plan.md`](../../implementation-plans/core/2026-08-24-pal-hal-best-practice-refactoring-plan.md) |
| **评审人角色** | 资深嵌入式系统架构师 |
| **评审基线** | 仓库实码：`wink-micro-os/pal/include/`、`wink-micro-os/targets/{esp32,host,wasm}/`、`wink-micro-os/dal/`、`wink_status.h`、`pal_resource.h`、ADR-0012 / ADR-0041 |
| **状态** | **Frozen（归档快照，不再修改）** |
| **关联 ADR** | [ADR-0004 静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0012 合约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0017 阻塞 API 隔离](../../decisions/core/0017-blocking-api-hard-isolation.md)、[ADR-0034 渐进式配置](../../decisions/core/0034-dal-progressive-config-disclosure.md)、[ADR-0041 HAL/OSAL 正交](../../decisions/core/0041-hal-osal-directory-orthogonality.md)、[ADR-0043 YAML 分层 lint](../../decisions/tools/0043-yaml-driven-layer-lint.md) |

> 本评审为时间点快照。后续若原计划修订，应新建 review 记录，不修改本文件。

---

## 0. 总评

**方向 8/10，落地细节 4/10。**

方向正确：解耦单体 `pal_hal.h`、定点 PWM、I2C 超时、防毛刺初始化、umbrella 向后兼容。但计划多处与仓库现状对不上，包含真 bug（整数溢出、命名错误），隐含 breaking change 未声明，跳过必要 ADR，WASM ABI 与资源归属两大块缺失。

**建议结论：当前版本不可直接执行。先合 Phase 0（5 个前置 ADR + 清理 + IDF v6.0 spike），再按修订后的 Phase 1–3 推进。**

---

## A. 必须修的硬伤

### A1. 命名错误：`permille` 是千分比，不是万分比

计划中 `pal_pwm_set_duty_permille(ch, permille)` 文档声称 `0..10000 = 0.00% ~ 100.00%`。

- permille = ‰ = 千分比，范围 0..1000。
- 万分比 = permyriad / basis point（bp / ‱），0..10000。

后果：DAL 作者按名字传 500 表示 50%，实际得到 5%。

**建议：**
- 改名 `pal_pwm_set_duty_bp(ch, basis_points)` 或 `pal_pwm_set_duty_per10k`。
- 参数名 `basis_points`，doxygen 明确 "1 bp = 0.01%"。
- 全计划文本、测试、迁移指南统一。

---

### A2. PWM 整数映射溢出 + 截断（真 bug）

计划写内部直接 `(1<<bits)-1` 乘 `0..10000`。ESP32-S3 LEDC 最高 20-bit → top = 1,048,575；`1,048,575 * 10,000 = 10,485,750,000`，超过 UINT32_MAX (4,294,967,295)。14-bit 以上即溢出。

后果：高频高分辨率下 duty 计算 wrap，输出错误占空比，电机 / 舵机失控。同时未做四舍五入，1-bit 分辨率下 50% 被截成 0。

**建议：**

```c
uint64_t numer = (uint64_t)basis_points * (uint64_t)top;
uint32_t cmp   = (uint32_t)((numer + 5000u) / 10000u); /* round-to-nearest */
if (cmp > top) { cmp = top; }
```

- `_Static_assert` 或 runtime assert 限制 `bits <= 20`。
- doxygen 写明量化误差上限 < 1 count。
- 测试覆盖 bits = 1..20 全矩阵。

---

### A3. "零软浮点"目标在当前迁移范围下达不成

计划新增定点接口，但**保留** `pal_pwm_set_duty(float)` 作为 public API。现状生产代码仍有 7 处传 float：

- `dal/src/output/dal_buzzer.c`（3 处，含 `50.0f`）
- `dal/src/actuator/dal_rc_servo.c`（3 处）
- `dal/src/actuator/dal_dc_motor.c`（1 处）

selftest 还有 3 处：`selftest_rmt_loopback.c:123`、`selftest_pwm_router.c:50,51`。

链接器只要看见任一 float 符号就会拉入 `__aeabi_f2iz` / `__aeabi_fdiv` / `__aeabi_fmul`，2–4 KB Flash 节省等于 0。

**建议：**

1. `pal_pwm_set_duty(float)` 标 `WINK_DEPRECATED_MSG("use pal_pwm_set_duty_bp")`（注意：**不是** `WINK_BLOCKING`，语义不同）。
2. Phase 3 迁移全部 7 + 3 处：buzzer 用 0/5000/10000 bp；servo 用整数角度→bp；dc_motor 同理。
3. 加编译开关 `PAL_PWM_HIDE_FLOAT_API`，新固件默认开启，过渡期可关。
4. 验收命令：

   ```bash
   arm-none-eabi-nm --size-sort build/firmware.elf | grep -E '__aeabi_f|__aeabi_d|__gnu_f2'
   ```

   必须为空。
5. CI 开启 `-Wdouble-promotion -Wfloat-equal`，禁止新增 float。

---

### A4. `WINK_BLOCKING` 已经是 deprecation 属性，新 API 一出生就被弃用

`wink_status.h:41`：

```c
#define WINK_BLOCKING \
    WINK_DEPRECATED_MSG("Blocking API forbidden in cooperative runtime; use non-blocking variant")
```

计划给 `pal_i2c_transfer_timeout`、`pal_i2c_transfer`、`pal_i2c_scan`、`pal_gpio_pulse_in` 全打 `WINK_BLOCKING`，每个 TU 都会触发 `-Wdeprecated-declarations`（host 实现现在就靠 `#pragma GCC diagnostic ignored` 压着）。

计划称 timeout 是"工业级升级"，但标签说"协作运行时禁用"——自相矛盾。

**建议（二选一拍 ADR）：**

- **方案 A（推荐本次）**：承认同步 I2C 是合法 task-context API，新 timeout 接口**不打** `WINK_BLOCKING`。`WINK_BLOCKING` 只保留给未来真要禁的协作运行时阻塞调用。另起 ADR / roadmap 做 `pal_i2c_transfer_async(...)`，仿 `pal_spi_transfer_dma` / `pal_uart_write_async` 的 completion 回调模型。
- **方案 B（彻底）**：本次直接迁 async，8 处 DAL（eeprom × 2、oled × 6）+ 全部测试改造，工作量与风险另算。

不能两边糊。

---

### A5. 破坏性签名变更未声明

计划中隐含以下 breaking change，但未列入"破坏性变更清单"：

1. `pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz)`
   → `(uint8_t, wink_pin_t, wink_pin_t, uint32_t)`。
   调用点：eeprom、oled、Arduino shim、5+ 测试。`wink_pin_t = int16_t`，signedness 与宽度都变。
2. `pal_gpio_reset_pin` / `pal_pwm_deinit` / `pal_i2c_bus_deinit` 返回 `void`，但底层 `pal_resource_release` 可能因 owner 不匹配失败。按 ADR-0012 合约诚实，应返回 `wink_status_t`。
3. 多个 DAL 把 `wink_pin_t` 强转 `uint16_t`（如 `dal_ultrasonic.c:135`、`dal_relay.c:100`）。签名收紧后这些 cast 要么爆 warning 要么直接错。

**建议：**

- 计划单列"破坏性变更清单"表格：旧签名 → 新签名 → 调用点 → 迁移补丁。
- 版本号 / changelog 体现。
- deinit 类统一返回 `wink_status_t`，release 失败返回 `WINK_ERR_INVALID_ARG` 或 `WINK_ERR_PERMISSION`。
- 清理所有错误 cast。

---

### A6. WASM ABI 完全没提

新增 3 个 C 函数 `pal_gpio_init_output` / `pal_pwm_set_duty_bp` / `pal_i2c_transfer_timeout`，WASM target 必须同步：

- `targets/wasm/wasm_bridge.h` 加 `js_pal_gpio_init_output` / `js_pal_pwm_set_duty_bp` / `js_pal_i2c_transfer_timeout` extern 声明。
- `targets/wasm/exported_runtime_functions.json` 更新导出表。
- JS 侧 `wink_sim_js.js` + `wink_sim_stub.js` 实现。
- `test/wasm/gpio_semantics_link_stubs.c` 加 stub。
- WASM ABI hash 重算（参考 commit `1f51c54 fix(wasm): resolve WASM PAL build errors and symbol conflicts`）。

漏任一项 wasm 链接失败或运行时 missing symbol。

**建议：** T2.1 / T2.2 / T2.3 每个任务下显式列 wasm bridge + json + JS 三件套，加 ABI hash 校验步骤。

---

### A7. 资源归属没拍板（架构缺陷）

当前三套规则并存：

- PWM ESP32 自己在 PAL 内 claim：`pal_hal_pwm_esp32.c:53,60` claim `PAL_RESOURCE_PWM_CHANNEL` + `PAL_RESOURCE_GPIO_PIN`。
- GPIO 自己不 claim，DAL 替它 claim：`dal_led.c:30`、`dal_relay.c:85,92`、`dal_buzzer.c:32,40,78`、`dal_ultrasonic.c:127,131,151`、`dal_load_cell.c:88,99`、`dal_encoder.c:86,93`。
- Arduino shim 又在 `Common.cpp:95` claim 一遍。

计划 T2.2 又说 `pal_pwm_init_ex` 要 claim pin，T2.1 没说 `pal_gpio_init_output` 要不要 claim。双 claim 必冲突（`WINK_ERR_BUSY`），漏 claim 则两驱动同脚不报错。

**建议：** 必须先出 ADR 决定：

- **方案 A（推荐）**：PAL 层谁打开谁 claim（RAII）。`pal_gpio_init*` / `pal_pwm_init*` / `pal_i2c_bus_init` 内部 claim 对应 GPIO_PIN / PWM_CHANNEL / I2C_PORT；DAL 禁止 claim 硬件资源，只 claim 业务级资源（如 `I2C_ADDR`、`SPI_CS`）。批量删 DAL 里 ~14 文件 30 处 GPIO claim。
- 方案 B：PAL 不 claim，硬件资源全归 DAL；PWM ESP32 现有 claim 删除。
- `pal_gpio_init` 没有 owner 参数，统一用 `"pal_gpio"` 或加 owner 参数（breaking，进入 A5 清单）。
- 明确 `pal_resource_claim` 的 owner 重复 claim 语义（同 owner 重入是否 OK）。

不能埋在 T2.2 一句话里。

---

### A8. `pal_target_caps.h` 不存在；容量宏散落且冲突

全仓没有 `pal_target_caps.h`，也没有项目内 `soc_caps.h`。容量宏四处散落：

- `pal_hal.h:20-22`：`PAL_PWM_CHANNELS=8`、`PAL_I2C_PORTS=2`
- `wink_status.h:128-130`：`PAL_PWM_CHANNELS=8`（**重复定义**）
- `pal_resource.h:51-63`：`PAL_PWM_CHANNEL_MAX=8`、`PAL_GPIO_PIN_MAX=50`、`PAL_UART_PORT_MAX=3` 等 12 个
- `pal_wasm_ch1b_pwm.c:17`：`WASM_PWM_MAX_CHANNELS=16`（与合约 8 冲突，违反 ADR-0012）
- `pal_hal_host.c:38`：`HOST_MAX_GPIO_PIN=50`
- `pal_adc.h`：`PAL_ADC_CHANNELS=16`
- `pal_spi.h`：`PAL_SPI_DEV_MAX_PER_BUS=4`

**建议：**

1. 新 ADR：target capability SSOT。
   - `targets/<plat>/pal_target_caps.h`（ESP32 从 `soc/soc_caps.h` 映射；host / wasm 自定）。
   - 公共头只 `#include "pal_target_caps.h"`，禁写 `#ifndef PAL_PWM_CHANNELS #define 8`。
   - CMake 按 target 把对应 caps 头放 include path 首位。
2. 删 `wink_status.h:128` 的 `PAL_PWM_CHANNELS`（状态码头不该塞容量）。
3. 统一命名：`PAL_PWM_CHANNELS` vs `PAL_PWM_CHANNEL_MAX` 二选一。推荐 `PAL_PWM_CHANNEL_MAX`（与 `PAL_UART_PORT_MAX` 一致）。
4. 解决 wasm 16 vs 8：caps 暴露 per-target 值，DAL 通过 `pal_resource_max(PAL_RESOURCE_PWM_CHANNEL)` 运行时查询；或 wasm 裁到 8。
5. 所有 `PAL_*_MAX` 集中到 caps，`pal_resource.h` 只 include。

---

## B. 漏掉的架构决策（需新 ADR）

按 `.claude/rules/docs-adr.md`，以下都是重大设计决策，不能埋在实施计划：

1. **Target Capability SSOT**（见 A8）。
2. **PAL 资源所有权归属**（见 A7）。
3. **PWM 浮点 API 下线时间线**（见 A3）：过渡期 + `PAL_PWM_HIDE_FLOAT_API` + 移除版本。
4. **I2C 同步 / 异步路线**（见 A4）：本次保留 sync timeout，async 进 roadmap。
5. **Include 路径规范**：现存 51 处 `"pal_hal.h"`（裸路径，靠 `pal/include/hal/` 在 `-I` 上）+ 6 处 `"hal/pal_hal.h"`。推荐把 `pal/include/hal/` 从全局 `-I` 去掉，强制 `"hal/..."`，一次迁移 57 处；`pal/include/pal.h:13` 的裸 include 也要改。
6. **Target 源文件命名规范**：ESP32 / host 用 `pal_hal_<periph>_<plat>.c`，wasm 用 `pal_wasm_ch<n>_<periph>.c`（axis 命名）。ADR-0041 冻结了目录但没冻结命名，要么 wasm 改名对齐，要么文档化 axis 约定。

---

## C. API / 语义问题

### C1. 防毛刺初始化不是一句话能搞定

计划称"在 `gpio_config()` 前设输出寄存器"。但：

- push-pull idle-low 先写 0 有效；**open-drain idle-high 要先写 1**。
- input + pullup 切 output 时 pad mux 切换仍有 ns 级抖动，软件无法完全消除。
- 必须同时 disable 中断、清 wakeup、设 `GPIO_PIN_INTR_DISABLE`，否则 config 过程可能误触发 ISR。
- ESP32-C3 / S3 / C5 / H2 的 GPIO / LP_IO 矩阵与经典 ESP32 不同。
- `PAL_GPIO_INPUT_OUTPUT`（bidirectional open-drain）的初始电平语义需定义。

**建议：**

- doxygen 明写"best-effort glitch-free，非硬件保证；安全关键输出（继电器 / CS）需外部 pull / hold 配合"。
- 加 `pal_gpio_set_hold(pin, bool)`（ESP32 `gpio_hold_en` / `gpio_hold_dis`），host / wasm 返回 OK 或 UNSUPPORTED。
- 实现按 mode 分支决定初始电平写入方向。

### C2. 中断优先级参数说了等于没说（违反 ADR-0012）

`pal_gpio_enable_interrupt_ex(prio)` 已存在，但 ADR-0012 G2 / G3 明确记录该 prio 在所有 target 上被忽略。计划继续保留签名不实现。

**建议二选一：**

- 本次实现三 target 优先级映射并加测试；
- 或 doxygen 明写"current targets treat prio as hint only; HIGH may return WINK_ERR_UNSUPPORTED if cannot honor"，codegen 检查。

不能继续静默忽略。

### C3. 缺 `pal_gpio_deinit` 一站式

现状注销要 `disable_interrupt → synchronize_interrupt → reset_pin` 三步，漏 `synchronize` 就 UAF。

**建议：** 加

```c
wink_status_t pal_gpio_deinit(wink_pin_t pin);
/* internally: disable_irq + synchronize_irq + release_resource + reset to high-Z */
```

DAL cleanup 路径只调一个。

### C4. 回调类型重复

`pal_gpio_isr_t = void(*)(void*)` 与 `pal_irq.h` 的 `pal_isr_t` 完全一样。删 `pal_gpio_isr_t`，统一用 `pal_isr_t`。

### C5. I2C 超时语义未定义

计划未明确：

- `timeout_ms` 是整笔传输 wall-clock，还是 per-byte？clock stretching 算不算？
- ESP32 legacy `i2c_set_timeout` 单位是 APB cycle，不是毫秒，换算要写。
- 超时后总线状态：自动发 STOP？发 SCL 9-pulse 恢复？返回 TIMEOUT 时总线可能仍锁死。
- `timeout_ms=0` 既表"用默认"又可能被读成"非阻塞轮询"，歧义。

**建议：**

- doxygen 明确定义 wall-clock 语义 + clock stretching 计入。
- `0` 表默认（`PAL_I2C_DEFAULT_TIMEOUT_MS`），`UINT32_MAX` 表无限。
- 超时后内部自动 bus recover（SCL 9-pulse）或返回 TIMEOUT 但置需恢复状态；加独立 `pal_i2c_bus_recover(port)` 公开 API，不只在 deinit 做。
- host / wasm 仿真要能注超时，不只 stub。

### C6. I2C 地址宽度没留口子

`dev_addr uint16_t` 但实际只支持 7-bit。ESP32 硬件支持 10-bit，Zephyr / Linux 都有 flags 字段。

**建议：** 加 `pal_i2c_addr_width_t` 或 `pal_i2c_transfer_ex(..., pal_i2c_flags_t flags)`。至少 `WINK_ASSERT(addr <= 0x7F)`。现在不留口子以后又 breaking。

### C7. I2C scan 参数校验

`start_addr / end_addr` 是 `uint8_t`，7-bit 上限 127；`bitmap_bytes` 对 0..127 应是 16。加 `INVALID_ARG` 检查：`start > end`、`end > 0x7F`、`bitmap_bytes < ((end >> 3) + 1)`。

### C8. PWM 频率 / 分辨率 auto 算法要写死

`resolution_bits = 0 → auto`：

- 算法：取满足 `src_clk_hz / 2^bits >= freq_hz` 的最大 bits，不超过硬件上限（ESP32 classic 15-bit PWM timer / 20-bit LEDC，要区分）。
- `clock_requirement = STABLE_REQUIRED` 映射哪个时钟？ESP32 有 APB（80 MHz，light sleep 会变）、XTAL（可关）、REF_TICK（1 MHz，稳定但慢）。
- 每个 target doxygen 列支持矩阵。

### C9. `pal_pwm_set_freq` 运行时改频行为

LEDC 改频可能使 duty 越界或分辨率变化。规定：通道 active 时改频内部 reapply 现 duty 到新 top 并返回 OK；或返回 `WINK_ERR_INVALID_STATE` 要求先 deinit。选一种，三 target 一致，加测试。

### C10. level 中断跨 target 支持矩阵

`PAL_GPIO_INTR_LOW_LEVEL / HIGH_LEVEL`：wasm / host 仿真是否支持？ESP32 支持但需要 filter。

- 头文件给支持矩阵表。
- wasm 不支持就返回 `WINK_ERR_UNSUPPORTED`，不能静默当 edge（ADR-0012）。

### C11. 可调用上下文没标注

参考 `pal_spi.h` 风格，每个 API doxygen 加 `@note Task | ISR-safe`：

- `pal_gpio_write / read / init_output` → ISR-safe（init 通常 task）。
- `pal_pwm_set_duty_bp` → ISR-safe（定点最大价值）。
- `pal_i2c_transfer_timeout` → task-only。
- `pal_gpio_init / deinit` → task-only。

### C12. `pal_pwm_config_t` 没 ABI 保护

新 struct 加字段会破 ABI。加：

```c
typedef struct {
    uint32_t size;   /* set to sizeof(pal_pwm_config_t) for forward compat */
    ...
} pal_pwm_config_t;
```

或 `_Static_assert(sizeof(...) == expected, ...)` 锁大小。wasm 边界尤其重要。

---

## D. 跨 target / 仿真保真

### D1. host 单体文件应拆

`targets/host/pal_hal_host.c` 21 KB 塞 GPIO / PWM / I2C / 其他，与 ESP32 一文件一外设风格相反。Phase 2 顺手拆：

- `pal_hal_gpio_host.c`
- `pal_hal_pwm_host.c`
- `pal_hal_i2c_host.c`
- 剩余保留或迁 `pal_hal_misc_host.c`

CMake 源列表同步。

### D2. `pal_pwm_channel_pin` 在 wasm 返回 UNSUPPORTED

`pal_wasm_ch1b_pwm.c:82-91` 直接 `WINK_ERR_UNSUPPORTED`。新 `pal_pwm_init_ex(cfg->pin = WINK_PIN_NC)` 依赖 default map，但 wasm 查不到。

**建议：** wasm 实现 default pin map（仿 host `s_host_pwm_pins[]`）；或头文档明写"wasm 必须显式传 pin，NC 返回 `WINK_ERR_UNSUPPORTED`"，DAL 模板 accordingly。

### D3. wasm 故障注入要扩

`test_i2c_timeout.c` 必须走现有 `pal_wasm_fault` 框架注 `WINK_ERR_TIMEOUT`，不是 host-only。

PWM 定点桥 `js_pal_pwm_set_duty_bp` 直接传整数到 JS，**不要**在 JS 侧再做浮点，否则 wasm 仿真的"同源定点"价值丢失。

### D4. `#ifdef SIMULATION` 范围

CLAUDE.md 明确"bypass 范围收窄"。新 host / wasm 代码在独立 TU，不扩大 bypass。T2.3 wasm 超时不能简单 stub，要仿真 wall-clock + clock stretch，让上层协议代码（OLED / EEPROM）能在 wasm 测到超时恢复路径。

---

## E. 构建 / Lint / 测试

### E1. Lint 规则太弱

`NEW_MODULE_DISALLOW_PAL_HAL_H` 只挡新文件，57 处老引用永不收敛。

**建议：**

- 禁所有裸 `#include "pal_hal.h"`（强制 `"hal/pal_hal.h"`，过渡期）。
- 对 `"hal/pal_hal.h"` 设 baseline count = 57，PR 只许降不许升，CI 比对。
- 新 DAL / App 调 `pal_pwm_set_duty(float)` → error（AI 代码生成护栏）。
- 实时路径禁 float / double：`-Wdouble-promotion -Wfloat-equal`，lint YAML 加 `no-float-in-realtime-path`。
- 确认 `no-direct-pal-in-app-bal` 覆盖新拆分头。
- 落在 ADR-0043 的 `layering` + `api` pack。

### E2. 测试矩阵不够

计划 5 个测试不够。补：

- 并发：两任务同 claim 同一 pin → `WINK_ERR_BUSY`；同 owner 重入语义。
- ISR 上下文调 `pal_gpio_write` / `pal_pwm_set_duty_bp`。
- PWM bp 量化矩阵 bits = 1..20、bp = 0 / 1 / 5000 / 9999 / 10000，覆盖溢出 + round。
- I2C：NACK、arbitration lost、SCL stuck low 恢复、10-bit 地址、bitmap 越界、`timeout_ms = 0 / UINT32_MAX`。
- 三 target parity：同一用例链接 esp32 / host / wasm（wasm 用 node smoke）。
- `_Static_assert` 锁 `pal_pwm_config_t` 大小 / 对齐防 ABI 漂移。
- `test_pal_contract.c` 扩到覆盖所有新 API。
- 负测：`pal_gpio_init_output(pin, INPUT_MODE, ...)` 必须 `INVALID_ARG`。

### E3. 体积验收不严

计划只查两个符号。要：

```bash
arm-none-eabi-nm --size-sort build/firmware.elf | grep -E '__aeabi_[fd]|__gnu_f2|__gnu_d2'
```

输出必须为空。同时：

- 确认 newlib-nano 的 `nano.specs` 没偷拉 `_printf_float`。
- before / after `.map` 文件 diff，给出实际 Flash / RAM delta，不要预估"2–4 KB"。
- 软核配置（`-mfloat-abi=soft`）下编译验证。

### E4. 头文件污染指标有陷阱

§4.2 "纯 GPIO 驱动预处理后无 `pal_pwm_*`"——只要 DAL 还 include umbrella `pal_hal.h` 就做不到。指标必须在 T3.1 完成后才成立，写明前置依赖。验证命令：

```bash
arm-none-eabi-gcc -H -E dal_led.c 2>&1 | grep -c pal_pwm
```

必须为 0。

### E5. ESP-IDF v5.4 vs v6.0 兼容

`pal_hal_i2c_esp32.c` 用 legacy `driver/i2c.h`，IDF v5.2+ 已推 `i2c_master.h`，v6.0 可能删 legacy。计划说要过 v5.4 & v6.0，但未确认。

**建议：** 加 spike：CI 矩阵实际跑 v5.4 + v6.0 nightly；若 v6.0 已删 legacy，本次顺手迁新驱动（或单独 ADR + 阶段）。GPIO / PWM（LEDC）在 v6.0 的 API 变动同样要确认。

### E6. 修已存在的坏测试

`test/unit/dal/test_dal_ws2812_sim.c:26` 用了不存在的 `PAL_GPIO_OUTPUT`（枚举只有 `_PUSH_PULL` / `_OPEN_DRAIN` / `_INPUT_OUTPUT`）。Phase 1 顺手修，否则新 lint / 编译炸。

### E7. 三 target stub 一致性

ESP32 每个 `pal_hal_*_esp32.c` 都有 `#else stub returning WINK_ERR_UNSUPPORTED`。新 API 的 stub 必须在 host / wasm / esp32 三端都有，漏一个链接失败。CI 加 `nm -u` 检查未定义符号。

---

## F. 文档 / 流程 / 时间

### F1. 文档回写清单不全

T3.3 只列两个设计规范。还要更新：

- `docs/dal-development-guide/dal-api-consistency-spec.md:277`（提到 `pal_gpio_init`）。
- `docs/design/07-platform-governance/01-device-model-registry.md` 及外设 YAML（若 codegen 生成 init 调用）。
- `.claude/skills/embedded-best-practice/references/` 内引用 `pal_hal.h` 的段落。
- `pal/include/pal.h` umbrella。
- DAL / codegen 模板（若有模板 emit `#include "pal_hal.h"`）。
- Arduino shim 文档（`frameworks/arduino/`）。

### F2. 安全审查等级没标

- Phase 1：低风险（头文件重组 + umbrella）。
- Phase 2：**高风险**（动 GPIO 初始化时序、PWM 占空比映射、I2C 阻塞路径、中断）。

按 `embedded-best-practice` skill：高风险走完整 12 阶段 safety checklist，输出：

```
Safety review:
- Risk level: High
- Checklist phases run: 1-12
- Findings:
- Fixed:
- Assumptions:
- Commands run:
```

计划每个 T2.x 任务后附此块。

### F3. 工期过于乐观

计划 11 天。实际：三 target × 3 新 API + wasm bridge / JSON / JS 三件套、57 处 include 迁移、7 + 3 处 float 迁移 + 链接验证、caps 头设计 + 资源归属重构（~14 文件 30 处 claim 改动）、5 个新 ADR、IDF v6.0 兼容 spike。

**建议拆：**

- **Phase 0（2–3 天）**：ADR + caps 头设计 + 资源归属决策 + 修坏测试，先合。
- Phase 1（2 天）：契约拆分 + umbrella。
- Phase 2（5–7 天）：三 target 实现 + wasm ABI。
- Phase 3（3 天，可独立迭代）：DAL 迁移 + lint + 文档。

总 12–15 天，Phase 0 是硬前置。

### F4. 回滚策略

- `pal_gpio_init_output`：加法，回滚安全。
- `pal_pwm_set_duty(float)` 改走 bp：行为变更（量化误差可能变）。必须 bit-exact 对照测试（buzzer 50%、servo 全角度扫描）。不过关保留原 float 快路径，加 `PAL_PWM_LEGACY_FLOAT_PATH` 临时开关，下个版本再删。
- `pal_i2c_bus_init` 签名变更：回滚要回所有 DAL 改动，建议单独 commit 便于 revert。
- umbrella 回滚：`pal_hal.h` 保留旧声明副本一个版本（`pal_hal_legacy.h`），紧急回滚切 include。

### F5. doxygen 分组

新头加 `@defgroup pal_gpio` / `@ingroup pal`，和 `pal_spi.h` 一致。`pal.h` umbrella 加 `@dir` 注释。

### F6. SPDX 与头文件风格

示例加了 `SPDX-License-Identifier: Apache-2.0`。先确认现有 `pal_spi.h` / `pal_uart.h` 是否同风格；不要新头一种、老头另一种。

### F7. `pal_hal.h` deprecation 策略

计划标 `@deprecated` 但保留永久。建议加移除版本（如 v2.0），不要"永久保留"；否则 umbrella 永远是 crutch，lint 规则会被绕过。

---

## G. 推荐补充的 Phase 0（决策门，必须先合）

动任何 `.c` 之前：

1. **ADR-XXXX Target Capability SSOT**：caps 头位置、命名、CMake 绑定、`PAL_*_MAX` 统一。
2. **ADR-XXXX PAL 资源所有权**：PAL claim 硬件 pin / channel / port，DAL 只 claim 业务资源；owner 语义。
3. **ADR-XXXX PWM 定点 API 与 float 下线**：`bp` 命名（不是 permille）、溢出算法、float 移除窗口、`PAL_PWM_HIDE_FLOAT_API`。
4. **ADR-XXXX I2C sync / async 路线**：本次保留 sync timeout（不打 `WINK_BLOCKING`），async 进 roadmap；超时恢复语义。
5. **ADR-XXXX Include 路径与 target 文件命名**：强制 `"hal/..."`，wasm axis 命名文档化。
6. **清理**：删 `wink_status.h:128` 重复 `PAL_PWM_CHANNELS`；修 `test_dal_ws2812_sim.c:26` 的 `PAL_GPIO_OUTPUT`。
7. **IDF v6.0 spike**：确认 legacy I2C driver 是否还在，决定是否纳入本次。

Phase 0 合完再开 Phase 1，避免返工。

---

## H. 优先级总结

| 优先级 | 项 | 阻塞 Phase |
| :--- | :--- | :--- |
| **P0** | A1 permille 改名、A2 溢出、A3 float 真下线、A6 wasm ABI、A7 资源归属、A8 caps | 全部 |
| **P0** | G1–G5 五个 ADR | Phase 1 |
| **P1** | A4 BLOCKING 标签矛盾、A5 breaking 清单、C1 防毛刺细节、C5 I2C 超时语义、E5 IDF v6 | Phase 2 |
| **P1** | C2 中断优先级诚实性、C3 `pal_gpio_deinit`、D1 host 拆分 | Phase 2 |
| **P2** | C6 10-bit 地址、C8 / C9 PWM 算法、C10 level 中断矩阵、C11 上下文标注、C12 ABI 保护 | Phase 2 |
| **P2** | D2 / D3 / D4 仿真保真、E1 lint 强化、E2 测试补全、E3 体积验收 | Phase 2 / 3 |
| **P3** | F1 文档回写、F3 工期、F4 回滚、F5–F7 doxygen / SPDX / deprecation | 全程 |

---

## I. 结论

计划抓住了真痛点（SRP、I2C 割裂、PWM 浮点、容量硬编码、glitch、超时），umbrella 向后兼容思路正确。但作为"工业级黄金标准"演进，当前版本**不能直接执行**：

1. 含 2 个功能性 bug（permille 命名、PWM 整数溢出）。
2. 核心目标（零软浮点）在当前迁移范围下达不成。
3. 跳过 5 个必要 ADR，其中 caps 和资源归属是架构级决策。
4. WASM ABI、IDF v6 兼容、breaking change 清单三大块完全缺失。
5. 工期低估，lint / 测试 / 体积验收偏弱。

**推荐路径：先合 Phase 0（5 ADR + 清理 + IDF v6 spike），再按修订后的 Phase 1–3 执行。** 修订时把 A 节全部纳入，B / C / D / E / F 按优先级 pick。

---

*本评审为 2026-08-24 时间点快照。原计划修订后应新建 review 文件，不修改本归档。*
