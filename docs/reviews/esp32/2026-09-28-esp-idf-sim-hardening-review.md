# ESP-IDF 仿真基建加固结项评审报告

- **报告编号**：REVIEW-20260928-ESP-IDF-SIM-HARDENING
- **评审日期**：2026-09-29
- **关联计划**：[PLAN-20260928-ESP-IDF-SIM-HARDENING](../../implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md)
- **技术设计规格**：
  - [代际句柄令牌原型与技术设计](../../zh/tech-designs/core/2026-09-28-esp-idf-h6-generational-handle-spike.md)
  - [Phase 4 Wasm 彻底热重启技术设计规格](../../zh/tech-designs/core/2026-09-28-esp-idf-phase4-wasm-hot-restart.md)
  - [ADR-0089 分类记账堆内存模型](../../design/decisions/0089-heap-caps-accounting-model.md)
  - [ADR-0053 虚拟时间因果同刻总序仲裁](../../design/decisions/0053-virtual-time-irq-total-order.md)
- **嵌入式仓基线**：`78b6c4a0`（审计时 HEAD）
- **工具仓基线**：`36eb90ff`
- **vendored SDK**：ESP-IDF v6.1 (`fff9895c`)
- **评审结论**：**通过并结项（Approved & Closed）**。加固计划所列之各项核心能力（H1~H6）已全部落地，并在 Windows Host 与真实 Wasm32 双目标下取得了完整、可重放的自动化测试与场景证据；CTest 专项 4 组标签 86/86 100% 通过；4 SoC × 3 Profile 矩阵 12/12 自动化配置通过；UniSim Headless 仿真场景全部 PASS。经用户 2026-09-28 决策，Linux POSIX Host 目标独立暂缓移交后续任务。

---

## 1. 加固背景与目标收敛

在 ESP-IDF 仿真初期阶段，框架层主要关注 API 门面的“存在性”与基础功能调通。随着仿真业务向多外设联动、网络协议栈模拟及高可信虚拟调试演进，原有的简单 mock 与静态池实现暴露出一系列深层基建缺陷：
1. **槽位 ABA 悬挂指针**：资源释放后同槽重新分配，导致旧持有者误操作新对象；
2. **栈污染与重入死锁**：网络回调（MQTT/Wi-Fi）直接在驱动底层同步调用用户回调栈；
3. **调度不可预测性**：毫秒同刻下硬件中断与超时任务抢占无序，无法保证虚拟时间因果全序与重放一致性；
4. **内存模型割裂**：标准 `malloc`/`free` 与 `heap_caps` 分离，造成双重跟踪与配额失真；
5. **Wasm 热重启不彻底**：缺乏跨模块实例的生命周期销毁与单调序号交接。

本次加固（`PLAN-20260928-ESP-IDF-SIM-HARDENING`）旨在全面打破“仅编译通过”的表象，将行为可信度、内存安全性、跨目标一致性与高保真生命周期治理落地为坚实的代码与自动化可重复证据。

---

## 2. 六大加固维度（H1~H6）落地盘点

### 2.1 H1：配置与构建契约对齐
- **问题与重构**：排查并修正了 `CONFIG_FREERTOS_QUEUE_STORAGE_SIZE` 等宏名不一致与 Profile 资源档位穿透风险。
- **矩阵全量校验**：实现了针对 4 款核心芯片架构（`esp32`, `esp32s3`, `esp32c3`, `esp32c6`）与 3 级资源配置（`LITE`, `STANDARD`, `PRO`）的自动化配置探针。12 组组合（4 × 3）在 CMake configure 中全部 0 errors 通过，验证了外设宏限制与静态队列/纤程配额的严格对齐。

### 2.2 H2：生命周期、复位拓扑与沙箱治理
- **构造期保护**：C++ 全局构造函数中创建的同步对象与资源在后续进入 `framework_init()` 时受到白名单保护，杜绝二次初始化清空；
- **`esp_restart()` 严格契约**：在调度器上下文置 `pending` 并安全出让控制权；在非调度器上下文下严禁静默 fall-through，执行断言终止并标明 `noreturn`；
- **自上而下的统一复位拓扑链**：规范了软复位时先停新事件入队、撤销异步令牌，依次排空 HTTP/MQTT、Wi-Fi/Netif/BLE、事件循环，最后重置外设与同步池的清理顺序。

### 2.3 H3：堆能力契约（ADR-0089 分类记账模型）
- **核心契约落地**：采纳 ADR-0089 方案 2，终结了传统嵌入式仿真中模拟分配器与标准 C 库 `malloc`/`free` 不兼容的缺陷；
- **分类记账与上限防溢**：标准分配直接调用系统分配器，不挤占有限元数据槽位；仅对 DMA、SPIRAM、指定对齐等特殊硬件内存执行配额记账与越界监控；经 MSVC ASan 单源探针及 Wasm 配额边界红绿测试充分验证。

### 2.4 H4：虚拟时间确定性与同刻调度总序（ADR-0053）
- **48 字节 POD 结构化 Trace**：定义了跨 32 位/64 位固定 48 字节的 `wink_sim_trace_entry_t` 环形无锁缓冲区，杜绝动态内存分配；
- **因果全序仲裁**：调度器在同一微秒虚拟时间片内，强制推行 `[Phase 0 IRQ 注入] -> [因果唤醒任务 Switch-In] -> [平级超时任务 Round-Robin]` 的同刻总序仲裁（ADR-0053）；
- **确定性回放**：在多任务并发抢占与槽位倒置场景下，经 5 轮完整重放比对，结构化 Trace 实现逐字段比特级 100% 完全一致。

### 2.5 H5：异步事件 FIFO 与网络驱动解耦（D2 契约）
- **深拷贝 FIFO 队列**：实现了容量 32 的静态环形 FIFO 队列与 1024 字节事件载荷深拷贝，彻底消除调用栈局部指针越界风险；
- **独立 Fiber 事件泵**：后台事件分发由独立的 `sys_evt` 纤程任务驱动，单步调用深度严格保持为 1，杜绝事件回调内再 post 引起的调用栈递归爆栈；
- **网络回调安全解耦**：全面重构 `esp_mqtt.c` 与 `esp_wifi.c`，通过事件信封快照机制投递系统事件队列，彻底切断驱动层在当前调用栈直接同步触发用户回调的侵入行为。

### 2.6 H6：代际令牌全家族迁移与 Phase 4 Wasm 彻底热重启
- **32 位代际令牌编码**：
  $$\text{Token} = [1\text{b 标记位}] \mid [6\text{b 槽位}] \mid [4\text{b 家族位}] \mid [21\text{b 全局单调序号}]$$
  覆盖 Task、Queue、Semaphore、EventGroup、NVS、GPTimer、I2C、SPI 全部 8 类句柄，全面消除了资源复用造成的 ABA 悬挂指针；
- **跨实例单调序号交接机制（Handover Mechanism）**：通过导出符号与 Emscripten `EM_JS` 零时延感知机制，在 Wasm 模块被彻底销毁并重新执行 `instantiate()` 时安全交接序号基准，确保新实例创建的句柄序号严格大于前代实例；
- **Node 3 实例端到端闭环**：验证了陈旧句柄在新实例中 100% 拒绝、新鲜句柄单调递增且工作正常、达到序号极限时安全拒绝发放且不发生回绕。

---

## 3. 验证证据与质量门禁

### 3.1 CTest 专项自动化回归（Windows Host）
构建配置：`host / esp32 / STANDARD / MinGW Makefiles`
所有专项标签测试均实现 100% PASS：

| CTest 标签 | 测试用例数 | 通过数 | 失败数 | 通过率 | 覆盖关键模块 |
|---|---:|---:|---:|---:|---|
| `esp_idf` | 33 | 33 | 0 | 100% | FreeRTOS, GPIO, Timer, I2C, SPI, NVS, Event, LEDC, Wi-Fi, MQTT, Heap |
| `esp_idf_corpus` | 8 | 8 | 0 | 100% | 官方上游示例行为同源编译 |
| `esp_idf_wasm` | 35 | 35 | 0 | 100% | Emscripten Wasm32 全量模块静态编译 |
| `esp_idfv61_vendor` | 10 | 10 | 0 | 100% | 官方驱动与组件精选集 |
| **专项总计** | **86** | **86** | **0** | **100%** | **全维度覆盖** |

### 3.2 UniSim Headless 产品级仿真场景验证
通过统一脚手架 `wink.py sim run --mode headless` 验证 Wasm 固件在仿真引擎内的运行表现：
1. **Scenario 1：`vendor/esp_idfv61/blink_gpio`**
   - 耗时：106ms
   - 结果：**7/7 断言 PASS**（GPIO 输出翻转与定时器周期完全吻合）
2. **Scenario 2：`fixtures/esp_idf_h6_handles`**
   - 耗时：90ms
   - 结果：**4/4 断言 PASS**（Queue/Semaphore/EventGroup/NVS 槽位复用旧句柄拒绝与新句柄正常读写）

### 3.3 自动化配置矩阵验证
12 组 SoC × Profile 矩阵探针全量执行结果：
- `esp32` × [LITE, STANDARD, PRO]：3/3 PASS (0 errors)
- `esp32s3` × [LITE, STANDARD, PRO]：3/3 PASS (0 errors)
- `esp32c3` × [LITE, STANDARD, PRO]：3/3 PASS (0 errors)
- `esp32c6` × [LITE, STANDARD, PRO]：3/3 PASS (0 errors)

### 3.4 静态治理与合规门禁
- **开源许可合规**：运行 `python .github/scripts/check_license_map.py`，全部源码与测试资产符合分层许可契约（LGPL-3.0-only / Apache-2.0 / GPL-3.0-only），**100% PASS**；
- **头文件收割防御**：运行 `python .github/scripts/check_harvested_headers.py`，**0 errors**；
- **架构分层门禁**：运行 `winkcli lint --pack layering --pack api --pack wasm`，App/BAL/DAL/PAL 各层依赖无倒灌，**0 findings**。

---

## 4. 授权豁免与暂缓边界说明

依据 2026-09-28 用户明确决策：
- **POSIX Host 宿主（Linux `sim_ctx_posix_ucontext.c`）与远端 CI 专项暂缓**：现行 Host target 架构基于 Win32 Fiber 实现，其在 Linux/WSL 环境下的 POSIX ucontext 移植、Linux Host 原生构建及远端 CI coverage 门禁独立归档，移交后续专用跨平台移植任务实施。
- **本地环境边界**：当前验证闭环覆盖 Windows (MinGW/Fiber) + Wasm32 (Emscripten/Node)，真实 ESP32 物理烧录与 idf.py 构建依赖物理硬件环境，与仿真基建加固正交。

---

## 5. 结项结论与后续建议

### 5.1 评审结论
ESP-IDF 仿真基建加固实施计划（`PLAN-20260928-ESP-IDF-SIM-HARDENING`）所规划之全部可执行切片（A、B、C、D、E、F）在规定边界内均已达成验收标准，核心技术设计规格与实施计划已完成归档，活文档与 API 覆盖矩阵已全面同步回写。**同意结项归档。**

### 5.2 后续演进建议
1. **POSIX Host 独立移植**：在独立计划中推进 Linux/macOS 环境下的 POSIX ucontext 纤程调度器开发与 CI 接入；
2. **更多上游外设语义深化**：借鉴 H4/H5/H6 范式，推进 BLE GAP/GATT、USB-CDC 及 LCD 显示驱动的高保真仿真；
3. **UniSim 场景库扩展**：将更多官方 corpus 示例扩充为 UniSim Headless 端到端场景测试，保持长效防劣化。
