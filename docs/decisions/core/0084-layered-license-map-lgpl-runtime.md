# ADR-0084：分层许可地图——运行时 LGPL-3.0-only，生成物 Apache-2.0

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-15）** |
| 日期 | 2026-09-15 |
| 触发 | ADR-0083 全仓 GPLv3 后评估"用户固件静态链接是否被迫开源"；低代码平台需要"运行时受保护 + 用户应用可闭源"的许可形态 |
| 影响范围 | `wink-micro-os/**`（244 文件 SPDX → LGPL、38 文件补 Apache）、`wink-firmware-carriers/**`、`wink-micro-app/**`（18 文件 → Apache）、`wink-tools` codegen 模板与 golden、README/AGENTS/`coding-conventions §7`、新增 CI 许可门禁 |
| 决策者 | 项目架构团队 / Owner |
| **关联 ADR** | [ADR-0083](0083-adopt-gpl-3.0-only-license-policy.md)（本 ADR 修订其 D1/D2 的适用范围）、[ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（零侵入拦截层）、[ADR-0073](0073-cms8s-adc-real-register-map-supersedes-ssot.md) |
| **关联规范** | [coding-conventions.md](../../zh/design/07-platform-governance/coding-conventions.md) §7；[AGENTS.md](../../../AGENTS.md)「开源许可」；`wink-micro-os/NOTICE` |

---

## 1. 背景（Context）

1. **GPLv3 运行时对用户不友好**：MCU 固件以静态链接为主，GPLv3 使"用户应用 + 运行时"构成 Combined Work，用户对外分发固件即须整体 GPLv3 开源，与平台"AI 生成应用归用户、可商用闭源"的定位冲突。
2. **GPL 无头文件豁免**：LGPLv3 §3 允许 App 目标码任意许可（内联物 ≤10 行甚至免声明），GPLv3 无此条款；用户 app `#include` PAL/DAL/`REGX52.H`/`sdcc_gate` 头即构成源码级衍生。
3. **三个真实泄漏点**：`wink-micro-os/CMakeLists.txt:341/380/423/444` 将 `WINK_APP_SOURCES` 与 runtime / `wink_arduino_compat` / `wink_mcs51_core` 链接进同一 `wink_simulator` wasm；`frameworks/arduino` 同时是 ESP32 IDF 组件；`frameworks/mcs51/tools/sdcc_gate/*.h` 以 `-I` 注入真机 SDCC 构建；codegen 模板向自动生成文件注入 GPL SPDX。
4. **"GPL + 私下不告"策略否决**：版权人不予执行不构成授权（可撤销、项目易主即失效），且用户侧 SCA 扫描、客户合同、并购尽调不依赖执法意愿；另有第三方版权（Arduino/Unity/SDCC）与未来外部贡献者不可控。低代码平台的分享/导出/演示场景天然构成分发。
5. **架构隔离方案否决（暂缓）**：将用户代码编译为独立 wasm 模块由 GPL 宿主加载（QEMU 模型）可保留核心 GPL，但需要重切 PAL/DAL 静态链接边界，成本高且收益仅为许可名义差异。

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 全仓 GPLv3 | ADR-0083 现状 | 否决：用户应用被迫开源 |
| B. 分层地图 | 运行时 LGPL-3.0-only；生成物/示例 Apache-2.0；工具/测试/平台 GPL-3.0-only | **采纳** |
| C. 运行时 Apache-2.0 | ESP-IDF/Zephyr 模式，最大采用度 | 否决：失去运行时改动的 share-alike 保护 |
| D. GPL + 不告 / 双许可 | 维持 GPL 招牌、私下豁免 | 否决：非授权、不可继承、用户法务不认；双许可另需 CLA 与商业条款 |
| E. 模拟器 GPL + 用户模块隔离加载 | QEMU 模型 | 暂缓：架构改造成本高，后续如需可再评估 |

## 3. 决策结论（Decision）

- **D1 运行时 LGPL-3.0-only**：`wink-micro-os/**`（pal/dal/bal/osal/runtime/trace/targets/frameworks，244 文件）；含 `frameworks/mcs51/tools/sdcc_gate/**`（注入用户 SDCC 构建）。
- **D2 生成物 Apache-2.0**：`wink-micro-os/codegen/**`（driver/role YAML、模板，38 文件补标）、`wink-tools` codegen 模板与 golden、以及全部自动生成文件（`device_tree.*`、`wink_arduino_bindings.*` 等）；禁止向生成文件注入 GPL/LGPL SPDX。
- **D3 宿主工具与测试保留 GPL-3.0-only**：`frameworks/mcs51/tools/*.py`（transpiler/lint/manifest，其输出归用户）与 `wink-micro-os/**/test/**`（含 vendored Unity 保持 MIT）。
- **D4 载体 LGPL-3.0-only**：`wink-firmware-carriers/**`（用户固件入口）。
- **D5 示例 Apache-2.0**：`wink-micro-app/**` 18 文件（可复制入自有工程）。
- **D6 其余默认 GPL-3.0-only**：docs/根/`wink-tools` CLI/`wink-plugin-peripherals`/unisim/前端平台。
- **D7 第三方不变**：ArduinoCore-API = LGPL-2.1-or-later、Unity = MIT；`GPL-2.0-only` 组件永久禁止合入。
- **D8 CI 许可门禁**：`.github/license-map.json` 为单一事实来源，`.github/scripts/check_license_map.py` 校验（require / dir-license / text / skip 模式 + 全局 GPL-2.0 红线），`license-gate` workflow 于 push/PR 执行。
- **D9 "不告策略"存档**：作为已评估并否决的选项记录在案（见 §1.4），后续不再重复讨论。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 用户固件可闭源商用；LGPL 义务（显著声明 + 许可证文本 + 重链接能力 + 不改限制逆向）在嵌入式可履行；OTP/ROM 场景依 GPLv3 §6 豁免安装信息 | LGPL 静态链接需为用户下游提供重链接能力（relink kit / 对应目标码或构建物） |
| 运行时变更必须回馈（share-alike），核心资产保护仍在 | 许可地图跨目录，必须由 CI 门禁兜底，防止再次批量刷错 |
| 生成物 Apache-2.0 + 输出归用户声明，AI 生成应用许可叙事自洽 | 模板/golden/生成物需同步切换（本次已完成） |
| 工具/测试/平台保持 GPL-3.0-only，工具链自身仍受 copyleft 保护 | 目录级边界必须在文档与门禁中显式维护 |

## 5. 遵循与后续（Compliance & Follow-up）

- [x] `wink-micro-os/**` 244 文件 SPDX → `LGPL-3.0-only`；`sdcc_gate` 5 头同步；
- [x] `wink-micro-os/LICENSE`（LGPLv3）+ `COPYING`（GPLv3）双文本；`idf_component.yml` → `LGPL-3.0-only`；`NOTICE` 更新许可地图与第三方归属；
- [x] `wink-micro-os/codegen/**` 38 文件补 `Apache-2.0`；`wink-tools/tools/codegen/boards/` 增 Apache LICENSE；
- [x] `wink-firmware-carriers/**` → `LGPL-3.0-only`；`wink-micro-app/**` 18 文件 → `Apache-2.0`；
- [x] `wink-tools` codegen 模板与 3 组 golden → `Apache-2.0`（私有仓，另行提交）；
- [x] CI 许可门禁（`.github/license-map.json` + `scripts` + workflow）；
- [ ] 后续可选：接入 REUSE/SPDX SBOM 输出；贡献者 DCO/CLA 与双许可策略（如启用商业闭源分发）。

---

*该 ADR 状态变更记录：*
- 2026-09-15：Proposed & Accepted（分层许可地图落地，同步修订 ADR-0083 D1/D2 适用范围）。
