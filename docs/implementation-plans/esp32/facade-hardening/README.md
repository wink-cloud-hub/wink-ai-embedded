<!-- SPDX-License-Identifier: Apache-2.0 -->
# ESP-IDF 仿真门面长期可维护性治理与架构演进计划集 (Facade Hardening)

> **创建日期**：2026-09-30  
> **更新日期**：2026-09-30（已全面吸收深度架构评审与防腐门禁意见，完成代码治理与 CI 证据核验四位一体闭环）  
> **状态**：Active（推进中：计划 01、02、03 已圆满完成并全量回归；横向计划 04 启动 PR 门禁与真核验引擎重构）  
> **前序废弃计划**：[`../2026-09-30-esp-idf-facade-governance-and-extensibility-plan.md`](../2026-09-30-esp-idf-facade-governance-and-extensibility-plan.md)（已废弃）

---

## 1. 拆分背景与评审纠偏要旨

在 2026-09-30 的多轮白盒深度审查与架构评审中，团队识别出原方案中多处“代码内部漏洞、形式主义伪闭环、虚构宏名、数字口径漂移”的致命硬伤。本方案据此进行了彻底的工程重构与纠偏：

1. **公开头文件闭包与单头自包含（铁律保护）**：
   - 绝不机械将 `runtime/include` 设为 `PRIVATE`。公开头 [`esp_check.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_check.h) 包含了 `wink_runtime.h` 和 `wink_fault.h`，生成的 [`driver/uart.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/driver/uart.h) 又包含了 `esp_check.h`。必须先在门面内部解耦公开头对 Wink 内核头的反向依赖，统一对齐原厂标准的 `_esp_error_check_failed()` 声明与 `abort()` 语义，建立“单头独立编译（Standalone Header Check）”门禁后，方可物理收敛 CMake 路径；
2. **拒绝自相矛盾与双重事实来源（SSOT）**：
   - 彻底删除计划 01 中移动 Markdown 报告的冲突任务（T3.3），保持 `api-coverage-matrix.inc.md` 和 `include-closure-inventory.inc.md` 现状，报告迁移单列后续工程；
   - 升级 `channels.json` 的核心在于确保 `handwritten` 与 `handwritten_entries` **集合严格 1:1 双向等价**，杜绝两套元数据源漂移；
3. **负测生命周期正确性与 `sim_internal` 物理隔离**：
   - 废弃在 CTest 中使用 `WILL_FAIL` 验证头文件包含失败的错误做法（编译期挂死无法进入 CTest 运行期），改用 CMake `try_compile` 隔离子构建；
   - 拒绝“仅在元数据标记 `sim_internal` 但文件仍留在公开 `include/`”的虚假隔离，将仅供门面内部使用的桩头物理迁入 `src/` 或 `sim_include/`；
4. **生命周期缺口审计取代虚假试点**：
   - 确认 [`esp_gptimer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c) 现已具备基于 `PAL_HWTIMERS_MAX` 的静态池、代际 token 与硬件注销，[`test_esp_gptimer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/test/core/test_esp_gptimer.c) 也已覆盖容量满与 ABA。肃清不存在的 `SOC_TIMER_GROUP_TOTAL_TIMERS` 虚构宏；计划 02 聚焦于**真实生命周期与异常状态回滚审计**；
5. **复位因果图 (DAG) 补全与科学断言**：
   - 固化复位调用链中既有的 `(void)esp_netif_init()` 以及 Wi-Fi reset 间接调用 `sim_network_broker_reset()` 的副作用，明确调度器边界不变量；
   - 修正 NVS 已提交数据模拟物理 Flash，掉电/复位必须保留，断电仅清除 RAM 缓存；解耦 `CONFIG_NVS_MAX_ENTRIES`（存储条目）与 64 句柄槽位限制；
   - 堆内存审计区分“正常优雅退出零泄漏 Delta”与“运行中强行 `esp_restart()` 依赖释放与基线重置”；
6. **软定时器上下文安全与网络架构严格分层**：
   - 警惕 [`wink_soft_timer.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/runtime/src/wink_soft_timer.c) 仅有全局 16 槽且受主循环 `WINK_LIGHT_HARD_LIMIT_US` 强力 WCET 审计的致命风险（连续 3 次超时才报 fault），严禁将 Wi-Fi 握手、DHCP 和用户事件直接塞入系统定时器回调；
   - 计划 03 严格执行文档流转规则，**前置产出《技术设计规格》（Tech Design）**，明确 Timer Daemon 纤程与有界工作队列机制；
   - 纠正 Broker 接口名偏差（`sim_network_broker_register_cb`），扩展回调携带 Netif 实例指针与事件类型，消除 [`esp_mqtt.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c) 中全局单例 `s_mqtt_token` 的并发冲突，制定多 Netif 出口路由规则；
7. **横向合入门禁从“形式主义”走向“真闭环”（最核心升级）**：
   - 识别出 CI 脚本长期依赖 `if: github.event_name == 'workflow_dispatch'` 导致 PR 从不自动跑门禁与分层 lint 的致命漏洞，将其全面接入 PR 自动化并设为 Required Status Check；
   - 废除 Gate 1 仅核验 64 位字符长度与文件名不含 `"fail"` 的虚假检查，重构真核验引擎（重算真实哈希、解析报告 status 和断言结果）；
   - 改造 Gate 4 为真实的已交付用例 Headless 仿真回归，废除仅写 warning 和未消费 pending 文件的设计；
   - 将防腐四铁律编码为 Gate 5.5 ~ 5.8 规则脚本并配齐单测；
   - 纠正多 SSOT 事实源与看板派生数字漂移。

---

## 2. 计划集拓扑与四位一体闭环架构

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
|               ESP-IDF 仿真基建解耦治理四部曲 (Decoupled Governance Quad)                    |
+─────────────────────────────────────────────────────────────────────────────────────────────+

  【计划 1：头文件公开闭包解耦与构建边界硬隔离】 (当前主线，聚焦头文件闭包与 SSOT 门禁)
  文件：01-include-and-build-boundary-plan.md
  ├─ 动作：对齐 _esp_error_check_failed 原厂签名与 abort() 语义，解耦 esp_check.h
  ├─ 动作：CMake PRIVATE/PUBLIC include 边界收敛，sim_internal 物理迁出公开目录
  ├─ 动作：CMake try_compile 隔离负测验证内部头屏蔽；自动化逐头编译验证公开头自包含性
  ├─ 动作：升级 channels.json，门禁强校验 handwritten 与 handwritten_entries 严格 1:1 双向等价
  ├─ 动作：规范 Out-of-Scope API 的 Fail-Loud 宏拦截机制 (依托 wink_sla.h)
  └─ 动作：收割规则显式冲突决议 (未登记同名头 Fail-Loud)，保持报告片段原位不动
                                      │
                                      ▼ (计划 1 绿灯且双 Target 编译通过后开启)
  【计划 2：全门面生命周期与错误语义审计、复位因果图与并发切出守卫】 (深度鲁棒性审计)
  文件：02-handle-pool-and-reset-causality-plan.md
  ├─ 动作：驱动错误码逐函数契约审计，消除有损映射与裸 ESP_FAIL，建立故障注入矩阵
  ├─ 动作：GPTimer 及外设生命周期缺口审计 (底层失败状态回滚、运行中修改 alarm 原子性)
  ├─ 动作：补齐 esp_idf_bridge.c 完整复位 DAG (含 Netif 与 Broker)，明确调度器边界断言
  ├─ 动作：修正 NVS 已提交数据 Flash 持久化语义；建立复位前资源差额 (Delta) 记账比对
  ├─ 动作：梳理全门面所有阻塞切出点，补齐 esp_freertos_assert_not_in_critical 守卫
  └─ 动作：LITE/STANDARD/PRO 容量与 esp_sim_handle 64 槽位上限编译期静态断言
                                      │
                                      ▼ (计划 2 验收完成且技术设计规格评审通过后开启)
  【计划 3：软定时器守护模型与网络出口时序解耦】 (已圆满完成，Timer Daemon / Broker 解耦 / 无头实证全绿)
  前置技术设计：docs/zh/tech-designs/esp32/03-timer-and-network-architecture.md (v1.1 已归档)
  文件：03-timer-and-network-evolution-plan.md
  ├─ 动作：严禁直塞 16 槽/WCET 约束的 soft_timer；落地 Timer Daemon 纤程或有界可取消工作项
  ├─ 动作：新增组件强制回写 esp_idf_bridge.c 复位 DAG，测试即将到期硬重启无幽灵回调
  ├─ 动作：FreeRTOS Timer 核心 API 逐项分类落地，增加 LITE 8 任务槽容量测试
  ├─ 动作：消除 esp_mqtt.c 全局 token/task 冲突，实例级下沉支持多客户端并发
  └─ 动作：多 Netif 默认路由与出口绑定规则落地，broker 回调携带 Netif 身份与事件枚举
                                      │
                                      ▼ (横向守护贯穿始终)
  【计划 4：证据真实性、全自动 PR 门禁与防腐机器拦截】 (横向门禁与真实闭环)
  文件：04-evidence-and-pr-gate-plan.md
  ├─ 动作：解除 CI workflow_dispatch 手动隔离，接入 PR 自动运行并设为 Required Status Check
  ├─ 动作：落地六要素真核验引擎 (重算资产与场景 SHA-256、解析执行报告 status 与断言)
  ├─ 动作：Gate 4 真实行为回归 (改动底层驱动强制运行受影响的已交付示例，超限分批阻断)
  ├─ 动作：防腐四铁律编码落地 (Gate 5.5 ~ 5.8 机器规则与正反例单测)
  ├─ 动作：多 SSOT 事实源与看板派生数字不变量动态校验门禁 (纠偏历史虚假统计)
  └─ 动作：按 Target/Backend 定义异构证据矩阵 (区分 Wasm 仿真 / 硬件真机 / 构建类示例)
```

---

## 3. 交付边界与客观完成定义 (Definition of Done)

为防止将“门面加固（Facade Hardening）”无限膨胀为“把 478 个示例的全部原厂功能一次性写完”，本计划集确立客观、诚实的事实口径与交付边界：

### 3.1 真实数据事实口径（消除口径漂移）
- **官方独立示例总数**：**478 个**
  - **规划支持范围内（In-Scope）**：**312 项**
    - **活跃支持（Active）**：**291 项**（`#001: blink_gpio` 1 项 `verified`，其余 290 项 `planned` 待适配）；
    - **明确暂缓（Deferred）**：**21 项**（暂不投入资源，但处于规划范围内）；
  - **明确排除范围（Out-of-Scope）**：**166 项**（不可逆硬件、物理介质缺失，编译/运行期 Fail-Loud 阻断）。

### 3.2 核心交付定义（SLA Contract）
> **在明确列出的 ESP-IDF 版本（v6.1）、默认 SoC（ESP32）、Profile（LITE/STANDARD/PRO）、Target（Wasm 仿真 + 物理 ESP32）配置下，所承诺的核心 API（Task/Queue/Sem/GPTimer/GPIO/I2C/SPI/UART/WiFi-STA/MQTT）具有完备的编译、行为、故障注入、复位因果和防回归实证；其余能力具有清晰的状态标记与 Fail-Loud 失败边界。**

本计划集完成后，代表**底层仿真基建、门面架构与 CI 机器门禁全面加固完成**，具备抵御大规模示例接入时的防腐能力；后续依据 [`PLAYBOOK.md`](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) 展开全部 312 项 In-Scope（包含 291 项 Active 与 21 项 Deferred）业务示例的批量适配。

---

## 4. 下游示例接入防腐契约（Checklist Ingestion Protocol）

后续开展 `vendor/esp_idfv61/checklist.md` 的逐项适配时，必须严格遵守以下四项防腐铁律（已深度注入至 [`PLAYBOOK.md`](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) 并由【计划 04】的 Gate 5 机器门禁强制拦截）：

1. **禁止侵入式 App 特化（Zero App-Specific Bypass）**：门面内核（`frameworks/esp_idf/`）严禁出现针对特定示例名称的 `if (strcmp(app, ...))` 分支；所有行为差异必须由原厂标准配置宏（Kconfig / sdkconfig）或场景注入（Scenario Fixture）驱动；（由 `g5.no_app_specific_branch` 机器拦截）
2. **禁止为了时序新增一次性任务（Zero Ad-hoc Delay Tasks）**：严禁在协议栈或驱动中随意调用 `xTaskCreate` 启动临时延时纤程，必须统一使用带代际 Token 的定时器工作项，防止打爆 LITE 8 任务槽；（由 `g5.no_raw_delay_tasks` 机器拦截）
3. **新增资源必须自锚复位链（Mandatory Reset Self-Anchoring）**：任何新增的外设驱动或协议栈，只要包含状态机、堆内存分配或句柄槽位，必须在 `esp_idf_bridge.c` 的复位 DAG 中注册对应的 `esp_xxx_sim_reset()`，并在其单测中通过“运行中调用 `esp_restart()` 基线干净”门禁；（由 `g5.reset_registration_verified` 机器拦截）
4. **禁止手写未收割的原厂公开头（Strict Harvester Pipeline）**：示例若需要新的原厂 API，必须通过 Harvester 工具链生成，严禁开发者私自在 `include/` 下手工捏造未经审定的原厂头文件。（由 `check_harvested_headers.py` 机器拦截）

---

## 5. 验收基准总守则与验证矩阵

1. **单头自包含性铁律（Standalone Header Check）**：每个乐鑫原厂规范的公开头文件必须能够在不预先包含其他头文件、不暴露私有内部路径的前提下单头编译成功；
2. **双 Target 同源编译验证**：所有头文件路径与宏改动，必须同时在 Native/Wasm 仿真构建与真实 ESP32 交叉构建（`wink.py esp32 --app devkitc_smoke`）下通过编译，禁止产生平台偏倚；
3. **真实凭据核验铁律**：六要素实证必须由 `evidence_verifier.py` 现场重算资产 SHA-256、解析执行报告 status 和断言结果，严禁仅查字符串长度或文件名；
4. **真实回归执行铁律**：改动底层门面必须触发 Gate 4 真实调用 Headless 运行器回归已交付示例，严禁仅以 warning 或 pending 文件充数；
5. **CI 全自动化阻断**：所有门禁与分层 lint 必须在 PR 模式下自动触发并以 Exit Code 1 阻断违规 PR，严禁仅在手动 `workflow_dispatch` 模式下跑通过；
6. **多 SSOT 零漂移**：数据源 `checklist.data.json`、看板 `CHECKLIST.md` 与生成脚本三方数字完全咬合，不变量门禁在 CI 中全绿。
