# Stage7：测试与契约收尾（关闭项）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S7-CLOSE` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-12；自审签署见 §7） |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-22（测试分层）、CPL-23（ABI 定稿） |
| **前置依赖** | stage1（双读/双轨已开）、stage6（目标已拆） |
| **总纲** | [`./00-README.md`](./00-README.md) |

> [!NOTE]
> **SUPERSEDED by ADR-0073 / ADR-0077**：附录 B/C 提及的历史 `wink-app.json schemaVersion: 2` 与 `SimTraceSpecV2` 遗留称呼已被 Day-0 V1.0.0 收敛方案取代，统一规范为 `wink-app-config@1.0.0` 与 `SimTraceSpec`（`traceVersion: 1`）。

## 1. 目标

- ✅ 删除合成通道双读兼容层、转发 shim、旧单体别名，无告警残留。
- ✅ `SimTraceSpecV2` 与前端视窗 DTO 定稿，ABI 版本记录归档（含发版顺序与回滚步骤）。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/src/cms8s_adc.cpp` 等兼容层 | 🗑️ | 删双读重定向；误用改 STRICT abort / Release `0x0FFF` 哨兵 + 计数 |
| `include/` 转发 shim（6 个） | 🗑️ | 删 stage3 遗留 shim（`REG_CMS8S78XX.H`/`cms8s78xx.h`/`cms8s_adc.h`/`cms8s_buzzer.h`/`mcs51_xsfr_allowlist.h`/`ADC0832.H`） |
| `CMakeLists.txt` 别名 | 🗑️ | 删旧单体别名 `wink_mcs51_compat(_strict)`；消费方改具名四目标 |
| `src/mcs51_bridge.cpp` + 芯片 register TU | ✏️ | S4-D5 过渡默认删除；芯片注册改**链接期自注册** |
| `runtime/include/wink_mcu.h` | ✏️ | 门面瘦身：51 路由下沉 `mcs51_family_route.h`（修复 MCS51-ISOLATION 存量发现） |
| 契约文档 + 场景夹具 | ✏️ | 版本号定稿归档；片上模拟场景迁物理 Pin key（AN0→0） |

## 3. 任务拆分

### Task S7-1：删兼容层 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 1**：删片上合成通道重定向（板级 32+ch key 不受影响），合成通道访问改为硬 fail / 哨兵 + 计数（裁决 S7-D1：STRICT abort + Release `CMS8S_ADC_SYNTH_REJECT_SENTINEL` + `cms8s_adc_synth_misuse_count`）。
  - 执行注记：删除 `cms8s_adc_dual_read_synth` 开关与重定向落值；检测规则保留（物理 pin 真 0V 且合成 key 有值才算误用；真 0V 短路照实上报；注入轨直通）。`test_cms8s_adc_instant.cpp` §14 四组改为哨兵/计数/真 0V/注入直通断言。
- [x] **Step 2**：删转发 shim 与别名目标；全仓 grep 零 `TODO(stage7)`/`#warning` 残留。
  - 执行注记：6 个 shim 文件删除；测试 include 面补 `chips/cms8s78xx/include`；`wink_mcs51_compat(_strict)` 删除（`test/CMakeLists.txt` 消费方改 `core+cms8s+at89+adc0832` 与 STRICT 孪生）；`mcs51_bridge.cpp` 的 `mcs51_family_select.h` 缝、fixture 目录与 `wink_micro_app` 侧无 ring；grep 零 `TODO(stage7)`、零 mcs51 `#warning`、零旧单体目标定义。
- [x] **Step 3**：`core-tests`/`cms8s-tests` 各自全绿后合入主线。
  - 执行注记：host mcs51 ctest **59/59**、wasm/Node **10/10**、本地与跨仓 lint 无发现；跨仓 headless 五载体 **5/5**、`mcs51_health_pot` **15/15**（详见 §4）。

### Task S7-2：契约定稿 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 1**：归档 ABI 版本（语义/版本号/发版顺序/回滚命令）——见附录 B。
- [x] **Step 2**：`SimTraceSpecV2` + DTO 一致性复核签字——见附录 C。

## 4. 验收（系列关闭门）

- ✅ **L0-L2：双轨全绿，E-02 无复发，绝缘单测全绿**
  - host（MinGW/GCC）mcs51 ctest **59/59**（含 `test_mcs51_cleanup_unit`、`test_mcs51_layering_gate`、10 项 host 树内 wasm 轨）；wasm/Node **10/10**。
  - 既有无关失败说明：整树 host 构建另有若干非 mcs51 目标因 `-Werror`（pal/dal 弃用告警等）编译失败，属既有噪声，与本系列零 diff（stage6 已同现象）。
  - **E-02 无复发终证（跨仓真链路，手动）**：`powershell -File wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1` → 五载体（UART TX / UART RX live / 模拟 ADC / INT0 / 轮询按键）**全 PASS**；`-App mcs51_health_pot` → 15 场景 **全 PASS**（片内 NTC 经 AN0→物理 Pin 0，冷启动室温、干烧/超温/开短路保护全绿）。
  - 跨仓 headless 在本阶段首次暴露并修复：`mcs51_analog_threshold` 从未启用 A-02 ADC LDO/Vref/模拟复用与 A-05 输出方向（TRIS），片上 ADC 链路在 stage3 保真门禁后实际已不可用；补齐 vendor 标准初始化后 8/8 复绿（详见附录 D S7-D2）。
- ✅ **L3：契约文档与实现一致；总纲状态置 ✅ 已完成**
  - `07-mcs51-simulation-interception.md`：v2 双空间 pin 语义、链接期自注册、Stage7 证据回写。
  - `03-ai-dsl-and-codegen-pipeline.md`：`mcs51_family_select.h` 段落改自注册定稿。
  - `03-directory-architecture.md`：别名注记改终态；`runtime/include/wink_mcu.h` 门面瘦身。
  - `00-README.md`：系列状态与阶段表置 ✅ 已完成。
- ✅ **L4：零兼容残留 grep 通过**
  - code/config 侧零 `TODO(stage7)`、零 mcs51 `#warning`、零双读符号（`dual_read_synth`/`synth_redirect`）、零 `mcs51_family_select`、零 shim 路径引用、零旧单体目标定义。
  - 仅存解释性文字提及（CMake 注释说明"旧别名已删除"），非遗留定义。

## 5. 风险与回滚

- R：删兼容层后旧前端版本断裂 → 缓解：发版顺序 + 版本号卡死，不兼容旧大版本属预期（见附录 B）。
- 回滚：stage7 变更按逻辑模块独立提交（① 自注册 `234a7c5` ② 双读→哨兵 `f264154` ③ shim+别名+门面 `ec7896c` ④ 场景+ demo `a0c2d9f` ⑤ app 注释 `05b84e2` ⑥ 本收尾提交 + 资产 chore），`git revert <hash>` 可独立回退任一模块；恢复双读只需回退 `f264154`（哨兵逻辑回退后 `cms8s_adc_dual_read_synth` 语义恢复）。
- 受控遗留（不阻塞关闭）：`vendor/cms8s78xx/adc_ldo`、`adc_hardware_trigger` 的 EOC 波形断言不绿，根因为 A-05 方向模型在 TRIS 输入→输出切换时不重驱锁存（既有模型缺口，`0f3e426` 起，非本系列回归）；两个 vendor 场景已按 v2 key 迁移（`adcChannel:0`），恢复绿需独立模型增强 + 场景重跑，另立任务跟踪（附录 E）。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）

- [x] **目录落位**：全仓物理目录结构 100% 匹配 `00-README.md §3.2` 终态目录树；`include/` 零 shim、`test/fixtures/` 移除、chip/device 头归位 `chips/*/include`、`devices/adc0832/include`。
- [x] **零残留门禁**：全仓 `grep` 零 `#warning` 转发 shim、零旧 target 别名定义、零片上合成通道误用（板级 32+ch key 合法保留；`TODO(stage7)` 同查）。
- [x] **契约归档**：ABI 文档（附录 B）与 `SimTraceSpecV2`/DTO 复核（附录 C）签字，版本号定稿。
- [x] **双轨与 CI**：host 与 wasm 双平台全绿，基线脚本 `--compare` 集合相等（计数以基线文件为准）；总纲置 ✅ 已完成。

## 7. 执行签名（2026-09-12）

- **Check 1 目录/文件**：通过。删除 6 shim + 2 fixture；`runtime/include/wink_mcu.h` 门面瘦身；chips 头公共 include 面上浮（`wink_mcs51_cms8s` PUBLIC）。
- **Check 2 依赖单向/残留**：通过。`python wink-micro-os/frameworks/mcs51/tools/lint/lint_mcs51_layering.py` → `LAYER-GATE PASS`；bridge stage7 waiver 行随代码剪除；跨仓 `python wink.py lint --pack layering --pack api` → **No lint findings**（stage6 移交的 `runtime/include/wink_mcu.h:29` MCS51-ISOLATION 存量清零）。
- **Check 3 双轨全绿**：通过。host mcs51 59/59、wasm 10/10、跨仓 headless 5/5 + health_pot 15/15；生产 wasm 链接期自注册实测生效（芯片模型参与转换/中断）。
- **Check 4 计划闭环**：通过。Task/Step 全勾选；总纲状态更新为 stage7 ✅；系列关闭。
- **复审待办**：无。变更集已按逻辑模块提交：`234a7c5`（自注册）、`f264154`（哨兵）、`ec7896c`（shim/别名/门面）、`a0c2d9f`（场景/demo）、`05b84e2`（app 注释）、本收尾提交 + `chore` 资产刷新；回滚命令见 §5 与附录 B。

## 附录 A：核心编码变更清单

1. **链接期芯片自注册（S7-D3 定稿）**：`chips/*/src/*_register.cpp` 增加静态初始化器（`s_<family>_register_at_link`）→ register OBJECT 被链接即注册；`mcs51_bridge.cpp` 删除 `__has_include("mcs51_family_select.h")` 缝、`MCS51_FAMILY_SELECT_REGISTER()` 调用与 `WINK_MCU_CMS8S78XX` 过渡默认，桥保持家族无关。测试 fixture（`test/fixtures/family_select/`）与 wasm 每样本拷贝逻辑删除；host/wasm 测试改为与生产同构（仅链 register OBJECT）。
2. **双读删除**：`cms8s_adc.cpp` 检测保留、落值改为哨兵；`cms8s_adc.h` 增 `CMS8S_ADC_SYNTH_REJECT_SENTINEL`、改名计数 `cms8s_adc_synth_misuse_count`；开关 `cms8s_adc_dual_read_synth` 删除。
3. **shim/别名删除**：6 shim 删除；框架 CMake 删 `wink_mcs51_compat(_strict)`；`test/CMakeLists.txt` 五处 STRICT 与 helper 改具名目标并补 chip include；`mcs51_test_harness.h` 注释同步。
4. **门面瘦身**：`runtime/include/wink_mcu.h` 51 分支改引 `mcs51_family_route.h`（可移植门面不再包含 Keil 寄存器头）。

## 附录 B：ABI 版本归档（S7-2 Step 1）

- **ABI 符号面**：`wasm_bridge.h` / `PAL_WASM_ABI_HASH` 全系列**零符号增删改名**（`js_pal_adc_read_norm(uint16_t pin)` 签名不变），故 hash 保持 `0x20149EFCu`（哈希规则只覆盖符号签名行）。变的只是 `pin` 参数的**值域语义**：
  - `v2`（冻结，本系列）：`0~31` = MCU fabric 物理 Pin（CMS8S AN0→Pin 0 经 `AN_TO_PIN`）；`32~63` = Board fabric 通道（device-tree/前端拥有，`devices/` 消费，ADC0832/健康壶板级键永久合法）。
  - `v1`（已废弃）：片上路径以 `32+ch` 借用合成区；双读兼容已删除，误用返回哨兵并在 STRICT 中止。
- **`SimTraceSpecV2` 版本**：`traceVersion: 1` 主版本不动；场景 `INPUT_ANALOG.adcChannel` 字段结构不变，数字域按 v2 双空间解释；`wink-app.json` `schemaVersion: 2` 不动。
- **发版顺序（锁定）**：仿真后端（本仓）→ 前端插件/DTO → stub/文档。本仓已按 v2 冻结；跨仓前端与 unisim 的旧 `adcChannel:32` 误用会得到哨兵（Release）或 STRICT 中止，不再静默假短路。
- **回滚命令**：
  - 恢复双读兼容：`git revert f264154`（恢复 `cms8s_adc_dual_read_synth` 与重定向逻辑）；
  - 恢复 shim/别名：`git revert ec7896c`（恢复旧 include 面与 `wink_mcs51_compat(_strict)`，供 V1 前端链接）；
  - 恢复 family_select 缝：`git revert 234a7c5`（恢复 S4-D5 过渡默认；不推荐，属已定稿退役路径）。
- **归档位置**：本附录为 ABI 版本记录正文；stage1 附录 B 为 v1→v2 原始升版记录（已标注 2026-09-12 关闭）。

## 附录 C：`SimTraceSpecV2` + 视窗 DTO 一致性复核（S7-2 Step 2）

- **C-ABI × Spec**：`wasm_bridge.h` 无符号变化；`js_pal_adc_read_norm` 的 pin 域在 `04-wasm-simulation`（通道-3 数据路径）与 `02-wink-micro-os/07` §2.1/§3.2 中已统一为 v2 双空间；`07-platform-governance/04-simulation-consistency.md` 的 trace 事件模型（`traceVersion:1`、语义事件）不受影响。
- **Scenario/DTO**：`INPUT_ANALOG.adcChannel` 为数值 rail key（unisim `AdcDomainHandler.resolveMcuPin` 逐字即 mcuPin）；本仓全部片上场景冻结为 `0`（AN0）。前端视窗 DTO 字段集不变（无 schema 变更），仅值域注记。
- **一致性断言**：18 个片上场景文件（analog_threshold 1 + health_pot 15 + vendor adc_ldo/adc_hardware_trigger 2）+ `analog_threshold.c` 注释 + health_pot `DESIGN.md` 已全部对齐 v2；`grep adcChannel.*32` 在本仓场景目录零命中。
- **签字结论**：✅ 一致，无需版本迁移；DTO/Schema 无破坏性变更，Postel 兼容（旧 32+ch 误用显式哨兵）。

## 附录 D：执行裁决（S7-D1~D3）

- **S7-D1（合成通道误用策略）**：STRICT abort + Release 哨兵 `0x0FFF`（满量程，对加热类失效安全）+ 计数 + 一次性告警；不做透明重定向。检测启发式保留（真 0V 短路原样上报；注入轨直通）。
- **S7-D2（场景迁移与 demo 修复）**：本仓片上场景 32→0（18 文件）；`mcs51_analog_threshold` 补 vendor 标准初始化（`ADC_EnableLDO` / `ADC_ConfigADCVref` / `GPIO_SET_MUX_MODE(P00CFG,AN0)` / `GPIO_ENABLE_OUTPUT(P1TRIS,0)`）——A-02/A-05 门禁是硅片语义，demo 缺参即应失败；修复后 headless 8/8。
- **S7-D3（家族选择定稿）**：用户授权两仓均可改，以长期可维护/简洁为准。裁决**链接期自注册**取代 `mcs51_family_select.h` codegen 缝：链接图本身即家族选择（manifest → `wink_mcs51_inject_<family>` + register OBJECT），消除外仓生成器前置依赖、双源事实与"生成缺失即静默无模型"失败模式；新家族仅需 chips 目录 + manifest（CMake 自动发现）。

## 附录 E：受控遗留项（非本系列回归）

- **现象**：`vendor/cms8s78xx/adc_ldo`、`adc_hardware_trigger` headless 的 `ASSERT_WAVEFORM`（EOC ISR 翻转 P3.2 频率）为 0。
- **根因（已定位到行）**：A-05 方向模型下，`GPIO_ENABLE_OUTPUT(P3TRIS,2)` 之前的 `P32=0` 写因输入方向被抑制（锁存=0，arbiter 仅存 bridge 上电种子 WEAK-HIGH）；随后 ISR `P32=~P32` 的 Read-Pin 回读命中 MCU 自身弱高驱动 → `~1=0` 与锁存同值 → 无 diff 边沿（`mcs51_gpio_bit_write` 短路）。硅片语义应在 TRIS 输入→输出切换时按锁存重驱（当前模型缺口）。
- **定性**：A-05 属 stage3（`0f3e426`，2026-09-11）引入的既有缺口；与 stage7 的注册方式/双读删除/场景迁移无关（场景迁移为契约必须，保留）。
- **处置**：另立模型增强任务（TRIS 方向切换重驱 + 自驱回读消歧），完成前两个 vendor EOC 场景不列入绿集；其余 vendor 场景按需重建资产。
- **✅ 关闭记录（2026-09-12，PLAN-20260912-MCS51-P3-TRIS）**：已修复（P3-A）。`PxTRIS` 是 SFR（0x9A/0xA1-A3），复用 SFR 代理写钩子实现方向切换语义：0→1 且非 AN 按锁存重驱（0 → SUPPLY / 1 → OD 则释放否则 WEAK）、1→0 `js_pal_gpio_release_mcu` 释放、OD latch=1 使能走释放、reset 释放全 32 脚。`vendor/cms8s78xx/adc_ldo`、`adc_hardware_trigger` headless `ASSERT_WAVEFORM` 转 **PASS**；五载体 5/5、`mcs51_health_pot` 15/15 无回归；host `test_mcs51_gpio_dir` T2/T8-T11 + wasm/Node 4/4。资产已随修复机械刷新（两 app `unisim-assets` 的 device-tree/js/wasm 更新）。自驱回读消歧降级为条件项（P3-B），以 P3-A 后不再有真实失真为由暂不排期。
- **资产刷新范围**：本阶段重建了五载体 + `mcs51_health_pot` + `mcs51_analog_threshold` 的 `unisim-assets`；vendor 演示资产（含 adc_ldo/adc_hardware_trigger）保持陈旧，属机械刷新，随模型增强任务一并处理（已在 P3-A 关闭记录中随修复刷新）。
