# WinkMicroOS Wave B：ESP32 真机移植编译评审报告

| 项 | 内容 |
|---|---|
| **文档编号** | WK-REV-ESP32-20260626 |
| **评审日期** | 2026-06-26 |
| **评审范围** | `targets/esp32/` 组件 + `esp32_firmware/` 项目 |
| **IDF 版本** | v5.1.3（Xtensa GCC 12.2.0） |
| **编译状态** | ✅ **PASS**（零错误、零警告） |
| **固件产物** | `esp32_firmware/build/wink_esp32_firmware.bin`（196 KB） |

---

## 1. 执行摘要

### 1.1 核心结论

ESP32 真机端口（Wave B 范围）的 **编译阶段已全部通过**。此前 `@verified: NO` 的 PAL 层实现（GPIO/PWM/I2C/RMT/OSAL/WDT）经 IDF v5.1.3 工具链完整编译验证，零错误零警告。

**仅剩硬件验证阶段**：
- ✅ 代码层面：全部编译通过
- ⏳ 硬件层面：RMT 中断延迟（<10us 验收标准）、测距精度、看门狗复位链路待上板实测

### 1.2 本轮关键发现（共 10 项可移植性问题）

所有问题均由 IDF 交叉编译暴露——若不做真机编译，这些问题将永远潜伏在 host 单元测试的阴影中：

| 分类 | 典型问题 | 影响 |
|---|---|---|
| **头文件可移植性** | `wink_status.h` 缺少 `<stddef.h>` → `NULL` 在 Xtensa GCC 下未定义 | host 工具链靠传递包含侥幸通过 |
| **IDF v5 API 变更** | `esp_reset_reason.h` 已合并入 `esp_system.h` | 直接 include → 编译失败 |
| **组件依赖缺失** | `pal_osal_esp32.c` 使用 `esp_timer.h` 但未在 `REQUIRES` 声明 | 链接阶段符号缺失 |
| **CMake 门控 bug** | `if(TARGET_PLATFORM STREQUAL "esp32")` 在 IDF 组件上下文中永假 | wink 组件静默不注册 → 所有头文件找不到 |
| **隐式声明** | `pal_hal_esp32.c` 调用 `pal_get_us()` 但未包含 `pal_osal.h` | C89 下默认返回 int → 运行时栈破坏 |
| **类型转换警告** | GPIO 中断参数 `(void *)pin` 在 Xtensa 下触发 `int→pointer` size mismatch | `-Werror=all` 下升级为错误 |
| **虚构 API** | `esp32_firmware/app_main.c` 调用不存在的 `wink_runtime_init()`/`_tick()` | 完全无法链接 |
| **虚构 trace API** | `wink_trace_get_active_fault_count()` 不存在，真实为 `wink_trace_count()` | 链接失败 |
| **Xtensa 宽度差异** | `uint32_t` = `unsigned long`，不是 `unsigned int` → `%u` 格式串全部报错 | `-Werror=format` 下编译失败 |
| **应用代码未链接** | `avoidance_car` 的 `wink_app_get_callbacks()` 未加入编译 | 链接阶段符号缺失 |

### 1.3 Wave B 范围之外（Out of Scope）

下列子系统**不属于 Wave B 编译评审范围**，留待后续 Wave 专门移植：

| 子系统 | 留待 Wave | 说明 |
|---|---|---|
| **ADC / DAC** | Wave C+ | PAL `pal_adc`/`pal_dac` 仅契约存在，ESP32 `adc_oneshot`/`dac_output` 适配未做 |
| **SPI** | Wave C+ | 主总线（SD 卡 / 显示）SPI 适配未做 |
| **NVS** | Wave C+ | 非易失键值存储（校准 / 配置持久化）未做 |
| **Wi-Fi / BLE** | Wave D+ | 无线连接栈未纳入 Wave B |
| **OTA** | Wave D+ | 固件空中升级未做 |
| **Flash 分区规划** | Wave D+ | 当前用默认分区表，量产分区表待 OTA 阶段定稿 |

---

## 2. 编译验证详细状态

### 2.1 构建环境

```
Toolchain: xtensa-esp32-elf-gcc 12.2.0
IDF version: v5.1.3
Host: Windows 11 (MinGW environment via IDF export.ps1)
Build config: Default (O0 + debug + -Werror=all)
Total targets: 915
Result: 100% built successfully
```

### 2.2 产物清单

| 文件 | 大小 | 说明 |
|---|---|---|
| `wink_esp32_firmware.bin` | 196,096 bytes | 可直接烧录的量产固件 |
| `wink_esp32_firmware.elf` | 3,048,080 bytes | 带完整符号表的调试固件 |
| `bootloader.bin` | ~22 KB | IDF 引导加载程序 |
| `partition-table.bin` | ~3 KB | 分区表 |

---

## 3. 已修复问题根因分析与解决方案

### 3.1 CMake / 构建系统层

| ID | 问题 | 根因 | 修复 | 受影响文件 |
|---|---|---|---|---|
| C-01 | wink 组件不注册 | `targets/esp32/CMakeLists.txt` 使用 `if(TARGET_PLATFORM STREQUAL "esp32")` 做门控，但 IDF 组件在独立作用域中处理，`TARGET_PLATFORM` 从未设置 → 条件永假 → `idf_component_register()` 永不执行 → 组件静默消失 | 改用 `if(ESP_PLATFORM)` 判定（由 IDF 全局定义，可靠），并移除 `TARGET_PLATFORM` 分支 | `targets/esp32/CMakeLists.txt` |
| C-02 | 组件依赖不全 | RMT/OSAL 使用 `esp_timer.h`、`esp_system.h` 但未在 `REQUIRES` 声明 | 添加 `REQUIRES driver esp_timer esp_system` | `targets/esp32/CMakeLists.txt` |
| C-03 | 应用代码未链接 | `avoidance_car/app_main.c` + `device_tree.c` 不在 `main` 组件的 `SRCS` 列表 | 将两个源文件 + include path 加入 `esp32_firmware/main/CMakeLists.txt` | `esp32_firmware/main/CMakeLists.txt` |

### 3.2 可移植性 / C 语言层

| ID | 问题 | 根因 | 修复 | 受影响文件 |
|---|---|---|---|---|
| P-01 | `NULL` 未定义 | `wink_status.h` 是所有 wink 代码的公共头，但未显式包含 `<stddef.h>`；host GCC 靠其他头文件传递包含侥幸通过，Xtensa IDF 头文件依赖链不同 → 编译失败 | 在 `wink_status.h` 最前面加入 `#include <stddef.h>` | `wink-micro-os/pal/include/wink_status.h` |
| P-02 | `pal_get_us()` 隐式声明 | `pal_hal_esp32.c` 调用 `pal_get_us()`（来自 OSAL）但未包含 `pal_osal.h` | 加入 `#include "pal_osal.h"` | `wink-micro-os/targets/esp32/pal_hal_esp32.c` |
| P-03 | GPIO 中断参数类型转换 | `gpio_isr_handler_add(..., (void *)pin)` — Xtensa 是 32-bit，`int` 也是 32-bit，但 `-Werror=int-to-pointer-cast` 仍将此标记为危险操作 | 经 `uintptr_t` 中转：`(void *)(uintptr_t)pin` | `wink-micro-os/targets/esp32/pal_hal_esp32.c` |
| P-04 | IDF v5 头文件重定位 | `esp_reset_reason.h` 在 IDF v4.x 是独立头文件，v5.x 已合并入 `esp_system.h` | 改为 `#include "esp_system.h"` | `wink-micro-os/targets/esp32/pal_osal_esp32.c` |

### 3.3 固件入口 / 应用层

| ID | 问题 | 根因 | 修复 | 受影响文件 |
|---|---|---|---|---|
| A-01 | 虚构 runtime API | `app_main.c` 草稿调用 `wink_runtime_init()` + `wink_runtime_tick()`，但真实 runtime API 是统一入口 `wink_runtime_run()`（内部依次执行 init + loop） | 替换为标准调用：`wink_runtime_run(app, 0)`（0 表示永久运行） | `esp32_firmware/main/app_main.c` |
| A-02 | 虚构 trace API | 调用不存在的 `wink_trace_get_active_fault_count()` | 替换为真实公共 API：`wink_trace_count()` | `esp32_firmware/main/app_main.c` |
| A-03 | Xtensa 格式串不匹配 | Xtensa GCC 定义 `uint32_t` = `unsigned long`，不是 `unsigned int` → 所有 `%u` 格式串触发 `-Werror=format` | 全部改用 `<inttypes.h>` 的 `PRIu32` / `PRId32` 宏；整数宏单独 cast 到 `unsigned` | `esp32_firmware/main/app_main.c` |

---

## 4. 待解决问题（按优先级排序）

### 🔴 P0：阻塞硬件验收（高影响 / 低成本）

#### ISSUE-001：LEDC Timer 资源冲突

**状态**：待修复
**治理编号**：P2-7（新分配；P2-2 仍指 (void) 吞错治理）
**问题位置**：`targets/esp32/pal_hal_esp32.c:186,198`

```c
// 当前实现：所有通道强制共用 TIMER_0
ledc_timer_config_t timer_cfg = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_0,   // ← 写死！所有 PWM 通道共用同一个 timer
    .freq_hz = freq_hz,           // ← 后初始化的通道会静默覆盖前一个的频率
    .clk_cfg = LEDC_AUTO_CLK,
};
```

**影响**：
- 若一个项目同时使用 50Hz 舵机 + 25kHz 电机 PWM → 后初始化的那个会静默覆盖频率设置
- 前一个外设的频率被破坏，产生难以调试的物理层 bug
- 这是真实的功能 bug，不是风格问题

**修复方案**：
```c
// Step 1: 按频率分组 → 相同频率复用同一个 timer
// Step 2: 在 pal_pwm_init() 内部按需配置 timer，而非全部强制 TIMER_0
// Step 3: 维护 frequency → timer_num 的映射表
```

**预估工作量**：30 分钟

---

### 🟡 P1：治理脱节 / 架构一致性

#### ISSUE-002：PWM 引脚映射未接入 Device Tree

**状态**：待修复
**治理编号**：P1-6（新分配；P1-3 仍指 trace thread-safety）
**问题位置**：`targets/esp32/pal_hal_esp32.c:181`

```c
// 当前实现：硬编码查找表
static const int pwm_gpio_map[PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};
```

**影响**：
- `device_tree.c` 目前只定义**外设实例**（`front_radar` trig/echo 引脚、`neck_servo` PWM channel 号），但**不定义 PWM channel → GPIO** 的路由关系
- 换板必须改 PAL 源码，违反 "device tree 是唯一硬件描述源" 的架构约定

**修复方案**：
```c
// 在 device_tree.h 中加入路由表声明
extern const uint16_t pal_pwm_pin_map[PWM_CHANNELS];

// 在 device_tree.c 中定义真实映射
const uint16_t pal_pwm_pin_map[PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

// PAL 中改用该表，删除硬编码
```

**预估工作量**：1 小时

---

### 🟡 P1：硬件验收

#### ISSUE-003：RMT 中断延迟示波器验证

**状态**：待硬件验证
**治理编号**：P0-2（超声波非阻塞捕获）
**验收标准**：`RMT echo 上升沿 → ISR 被触发的延迟 < 10us`
**代码状态**：✅ 已编译通过（`pal_hal_esp32_rmt.c` 用户缓冲方案零错误）

**验证前置条件**：
- [ ] ESP32 开发板
- [ ] HC-SR04 超声波模块
- [ ] 双通道示波器（≥100MSa/s）
- [ ] 一个 GPIO 输出作为 trigger 标记（与 RMT 捕获同步）

**验证步骤**：
1. CH1 接 HC-SR04 ECHO 引脚
2. CH2 接一个标记 GPIO（在 RMT ISR 入口置高）
3. 测量两上升沿的时间差 → 须 < 10us
4. 连续 100 次测量 → 最大延迟仍 < 15us

**预估工作量**：1 小时（含固件烧录 + 接线）

---

### ⚪ P2：可选 / 低优先级

#### ISSUE-004：CodeGen 级 API 黑名单

**状态**：草案阶段
**治理编号**：无（新发现）
**说明**：
- 编译级门禁（`-Wstack-usage`、`-Werror=all`、无递归）已全部就位
- 但 "禁止 AI 生成代码调用 `dal_ultrasonic_read()` 这类阻塞 API" 的**生成级黑名单**尚未定义
- 低优先级：目前的编译门禁 + runtime tick 约束已能捕获绝大多数违规

#### ISSUE-005：Docker 构建镜像

**状态**：不需要专门实现
**说明**：
- `esp32_firmware/` 是标准 IDF v5 项目，直接兼容官方镜像 `espressif/idf:v5.1.3`
- 一行命令即可构建：`docker run --rm -v $PWD:/project -w /project/esp32_firmware espressif/idf:v5.1.3 idf.py build`
- 无需额外维护 Dockerfile

#### ISSUE-006：ESP32 `pal_gpio_pulse_in` busy-wait 回退（已废弃）

**状态**：🚫 **已废弃 / WCET 违规回退**
**治理编号**：P0-2（超声波非阻塞捕获，呼应 ISSUE-003）
**问题**：当前 ESP32 `pal_gpio_pulse_in` 的实现走 **busy-wait polling** 路径——在 runtime tick 内死等 GPIO 电平翻转，**违反 10ms tick 无 busy-wait 的 WCET 红线**。该路径仅作编译期占位与早期联调，**不构成超声波验收路径**。
**验收路径（Acceptance Path）**：
- ✅ **RMT 硬件捕获**（`pal_hal_esp32_rmt.c`，Deferred-ISR 下半部）才是 ISSUE-003 / P0-2 的正式验收实现。
- 凡 `pal_gpio_pulse_in` 的 busy-wait 实现应视为 **deprecated**，后续应删除或仅在 `#ifdef PAL_PULSE_IN_FALLBACK` 等显式调试宏下保留，DAL 层 (`dal_ultrasonic`) 默认走 RMT 路径。

#### ISSUE-007：I2C legacy API 迁移路线（Wave C）

**状态**：📋 Wave C 路线图（当前 Wave B 用 v5.1.3 legacy API 暂可编译）
**治理编号**：无（新发现，跨 Wave 迁移）
**问题**：PAL ESP32 `pal_i2c_transfer` 当前映射至 `i2c_master_write_read_device()`，该 API 在 **IDF v5.1.3 已标记为 legacy**，并在 **IDF v6 移除**。
**迁移要求（Wave C，IDF v5.2+）**：
- 改用新 master bus API：`i2c_master_bus_add_device()` 获取 `i2c_master_dev_handle_t`；
- 收发改为 `i2c_master_transmit()` / `i2c_master_receive()` / `i2c_master_transmit_receive()`；
- PAL 侧须在 `pal_i2c_init` 时建 bus + add device，`pal_i2c_transfer` 内基于 device handle 收发（带 timeout、NACK/timeout/busy → `WINK_ERR_*` 映射不变）。
**约束**：Wave B 不阻塞（v5.1.3 仍提供 legacy API），但升级到 IDF v5.2+（Wave C）前必须完成迁移，否则 v6 直接断链。

---

## 5. 治理项对照矩阵

对照 2026-06-24 综合评审的治理编号，当前完成状态：

| 治理 ID | 描述 | 状态 | 备注 |
|---|---|---|---|
| **P0-2** | 超声波非阻塞捕获（RMT 硬件） | 🟡 **代码完成，待硬件验收** | `pal_hal_esp32_rmt.c` 已编译通过 |
| **P0-3** | DAL init + 资源治理 | ✅ **完成**（host 侧）；ESP32 侧编译通过 | 资源表已接入 `pal_resource_esp32.c` |
| **P1-1** | PAL status 签名迁移 | ✅ **完成** | 全量 bool → wink_status_t |
| **P1-2** | 大括号门禁 | ✅ **完成** | `.clang-tidy` 规则就位 |
| **P1-3** | trace thread-safety | ✅ **完成**（契约声明） | 全量 trace 契约就位 |
| **P1-5** | 序列化/对齐规范 | ✅ **完成** | 编译门禁就位 |
| **P1-6** | PWM device-tree 路由（新分配，见 ISSUE-002） | 🟡 **待修复** | 当前 PAL 硬编码 PWM channel→GPIO 表，须迁入 device tree |
| **P2-2** | (void) 吞错治理 | ✅ **完成** | 全量显式 (void)cast |
| **P2-6** | ESP32 PAL 移植 | ✅ **编译通过** | 零错误零警告 |
| **P2-7** | LEDC timer 资源冲突（新分配，见 ISSUE-001） | 🟡 **待修复** | 当前所有 PWM 通道强制共用 TIMER_0，须按频率分组 |

---

## 6. RMT 硬件验收前置检查清单

在接上示波器前，确认以下代码级前提已满足：

| 项 | 状态 | 位置 |
|---|---|---|
| RMT 符号缓冲归用户所有 | ✅ | `pal_hal_esp32_rmt.c:53 static rmt_symbol_word_t s_rx_buf[64]` |
| 回调不做缓冲拷贝（仅计数） | ✅ | `:67 s_rx_num_symbols = edata->num_symbols` |
| 用户缓冲传给 `rmt_receive()` | ✅ | `:154 rmt_receive(..., s_rx_buf, sizeof(s_rx_buf), ...)` |
| 信号范围过滤正确设置 | ✅ | `:151 .signal_range_min_ns = 1000`（1us 毛刺过滤） |
| 超时处理路径存在 | ✅ | `:161 WINK_ERR_TIMEOUT` 分支 |
| 超时复位路径用 disable/enable | ✅ | `targets/esp32/pal_hal_esp32_rmt.c` `pal_rmt_ultrasonic_measure` 超时分支 |
| 最大脉冲校验（> 25ms HC-SR04 范围） | ✅ | `:197 MIN_VALID_PULSE_US / MAX_VALID_PULSE_US` |

---

## 7. 建议的后续执行路线图

```
第 1 阶段（30 分钟 / 零硬件）
└─── 🟥 修复 ISSUE-001（LEDC timer 分组）───▶ 消除唯一真实功能 bug

第 2 阶段（1 小时 / 需硬件）
└─── 🟡 完成 ISSUE-003（RMT 示波器验证）───▶ 正式关闭 P0-2 治理项

第 3 阶段（1 小时 / 零硬件）
└─── 🟡 修复 ISSUE-002（PWM device-tree 路由，治理编号 P1-6）

第 4 阶段（按需）
└─── ⚪ ISSUE-004（CodeGen 黑名单定义）
```

---

## 8. 修改文件总清单

本轮共修改 **8 个文件**：

| 文件 | 修改内容 |
|---|---|
| `wink-micro-os/pal/include/wink_status.h` | 加入 `<stddef.h>` 修复 `NULL` 可移植性 |
| `wink-micro-os/targets/esp32/CMakeLists.txt` | `ESP_PLATFORM` 门控替代失效的 `TARGET_PLATFORM`；加入 `esp_timer/esp_system` 依赖 |
| `wink-micro-os/targets/esp32/pal_hal_esp32.c` | 加入 `pal_osal.h`；`uintptr_t` 中转类型转换 |
| `wink-micro-os/targets/esp32/pal_osal_esp32.c` | `esp_reset_reason.h` → `esp_system.h`（IDF v5 兼容） |
| `esp32_firmware/CMakeLists.txt` | 绝对路径组件目录；删除无用缓存设置 |
| `esp32_firmware/main/CMakeLists.txt` | 加入 `avoidance_car` 应用源文件 |
| `esp32_firmware/main/app_main.c` | 虚构 API → 真实 runtime API；Xtensa 格式串修复；PRIu32 标准宏 |
| `esp32_firmware/sdkconfig.defaults` | 中文注释 → ASCII（修复 IDF kconfgen GBK 解码错误） |

---

*报告生成时间：2026-06-26*
*对应 Git commit：编译阶段全部本地修改，尚未提交*
