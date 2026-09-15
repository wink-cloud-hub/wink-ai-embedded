# ADR-0083：项目开源许可统一为 GPL-3.0-only

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-15）** |
| 日期 | 2026-09-15 |
| 触发 | 仓库新增 GPLv3 根许可证；第三方依赖（ArduinoCore-API / Unity / ESP-IDF / 前端与 Python 依赖 / 厂商 SDK）许可合规审计 |
| 影响范围 | 根 `LICENSE`、全仓自有代码 SPDX 头、`wink-micro-os/idf_component.yml`、`wink-micro-os/NOTICE`、`wink-plugin-peripherals/**/package.json`、`wink-micro-os/third_party/ArduinoCore-API/`、`wink-micro-os/test/unity/`；姐妹仓 `wink-tools`（codegen 模板 + winkcli 元数据）与 `@wink-ai/unisim` / `@wink-ai/unisim-ui` |
| 决策者 | 项目架构团队 / Owner |
| **关联规范** | [coding-conventions.md](../../zh/design/07-platform-governance/coding-conventions.md) §7；[AGENTS.md](../../../AGENTS.md)「License」小节 |
| **关联历史文档** | 归档计划 `docs/implementation-plans/core/00-master-i18n-strategy-and-phase1-plan.md`（Apache-2.0 Header 注入策略，已被本 ADR 取代）、`docs/implementation-plans/core/2026-09-08-mcs51-cms-refactor-and-tier3-evolution-plan.md`（ucsim GPLv2 禁令） |

---

## 1. 背景（Context）

1. **根许可证落地**：2026-09-15 仓库根目录新增 GNU GPL v3 全文（`LICENSE`），README 声明 GPLv3。
2. **自有代码 Header 与实际许可冲突**：i18n 改造计划（`PLAN-20260805-OPEN-SOURCE-I18N-P1`）曾向全部自有代码注入 `SPDX-License-Identifier: Apache-2.0`（共 482 处，覆盖 C/C++/Python/CMake/模板资产），`wink-micro-os/idf_component.yml` 亦声明 `license: "Apache-2.0"`。Apache-2.0 为宽松许可，逐文件声明会使接收方仍可按 Apache-2.0 使用，GPLv3 的 copyleft 目标形同虚设。
3. **第三方依赖审计结论**（对 GPLv3）：
   - `ArduinoCore-API` v1.5.2（vendored）为 **LGPL-2.1-or-later**：兼容 GPLv3（LGPL-2.1 §3 允许升级至更高版本 GPL）；静态链接在全源码分发下满足 §6 义务，但仓库内缺 LGPL-2.1 全文副本。
   - `Unity` 2.6.3（vendored）为 **MIT**：兼容；缺 MIT 全文副本。
   - ESP-IDF（Apache-2.0）、Emscripten（MIT/UIUC）、`@wokwi/elements`（MIT）、Vue/Vite/TypeScript/Playwright、Python 侧 Jinja2/PyYAML/pytest：均为宽松许可，兼容。
   - free-pdk `pdk-includes` / `easy-pdk-includes` / SDCC 为 **GPL-2.0-or-later**（本地 `docs/vendors/`，gitignore 不入库）：可升级随 GPLv3 分发；`pdk_button_led.hex` 构建产物合规。
   - 厂商 SDK（Cmsemicon / PDK / MiniC）为专有 EULA，永久排除在版本控制之外（E-003）。
4. **库元数据缺失**：`wink-plugin-peripherals` 及其 builtin 插件、姐妹仓 `@wink-ai/unisim` / `@wink-ai/unisim-ui` 的 `package.json` 均无 `license` 字段（默认“保留所有权利”），与对外分发不兼容。

## 2. 方案比选（Options）

| 方案 | 描述 | 结论 |
|---|---|---|
| A. 保持 Apache-2.0 | 将根许可证改为 Apache-2.0，与既有 Header 一致 | 否决：Owner 决策为 GPLv3 强 copyleft |
| B. 全仓 GPL-3.0-only | 自有代码 SPDX 统一为 `GPL-3.0-only`，第三方保留原许可并补齐许可证文本 | **采纳** |
| C. GPL-3.0-or-later | 允许接收方按更高版本 GPL 使用 | 否决：对齐根 `LICENSE` 的“GPLv3”字面语义，暂不预授未来版本 |

## 3. 决策结论（Decision）

- **D1 自有代码统一 `GPL-3.0-only`**：根 `LICENSE` 为 GPLv3 全文；自有 C/C++/Python/模板/资产文件的 SPDX 头统一为 `GPL-3.0-only`（含此前的 MIT 自有文件 `dal_load_cell.c/.h`）。
- **D2 元数据同步**：`wink-micro-os/idf_component.yml`、`wink-plugin-peripherals/**/package.json`（14 个）补 `license` 字段。
- **D3 第三方组件矩阵**：
  - ArduinoCore-API：保留 LGPL-2.1-or-later 与版权头，新增 `third_party/ArduinoCore-API/LICENSE`（LGPL-2.1 全文）；与 GPL-3.0-only 组合依据 LGPL-2.1 §3。
  - Unity：保留 MIT，新增 `test/unity/LICENSE`（MIT 全文）。
  - ESP-IDF / Emscripten / Wokwi / 前端与 Python 依赖：工具链或宽松许可，保留归属，无需再分发许可。
  - 厂商 SDK 与私有夹具：继续 gitignore（`docs/vendors/`、`.internals/`），永不入库。
- **D4 姊妹仓 wink-ai 库同步**：`wink-tools`（winkcli 元数据 + codegen SPDX 模板）与 `@wink-ai/unisim` / `@wink-ai/unisim-ui` 同步 `GPL-3.0-only`；由其仓库自行提交与验证。
- **D5 归档文档同步刷新**：`docs/reviews/`、`docs/implementation-plans/`、`docs/todolist/` 中的历史 `Apache-2.0` / 自有代码 `MIT` 字样（9 个文件、18 处）一并替换为 `GPL-3.0-only`，确保全仓无过期许可指引；仅保留对第三方组件（ESP-IDF = Apache-2.0、Unity = MIT）的客观描述与编码规范中的禁止条款。
- **D6 已知例外**：`wink-micro-app/vendor_cms8s78xx_v202/`（原厂示例）按 Owner 决定保留在仓库内；若对外商业分发，需另行取得厂商书面授权或移除（其 EULA 与 GPLv3 再分发存在张力）。
- **D7 兼容性红线**：**GPL-2.0-only** 组件（如 SDCC ucsim）与 GPL-3.0-only 不兼容，严禁裁剪进分发交付；引入新第三方依赖须先比对本矩阵并在 NOTICE 登记。

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| copyleft 语义闭环：逐文件头、根 LICENSE、包元数据三方一致 | 482 个文件 SPDX 头一次性切换，review 成本集中在本次提交 |
| 依赖许可风险清账并形成可复用矩阵（D3） | 商业闭源分发需 Owner 双许可/单独授权；未来接入 CI 许可门禁（`reuse` / `scancode`） |
| Arduino/Unity 许可证全文与 NOTICE 归属补齐，审计可举证 | 新增依赖须同步维护 NOTICE 与本文矩阵；GPL-2.0-only 组件永久禁止合入 |
| 厂商 SDK 边界（E-003）与构建产物合规路径明确 | 原厂示例入库系已知例外（D6），对外发布需复核 |

## 5. 遵循与后续（Compliance & Follow-up）

- [x] 根 `LICENSE`（GPLv3 全文）与 README 许可章节；
- [x] 482 处自有代码与归档文档 SPDX 头 → `GPL-3.0-only`（D5）；
- [x] `idf_component.yml`、14 个 `package.json` 元数据同步（D2）；
- [x] `third_party/ArduinoCore-API/LICENSE`（LGPL-2.1）与 `test/unity/LICENSE`（MIT）；`wink-micro-os/NOTICE` 重写为第三方归属；
- [x] 姊妹仓 `wink-tools` codegen 模板 SPDX 与库元数据同步（D4，待其仓库提交）；
- [ ] CI 许可门禁（`reuse lint` 或 scancode）接入，防止 vendor 源码与 Apache-2.0 头回流；
- [ ] 贡献者流程（DCO/CLA）与双许可策略（如需闭源分发）由 Owner 另行立项。

---

*该 ADR 状态变更记录：*
- 2026-09-15：Proposed & Accepted（许可合规审计后随代码变更同步落地）。
- 2026-09-15：D5 修订——归档文档（含 `docs/reviews/`）一并刷新，消除过期许可指引（Owner 决定）。
- 2026-09-15：D1/D2 适用范围由 [ADR-0084](0084-layered-license-map-lgpl-runtime.md) 修订（运行时 LGPL-3.0-only、生成物 Apache-2.0、MCS-51 工具与测试保留 GPL-3.0-only），本 ADR 其余决策继续有效。
