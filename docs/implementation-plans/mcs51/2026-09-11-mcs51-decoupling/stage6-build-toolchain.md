# Stage6：构建系统与工具链配置化治理

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S6-BUILD` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | ✅ 已完成（2026-09-12；本阶段签署见 §7，变更集待提交） |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-15（CMake 单体/宏污染）、CPL-16（工具链硬编码）、CPL-24（STRICT/XDATA knob） |
| **前置依赖** | stage3（头文件已就位，可分目标）+ 外仓 wink-tools codegen 具备 `mcs51_family_select.h` 生成能力（generator 在外仓，本仓仅 boards 数据；`WINK_TOOLS_ROOT` 指向外仓，见 `cmake/wink_tools.cmake`）。若外仓未就绪：测试用检入式 fixture 头顶替验证，生产注入链路标 deferred，不阻塞拆目标本身 |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `wink_mcs51_compat` 拆 `wink_mcs51_core` + `wink_mcs51_cms8s`（+ `_at89`、`_adc0832`），旧单体名保留别名一版；STRICT 走 per-target 定义，不搞 2×2 四库。
- ✅ `WINK_MCS51_XDATA_SIZE` 下沉板级；`mcs51_board_config.h` 限 app 作用域。
- ✅ gate/cleanup/lint 改读 `tools/manifests/chips/*.yaml`，零硬编码正则/内存上限。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `frameworks/mcs51/CMakeLists.txt` | ✏️ | 分目标 + 别名 + per-target STRICT（`_at89` 编 STATIC + register 空实现，四目标对称）；新增 `mcs51_sources.cmake` 共享源列表 + 新芯片包自动发现 |
| `tools/manifests/chips/` | 🆕 | `at89c52.yaml`、`cms8s78xx.yaml`、`schema.json`（key = 文件名 stem = chips 目录名，见总纲 §3.1c）+ `tools/mcs51_manifest.py` 载入/校验器 |
| `tools/mcs51_sdcc_gate.py`、`mcs51_cleanup.py`、`tools/lint/` | ✏️ | manifest 驱动（`BUDGETS`/`FAMILY_BY_MCU` 迁移进 manifest；cleanup 头名 alternation；layering/safety/sim_compat 残留与头事实全部来自 manifest） |
| board_config 生成链 + `test/CMakeLists.txt` 中央注册 | ✏️ | `mcs51_family_select.h` 注入缝（见 S6-1 Step 2）；board_config 缝与 facade/shim 段路径同步；design 02/03 文档同步 |
| `test/mcs51/wasm/` 构建 | ✏️ | 手写源列表改读 `mcs51_sources.cmake`（彻底删除，stage1 Step 0a 的临时补齐作废）；每测试按家族拷贝 fixture 家族选择头 |
| `wink-micro-app/mcs51_health_pot/CMakeLists.txt` | ✅ | 验证零改（`git status -- wink-micro-app` 零 diff） |

## 3. 任务拆分

### Task S6-1：CMake 分目标与物理源文件归位 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 1（物理迁移）**：`git mv src/cms8s_adc.cpp src/cms8s_buzzer.cpp src/cms8s_sys.cpp chips/cms8s78xx/src/`。`chips/cms8s78xx/src/` 现含全部 8 个专有 TU（adc/buzzer/gpio/uart/timer/extint/sys + register）；`src/` 仅 18 个通用 `mcs51_*.cpp`，零专有文件。
- [x] **Step 2（CMake 拆目标 + 零改注入）**：拆出 `wink_mcs51_core`、`wink_mcs51_cms8s`、`wink_mcs51_at89`、`wink_mcs51_adc0832` 四目标（统一 `STATIC EXCLUDE_FROM_ALL`，ESP 平台守卫保留；`at89` 仅一行空 register 也编静态库，四目标属性对称）。生产注入按 `wink-app.json mcu` 选择 `core + wink_mcs51_${MCU}`（cms8s 另链 `wink_mcs51_cms8s_register` 对象与按需 `adc0832`），`wink-micro-app` 用户代码零改；旧单体名 `wink_mcs51_compat` / `wink_mcs51_compat_strict` 保留 INTERFACE 别名一版（R-04 回滚）。`mcs51_family_select.h` 生成缝按外仓生成器 `EXISTS` 门控（未就绪 → 检入式 fixture 验证 `__has_include` 分支，生产走 deferred 过渡默认，见 S6-D3）。
- [x] **Step 3（编译开关与下沉）**：STRICT 改为 per-target 编译定义（删除框架级 `WINK_MCS51_STRICT` cache knob；`_core_strict`/`_cms8s_strict` 孪生 + 别名保持旧链接名）；`WINK_MCS51_XDATA_SIZE` 从框架 CMake cache 删除，孔径归板级作用域（`absacc.h` 保留 8192 默认，板/app 按需下发编译定义）；board_config 移出静态库 include 路径（仅绑定方 TU 可见）。design 02/03 同步更新。
- [x] **Step 4（wasm 改链库）**：`add_wink_wasm_mcs51_test.cmake` 手写源列表删除，改读 `mcs51_sources.cmake` 的 `MCS51_CORE_WASM_SOURCES`/`_CMS8S_/_AT89_/_ADC0832_` 与 include 面；每样本生成独立家族选择 fixture 目录（cms8s/classic 并行构建无竞态）。
- [x] **Step 5**：新芯片演练：临时只加 `chips/newchip/src/newchip_register.cpp` 空壳 + `tools/manifests/chips/newchip.yaml`，configure 自动发现并成功构建 `wink_mcs51_newchip`，cleanup 同时学会新头名（`newchip.h → <wink_mcu.h>`）；演练后删除。证据见 §7 附录 A。

### Task S6-2：工具链 manifest 化 `[状态: ✅ 已完成（2026-09-12）]`

- [x] **Step 0**：manifest schema 冻结（`tools/manifests/chips/schema.json`），五组字段（`family`/`aliases`/`headers`/`memory`/`sdcc_gate`），多一组即校验失败：
  ```yaml
  family: cms8s78xx            # key = 文件名 stem = chips 目录名（总纲 §3.1c）
  aliases: []                  # 构建选择侧接受的兼容别名（at89c52.yaml 收编 stc89c52）
  headers:                     # 清洗 pass 与 lint 的头文件事实源
    vendor_include_dir: chips/cms8s78xx/include
    allow_regex: ["REG_CMS8S78XX\\.H", "cms8s_.*\\.h"]
    forbid_in_core_regex: ["(?i)cms8s", "0xF0[0-9A-Fa-f]{2}", "ADCLDO", "FUNCCR", "\\bPS_[A-Z0-9]+\\b"]
    cleanup_alias_regex: ["cms8s", "cms8s\\d[a-z0-9]*", "reg_cms\\d*[a-z0-9]*"]
  memory:                      # SDCC/描述符事实源
    xram_bytes: 1024
    xsfr_window: [0xF000, 0x1000]   # base + size，无窗口填 null
  sdcc_gate:                   # 编译门禁事实源
    mem_limits: { code_max: 16384, iram_max: 256, xdata_max: 1024 }
    vendor_device_header: docs/vendors/.../cms8s78xx.h   # null = 无
    vendor_stddriver_dir: docs/vendors/.../StdDriver      # null = 无
    stddriver_link: ["cms8s78xx_stdriver"]
  ```
  载入器 `tools/mcs51_manifest.py`：PyYAML 优先 + 内置受限 YAML 子集解析兜底；结构/正则/类型/别名冲突校验 fail-fast；`family == 文件名 stem` 强校验。
- [x] **Step 1**：gate/cleanup/lint 只读 manifest。迁移前后 verdict 一致性验证（实跑，2026-09-12）：
  - `python wink-micro-os/frameworks/mcs51/tools/mcs51_sdcc_gate.py wink-micro-app/mcs51_health_pot` → 新旧均 `[PASS] CODE=10923B (limit 16384), stack free=184B`（cms8s78xx）；
  - `... wink-micro-app/mcs51_button_led` → 新旧均 `[PASS] CODE=287B (limit 8192), stack free=247B`（at89c52）；
  - 未知 mcu fail-fast 实测：`[FAIL] ... unknown mcu 'unknownchip'`，退出码 1（不再静默回落 at89c52）。
- [x] **Step 2**：删除 Python 内 `BUDGETS`/`FAMILY_BY_MCU` 硬编码表、cleanup 的 `cms8s|stc` 硬编码头正则、layering/safety/sim_compat 的 `REG_CMS8S78XX.H` 强绑定（全部改由 manifest 派生；缺 manifest 时 layering/cleanup 直接失败）。`tools/sdcc_gate/<family>` 选择由 manifest `family` 键驱动。

## 4. 验收

- ✅ L0：三 target（host/wasm/app 注入）全绿：
  - host：mcs51 ctest **68/68**（含 10 项 wasm 轨在 host 树内构建执行、新增 `test_mcs51_cleanup_unit`）；四拆分目标 + 两个 STRICT 孪生独立编译成功；
  - wasm：`add_wink_wasm_mcs51_test.cmake` 10/10（每家族 fixture 注入生效）；
  - app 注入：`mcs51_health_pot`（cms8s）与经典系 app 生产 wasm 构建成功，实跑 **15/15** 个可配置载体成功（5 官方 + health_pot + 9 vendor 样例，含修复后的 `uart0_printf`/`uart0_rxtx`，见 S6-H7）；其余构建目录要么无对应 app（历史重命名：`mcs51_seg_display`、`timer2_timming_mode`），要么从未配置过，与本阶段无关。
- ✅ L4：通用核心 app 不链接 cms8s 目标——`mcs51_button_led` 生产 linkLibs 仅 `libwink_mcs51_core.a + libwink_mcs51_at89.a`；`mcs51_health_pot` 为 `core + cms8s (+ register 对象)`。全仓 `grep -rn 'MCS51_HAS_ADC0832'` 仅 `devices/`-侧测试 harness、app 注释与文档命中；`src/` 下 `grep -Ei 'cms8s|0xF0'` 剩余两处（`mcs51_family.cpp` 描述符 by-design、`mcs51_bridge.cpp` S4-D5 deferred 过渡默认）均带 lint waiver 且有移除阶段标记。

## 5. 风险与回滚

- R：app 链接断裂 → 缓解：旧目标别名保留一版（本阶段实证四目标 + 别名双通）；`wink_micro_app` 零 diff。
- R（实施新增）→ 缓解：core（桥调芯片 register）↔ chip（模型调 core API）静态库对象级环 → 芯片 register TU 编为 OBJECT 库由最终可执行直接链接（对象先于归档入链，CMake 3.15/MSVC 通吃，不引入 LINK_GROUP/whole-archive）；`wink_mcs51_core` 显式 PUBLIC 链接 `wink_runtime` 修正静态扫描序。
- 回滚：先 revert 工具链 commit，再 revert CMake commit（顺序写死）；别名保留使旧链接名可立即回退。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）

- [x] **目录落位**：`chips/cms8s78xx/src/` 含全部 7 个专有实现文件（adc/buzzer/gpio/uart/timer/extint/sys）+ `cms8s_register.cpp`；`src/` 彻底零专有 `.cpp`（仅 18 个通用文件）。新增支撑文件均落于规划面内：`mcs51_sources.cmake`（框架根构建辅助，与 CMakeLists 同处）、`tools/manifests/chips/`、`tools/mcs51_manifest.py`、`test/fixtures/family_select/`。
- [x] **用户零改**：`wink-micro-app` 全仓用户代码零 diff（`git status -- wink-micro-app` 空）；旧单体别名可用（`wink_mcs51_compat`/`_strict` 双通）。
- [x] **Manifest 闭环**：`tools/manifests/chips/` 的 yaml 为唯一事实源（gate/cleanup/layering/safety/sim_compat 零硬编码家族事实；缺 manifest fail-fast）；`tools/sdcc_gate/<family>` 选择由 manifest 驱动；schema.json 与载入器校验对齐。
- [x] **双轨状态**：host 与 wasm 下 4 个拆分 target 均独立编译成功，测试全绿；手写 wasm 源列表已删除（改读共享 `mcs51_sources.cmake`）。
- [x] **门禁进 CI 评估**（逐项结论，不留灰色地带）：
  - `sdcc_gate`：**书面豁免**（需真实 SDCC + 未入库的 `docs/vendors` 树，属 Tier-S 手动门；本阶段完成新旧 verdict 一致性实证）。
  - `shim_audit` freshness：**已接线**（`test_mcs51_xsfr_allowlist_fresh` 在 host ctest 内，随 CI ctest 跑）。
  - `cleanup pytest`：**已接线**（`test_mcs51_cleanup_unit` 进 host ctest；hermetic，stdlib + 检入 manifest；复审后补 2 条 manifest 驱动用例，S6-H4）。
  - `layering gate`：**已接线**（复审 S6-H5：`test_mcs51_layering_gate` 进 host ctest）。
  - `headless 取证`：**书面豁免**（Windows-only + 需外仓 wink.py；保持手动聚合脚本）。

## 7. 执行签名（2026-09-12）

- **Check 1 目录/文件**：通过。物理迁移 3 文件 + 新 8 文件全部落位（§6 第 1 项）；`src/` 零专有 `.cpp`。
- **Check 2 依赖单向/残留**：通过。`python wink-micro-os/frameworks/mcs51/tools/lint/lint_mcs51_layering.py` → `LAYER-GATE PASS`；residue 模式改由 manifest 驱动后 PASS 反证迁移无损；bridge S4-D5 过渡默认重新标记 `stage7`（外仓生成器未就绪，deferred）。外仓 `wink lint --pack layering --pack api` 实跑在 HEAD 与工作区**同报** 1 条既存发现（`runtime/include/wink_mcu.h:29` MCS51-ISOLATION，stage3 e4b9041 引入、本阶段零 diff；属 facade 瘦身遗留，移交 stage7）。
- **Check 3 双轨全绿**：通过。host mcs51 ctest **69/69**（含 `test_mcs51_cleanup_unit`、`test_mcs51_layering_gate`、10 wasm）；四目标 + 两 STRICT 孪生独立构建；生产 cms8s/经典双系 app 构建；L4 链接证据（经典不链 cms8s）；SDCC verdict 双家族新旧一致；`sizeof(Mcu51Context)`/功能行为零变。
- **Check 4 计划闭环**：通过。Task/Step 全勾选；总纲 [00-README.md §5](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/mcs51/2026-09-11-mcs51-decoupling/00-README.md) 状态更新为 stage6 ✅。
- **复审闭环（2026-09-12，S6-H1~H7）**：人类全量代码评审查出 6 组问题，全部修复并复验，见附录 B；修复后 `test_mcs51_cleanup_unit` 10/10、mcs51 ctest 69/69、新芯片生产注入演练端到端通过、15/15 可配置 app 载体构建通过。

## 附录 A：Stage6 执行记录与裁决（S6-D1~D9）

- **S6-D1（STRICT 形态）**：删除框架级 `WINK_MCS51_STRICT` cache knob；仅 core/cms8s 有 STRICT 代码，故只建 `wink_mcs51_core_strict`/`_cms8s_strict` 孪生（per-target 编译定义），`wink_mcs51_compat_strict` 作 INTERFACE 别名保留——不搞 2×N 矩阵，严格测试 TU 不变。
- **S6-D2（register 对象链接）**：core 桥调用家族 register、register/模型回调 core，构成对象级环。裁决：`cms8s_register.cpp` 编为 `wink_mcs51_cms8s_register` OBJECT 库，由测试/app 最终链接（对象先于归档；CMake 3.15 与 MSVC 均无需 `LINK_GROUP`/whole-archive；`-u` force 因 MinGW 符号下划线不可移植被否决）。
- **S6-D3（家族选择注入）**：`mcs51_bridge.cpp` 的 `__has_include("mcs51_family_select.h")` 缝保持不变；外仓生成器未就绪 → host 测试挂检入 fixture（`test/fixtures/family_select/cms8s78xx/`）、wasm 每样本拷贝对应 fixture，实测桥对象引用 `cms8s78xx_register` 走注入分支；生产 S4-D5 过渡默认保留并延期至 stage7。
- **S6-D4（XDATA 下沉）**：框架 cache 与 compile definition 移除；孔径归板级（板/app 编译定义或 `absacc.h` 8192 默认）。lint 增补负向 schema 断言防回退。
- **S6-D5（board_config 作用域）**：静态库 include 路径与生成依赖移除；仅绑定方 TU（e2e 驱动/板胶）可见，框架库编译零 `MCS51_HAS_ADC0832`。
- **S6-D6（新芯片扩展）**：`mcs51_sources.cmake` 汇总源列表；框架 CMake 对未显式命名的 `chips/*/` 目录自动生成 `wink_mcs51_<family>`（含 `wink_mcs51_apply_common` 方言）；manifest 自动纳入 cleanup/lint 事实源。演练见 §3 Step 5。
- **S6-D7（manifest 布局）**：`schema.json` 五组顶层字段；`headers` 增 `cleanup_alias_regex`、`sdcc_gate` 增 `vendor_device_header`/`vendor_stddriver_dir` 子字段承载原硬编码（`cleanup_alias_regex` 区分标准 `reg51/52` 与厂商头）。载入器 PyYAML 优先 + 内置兜底，保证构建 Python 无 yaml 时的健壮性。
- **S6-D8（lint 基线重构）**：residue waiver 从 `(file, pattern-key, line-rx, stage)` 收敛为 `(file, line-rx, stage)`，pattern-key 由 manifest family 承担；`wink_mcs51_xsfr_allowlist.h` / `mcs51_family.h` / route 头 by-design 语义不变。
- **S6-D9（门禁 CI 处置）**：见 §6 第 5 项；cleanup 单测接线进 ctest，其余三项给出书面豁免（含理由）。

## 附录 B：Stage6 复审闭环（S6-H1~H7，2026-09-12）

人类全量代码评审提出 6 组问题（含 1 处“新芯片生产注入未闭环”关键项），逐项裁决与修复：

- **S6-H1（关键：根构建硬编码 → 新芯片注入闭环）**：新增 `cmake/mcs51_families.cmake`，以受限 YAML 子集解析 `tools/manifests/chips/*.yaml`（family + aliases），根 CMake 的 `_wink_app_is_mcs51` 检出与生产注入全部改为 manifest 解析（`mcu` 优先、`board` 子串回退）；框架为每个家族建 `wink_mcs51_inject_<family>` INTERFACE 包。**端到端演练重做**：临时 `newchip` 家族（仅 register 空壳 → 补一个模型 TU）+ `mcu: newchip` 测试 app + 预置家族选择头，生产 `wink_simulator` 两次均构建成功并链接到 `wink_mcs51_newchip_register`/`libwink_mcs51_newchip.a`，全程零根 CMake 修改；演练后清理。
- **S6-H2（自动发现缺 register 剥离）**：自动发现对 `chips/<family>/src/*_register.cpp` 剥离为 `wink_mcs51_<family>_register` OBJECT 库，`inject_<family>` 包只含模型库；仅 register 的空壳家族给出“register-only shell”状态并允许仅链 core+register（演练覆盖）。
- **S6-H3（旧别名不对称）**：`wink_mcs51_compat` 与 `wink_mcs51_compat_strict` 统一为 core/family/at89/adc0832 对称组合；`wink_mcs51_adc0832` 不再 PUBLIC 链接 core（改为自带 include，core 符号由消费方提供），避免 STRICT 二进制被拖入 release core；register OBJECT 无法经 INTERFACE 传递（CMake 语义）已在两处注释显式记录，由最终可执行显式链接（测试与生产 root 均已接线）。
- **S6-H4（清洗单测盲区）**：`test_mcs51_cleanup.py` 增补 2 用例——manifest 驱动的设备头改写（`reg52/cms8s78xx/REG_CMS8S78XX/stc89c52` → `<wink_mcu.h>`，`cms8s_flash.h` 保持不动）与“缺 manifest fail-fast”（临时目录 + 缓存复位），cleanup 套件 10/10。
- **S6-H5（layering 未进 ctest）**：新增 `test_mcs51_layering_gate`，随 host ctest 跑。
- **S6-H6（跨平台微瑕）**：`mcs51_sdcc_gate.py` 仓库根定位改 pathlib 按 `wink-micro-os` 目录名回溯（MSYS/Git-Bash 正斜杠路径安全）；`mcs51_bridge.cpp` S4-D5 注释同步为“deferred to stage7”。
- **S6-H7（UART 载体 putchar 双定义，验证中新发现）**：框架 `char putchar(char)` 改 WEAK（GCC/Clang/emcc），载体自定义 putchar 覆盖之；框架内部 SBUF 控制台镜像改用 `fputc` 直写，避免经载体 putchar 写 SBUF 形成重入。`uart0_printf`/`uart0_rxtx` 两个历史无法链接的 vendor 载体修复，可配置载体达 15/15。

