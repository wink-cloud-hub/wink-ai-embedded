<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF 仿真治理真理源（SSOT）深度白盒专项审计与纠偏报告

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-09-29 / 2026-09-30 |
| 审计对象 | [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) (58 项能力)、[`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json) (478 条目)、[`CHECKLIST.md`](../../../wink-micro-app/vendor/esp_idfv61/CHECKLIST.md) (只读看板) |
| 嵌入式仓 HEAD | `4902ab1f` -> `f945a6dc`（`fix(esp32): align capability catalog with disk realities and correct OTA/MIPI triage in checklist data`） |
| 关联 ADR | [ADR-0090](../../decisions/unisim/0090-centralized-pluggable-gate-system.md)、[ADR-0091](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)、[ADR-0092](../../decisions/unisim/0092-esp-idf-3tier-peripheral-decoupling.md) |
| 审计性质 | 架构级真理源（SSOT）白盒穿透审计与即时纠偏闭环 |

---

## 一、 专项审计总体结论

在用户高度重视与深度质量要求的指导下，架构组对 `.governance/catalog/capability-catalog.yaml`（能力图谱字典）与 `.governance/data/checklist.data.json`（全量 478 项示例总账）启动了**全维度四层靶向白盒审计**。

### 核心结论摘要：
1. **基底拓扑结构绝对健康**：
   - 58 项原子能力的依赖关系图（`depends_on`）经 DFS 拓扑排序校验，**循环依赖数（Cycle）为 0**，**悬挂依赖（Dangling Reference）为 0**；
   - 478 个官方示例引用的全部原子能力 ID，**100% 合法注册于字典中**，无空依赖或野指针引用；
   - 黄金标杆工程（`#001: get-started/blink_gpio`）的场景脚本真实 SHA-256 与登记防伪哈希 **比特级 100% 吻合**。
2. **排查并彻底清除了 3 类深层隐患**：
   - **分层枚举非法**：修正了 4 处不合规的 `layer` 枚举（如 `build`、`host_bridge` 归正为合法的 `facade`）；
   - **代码归属路径漂移**：修正了 13 处已实现能力在 `owned_paths` 中因早期臆断导致的路径脱节（如 `freertos_tokens.c`、`src/heap/` 漂移），使其与物理磁盘真实源码 100% 咬合；
   - **状态虚标降级**：将 10 项尚未编码的代码能力（如网络宿主隧道、mbedtls 拦截等）从虚假的 `implemented` 降级为真实的 `planned`，恢复契约诚实；
   - **排除范围纠偏**：修正了 `#042 mipi_dsi` 理由错乱问题，并将被误判排除的 `#142` 与 `#145` 2 项 OTA 固件升级示例纠偏回归为 `in_scope (deferred)`。

---

## 二、 四大切片审计发现与纠偏记录

### 1. 切片一：`capability-catalog.yaml` 能力字典审计

| 审计维度 | 审计前状态 | 发现的缺陷/隐患 | 采取的纠偏动作 | 审计后状态 |
|---|---|---|---|:---:|
| **DAG 拓扑** | 58 项能力 | 无闭环死锁，无循环依赖 | 确认保留 | ✅ **PASS** |
| **Layer 分层合法性** | 58 项能力 | 4 项能力使用了规范外非法 layer：`cap.build.kconfig_parse` (`build`), `cap.crypto.mbedtls_shim` (`host_bridge`), `cap.net.host_socket` (`host_bridge`), `cap.net.host_ws_tunnel` (`host_bridge`) | 全部规整映射至标准六层中的 `facade` | ✅ **PASS** |
| **`owned_paths` 磁盘咬合** | 58 项能力 | 13 项能力的 `owned_paths` 为早期手写虚拟路径（在磁盘上根本不存在，如 `src/heap/esp_heap_caps.c`、`pal_wasm_i2c.c`） | 全量核验真实源码树，重定向至真实文件（如 `src/core/esp_heap_caps.c`、`pal_wasm_ch2_bus.c`、`pal_wasm_ch1_gpio.c`、`pal_rmt.h`） | ✅ **PASS** |
| **`status` 契约诚实度** | 58 项能力 | 10 项尚未编写 C 源码的远期能力被虚假标记为 `implemented` | 依循 ADR-0012「契约诚实优于静默降级」，全量降级回 `planned` | ✅ **PASS** |

### 2. 切片二：已实证黄金用例（`#001: blink_gpio`）实据穿透

| 审查要素 | 规范契约要求 | 实际核对结果 | 结论 |
|---|---|---|:---:|
| **范围与成熟度** | `in_scope` + `active` | 符合（`schedule: active`） | ✅ 合规 |
| **需求审计确权** | `verdict: audited` + `arch_team` | 符合（审计配置 `wasm_sim_standard`） | ✅ 合规 |
| **依赖闭包满足度** | 依赖项全部实现且无死锁 | 依赖闭包 4 项能力（`fiber_task`, `sync_tokens`, `edge_trigger`, `ws2812`）全部为 `implemented` | ✅ 合规 |
| **场景脚本防伪哈希** | `scenario_sha256` 匹配物理磁盘 | 磁盘计算：`48e7494366d5...` ⟷ 登记值逐字一致 | ✅ 合规 |
| **工程落盘路径** | `target_app_dir` 真实存在 | 物理目录 `get-started/blink_gpio/` 真实存在且 `wink-app.json` 完整 | ✅ 合规 |

### 3. 切片三：168 项产品排除（Out-of-Scope）边界审查

经全量分类统计，168 项排除项中，**165 项具备不可逆物理介质事实**，但在审查中纠偏了 **3 项历史复制错乱与误排除条目**：

- **缺陷 1：`#042 (peripherals/lcd/mipi_dsi)` 理由错位**：
  - *原理由*：误填为“调用底层硬件 eFuse 物理熔断驱动”；
  - *纠偏*：修正为客观物理事实“MIPI-DSI 高速差分显示物理接口”。
- **缺陷 2 & 3：`#142 (system/ota/advanced_https_ota)` 与 `#145 (system/ota/partitions_ota)` 误判排除**：
  - *原判定*：被误划为 `out_of_scope`，理由同样被错填为 eFuse；
  - *架构真相*：OTA 固件空中升级依赖的是 Flash 分区写入与网络流式下载，在 Sprint 0 交付 RAM 分区表与 VFS 沙箱后，**完全属于纯软件可模拟范畴**；
  - *纠偏*：移出排除名单，确权为 `in_scope`，排期设置为 `deferred`（待 Sprint 3 网络协议期交付）。

**当前排除项分布（纠偏后共 166 项）：**
- 蓝牙射频基带与 Controller 调测：98 项
- USB-OTG 物理差分 PHY 收发器：17 项
- 硬件 eFuse 不可逆熔丝烧写与加密引擎：10 项
- 802.15.4 / Zigbee 2.4GHz 射频物理层：10 项
- TWAI/CAN 差分总线物理收发器：5 项
- 芯片 ROM 硬件二级引导链：4 项
- 外部以太网 RMII PHY 变压器：3 项
- 硬件编解码/ISP/PPA 加速器：7 项
- 其他底层微功率/工厂射频校准：12 项

### 4. 切片四：全量 478 项数据的门禁机械一致性

运行 Gate 1~4 全量集中式门禁，自动化穷举验证结果：
- `g1.path_unique`：478 项 `upstream_path` 与 `target_app_dir` 全局唯一，零冲突；
- `g1.cap_id_exists`：478 项引用的能力全部合法存在，零悬挂；
- `g1.id_format` & `g1.version_alignment`：全部符合 `Spec v2.0.0` 语义契约；
- `g1.execution_configs` & `g1.orthogonal_states`：五维正交状态逻辑自洽，无冲突。

---

## 三、 归档影响与后续执行准则

1. **真理源纯度达到 100% 生产级**：经过本次白盒精审与实地校正，`capability-catalog.yaml` 与 `checklist.data.json` 已经完全杜绝了“概念性虚标”与“路径悬空”，成为完全可信的架构资产。
2. **后续单项迁移操作红线**：
   - 严禁手工编辑 `CHECKLIST.md`，任何状态变动仅允许在 `checklist.data.json` 中增量修改并通过 `generate_checklist_v1_1.py` 刷新；
   - 任何新声明的 `target_app_dir` 必须严格遵循 `<category>/<app_name>` 两级目录标准；
   - 任何新标记为 `verified` 的条目，必须严格提供无头仿真真实日志并计算防伪 SHA-256 填入 `evidence`，否则 Gate 1 会直接 Fail-Loud 阻断合入。
