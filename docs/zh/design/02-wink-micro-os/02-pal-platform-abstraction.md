# 3.2 平台抽象层 (PAL) API 设计规范 (OSAL & HAL)

平台抽象层 (PAL, Platform Abstraction Layer) 是 WinkMicroOS 内核屏蔽芯片物理差异、操作系统差异的统一契约接口层。

---

## 1. PAL 架构分层设计思想

PAL 主要由两大部分组成：
1. **HAL (Hardware Abstraction Layer / 硬件总线与外设接口)**：统一抽象 GPIO、PWM、I2C、SPI、ADC 等基础通信总线的初始化、读写与中断。
2. **OSAL (OS Abstraction Layer / 操作系统与内核环境抽象)**：统一抽象高精度系统 Tick、微秒/毫秒阻塞挂起、线程同步互斥锁等内核服务。

```text
  ┌────────────────────────────────────────────────────────┐
  │                 器件抽象层 (DAL Drivers)                │
  └───────────────────────────┬────────────────────────────┘
                              │ 调用
                              ▼
  ┌────────────────────────────────────────────────────────┐
  │             平台抽象层 (PAL) [统一契约定义]             │
  │   - pal_hal.h (总线/外设)    - pal_osal.h (系统服务/同步)  │
  └───────────────────────────┬────────────────────────────┘
                              │
            ┌─────────────────┴─────────────────┐ (CMake 静态装配路由 - ADR-0041)
            ▼                                   ▼
  ┌──────────────────┐                ┌──────────────────┐
  │ 硬件外设适配层   │                │ 操作系统适配层   │
  │ (TARGET_PLATFORM)│                │ (WINK_OSAL_TYPE) │
  │  - targets/wasm  │                │  - osal/wasm     │
  │  - targets/esp32 │                │  - osal/freertos_│
  │  - targets/host  │                │    esp32         │
  │  - targets/stm32 │                │  - osal/host     │
  └──────────────────┘                └──────────────────┘
```

为了实现极致的执行效率，PAL 在真机模式下**不采用**动态 C++ 虚函数表（vtable）或 C 语言运行期函数指针注册的多态形式。而是通过 **CMake 静态条件编译绑定**，利用 `TARGET_PLATFORM` (代表硬件平台/HAL) 与 `WINK_OSAL_TYPE` (代表操作系统适配/OSAL) 两个维度的正交组装（详见 [ADR-0041](../../decisions/core/0041-hal-osal-directory-orthogonality.md)），使上层调用在编译期直接路由绑定到对应平台的物理代码实现，做到**“零运行期封装开销”**。

---

## 1.1 核心架构决策摘要（ADR 回写）

> 本节是 [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0025](../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md) 的正式回写，为系统单一事实来源。

| ADR 编号 | 决策主题 | 核心约定 |
|---------|---------|---------|
| **ADR-0001** | 负数错误码约定 | ✅ 所有可能失败的函数返回 `wink_status_t` <br> ✅ `0 = WINK_OK = 成功` <br> ✅ **负数 = 错误**（如 `-1 = WINK_ERR_INVALID_ARG`） <br> ✅ 检查范式：`status < 0` 或 `wink_status_is_error(status)` <br> 📘 **每一码语义 / 恢复策略 / 是否可作 `WINK_PT_EXIT` 条件**：见 [错误模型规范 §11](../07-platform-governance/02-error-fault-model.md#11-ai-codegen-错误码语义详表)（SSOT） |
| **ADR-0002** | 双 target 同源编译 | ✅ 一份 C 代码同时编译到 Emscripten/Wasm32 与 ESP-IDF/xtensa <br> ✅ CMake 按目标平台静态路由实现文件，零运行期开销 <br> ✅ 仿真代码严格隔离于 `targets/*/` + `#if defined(SIMULATION)`，真机零编译污染 |
| **ADR-0004** | 编译期静态分发 | ✅ **禁止虚函数表（vtable）**、禁止运行期 `ops` 函数指针表、禁止 `container_of` 强转 <br> ✅ 采用「命名式 API + POD 结构体」范式，所有调用在编译期静态绑定 <br> ✅ DAL 外设实例为纯数据结构体，由具名函数操作（如 `dal_rc_servo_init(&servo, pin)`） |
| **ADR-0006** | ESP-IDF v6.x I2C 兼容 | ✅ ESP-IDF v5.x → v6.x I2C API 破坏性变更由 PAL 层抹平 <br> ✅ MVP 固定 GPIO 映射（I2C0: 21/22, I2C1: 33/32），Phase 2 可配置化 |
| **ADR-0012** | 契约诚实 > 静默降级 | ✅ PAL/HAL 头文件承诺必须与所有 target 实现对齐；某 target 无法兑现时 **显式返回 `WINK_ERR_UNSUPPORTED`**，禁止静默降级 <br> ✅ 跨 target 行为差异必须在头文件 doxygen 里显式标注 <br> ✅ 新增/修订 PAL/HAL 接口时，必须做"target 能力矩阵"评估（能兑现 / 拒接 / 头文件下调三选一） <br> 📘 已作为原则贯穿 ADR-0015 / ADR-0016 / ADR-0018 |
| **ADR-0025** | App 阻断诚实性 pragma 规范 | ✅ **抑制警告最小化**：禁止在 App 事件回调或主 loop 裸写 file-scope 抑制 pragma。<br> ✅ **编译期抑制宏**：PAL / Runtime 提供 `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END`（BAL内部）与 `WINK_INIT_BLOCKING_REGION_BEGIN/END`（App init一次性诊断）语义宏封装。<br> ✅ **Wasm STRICT_NONBLOCKING=1**：仿真 target 默认强制开启，在编译和链接期 fail-fast 拦截非法阻塞调用。 |
| **ADR-0047** | FOC ISR 分层与 `pal_hwtimer` | ✅ 公共契约 `pal_hwtimer_*` + PWM–ADC 硬件触发（禁止长期 target 野路子）<br> ✅ 回调 ABI：**IRAM-safe**（ESP-IDF `IRAM_ATTR`）；禁 flash 访问 / `pal_log` / malloc / 阻塞（R-008）<br> ✅ 两类 ISR 分列注册：周期控制 vs nFAULT 保护（R-007）<br> ✅ 实现挂 Wave C；本节 §2.2 为契约草案（可先文档，不必立刻落 `.h`） |
| **ADR-0064** | Target Capability SSOT 体系 | ✅ 统一头文件 `hal/pal_target_caps.h` 作为各 Target 硬件规格与通道容量单一事实来源 <br> ✅ 消除各模块硬编码上限与重复宏定义 |
| **ADR-0065** | PAL 独占硬件生命周期 RAII 资源所有权 | ✅ PAL 初始化/反初始化自动独占物理资源（Pin、PWM 通道、I2C 端口），提供防毛刺 `pal_gpio_init_output`、`pal_gpio_set_hold`、`pal_gpio_deinit` <br> ✅ DAL/上层禁止直穿申领物理硬件 Pin |
| **ADR-0066** | PWM Basis Points (bp) 定点规范与软浮点下线 | ✅ 统一定点万分比 `0..10000` (`bp`)，杜绝 20-bit 32位乘法溢出与 Cortex-M0/M3 软浮点库代码膨胀 <br> ✅ 浮点 `pal_pwm_set_duty` 弃用并由 `PAL_PWM_HIDE_FLOAT_API` 条件门控 |
| **ADR-0067** | I2C 同步超时恢复与异步 DMA 演进 | ✅ `pal_i2c_transfer_timeout` 显式超时防护，提供 SCL 9脉冲总线防死锁恢复 `pal_i2c_bus_recover` |
| **ADR-0068** | 模块化 `"hal/..."` 强制包含与 Umbrella 聚合 | ✅ 严格通过 `"hal/pal_gpio.h"` 等包含细粒度接口，`pal_hal.h` 仅作为向下兼容聚合头文件 |

---

## 2. HAL 抽象总线接口规范 (`pal_hal.h`)

`pal_hal.h` 定义了最简但足够支撑大部分物联网与传感器场景的硬件控制器接口。

### 2.1 完整 API 定义

```c
#ifndef PAL_HAL_H
#define PAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"   /* Phase 3：失败型 API 返回 wink_status_t (ADR-0001) */

/* --- 1. GPIO 数字输入输出 --- */

typedef enum {
    PAL_GPIO_INPUT,                 ///< 数字输入，默认浮空
    PAL_GPIO_INPUT_PULLUP,          ///< 数字输入，内部上拉
    PAL_GPIO_INPUT_PULLDOWN,        ///< 数字输入，内部下拉
    PAL_GPIO_OUTPUT_PUSH_PULL,      ///< 推挽输出
    PAL_GPIO_OUTPUT_OPEN_DRAIN      ///< 开漏输出
} pal_gpio_mode_t;

typedef enum {
    PAL_GPIO_INTR_DISABLE,          ///< 禁用中断
    PAL_GPIO_INTR_RISING_EDGE,      ///< 上升沿触发
    PAL_GPIO_INTR_FALLING_EDGE,     ///< 下降沿触发
    PAL_GPIO_INTR_ANY_EDGE          ///< 双沿触发
} pal_gpio_intr_t;

typedef void (*pal_gpio_isr_t)(void *arg);

/**
 * @brief 初始化 GPIO 引脚配置
 * @note 失败型 → wink_status_t（ADR-0015）；read/write 已同步 wink_status_t + out-param。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode);

/**
 * @brief 写入 GPIO 引脚输出电平（ADR-0015，v2.3 契约）
 * @note 失败型 → wink_status_t：
 *   - WINK_ERR_INVALID_ARG：pin 越界（ESP32 目标额外 `GPIO_IS_VALID_GPIO` 校验）
 *   - WINK_ERR_INVALID_STATE：pin 未通过 `pal_resource_claim` 登记（host/wasm 强校验，
 *     把冲突从真机偶发提前到 host 单测暴露；ESP32 视 target 能力启用）
 *   - WINK_ERR_IO：硬件层写失败（ESP32 捕获 `gpio_set_level` 的 `esp_err_t`）
 *   - 静默丢弃 write 失败被 ADR-0015 §决策 §6 明确禁止
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_write(uint16_t pin, bool level);

/**
 * @brief 读取 GPIO 引脚输入电平（ADR-0015，v2.3 契约）
 * @note out-param：*out_level 仅在 `WINK_OK` 时有效；错误路径下由实现在**函数最开始**
 *       显式写入 `false`（防御性兜底，即便调用方忽略返回码也不 UB）。
 *       失败型 → wink_status_t：
 *   - WINK_ERR_INVALID_ARG：pin 越界 / out_level == NULL（`GPIO_IS_VALID_GPIO` 校验）
 *   - WINK_ERR_INVALID_STATE：pin 未 `pal_resource_claim` 登记（同 write）
 *   - WINK_ERR_IO：硬件层读失败
 *   - WINK_ERR_DISCONNECTED：host/wasm 上 `PAL_GPIO_INPUT`（无内部上下拉）且仿真未注入外部电平（ADR-0034）；*out_level 不可用
 * @note 与"读到低电平（*out_level = false, WINK_OK）"必须**严格可辨**；不允许用
 *       false 混淆错误。
 * @note host `pal_gpio_read` 保留"echo 边沿虚拟时间推进"副作用（ADR-0015 §5），
 *       签名重构后行为不变、只是错误码路径新增；`test_host_pal.c` 依赖此机制。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_read(uint16_t pin, bool *out_level);

/**
 * @brief 配置并启用 GPIO 引脚中断
 *
 * v2.3 契约（2026-07-02，ADR-0018 收窄落地）：
 * - 各 target 共享一个 GPIO ISR dispatch service；prio 采用**首次注册锁定**语义。
 * - 首次注册：底层 install service，硬件优先级绑定到映射后的 flag（ESP32:
 *   `ESP_INTR_FLAG_LEVELn | IRAM`；host/wasm 仅记录状态）。
 * - 后续注册：prio 与首次一致 → 正常注册；不一致 → 返回 WINK_ERR_INVALID_ARG。
 * - 一旦锁定，进程生命周期内不再释放（disable 也不解锁；拒绝
 *   disable→uninstall 方案，规避 TOCTOU / SMP UAF）。
 * - 优先级枚举收窄到 3 级：`PAL_IRQ_PRIO_LOW / NORMAL / HIGH`。旧的
 *   `LOWEST / HIGHEST / REALTIME` 已由 ADR-0018 删除；三级均为 RTOS 安全
 *   （可调 xxxFromISR API）。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t intr_type, pal_gpio_isr_t callback, void *arg);

/**
 * @brief 启用 GPIO 引脚中断（扩展版，显式指定 prio）
 * @note 见 pal_gpio_enable_interrupt 的 v2.2 契约段；非 ex 版内联到本函数并传入
 *       PAL_IRQ_PRIO_NORMAL —— 若之前用非 ex 版注册，会隐式锁定为 NORMAL。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_enable_interrupt_ex(uint16_t pin, pal_gpio_intr_t intr_type, pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg);

/**
 * @brief 禁用 GPIO 引脚中断（**不释放 prio 锁定**）
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_disable_interrupt(uint16_t pin);

/**
 * @brief 捕获引脚脉冲宽度（过渡 capture API；Phase 4 Task 4-2）
 * @note 失败型 → wink_status_t：pulse_us NULL → INVALID_ARG；echo 起始 > timeout → TIMEOUT；
 *       无 pin 映射 → UNSUPPORTED。禁从 App/runtime tick 直接调用（仅供非阻塞 DAL 过渡）。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);


/* --- 2. PWM 控制器抽象 (调光/电机/舵机) --- */

/**
 * @brief PWM 时钟需求（ADR-0034）
 * AUTO：平台选择（ESP32: LEDC_AUTO_CLK）
 * STABLE_REQUIRED：必须兑现的 DFS-stable 源（ESP32: LEDC_USE_REF_TICK）；
 *   host/wasm → WINK_ERR_UNSUPPORTED。不保证 Light-sleep keep-alive。
 */
typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    freq_hz;          /* >0 */
    uint8_t                     resolution_bits;  /* 0 = AUTO → ESP32 默认 13 */
    pal_pwm_clock_requirement_t clock_requirement;
} pal_pwm_config_t;

/**
 * @brief 初始化指定通道的 PWM（兼容薄包装，≡ init_ex with AUTO/13-bit）
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

/**
 * @brief 扩展初始化：频率 + 分辨率 + 时钟需求（ADR-0034）
 * @note target 先解析 AUTO → effective timer profile，再交给 profile-aware Router。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);

/**
 * @brief 设置指定通道的 PWM 占空比
 * @param channel 逻辑 PWM 通道号
 * @param duty_cycle_percent 占空比百分比 (0.0f 到 100.0f) —— **跨 target 公共语义始终为百分比**
 * @note ESP32 按该 channel 所属 timer 的 effective resolution 换算 raw duty；
 *       host/wasm 保持百分比观测（不改 sim_last_pwm_duty / JS bridge）。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);


/* --- 3. I2C 串行总线抽象 --- */

/**
 * @brief I2C 双向传输接口 (屏蔽寄存器重发、起始/结束位物理时序)
 * @param port 逻辑 I2C 端口号
 * @param dev_addr 目标从机 I2C 地址 (7位/10位)
 * @param write_buf 待写入数据缓冲区，为 NULL 则不写入
 * @param write_len 待写入数据长度，为 0 则不写入
 * @param read_buf 待读取数据缓冲区，为 NULL 则不读取
 * @param read_len 待读取数据长度，为 0 则不读取
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len);

#endif // PAL_HAL_H
```

### 2.2 `pal_hwtimer` / PWM–ADC sync 契约草案（ADR-0047）

> **状态**：契约草案（Task B2 回写）。**不**在本波交付生产 `.h` / 真机实现；Wave C（Task C2）落地头文件与 target 实现。若提前落 stub 头，须仅注释 + 未实现路径返回 `WINK_ERR_UNSUPPORTED`（ADR-0012），禁止假实现。  
> **Scope**：仅 **SimpleFOC 本地算法型**；`dal_vesc` / ODrive 协议驱动不消费本契约。决策全文：[ADR-0047](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md)。

#### 2.2.1 公共契约方向

| 项 | 约定 |
|---|---|
| API 形态 | 公共 `pal_hwtimer_*`（命名最终以 C2 头文件为准）：init / start / stop / register_isr / bind_pwm_adc_trigger 等 **命名式静态 API**（ADR-0004），禁止运行期 ops 表 |
| 用途 | (a) 周期控制快环定时（典型 $10\text{kHz}+$）；(b) 与 PWM 定时器联动的 ADC 采样硬件触发 |
| Target 矩阵 | ESP32：MCPWM / LEDC+ADC 数字触发等；host/wasm：虚拟时间确定性软步进（降级，见 DAL §8.3.2 / R-009）；无能兑现的 target → `WINK_ERR_UNSUPPORTED` |
| 禁止 | 长期把 FOC 定时器 / ISR 注册藏在 target 私有野路子；在 BAL 公共头暴露本契约符号 |

#### 2.2.2 回调 ABI（R-008）— 施工红线

`pal_hwtimer` 注册的用户回调（含 DAL/target `foc_isr_trampoline` 入口）必须：

| # | 要求 | 说明 |
|---|---|---|
| 1 | **IRAM-safe（ESP-IDF）** | 回调与其所调热路径须 `IRAM_ATTR` / 放入 IRAM；可用 `PAL_ISR` 属性宏（见 §3.3） |
| 2 | **禁止 flash 访问** | 禁从 flash 取指或读 `.rodata`（cache 失效期会崩） |
| 3 | **禁止 `pal_log`** | 日志可能触 flash / 阻塞 |
| 4 | **禁止 malloc / free** | 含隐式堆分配 |
| 5 | **禁止阻塞** | 禁 `pal_delay_*`、阻塞 mutex、busy-wait、同步总线 I/O |
| 6 | **有限栈** | 与 BAL 快环约束一致；栈预算由 trampoline / C2 标注 |

#### 2.2.3 两类 ISR 注册入口与优先级（R-007）

| 类 | 角色 | 注册方向（草案） | 优先级 / 时延 |
|---|---|---|---|
| **周期控制 ISR** | 跑 BAL 纯数学 + 读写 DAL 积木 | `pal_hwtimer_register_periodic(cb, arg, …)` → DAL/target trampoline | 典型 `PAL_IRQ_PRIO_NORMAL` 或 HIGH（仍须在 syscall 优先级内，可 `xxxFromISR`）；时延预算按电流环周期钉死 |
| **nFAULT 保护 ISR** | 异步/亚微秒关断；绕软件层直接寄存器或硬件 BRK（对齐 [ADR-0024](../../decisions/core/0024-fault-three-phase-model-and-dal-deinit-contract.md)） | `pal_hwtimer_register_fault` **或** GPIO/驱动桥 nFAULT 经 `pal_gpio` / 驱动专属 fault 入口（与周期入口**分列**，不得共用同一 trampoline） | **高于**周期控制 ISR；路径极短：清标志 → 硬件关断 → 置 fault flag；**禁止**在保护 ISR 内跑 Clarke/Park/SVPWM |

#### 2.2.4 PWM–ADC 硬件同步触发绑定（R-007 / R-008 配套）

| 方向 | 约定 |
|---|---|
| **真机** | ADC 采样由 **PWM 定时器 TRGO / Underflow（或等价事件）硬件触发**；`pal_hwtimer` / PWM–ADC bind API 负责声明「哪个 PWM 时基 → 哪个 ADC 组」，禁止软件轮询凑同步 |
| **绑定责任** | DAL 积木（电流采样）在 init 时经 PAL 声明绑定；trampoline **不**在 ISR 内重新配置触发源 |
| **仿真降级** | host/wasm 无硬件 TRGO：软步进近似（DAL §8.3.2）；虚拟时间确定性，禁墙钟 / `rand` |

草案示意（非最终签名；C2 落头时以 `.h` 为准）：

```c
/* pal_hwtimer.h — CONTRACT DRAFT only; Wave C implements */
typedef void (*pal_hwtimer_isr_t)(void *arg); /* must be IRAM-safe on ESP-IDF */

wink_status_t pal_hwtimer_init(uint8_t timer_id, uint32_t rate_hz);
wink_status_t pal_hwtimer_register_periodic(uint8_t timer_id, pal_hwtimer_isr_t cb, void *arg);
wink_status_t pal_hwtimer_register_fault(uint8_t src_id, pal_hwtimer_isr_t cb, void *arg);
wink_status_t pal_hwtimer_bind_pwm_adc(uint8_t pwm_timer_id, uint8_t adc_unit);
wink_status_t pal_hwtimer_start(uint8_t timer_id);
wink_status_t pal_hwtimer_stop(uint8_t timer_id);
```

### 2.3 PAL ADC 子系统 (`pal_adc.h`) — ADR-0057

> 回写记录：2026-08-05 据 [ADR-0057](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)（Accepted）新增。落地排期见 [`00.5-pal-adc-subsystem-plan.md`](../../implementation-plans/frontend/00.5-pal-adc-subsystem-plan.md)。

片上 ADC（≤16-bit）的目标无关抽象，解锁通道 3（模拟量）仿真与 `analog_knob`/`analog_sensor`/`heart_rate`/`load_cell` 等模拟外设。

**设计原则（ADR-0057）：**

- **目标无关配置**：公共配置只含 `pin` / `full_scale_mv` / `resolution_bits`。**不导出 `pal_adc_atten_t`**——衰减/增益是 esp32 内部细节，由其按 `full_scale_mv` 选择最接近的硬件 atten + eFuse 曲线校准；非标准档走 target-private `<pal_adc_esp32.h>`。
- **逻辑通道句柄**：上层只持 `pal_adc_channel_t`（`0..PAL_ADC_CHANNELS-1`，默认 16）。`pal_adc_init(ch, cfg)` 内部按 `cfg->pin` 经板级路由映射到物理 ADC 单元/通道（如 ESP32 ADC1_CH0），上层不感知硬件通道重叠。运行时路由复用既有 weak pin-map + app `board_config.c` 强定义机制（与 `pal_pwm_pin_map` 同构）。
- **采样时刻一致**：per-channel 缓存 `last_raw`/`last_sample_us`；`read_raw` 触发一次 oneshot 采样并刷新缓存，`read_mv` 复用同一缓存换算，保证两者时刻一致、不重复触发采样。
- **范围界定**：仅 MCU **片上/内置 ADC**（分辨率 ≤16-bit，`uint16_t` 不溢出）。24-bit 外部 ADC（如 HX711）走专用 GPIO/SPI Bit-bang 驱动，**不进入** `pal_adc.h`。
- **单次 oneshot 转换（短时阻塞）**：read 为单次转换（esp32 上 µs 级忙等），公共头标 `WINK_BLOCKING`（最坏 = 一次转换时间）；连续采样 + PWM 硬件触发属 ADR-0047 `pal_hwtimer_bind_pwm_adc`（§2.2），不在本子系统。

**API：**

```c
typedef uint8_t pal_adc_channel_t;   /* [0, PAL_ADC_CHANNELS) */

typedef struct {
    wink_pin_t pin;                /* 映射 GPIO；int16_t 兼容 NC(-1) */
    uint16_t  full_scale_mv;       /* 0 = 平台默认 (ESP32≈3100, wasm/host=3300) */
    uint8_t   resolution_bits;     /* 0 = 平台默认 (12) */
} pal_adc_config_t;

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg);
void          pal_adc_deinit(pal_adc_channel_t ch);

wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin);
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch);

wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);
wink_status_t pal_adc_read_mv (pal_adc_channel_t ch, uint16_t *out_mv);
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv);
```

**资源治理（归 DAL，§4.1）：** `pal_resource_type_t` 增 `PAL_RESOURCE_ADC_CHANNEL = 6`。`pal_adc_init` **自身不 claim**（遵循"DAL 持 claim、PAL 只配硬件"原则）；消费 ADC 的 DAL 驱动须同时 claim `PAL_RESOURCE_ADC_CHANNEL, ch` 与 `PAL_RESOURCE_GPIO_PIN, pin`（同一 device-owner，失败回滚），deinit 同步释放，以拦截 ADC 脚与数字脚的跨子系统冲突。

**三 target 矩阵（+ baremetal 面向将来）：**

| target | 实现 |
|---|---|
| esp32 | ESP-IDF **v6.0.1** `adc_oneshot` + `adc_cali`；按 board 映射 (unit,channel)；`init` 创建校准句柄、`deinit` 显式删除防堆泄漏；不运行时探测 Wi-Fi（ADC2/Wi-Fi 冲突由 codegen 静态门禁拦截，见下） |
| wasm | 新增导入 `js_pal_adc_read_norm(pin)→float`（读 `PinArbiter.readAnalog` 的归一化 `[0,1]`，**不返回 mV**）；C 侧同源换算 raw/mv，并经退化引擎叠加 RC 低通 + 高斯噪声 + 预热/采样间隔判定；per-channel PRNG（seed=hash(pin)）隔离噪声随机数消耗 |
| host | 确定性注入 `pal_host_adc_inject_raw/mv`，供 CTest；faults=0 直通 |
| baremetal | 当前仓库**无 baremetal HAL target**（`TARGET_PLATFORM` 仅 wasm/host/esp32；baremetal 仅有 OSAL）。将来新增该 target 时补 `WINK_ERR_UNSUPPORTED` stub，零 `wink_sim_physical` 符号 |

**Codegen 静态门禁（ADR-0057 决策 3）：** board json（`wink-tools/tools/codegen/boards/*.json`）声明 `adc.pins[].{channel,unit,wifi_conflict,default_full_scale_mv,default_resolution_bits}`；App 侧通过顶级可选段 `system.connectivity.{wifi,ble}`（布尔，默认 `false`）声明是否启用 **SoC 内置 radio**（外接 UART/SPI Wi-Fi 模组不算）。codegen 在设备树生成期对三类拓扑错误 **Fail-Loud**：
- **R1**：`analog_input` role 设备所接 `gpio_pin` 不在 board `adc.pins` 中（该脚无 ADC 能力）；
- **R2**：所接脚 `wifi_conflict: true`（ADC2 通道）且 `system.connectivity.wifi || ble` 为真；
- **R3**：两个模拟外设实例共享同一 ADC `(unit, channel)`。

判定基于设备有效 role（用户声明的 `role` 或驱动 `default_role`），不硬编码 type 名。运行时 PAL 对网络栈保持无知，不探测 Wi-Fi 状态。配置文档见 [`wink-app-json-guide.md` §1.4](../../../wink-micro-os/docs/wink-app-json-guide.md#4-系统能力声明-systemconnectivity)。

**Role：** 上层经 `analog_input` role（`read_promille` / `read_mv` / `read_promille_status`，见 Role SSOT §3.2-5）访问；`read_promille` 由 DAL 调 PAL `read_raw` 换算。

---

## 3. OSAL 操作系统抽象规范 (`pal_osal.h`)

`pal_osal.h` 抽象了最基础的阻塞延时、高精度时钟获取、以及并发资源互斥锁服务。

### 3.1 完整 API 定义

```c
#ifndef PAL_OSAL_H
#define PAL_OSAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"   /* Phase 3：mutex lock/unlock 失败型 → wink_status_t */

/* --- 1. 系统时间与高精度延时 --- */

/**
 * @brief 系统毫秒延时 (主动阻塞让出 CPU 调度)
 */
void pal_delay_ms(uint32_t ms);

/**
 * @brief 系统微秒延时 (高精度短等待)
 */
void pal_delay_us(uint32_t us);

/**
 * @brief 获取系统从启动至今的毫秒数 (用于周期定时与超时判断)
 */
uint64_t pal_get_ms(void);

/**
 * @brief 获取系统从启动至今的微秒数 (用于高精度传感器电平宽测算)
 */
uint64_t pal_get_us(void);


/* --- 2. 线程同步互斥锁 (Mutex) --- */

typedef void* pal_mutex_t;

/**
 * @brief 创建一个互斥锁句柄
 */
pal_mutex_t pal_mutex_create(void);

/**
 * @brief 获取互斥锁 (锁定)
 * @param mutex 锁句柄
 * @param timeout_ms 阻塞超时时间，传入 0xFFFFFFFF 代表无限等待
 * @note 失败型 → wink_status_t：NULL mutex → WINK_ERR_INVALID_ARG；
 *       timeout → WINK_ERR_TIMEOUT；不支持 target → WINK_ERR_UNSUPPORTED。
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_lock(pal_mutex_t mutex, uint32_t timeout_ms);

/**
 * @brief 释放互斥锁 (解锁)
 */
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_unlock(pal_mutex_t mutex);

/**
 * @brief 销毁互斥锁并释放内存
 */
void pal_mutex_destroy(pal_mutex_t mutex);


/* --- 3. 全局临界区 task/ISR 双入口 (ADR-0016) --- */

/**
 * @brief 进入全局临界区（TASK 上下文专用）
 * @return 恢复临界区所需的原始状态键值
 * @warning 从 ISR 上下文调用行为未定义：
 *   - ESP32 使用 portENTER_CRITICAL（task-only）；ISR 调用触发 assert / SMP deadlock
 *   - host/wasm 单线程下语义安全但语义错误
 *   ISR 上下文请改用 pal_os_critical_enter_isr()。
 */
uint32_t pal_os_critical_enter(void);
void pal_os_critical_exit(uint32_t key);

/**
 * @brief 进入全局临界区（ISR 上下文专用）
 * @note 与 task 版共享同一 mux，跨 task/ISR 互斥保证有效：
 *   - ESP32: portENTER_CRITICAL_ISR(&s_global_mux)
 *   - host/wasm: 单线程退化为 task 等价实现（no-op / 共享 mux）
 *   - baremetal: 关中断
 * @note 命名与 ESP-IDF `xxxFromISR` 惯例对齐（xQueueSendFromISR 等）。
 */
uint32_t pal_os_critical_enter_isr(void);
void pal_os_critical_exit_isr(uint32_t key);

/* Host/Wasm sim-hook（ADR-0016 §4.2）——仿真器/单测在向模拟中断分发前
 * 调 true、返回后调 false；Debug 构建下四个 critical 入口据此 assert
 * 上下文匹配（真机 ESP32 / baremetal 上此对为 no-op）。 */
void pal_os_set_sim_isr_context(bool in_isr);
bool pal_os_in_sim_isr_context(void);

#endif // PAL_OSAL_H
```

### 3.2 临界区双入口契约（ADR-0016）

**背景**：`wink_trace` 环形缓冲需在 task 与 ISR 两条路径下都能记录 fault；ESP32 上 `portENTER_CRITICAL` 与 `portENTER_CRITICAL_ISR` 是不同 spinlock 路径，混用会 assert。

**决策（选项 B · 双入口显式分流）**：
- 拒绝"context-aware 单一入口内部 detect"方案——违反 ADR-0012 契约诚实，掩盖误用。
- 拒绝"单一入口 + doxygen 禁止 ISR 调用"方案——丢失 ISR fault 记录能力，与 `wink_trace.c` 声称的 "ISR-safe" INVARIANT 冲突。
- 采纳：`pal_os_critical_enter/exit` 保留为 task-only；新增 `_isr` 变体供 ISR 使用；`wink_trace_fault` 同步拆双入口（`wink_trace_fault` task-only；`wink_trace_fault_from_isr` ISR-only），命名对齐 ESP-IDF 生态惯例。

**调用方使用规则**：

| 调用点上下文 | 应使用 | 后果 |
|-------------|-------|------|
| task 上下文（`app_loop`、DAL 主流程） | `pal_os_critical_enter/exit` | 正常互斥 |
| ISR 回调（GPIO ISR wrapper、timer ISR、fault handler） | `pal_os_critical_enter_isr/exit_isr` | 正常互斥 |
| task 里调 `_isr` 版 | ❌ 禁止 | task/task 竞态未保护（ESP32 上仍能跑，但语义错误） |
| ISR 里调 task 版 | ❌ 禁止 | ESP32 assert / SMP deadlock |

**跨 target 行为矩阵**：

| Target | task 版实现 | ISR 版实现 |
|-------|-----------|-----------|
| ESP32 | `portENTER_CRITICAL(&s_global_mux)` | `portENTER_CRITICAL_ISR(&s_global_mux)`（共享同一 mux） |
| host | no-op + `assert(!s_sim_in_isr)` | no-op + `assert(s_sim_in_isr)` |
| wasm | no-op + `assert(!s_sim_in_isr)` | no-op + `assert(s_sim_in_isr)` |
| baremetal | 关中断 | 关中断 |

**Host / Wasm sim-hook**（ADR-0016 §4.2 落地为可执行契约）：
- `pal_os_set_sim_isr_context(bool)` / `pal_os_in_sim_isr_context()` 在 host/wasm target 上真正切换 `s_sim_in_isr` 标志；ESP32 与 baremetal 上为 no-op（真机上下文由 `xPortInIsrContext()` / BSP 寄存器判定）。
- 仿真器 / 单测在向模拟中断回调分发前后调用该对；四个 critical 入口据此在 Debug 构建下 assert 命中入口误用，属编译期 + 运行期双保险。
- `wink_trace` 层随本 ADR 拆双入口——`wink_trace_fault` 用 task 版临界区，`wink_trace_fault_from_isr` 用 ISR 版；`test_wink_trace_isr_equivalence` 硬门槛验证 buffer/head/count bit-for-bit 等价（PLAN-Q3 Task D-2）。

**演进路径**：未来引入多虚拟核（若推翻 ADR-0014 单虚拟核决策），`s_global_mux` 需按核区分，但 API 签名不变——双入口模式已包容此演进。若 host 仿真升多线程，`s_sim_in_isr` 需升 `_Thread_local`；若引入嵌套模拟中断，需将布尔改为嵌套计数器（ADR-0016 addendum 已记录）。

---

### 3.3 PAL IRQ 公开面收窄（ADR-0018）

**背景**：`pal_irq.h` v2.2 公开约 25 个符号，AI Codegen 组合空间 ~36 种。深挖发现 6 处过度设计或虚标契约（`pal_irq_direct_connect`、`pal_irq_shared_register`、`pal_irq_synchronize`、6 级优先级、`REALTIME`、`PAL_CRITICAL_SECTION_STRICT`），业务代码零使用。参见 [ADR-0018](../../decisions/core/0018-pal-irq-api-narrowing.md) 与 [2026-07-02 收窄评审](../../reviews/core/2026-07-02-pal-irq-api-narrowing-review.md)。

**决策（2026-07-02 Accepted）**：一次性收窄公开面 48%，删除虚标契约，物理隔离系统级 API。

**新公开面**（`pal_irq.h`）：

| 分类 | 收窄后 |
|------|-------|
| Handler 原型 | `pal_isr_t (void *arg)` 一个 |
| 优先级枚举 | `PAL_IRQ_PRIO_LOW / NORMAL / HIGH`（3 级，全 RTOS 安全） |
| 注册控制 | `pal_irq_enable / pal_irq_disable / pal_irq_set_pending / pal_irq_clear_pending` |
| 临界区 | `PAL_CRITICAL_SECTION(code)` 唯一宏（内部走 `pal_irq_save_rtos_safe`） |
| 属性宏 | `PAL_ISR`（IRAM_ATTR / 空）、`PAL_DEFINE_ISR(name, T, arg)`（类型安全） |

**已删除**（不再存在）：
- `pal_irq_direct_connect`（虚标"零延迟直派"，三 target 均走软派发）
- `pal_irq_shared_register` + 责任链（DAL/App 零使用；wink 目标场景无共享向量需求）
- `pal_direct_isr_t` / `pal_irq_shared_handler_t`（合并到 `pal_isr_t`）
- `PAL_IRQ_PRIO_LOWEST / HIGHEST / REALTIME`（前两个是别名，后者全 target 拒接）
- `WINK_HOST_ALLOW_REALTIME_FOR_TESTING`（伴随 REALTIME 删除；`python wink-tools/wink.py test` opt-in pass 一并退休）

**物理隔离到 `pal_irq_advanced.h`**（仅系统级驱动，`#ifndef WINK_ALLOW_ADVANCED_IRQ_APIS` `#error` 硬门控）：
- `pal_irq_synchronize`（SMP 资源热释放同步；普通 App/DAL 用不到）
- `pal_irq_save` + `PAL_CRITICAL_SECTION_STRICT`（全屏蔽 <1µs 极端原子场景）

**跨 target 优先级映射**：

| Target | LOW | NORMAL | HIGH |
|-------|-----|--------|------|
| ESP32 | `ESP_INTR_FLAG_LEVEL1` | `ESP_INTR_FLAG_LEVEL2` | `ESP_INTR_FLAG_LEVEL3`（configMAX_SYSCALL_INTERRUPT_PRIORITY） |
| host  | 仿真调度顺序 | 仿真调度顺序 | 仿真调度顺序 |
| wasm  | 仿真调度顺序 | 仿真调度顺序 | 仿真调度顺序 |
| baremetal | NVIC 低段 | NVIC 中段 | NVIC 高段（不超 syscall pri 边界） |

三级均在 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 之内 → 全部可安全调用 `xxxFromISR` API。

**裸机降级契约**：无 RTOS / 无嵌套优先级的裸机 target 上，`pal_irq_save_rtos_safe` 降级为 `__disable_irq()`（全屏蔽），保证跨 target 编译时物理完整性。

**AI Codegen 使用规则**：
- 默认路径：`pal_irq_enable(...) → PAL_CRITICAL_SECTION({ ... })`，无第二个选择。
- 组合空间 3 种（LOW/NORMAL/HIGH × 1 handler × 1 临界区），全部合法。
- `pal_irq_advanced.h` 默认**不**生成 include；仅在 SMP 资源热释放或系统级临界区场景用。

**未来路径**：真需要硬件矢量直派时，通过独立接口 `pal_irq_direct_connect_unsafe()`（`_unsafe` 后缀显式表达"绕过 PAL 保护"），不作为 `pal_irq_prio_t` 的新成员。

---

## 4. 各平台 (Targets) 移植绑定规范

### 4.1 ESP32 (基于 ESP-IDF & FreeRTOS)
*   **`pal_delay_ms`** 映射至 ESP-IDF 的 `vTaskDelay(ms / portTICK_PERIOD_MS)`。
*   **`pal_get_us`** 映射至 `esp_timer_get_time()`。
*   **`pal_gpio_init`** 映射至 `gpio_config()`，自动处理引脚的推挽/开漏模式配置。
*   **`pal_i2c_transfer`** 映射至 ESP-IDF I2C API（**v5/v6 双版本兼容**，见下方移植清单与 [ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)）。
*   **`pal_gpio_pulse_in`**（Phase 4 过渡 capture）：ESP32 须用 **RMT** 或 **GPIO 双沿 ISR + 硬件 timer** 捕获 echo 脉宽，**runtime tick 内无 polling/busy-wait**。
    > **架构红线（Deferred ISR / 下半部）**：ISR 内部绝对禁止任何阻塞操作——仅读 timer/RMT 状态、清中断标志，将脉宽投递至无锁事件队列或置 flag；由 `app_loop` 在正常任务上下文取出处理（Bottom-Half），杜绝中断嵌套与 RTOS 卡顿。
    > **wasm 对称约束（review D1 / Phase 1 Task 1-5）**：wasm 下 JS 模拟中断同样不得在 Asyncify sleeping 窗口直调 `_trigger_wasm_interrupt`，须经 JS 排队、tick 边界 flush。两 target 共享同一「Deferred 中断」语义（ESP32 = Bottom-Half 任务上下文，wasm = tick 边界），符合 ADR-0002 双 target 同源。`pal_gpio_pulse_in` 为过渡，最终目标是 async capture/callback 或 driver-internal worker。

**ESP32 PAL 移植清单（Phase 6 Task 6-5 / P2-6 ROADMAP，随 ESP-IDF spike 推进填充）：**
* **GPIO**：init/read/write 经 `gpio_config()` / `gpio_get_level()` / `gpio_set_level()`；中断经 `gpio_isr_handler_add`（Deferred-ISR 下半部）。
* **PWM**：经 **LEDC**（`ledc_timer_config` + `ledc_channel_config`）；channel/timer 由 profile-aware Router 分配（ADR-0034：freq+bits+clock）。`STABLE_REQUIRED` → `LEDC_USE_REF_TICK`；AUTO → `LEDC_AUTO_CLK` + 默认 13-bit。
* **I2C**：**v5/v6 双版本兼容**（编译期 `ESP_IDF_VERSION` 静态门控，[ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)）。v6.x（`driver/i2c_master.h`）走总线-设备二级句柄模型 + 设备句柄懒加载缓存（FIFO 替换，4 slot/port）；v5.x（`driver/i2c.h`）走 `i2c_master_write_read_device()`。带 timeout + 精细错误码映射（7 种 ESP err → `WINK_ERR_TIMEOUT/DISCONNECTED/INVALID_ARG/...`）。技术细节见 [`tech-designs/pal-i2c-v6-compatibility.md`](../../tech-designs/core/pal-i2c-v6-compatibility.md)；可用 `CONFIG_WINK_I2C_FORCE_V5_API` 强制回退。
* **OSAL**：delay/time 经 FreeRTOS `vTaskDelay` / `esp_timer_get_time()`；mutex 经 `SemaphoreHandle_t`。
* **Watchdog**：ESP-IDF task watchdog 或 RTC watchdog → `pal_watchdog_init/feed`。
* **Reset reason**：`esp_reset_reason()` → `pal_reset_reason_t` 映射（POWER_ON / TGWD / panic 等）。
* **Ultrasonic capture**：RMT 或 GPIO 双沿 ISR + timer，**runtime tick 内禁 busy-wait**（呼应 Phase 4）。
* **Boot fail-safe**：每执行器板级 pull-down / 电源门控 / 使能脚默认关断须文档化（硬件级默认安全态由板级电路保证，软件 boot safe-lock 只补闭环，见 [04-runtime-and-trace.md](./04-runtime-and-trace.md) §3）。

### 4.2 STM32 (基于 STM32 HAL & FreeRTOS)
*   **`pal_delay_us`** 通过高精度硬件定时器（如 TIM6 / TIM7）的计数器进行死等，而 `pal_delay_ms` 映射至 FreeRTOS 的 `osDelay`。
*   **`pal_mutex`** 映射为 FreeRTOS 的 `SemaphoreHandle_t` (用作 Mutex)。

### 4.3 WebAssembly 仿真端 (基于 Emscripten JS 桥接)
*   **`pal_gpio_write`**（ADR-0015 v2.3 签名 `wink_status_t + level`）转换为对 `js_pal_gpio_write(pin, level)` 导入函数的调用，该函数通知前端 Web Worker 去刷新 PinManager；C 侧返回码路径由 wasm `pal_hal_wasm.c` 兜底（未登记引脚 → `WINK_ERR_INVALID_STATE`）。
*   **`pal_delay_ms`** 转换为异步 JS API，并在 Wasm 中调用 Emscripten Asyncify 挂起当前协程：
    ```c
    // targets/wasm/pal_hal_wasm.c 示例
    extern void js_pal_delay_ms(uint32_t ms);
    void pal_delay_ms(uint32_t ms) {
        js_pal_delay_ms(ms); // 该函数由 emcc 的 ASYNCIFY 标记为异步挂起函数
    }
    ```

> **库类型注（A* §4/§5）**：`pal/` 是 **INTERFACE 契约库**（仅头、无 `.c`、无符号）。所有 PAL 实现下沉到 `targets/<platform>/`（wasm/esp32/host）。host 升格为一等 target，供 host 端 PAL→DAL→runtime→App 全链路测试。详见 [03-directory-architecture.md](./03-directory-architecture.md)。

---

## 4.4 Target 内公共设施（`targets/common/`）

为避免 ESP32 / wasm / host 三 target 复制粘贴相同的算法与数据结构，
`targets/common/` 目录承载 **target 无关但 PAL 私有** 的公共层。此层位于 `pal/`
（INTERFACE 契约库）与 `targets/<platform>/`（平台适配实现）之间，是**多个 target 适配层可共享的算法/数据结构 SSOT**。

| 文件 | 用途 | 引入日期 | 相关文档 |
|------|------|---------|---------|
| `wink_sim_physical.{h,c}` | 物理退化算法库（弹跳/阻尼/衰减等仿真时间线算子），wasm & host target 复用 | 2026-06-28 | [ADR-0009](../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md) |

> **历史备注**：`pal_shared_chain.{h,c}`（PAL 中断共享责任链）曾于 2026-07-01
> 引入，2026-07-02 由 [ADR-0018](../../decisions/core/0018-pal-irq-api-narrowing.md) 删除。
> 责任链共享中断在 wink-micro-os 场景下真实需求 = 0（DAL/App 层从未直接使用）；
> 且强行引入了运行期 `malloc` 依赖，违反 ADR-0004 静态分配原则。

### 设计约束

- 公共层**不进入** `WINK_CORE_INCLUDE_DIRS` —— 属 target-private，不对 DAL/runtime 暴露。
- 涉及并发差异（如 ESP32 SMP 抢占 vs wasm/host 单线程）通过 target 内部实现自行处理；
  wink-micro-os 明确**不**引入 vtable / ops 表跨 target 分发（[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)）。

---

## 4.1 资源占用治理 (`pal_resource.h`)

`pal_resource` 提供 target 无关的语义级资源冲突检测,零动态分配。**SSOT 边界**:资源
所有权登记归 DAL 层拥有(`dal_*_init` 时以 `cfg->owner`(AI Codegen 稳定名)调用
`pal_resource_claim`);PAL HAL 层**不**参与 owner 表登记,只负责硬件寄存器配置。

**为什么 DAL 是 SSOT**:资源冲突是**设备级语义问题**("两个 `dal_led` 抢同一 pin"),
不是**寄存器级问题**。若 PAL 也以固定 owner 自 claim,两个 DAL 实例配同 pin 时:
- 第一次 DAL claim 成功(owner=A),随后 PAL init 又以 owner=`"pal_hal_xxx"` 二次 claim
  → 恒返 `WINK_ERR_BUSY`,把正确路径当冲突。
- 且 PAL 固定 owner 掩盖了真正的冲突者身份,DAL 层错误信息定位困难。

统一由 DAL 持 claim,PAL 只做硬件初始化,语义清晰、冲突信息可归因到具体 device。

| 资源类型 | 粒度 | 说明 |
|---|---|---|
| `PAL_RESOURCE_GPIO_PIN` | 单引脚 | DAL 以 `cfg->owner` claim(`dal_button` / `dal_led` / `dal_ultrasonic` / `dal_gps`) |
| `PAL_RESOURCE_PWM_CHANNEL` | 单通道 | DAL 以 `cfg->owner` claim(`dal_servo`) |
| `PAL_RESOURCE_I2C_PORT` | 整端口 | 保留,未启用设备级治理 |
| `PAL_RESOURCE_I2C_ADDR` | `(port, 7位地址)` | DAL 以 device-owner claim;同 port 不同地址不冲突;同 `(port,addr)` 异 owner → `WINK_ERR_BUSY` |
| `PAL_RESOURCE_UART_PORT` | 单端口 | DAL 以 `cfg->owner` claim(`dal_gps` 等串行外设) |
| `PAL_RESOURCE_ADC_CHANNEL` | 单逻辑 ADC 通道 | 模拟 DAL（`dal_analog_knob`/`dal_analog_sensor` 等）以 device-owner claim，并**同时 claim 对应 `GPIO_PIN`**（ADR-0057 §2）；`pal_adc_init` 自身不 claim |
| `PAL_RESOURCE_PWM_TIMER` | 单 LEDC timer 槽 | ESP32 `pal_pwm_router` 按**完整 effective profile**（`freq_hz` + effective `resolution_bits` + effective `clock_source`）复用 timer 槽（ADR-0034）；仅 profile 相同才共享；同频不同 bits **不得**复用 |

**约束**:
1. 直接调用 PAL HAL(绕过 DAL)的 sample/test **不会**在 owner 表中登记——这是正确设计,
   因为资源冲突检测是 device 级关注点;PAL 层用例天然是单实例、单契约。
2. DAL init 若涉及多资源(如 `dal_ultrasonic` 的 trig+echo GPIO),第二次 claim 失败必须
   **回滚**第一次 claim(`pal_resource_release`),避免半开状态。同理 `dal_ssd1306` 在
   `pal_i2c_transfer` init 命令序列失败时也须释放已 claim 的 I2C 地址。
3. Owner 字符串**必须**具有静态生命周期(rodata 字面量、`__func__`、或稳定字符串常量);
   传入临时字符串(栈变量)行为未定义。
4. `pal_resource` 是跨 target 统一契约(host 与 esp32 提供检测实体,wasm 提供 no-op
   占位),DAL 可无条件调用 `pal_resource_claim` 而不引入 app 层 `#ifdef`。

*本节回写记录*:2026-07-02 由计划 [`2026-07-01-wmos-code-optimization-q3-plan.md`](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md)
Track A(Task A-2 及其 Amendment 衍生 PAL 撤 self-claim)落地;替代此前 "GPIO/PWM 由
PAL HAL 以固定 owner claim" 的历史占位描述。

## 4.2 非易失覆写存储与设备树逃生通道 (`pal_storage` + `wink_dev_config`) — ADR-0008

[ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md) 引入「静态 POD 实例 + Flash 配置动态覆写」逃生通道：免编译、秒级微调引脚/参数（覆写失败静默降级到编译期默认，绝不 Panic）。PAL 层新增两个 target 无关抽象，host 全单测覆盖：

**`pal_storage.h`** — 键值式非易失存储（覆写 blob 存取），三 target 静态绑定：

| target | 实现 | read 缺省语义 |
|---|---|---|
| host | 进程内内存单槽（测试用，`pal_storage_reset`） | 空 → `WINK_ERR_EMPTY` |
| esp32 | NVS（namespace `"wink"`，key `"dtcfg"`） | key 不存在 → `EMPTY`；按 key 原子覆写 |
| wasm | no-op stub | 恒返 `WINK_ERR_UNSUPPORTED` → 运行期降级 |

API：`pal_storage_read/write/erase(key, buf, len)`（均 `WINK_WARN_UNUSED_RESULT`）。

**`wink_dev_config.h`** — 覆写 blob 解析器 + CRC32（共享核心，target 无关）：
- blob 布局：`[magic:u32][version:u16][count:u16]`(header 8B) + `[device_id:u32][params:16B]`×count(每 item 20B) + `[crc32:u32]`；**offset+memcpy 逐字段反序列化**（禁 packed 指针强转，规避非对齐/别名 UB）。
- **CRC32 契约（前端 Codegen 对接权威参考）**：CRC-32/ISO-HDLC（zlib/PNG 同款），多项式反射值 `0xEDB88320`、init/final-XOR `0xFFFFFFFF`、覆盖 header+items（不含末尾 CRC）。CRC 只防字节损坏，不防语义错误。
- 覆写注册表：`(device_id → 类型化 dev → apply_fn)` 类型正确三元组（固件侧类型安全）；各 DAL 提供 `dal_*_apply_override(void*, params, len)`（轻校验 + `dal_*_init` 权威校验纵深）。
- 降级：magic/version/长度/CRC 任一不符 → 整体静默回编译期默认（返对应错误码，不写字段）；单 item 未命中/apply 失败 → 跳过该项不中断。
- `device_id` 为 codegen 稳定 uint32；任一 DAL params 布局变更须 bump blob `version`（旧 version 一律降级）。

**hook 点**：每个 sample `app_init` 顶部、`dal_*_init` 之前调用 `device_tree_apply_flash_config()`（读 `pal_storage` → `wink_dev_config_apply` → 改写静态实例字段），随后从结构体字段重建 config/引脚喂 init。`HAS_FLASH_CONFIG_ESCAPE` 编译期裁剪开关保留给未来低资源 target，核心阶段统一运行期降级、不定义/不引用。

实现/测试细节见 [实施计划](../../implementation-plans/core/2026-06-28-adr-0008-flash-device-tree-override-plan.md)。

---

## 5. DAL ↔ PAL 契约与外设依赖矩阵

在微内核运行期，**DAL 驱动层并不直接包含芯片寄存器操作，而是作为“时序翻译官”强依赖 PAL 提供的总线和系统服务接口。** 

不同类别的 DAL 外设在向新硬件平台（如 STM32 等物理 MCU）移植适配时，对 PAL 接口的依赖具有明确的分群关系。开发人员可对照以下的外设分组来了解各外设的移植依赖与适配先决条件：

### 外设分组

#### 1. 简易数字与高精度时序类外设
* **依赖的 PAL 接口**：`pal_gpio`（数字读写 / 外部中断） + `pal_delay`（微秒级/毫秒级时序）。
* **适配先决条件**：芯片 GPIO 基础输入输出正常，并且需要实现高精度硬件定时器死等（微秒级）以保障时序的准确性。
* **典型外设实例**：
  * **超声波测距传感器 (HC-SR04)**：通过 GPIO 发送 10us 脉冲启动，利用 `pal_gpio_pulse_in`（ESP32: RMT / 双沿 ISR + timer，Deferred-ISR 下半部）捕获 Echo 脉宽；DAL 经非阻塞 `dal_ultrasonic_request_measurement` + `get_cached_distance` 消费，10ms tick 内无 busy-wait。
  * **单总线温湿度传感器 (DHT11)**：通过精确到微秒级的 GPIO 输入输出翻转完成握手与数据读取。
  * **红外接收头 (VS1838B)**：利用 GPIO 外部中断高精度捕获红外载波的脉宽数据进行协议解码。

#### 2. 模拟采集与动力执行类外设
* **依赖的 PAL 接口**：`pal_pwm`（占空比调节） + `pal_adc`（电压模拟量读取）。
* **适配先决条件**：芯片硬件定时器 PWM 发生器通道配置正常，ADC 通道多路采样与校准工作正常。
* **典型外设实例**：
  * **模拟舵机 (SG90/MG996R)**：依赖固定 50Hz 频率，可变占空比的 PWM 脉宽信号控制旋转角度。
  * **DC 减速电机**：通过可调频率和占空比的 PWM 信号控制驱动桥（如 L298N）输出的平均功率。
  * **阻抗/模拟电压传感器**（光敏电阻、土壤湿度、摇杆电位器）：通过 ADC 采集传感器输出的模拟电压值。

#### 3. 同步串行总线协议类外设 (I2C/SPI)
* **依赖的 PAL 接口**：`pal_i2c`（双向半双工） 或 `pal_spi`（高速全双工）。
* **适配先决条件**：实现芯片的硬件 I2C/SPI 驱动；或者在没有硬件控制器时，利用 GPIO 实现满足时序的软件模拟总线。
* **典型外设实例**：
  * **I2C 传感器群**（九轴加速度计 MPU6050、数字气压计 BMP280）：利用 `pal_i2c_transfer` 读写器件寄存器。
  * **SPI 显示与存储设备**（OLED/TFT 屏幕、SPI Flash、RC522 RFID 刷卡模块）：依赖 SPI 进行高频数据或大文件传输。

#### 4. 异步串行通信与智能模组类外设 (UART)
* **依赖的 PAL 接口**：`pal_uart`（异步收发 / 接收中断）。
* **适配先决条件**：单片机 UART 驱动正常工作，并配置有足够容量的环形缓冲区（RingBuffer），支持中断或 DMA 接收以防高频丢包。
* **典型外设实例**：
  * **无线透传模组**（经典蓝牙 HC-05、WiFi 模块 ESP8266/ESP32-C3 AT 指令版）：通过 AT 指令和串口帧与主控交互。
  * **智能外设模块**（GPS/GNSS 定位模块、智能 MP3 播放模块、语音识别模块、工业 Modbus-RTU 传感器）：通常具有独立 MCU，通过定制串口协议与主控通信。

#### 5. 系统支撑与非易失性安全类外设
* **依赖的 PAL 接口**：`pal_wdt`（看门狗服务） + `pal_flash`（非易失性存储 NVS）。
* **适配先决条件**：芯片独立看门狗寄存器可用，Flash 读写扇区划归及擦写保护正常。
* **典型外设实例**：
  * **系统看门狗 WDT**：用于 fail-safe 任务超时复位。
  * **配置存储外设**：用于断电保存 WiFi SSID、设备配网参数及报警阀值。

### 移植解耦设计要点

1. **按需分步移植**：若移植到新单片机系列，可按此矩阵“逐步点亮”。例如先实现 `pal_gpio` 和 `pal_delay`，此时所有简易时序外设即可在不改任何 DAL 代码的前提下直接跑通，而无需等待复杂的 I2C/SPI 物理外设驱动写完。
2. **实例复用与引脚隔离**：DAL 驱动只通过 `device_tree.c` 传入的逻辑引脚编号调用 `pal_gpio_write(dev->trig_pin, level)`（ADR-0015 v2.3 起返 `wink_status_t`，DAL 层错误码透传）。硬件引脚分配和物理时序完全在底层被 PAL 吸收，从而保障了 DAL 代码和业务 App 代码的 100% 同源与平台无关性。

---

## 6. WINK_BLOCKING 警告抑制与阻塞区域宏契约（ADR-0025）

为了在编译期区分“合法阻塞”与“非法阻塞”，并且让编译器警告不被粗暴地全局屏蔽（例如 file-scope pragma 导致真正的业务阻塞 bug 被隐藏），系统建立了阻塞警告的细粒度语义抑制宏。

### 6.1 编译期警告抑制宏规范

系统在 `runtime/include/wink_blocking_region.h` 中针对主流编译器（GCC、Clang、MSVC）提供了两对对称的控制宏：

1. **`WINK_INTERNAL_BLOCKING_REGION_BEGIN / END`**：
   - **设计定位**：专供 BAL 内部（如 `wink_ultrasonic_poll.c` 内的 `MAY_BLOCK` 任务循环）或 Runtime 内部（如 `wink_selftest.c`）使用。
   - **作用域**：包裹在包含所有 `#include` **之后**的实现段。
2. **`WINK_INIT_BLOCKING_REGION_BEGIN / END`**：
   - **设计定位**：专供应用层在 `app_init_status()` 或 `app_on_fault_status()` 等非协作调度上下文（同步初始化阶段）内部，对小颗粒度的同步诊断阻塞调用进行包裹。
   - **作用域**：精确包裹具体的阻塞语句块，禁止扩大至整个函数或文件级别。

### 6.2 跨编译器底层展开原理

这两对宏在底层对编译器警告（`GCC -Wdeprecated-declarations`、`MSVC C4996`）进行入栈、抑制和出栈恢复保护：

*   **GCC / Clang**：
    ```c
    #define WINK_INTERNAL_BLOCKING_REGION_BEGIN \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
    #define WINK_INTERNAL_BLOCKING_REGION_END  _Pragma("GCC diagnostic pop")
    ```
*   **MSVC**：
    ```c
    #define WINK_INTERNAL_BLOCKING_REGION_BEGIN  __pragma(warning(push)) __pragma(warning(disable:4996))
    #define WINK_INTERNAL_BLOCKING_REGION_END    __pragma(warning(pop))
    ```

---

有关 Wasm 仿真平台中 Asyncify 调度以及 Web Worker 线程交互的具体实现，请参阅专门章节：**[04-wasm-simulation/01-wasm-sandbox-lifecycle.md](../04-wasm-simulation/archive/01-wasm-sandbox-lifecycle.md)**。


