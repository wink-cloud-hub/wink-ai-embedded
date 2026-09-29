<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF v6.1 示例分类规范（CLASSIFICATION-SPEC v1.0）架构评审

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-09-29 |
| 评审对象 | [`wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md`](../../../wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md) v1.0（538 行，宣称"正式生效版"） |
| 嵌入式仓 HEAD | `81a4fd9b2b6a50188bcf7ebc65e084297c7dfa85`（`docs(esp_idfv61): add CLASSIFICATION-SPEC.md taxonomy and governance spec`） |
| 关联文档 | [`PLAYBOOK.md`](../../../wink-micro-app/vendor/esp_idfv61/PLAYBOOK.md) v1.0、[`CHECKLIST.md`](../../../wink-micro-app/vendor/esp_idfv61/CHECKLIST.md) v1.1、[`README.md`](../../../wink-micro-app/vendor/esp_idfv61/README.md) |
| 关联 ADR | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0043](../../decisions/tools/0043-yaml-driven-layer-lint.md)、[ADR-0085](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)、[ADR-0087](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)、[ADR-0088](../../decisions/core/0088-esp-idf-version-strategy-triggers.md)、[ADR-0089](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)、[ADR-0053](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)、[ADR-0014](../../decisions/unisim/0014-sim-single-virtual-core.md) |
| 关联设计规范 | [`docs/zh/design/04-wasm-simulation/`](../../zh/design/04-wasm-simulation/00-README.md)（UniSim 现行保真轴 A~F SSOT） |
| 关联评审 | [2026-09-28-esp-idf-sim-baseline-review](2026-09-28-esp-idf-sim-baseline-review.md)、[2026-09-28-esp-idf-sim-hardening-review](2026-09-28-esp-idf-sim-hardening-review.md) |
| 评审性质 | 文档/架构评审（只读快照）。本记录不修改被评审文件 |

---

## 一、总体结论

**方向正确，法理缺失。** 规范提出的问题识别（六层职责、能力图谱替代静态 Tier、六态分类、Fail-Loud）全部命中真实病灶，`§一` 对早期脚本路径字符串匹配的批判也切中要害。

但 v1.0 存在 **6 项阻断级缺陷**，其中 4 项是规范**违反自己刚立的铁律**：判定依据本身未被机器验证（ID 手填、`tier` 是死字段、能力字典是散文、`[x]` 守在派生文件上）。按现状发布为"宪章"，会把这批系统性偏差固化为全量 478 条的既定基线。

**建议裁决：`v1.0` 状态由"正式生效版"回退为 `Proposed`**，修完 C-01~C-06 后再定版。

### 缺陷分布

| 严重度 | 数量 | 编号 |
|---|---:|---|
| 阻断（Blocking） | 6 | C-01 ~ C-06 |
| 架构（Architectural） | 5 | A-01 ~ A-05 |
| 工程化（Engineering） | 10 | E-01 ~ E-10 |

---

## 二、阻断级缺陷（Blocking）

### C-01 `§六` 打样 ID 与 SSOT 大面积对不上，2 个示例不存在

| 规范打样 | 规范声明 | `CHECKLIST.md` 实际 |
|---|---|---|
| 打样 1（:396） | `#004 peripherals/adc/oneshot_read` | ✅ `CHECKLIST.md:82` 一致 |
| 打样 3（:425） | `#028 peripherals/rmt/led_strip` | ❌ 真实为 **#064**（`CHECKLIST.md:142`）；#028 是 `peripherals/i2s/i2s_advance/i2s_usb`（`:106`） |
| 打样 4（:439） | `#029 peripherals/rmt/ir_nec_transceiver` | ❌ 真实为 **#063**（`CHECKLIST.md:141`）；#029 是 `peripherals/i2s/i2s_basic/i2s_pdm`（`:107`） |
| 打样 2（:411） | `#176 system/ulp/ulp_fsm/ulp_adc` | ❌ 真实为 **#171**（`CHECKLIST.md:257`）；#176 是 `system/ulp/ulp_riscv/gpio_interrupt`（`:262`） |
| 打样 5（:453） | `#197 protocols/websocket/server` | ❌ **语料中不存在**；#197 是 `protocols/http_server/ws_echo_server`（`:291`） |
| 打样 6（:467） | `#426 build_system/cmake/custom_component` | ❌ **语料中不存在**；#426 是 `build_system/cmake/component_manager`（`:568`） |

连带影响：

1. `§一` 偏差溯源的**案例 3（WebSocket 服务端方向倒置）与案例 4（build_system 整组排除）两个"根因证据"，建立在不存在的示例上**。
2. 案例 4 的"整组 19 个工程"数字对不上：`build_system/cmake/*` 实际仅 7 条（#426~#432），19 是 `build_system/*` 全类跨度（#426~#444）。
3. 规范全篇在批判"脚本机械匹配"，而本节的 ID 恰恰是手写拍脑袋——**规范重复了它所批判的错误**。

**根因**：`id` 采用连续序号（Schema `pattern: ^[0-9]{3,4}$`，:145），478 条语料一旦增删，全文档/ADR/PLAN/CHECKLIST 的引用全部漂移，且无任何机制发现漂移。

**建议**：

- 立即修正全部 4 处错误 ID；删除或替换 2 个不存在的示例打样。
- `id` 改为**内容派生稳定标识**（如 `esp-per-rmt-led_strip`），序号退化为纯展示字段。
- 增加 CI 校验：`upstream_path` 必须唯一存在于 478 语料清单中，路径不存在直接 fail（这是最小成本、最高收益的一条门禁）。

---

### C-02 `unisim_fidelity_profile` 的 A~D 与 UniSim 现行 A~F 语义完全相反

`CLASSIFICATION-SPEC.md:212-220` 定义：

```
axis_a_timing / axis_b_concurrency / axis_c_electrical / axis_d_peripheral_state
```

字段描述写"与 UniSim 现行保真轴 (A~F) 的显式映射，**消除两套保真语言并行的歧义**"。

但现行 SSOT（[`docs/zh/design/04-wasm-simulation/03-axes/00-README.md`](../../zh/design/04-wasm-simulation/03-axes/00-README.md)）的 A~F 是：

| 轴 | 现行语义 | Primary home |
|---|---|---|
| **A** | **物理/外设来源**（Pin / Bus / Analog / Buffer 通道） | `08-channel-routing` |
| B | 时基（delay / 超时 / 节拍统一） | `02-virtual-clock` |
| C | 定时器语义（HW timer / PWM 占空 / capture） | `09-timer-and-pwm-semantics` |
| D | 中断模型（ISR 延迟 / 嵌套 / 分发） | `04-interrupt-model` |
| E | 调度与并发（任务/纤程/中断上下文） | `03-scheduler-and-concurrency` |
| F | 故障与可观测化（OOM / WDT / 断言 / Trace） | `05-memory-and-faults` |

**字母 A 在两处指代完全不同的东西。** 代码侧已经能佐证：仓库现役实现按"A = 外设来源"命名（`wink-micro-os/targets/wasm/pal_wasm_ch4_buffer.c:4`：`Wasm target Axis A (CH4) Buffer Payload`）。

后果：CI 一旦按字母做保真校验必然误判；且规范在声称消除歧义的同时**新增了第三套保真语言**（第三套是场景 header 已有的 `accuracyMode`，见 `PLAYBOOK.md:169`）。

**建议**：

- **删除 `unisim_fidelity_profile` 整块**。
- `fidelity_contract` 只保留一处保真声明，引用真实 A~F 轴 ID（`axis: [B, C]` 形式）+ 场景 `accuracyMode` 枚举。
- 顺带消除 `timing_model`（:208）与 `axis_a_timing` 的语义重复——同一件事被编码两次，且两处词汇表不同。

---

### C-03 Gate 1 校验的是派生文件，直接违反铁律四

- 铁律四（:38-40）："Markdown 为派生视图，**严禁纯手工随意修改表格数据**而破坏机器校验链条"。
- Gate 1（:513）："如果某个 PR 试图将某个示例标记为 `[x]`（已完成），但该示例在数据库中的状态依然是 `pending_audit`…CI 立即判为失败"。

`[x]` **只存在于 `CHECKLIST.md` 这个派生视图里**。门禁守在派生文件上，SSOT 反而无人守——这是铁律四的直接反面。

**根因**：`scope_and_maturity` 六态描述的是**范围**（要不要做），不是**成熟度**（做没做完）。字段名 `scope_and_maturity` 名不副实。真正的交付态与证据全部寄居在 Markdown 勾选框。

**建议**：Schema 新增交付态维度，`[x]` 改为单向渲染产物：

```json
"delivery": {
  "state": "planned | building | verified | regressed",
  "app_dir": "wink-micro-app/vendor/esp_idfv61/<name>",
  "assets": { "device_tree_sha256": "", "js_sha256": "", "wasm_sha256": "" },
  "scenario_sha256": "",
  "last_verified_at": "ISO8601",
  "last_verified_commit": "<git sha>",
  "harness": "run_esp32_headless_evidence.ps1"
}
```

渲染规则：`[x]` ⟺ `delivery.state == "verified"` 且 `assets` 三件套哈希非空。Gate 1 改为校验 `delivery.state` 跃迁的前置条件；Gate 4 的"证据陈旧作废"也才有可比较的基线（当前无基线可比）。

---

### C-04 Capability Catalog 是 Markdown，却被当作机器校验源

铁律四要求 SSOT 机器可读，但 `§四`（:277-330）的 Capability Catalog 只是一张 ASCII 表。CI 第 3 步（:508）却要"校验所声明的能力 ID 是否全部在 `Capability Catalog` 中存在"——**只能靠解析 Markdown 表格**，与铁律四自相矛盾。

附带问题：`§四` 的"预留命名空间"（`cap.ble.*` / `cap.usb.*` / `cap.eth.*` / `cap.sec.*`，:325-328，标注"待定"）在数据层与已实现能力**完全无法区分**，`required_capabilities` 的 `pattern` 也不阻止引用未定义语义的 ID。

**建议**：抽出 `capability-catalog.yaml` 作为 SSOT，Markdown 表由其渲染。字段需覆盖门禁所需的全部信息：

```yaml
cap.pulse.tx_buffer:
  layer: pal                # 单值，见 C-06
  status: implemented       # planned | implemented | stub | verified
  owned_paths:              # Gate 4 可计算的前提，见 E-01
    - "wink-micro-os/pal/include/hal/pal_rmt.h"
    - "wink-micro-os/targets/*/pal_*rmt*"
  soc_support: [esp32, esp32s3, esp32c3, esp32c6]
  cross_mcu_evidence:       # 落实 Gate 2 要求的 8051 通用性论证
    - "wink-micro-os/frameworks/mcs51/..."
```

---

### C-05 ADR-0012 误引；现清单 38% 条目正建立在这个误引之上

`§二 Q1`（:106）与 `§五 [6]`（:384-386）把 `out_of_scope_product` 的依据定为 ADR-0012。

**ADR-0012 的实际决策范围**：其"决策结论"是"PAL/HAL 新增接口时如何处理跨 target 行为差异"（选项 C：显式拒接 + 头文件契约下调）。全文**不涉及任何产品范围剪枝授权**，也不含摄像头/熔丝/ETH 变压器等物理介质的判定标准。

而 `CHECKLIST.md:19` 与 `:53` 已经用这个误引把 **183/478（38%）** 条目标为 Out-of-Scope，理由为"缺乏物理射频波形、外部 PHY 变压器硬件或物理熔丝"——**这些恰恰是铁律三要清算的对象**。规范目前等于"用错误的法典去纠正错误的判决"。

**建议**：为 `out_of_scope_product` / `contract_blocked` 增设**可验证的证据字段**，把"承诺"（:386、:381）变成机器断言：

```json
"exclusion_evidence": {
  "physical_medium": "MIPI-CSI/DVP 物理差分对",
  "sla_block_symbols": ["esp_efuse_write_block", "phy_init"],
  "build_must_fail_with": "Wink SLA Violation"
}
```

**关键可行性**：`WINK_SLA_ERROR` 机制**已经真实存在并大规模使用**——`wink-micro-os/frameworks/esp_idf/include/wink_sla.h:7`（`__attribute__((error))`），已覆盖 `driver/spi_slave_hd.h`、`driver/uhci.h`、`esp_mac.h`、`esp_intr_alloc.h`、`spinlock.h` 等数百个符号；收割器侧已有 `sla.supported_prefixes` 机制与覆盖矩阵（`frameworks/esp_idf/docs/02-api-coverage-matrix.md`）。

因此 CI 可以真的去编译该示例、**断言它以指定错误文本编译失败**。这比任何文字描述都有说服力，也让 `contract_blocked` 的"必须记录无法兑现的具体技术机理"（:381）有地方可写。

---

### C-06 铁律二禁 Tier，Schema 却 `required` tier；且 Catalog 自违"能力只能属一层"

**(a) Tier 死字段**：铁律二（:29-32）"严禁单维静态 Tier 固化"，但 `scope_and_maturity.tier` 在 :225 的 `required` 列表中，且 6 个打样全部打了 tier。而 Gate 1 只检查 `pending_audit` / `candidate_provisional`，**没有任何门禁消费 `tier`** → 既是死字段，又直接违宪。

**(b) Catalog 双层标注**：`§二`（:67）写"任何示例所依赖的任何一项特性，其实现**必须且只能**归属于以下六层之一"，但 Catalog 里：

- `cap.core.category_heap [层级①/③]`（:285）
- `cap.analog.adc_dma [层级②/④]`（:289）

即规范自己的字典违反了自己的不变量，且 Schema 无字段可表达多层级归属。

**(c) 打样 6 无法通过自己的 Schema**：`architectural_placement.behavior_layer` 枚举仅 `["pal_target","sim_kernel","device_model","host_tunnel"]`（:182），而打样 6（:473）的行为实现是"`wink-tools` CMake 原生组件解析器"——**不对应任何枚举值**，该打样无法被自己定义的 Schema 编码。

**建议**：

- `tier` 删除，或降级为非必填、纯排期提示（并从 `required` 移除）。
- **能力→层的映射从 Catalog 单向派生**：`architectural_placement` 不由人填，而由 `required_capabilities` 反查 Catalog 计算。这才真正实现铁律二的"网状多对多复用"，同时消灭一整类人工填错。
- 打样 6 的 `behavior_layer` 需扩展枚举（如新增 `build_tooling`）或重新归类。

---

## 三、架构级缺陷（Architectural）

### A-01 打样 3 的"特别红线"已被现役代码突破，且 Gate 2 抓不到

打样 3（:433）立下红线：

> **严禁在 PAL 层编写 WS2812 颜色协议代码！** PAL 只接收脉冲符号流，由领域模型负责将脉冲解析为 RGB 数组送给前端。

现役代码同时违反红线与其自身描述：

| 位置 | 事实 | 冲突 |
|---|---|---|
| `wink-micro-os/targets/wasm/pal_wasm_ch4_buffer.c:18` | `wink_status_t pal_ws2812_write(wink_pin_t pin, const uint8_t *rgb_buf, size_t num_leds)` | **PAL 签名直接吃 RGB 帧缓冲**——正是红线禁止的 |
| `wink-micro-os/dal/src/output/dal_ws2812.c:92` | `/* WS2812 order: G -> R -> B */` + reset symbol 编码 | **WS2812 线协议住在 DAL 层**，而规范归其为层级④ 领域模型 |
| 打样 3（:432） | `state_owner: "rmt_tx_channel_t" + "dal_ws2812"` | 规范自己把协议状态持有者写进了 DAL |

即：**规范、红线、现状三者互相矛盾**，而规范未识别这一点。

**且 Gate 2 抓不到**：Gate 2（:515）仅在 PR 修改 `wink-micro-os/pal/include/` 时触发，而 `pal_ws2812_write` 的声明/实现在 `targets/` 下。

**建议**：

1. 单独立 ADR 裁决 `dal_ws2812` / `pal_ws2812_write` 的分层归属（属"破坏性调整"，不能由本规范单方面裁定）。
2. Gate 2 扫描面扩至 `pal/` + `targets/` + `osal/`。
3. 增加**负向规则**：`pal_*` 符号名与参数类型中禁止出现协议/器件语义词（`ws2812|rgb|pixel|frame|necir|at24|i2c_addr` …）。这条规则本身比任何红线文字都有效。

### A-02 六层词汇与仓库既有分层/lint pack 无映射，Gate 3 只跑了 2/7 个 pack

仓库实际分层词汇是 **App / BAL / DAL / PAL**（`AGENTS.md`、ADR-0023/0038/0049）。规范另起一套 Facade / PAL-DAL / CoreSim / Model / Build / HostBridge，**缺失 BAL 层**——而 BAL 是本项目承重的业务抽象层，官方示例虽多不直接用，但 `arch` 分类与 lint 口径必须与之对齐，否则 Gate 3 不可执行。

`wink-tools/tools/lint/rules/` 实际有 **7 个 pack**：

| pack id | Gate 3 是否启用 | 与本规范的关系 |
|---|---|---|
| `layering` | ✅ | 分层倒灌 |
| `api` | ✅ | API 形态 |
| `dal` | ❌ | **DAL 越界——本规范核心关切** |
| `isr_rules` | ❌ | **ISR 边界（对应 cap.irq.*）** |
| `user_surface` | ❌ | **门面用户面（对应层级①）** |
| `wasm_parity` | ❌ | **双靶一致性（ADR-0002 根基）** |
| `i18n` | ❌ | 无关 |

最关心的 4 个恰好在漏掉的里面。且规范从未给出"规范条款 → lint rule id"的映射表，Gate 3 只是"跑一下"而非"可判定"。

**建议**：补映射表；Gate 3 改为全量 pack（`layering, api, dal, isr_rules, user_surface, wasm_parity`）。

### A-03 `external_topology` 三处硬伤

**(a) `bus` 枚举与 Catalog 不闭合**（:161 vs :300-328）。枚举仅 `gpio/i2c/spi/uart/rmt/adc/dac/sdio/usb/virtual_netif`，而 Catalog 声明了 `cap.proto.i2s_stream`、`cap.proto.twai_can`、`cap.ble.*`、`cap.usb.*`、`cap.eth.*`、`cap.sec.*`。**任何依赖 I2S / TWAI / BLE / ETH / SEC 的示例都无法编码**——这直接封死了 Bluetooth 大类（147 条，#244~#390）与 Low Power 大类。

**(b) 与 `wink-app.json` / `device-tree.json` 构成第二套拓扑 SSOT**。`PLAYBOOK.md:150-157` 已有 `devices` 拓扑，且 `runtime_device_tree.py` 是唯一生成器。`checklist.data.json` 再存一份引脚映射 = 第二事实源。

**建议**：`checklist.data.json` 只声明**需求**（需要几个什么器件、什么连接关系），实际引脚映射由 `device-tree.json` 持有，CI 做一致性比对。

**(c) 表达不了双端点**。打样 4（:441）把 `GPIO18->GPIO19` 塞进单个自由文本 `pin_or_addr`。应拆成两条 entry 并新增 `role: tx|rx`。

### A-04 `target_soc` 单值表达不了 `soc_mismatch`

Schema 用单标量（:152）。但 `soc_mismatch` 的定义（:383）恰恰是"该 SoC 上物理不存在"——同一示例在 ESP32 是 `in_scope_deficit`、在 C3 是 `soc_mismatch`，单值无法表达。而打样 1/3/4/6 全部填 `"all"`，等于没填。

**仓库已证明 per-SoC 是一等公民**：`build/soc_c3/`、`build/soc_c6/`、`build/soc_s3/` 三个独立构建树已存在；[ADR-0085](../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md) D1 明确"门面以 `SOC_*` 为唯一合法性判据，随 `WINK_ESP_TARGET` 切换"。

**建议**：改为矩阵。

```json
"soc_matrix": {
  "esp32":   { "status": "in_scope_deficit", "reason": null },
  "esp32c3": { "status": "soc_mismatch", "reason": "SOC_DAC_SUPPORTED = false" }
}
```

### A-05 顶层缺 `spec_version`，`§八` 版本演进与迁移脚本无从校验

`§八`（:532-537）定义了 MAJOR.MINOR 语义化版本、Breaking 变更需"提供迁移转换脚本，对 `checklist.data.json` 中全量已审定档案执行版本升级回写"。但条目 Schema 与任何顶层 manifest 中**都没有 `spec_version` / `schema_version` 字段**——CI 无从判断一份数据是否需要迁移，迁移脚本也无从定位目标版本。

**建议**：顶层 manifest 增加 `spec_version` / `schema_version`，每条 entry 冗余 `written_at_spec_version`。

---

## 四、工程化缺陷（Engineering）

| 编号 | 问题 | 位置 | 建议 |
|---|---|---|---|
| **E-01** | **Gate 4 不可扩展**："底层核心调度器变动 → 全量 478 重验"。缺 capability→源码路径映射，CI 无法计算影响闭包 | :518-519 | 用 Catalog 的 `owned_paths` 做变更影响闭包，分 **PR 阻断集 / nightly 全量** 两档；补隔离区（quarantine）机制 |
| **E-02** | **断言容差无法表达**：打样 1 要求 `2048 (±5)`；CMS8S 清单已沉淀 `$between [150,260]` 这类"按可观测层诚实标定"经验（`vendor/cms8s78xx/CHECKLIST.md`） | :256, :407 | `failure_criteria` 改数组 + 新增 `matcher: {op: eq\|between\|regex, lo, hi}` |
| **E-03** | **`observability_level` 枚举不可机读**：值含空格与全角括号（`"Level 1 (UI可视)"`）；且丢失 CMS8S 清单已有的 `🚫 Blocked（未建模会死锁）` 态 | :250 | 改 slug 枚举 `L1_ui / L2_log / L3_probe / L4_internal / LX_deadlock` |
| **E-04** | **`timeout_ms` 语义歧义**：Schema 默认 30000 是墙钟，而场景 header 是 `timeoutUs` 虚拟时间（`PLAYBOOK.md:169`），打样实证为"虚拟 3500ms / 墙钟 95ms" | :253 | 拆为 `timeout_virtual_us` + `timeout_wall_ms`，证据同时记录两者 |
| **E-05** | **`failure_criteria` 单值**：实际需 assert + timeout 组合判定 | :256 | 改数组 + `combine: all \| any` |
| **E-06** | **478 × 七维人工审定不可行**：`§八` 只说"以案促建"，无排序规则 | :530 | 加**能力优先审计序**：先审被 N 个示例依赖的原子能力；并规定"示例 `audit_verdict` 置 `audited` 前，其 `required_capabilities` 必须全部 `audited`"。一次深审服务数十示例 |
| **E-07** | **缺负例要求**（详见下节） | `acceptance` | 见 §五 |
| **E-08** | **铁律五 3 天仲裁无字段**：`§零` 铁律五（:42-46）规定 `classification-dispute` 标注 + 3 工作日裁定，Schema 无 `auditor` / 时间戳 / 裁定路径 | :42-46 | 新增 `audit: { auditor, audited_at, dispute_ref, ruling_path }` |
| **E-09** | **`compatibility.source_code_policy` 与"一行不改"门禁脱节**：枚举含 `wrapper_main` / `shimmed_harness`（允许改写），但 Schema 无 `mirrored_files` 哈希字段；而 `PLAYBOOK.md:23` 要求 SHA-256 锁定，数据却存在 `wink-app.json` | :190-198 | 哈希以 `checklist.data.json` 为 SSOT（呼应 C-03），`wink-app.json` 由其校验；明确各 policy 的适用条件与审批要求 |
| **E-10** | **缺真机-仿真差分维度**：七维元数据只记录仿真侧单边验收，无法发现"仿真跑绿、真机跑挂"的漂移——而这是同源双靶产品的核心失败模式 | `acceptance` | 增设 `differential: { golden_vectors, tolerance_us }`。`frameworks/esp_idf/test/corpus/` 已有真实原厂源码语料，基础设施现成 |

---

## 五、专项建议：负例语料（对应 E-07）

当前 `acceptance` 只要求正向断言通过。但**一个永远返回成功的假空桩，可以 100% 通过所有正向断言**。

这正是 ADR-0012 在 API 层禁止的"静默降级"，在器件模型层的等价物——而 ADR-0012 的影响范围（:8）明确限定为 `pal/include/` 与 `targets/`，**没有覆盖模型层**。规范 `§二` 层级④ 要求的"外部硬件器件应答（EEPROM, Sensor, LCD）"目前处于契约真空。

**建议**：

```json
"acceptance": {
  "positive_cases": [ ... ],
  "negative_cases": [
    {
      "stimulus": "AT24C02 写入越界页 (0xFFFF)",
      "expect": "器件 NACK + 应用收到 WINK_ERR_TIMEOUT",
      "detects": "always-succeed stub"
    }
  ]
}
```

**配套门禁**：任何能力从 `status: stub` 升级到 `verified`，必须至少 1 条负例。

这是把"契约诚实"从口号变成可测不变量的最小代价，也是本评审认为投入产出比最高的一条新增要求。

---

## 六、建议修订包（按执行序）

**第一批 · 立即（发布前必做）**

1. 修正 C-01 全部 ID；删除/替换 2 个不存在的示例打样；`id` 改内容派生稳定标识。
2. 删除 `unisim_fidelity_profile`（C-02），`fidelity_contract` 收敛为单一保真声明并引用真实 A~F。
3. Schema 新增 `delivery` / `audit` / `spec_version`（C-03 / A-05 / E-08），`[x]` 改为渲染产物。
4. 更正 ADR-0012 引用（C-05），新增 `exclusion_evidence` 并接 `WINK_SLA_ERROR` 实际编译断言。
5. `tier` 移出 `required`；`architectural_placement` 改为从 Catalog 派生（C-06）。

**第二批 · 本迭代**

6. Catalog 抽成 `capability-catalog.yaml`（带 `layer/status/owned_paths/soc_support`）（C-04 / E-01）。
7. Gate 2 扫描面扩至 `targets/`，加 `pal_*` 语义词负向规则（A-01）。
8. Gate 3 改全量 6 pack + 补"条款 → rule id"映射表（A-02）。
9. `acceptance` 补 `matcher` 容差、`negative_cases`、双时钟超时、数组化 `failure_criteria`（E-02/04/05/07）。
10. `target_soc` 改 `soc_matrix`；`external_topology` 收敛为"需求声明"并补 `role`（A-03 / A-04）。

**第三批 · 需先立 ADR**

11. `dal_ws2812` / `pal_ws2812_write` 分层归属裁决（A-01），结论回写本规范打样 3 与 `§二` 层级②/④ 边界。
12. 183 条历史 Out-of-Scope 条目依 C-05 新证据字段逐条重裁。
13. 增补差分证据维度（E-10）。

---

## 七、复验方式

本评审全部结论可按下述方式独立复现（Windows PowerShell，仓根执行）：

```powershell
# C-01 打样 ID 与 SSOT 对账
Select-String -Path "wink-micro-app\vendor\esp_idfv61\CHECKLIST.md" `
  -Pattern "^\| \[.\] \| (004|028|029|063|064|171|176|197|426) \|"

# C-01 不存在的示例
Select-String -Path "wink-micro-app\vendor\esp_idfv61\CHECKLIST.md" `
  -Pattern "websocket/server|cmake/custom_component"

# A-01 WS2812 分层现状
Select-String -Path "wink-micro-os\dal\src\output\dal_ws2812.c" -Pattern "WS2812 order"
Select-String -Path "wink-micro-os\targets\wasm\pal_wasm_ch4_buffer.c" -Pattern "pal_ws2812_write|Axis A"

# C-05 Fail-Loud 机制现役证据
Select-String -Path "wink-micro-os\frameworks\esp_idf\include\wink_sla.h" -Pattern "WINK_SLA_ERROR"

# A-02 lint pack 全集
Select-String -Path "wink-tools\tools\lint\rules\*.yaml" -Pattern "^id:"

# C-02 UniSim 现行保真轴语义
Get-Content "docs\zh\design\04-wasm-simulation\03-axes\00-README.md" -Encoding UTF8 |
  Select-String -Pattern "Primary home" -Context 0,8
```

---

*本记录为只读评审快照。后续修订请另开评审记录或更新被评审文档的版本号；本文件不再编辑。*
