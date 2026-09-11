# Stage6：构建系统与工具链配置化治理

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S6-BUILD` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
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
| `frameworks/mcs51/CMakeLists.txt` | ✏️ | 分目标 + 别名 + per-target STRICT（`_at89` 编 STATIC + register 空实现，四目标对称） |
| `tools/manifests/chips/` | 🆕 | `at89c52.yaml`、`cms8s78xx.yaml`、`schema.json`（key = 文件名 stem = chips 目录名，见总纲 §3.1c） |
| `tools/mcs51_sdcc_gate.py`、`mcs51_cleanup.py`、`tools/lint/` | ✏️ | manifest 驱动（含 `BUDGETS`/`FAMILY_BY_MCU`/`MEM_LIMITS` 迁移） |
| board_config 生成链 + `test/CMakeLists.txt` 中央注册 | ✏️ | `mcs51_family_select.h` 生成（见 S6-1 Step 2）；board_config 缝与 facade/shim 段路径同步；design 02/03 文档同步 |
| `test/mcs51/wasm/` 构建 | ✏️ | 手写源列表改链分目标库（彻底删除，stage1 Step 0a 的临时补齐作废） |
| `wink-micro-app/mcs51_health_pot/CMakeLists.txt` | ✏️ | 验证零改（用户代码不动；断则回滚，见 R） |

## 3. 任务拆分

### Task S6-1：CMake 分目标与物理源文件归位 `[状态: ⏳ 待开始]`

- [ ] **Step 1（物理迁移）**：将此前在 `src/` 中过渡编译的专有实现执行物理归位：
  - `git mv src/cms8s_adc.cpp chips/cms8s78xx/src/cms8s_adc.cpp`
  - `git mv src/cms8s_buzzer.cpp chips/cms8s78xx/src/cms8s_buzzer.cpp`
  - `git mv src/cms8s_sys.cpp chips/cms8s78xx/src/cms8s_sys.cpp`
  - 确认 `devices/adc0832/src/mcs51_adc0832.cpp` 落位完毕（`at89_bus.cpp` 已取消，见 stage2 extbus 决议）。
- [ ] **Step 2（CMake 拆目标 + 零改注入）**：拆分 `wink_mcs51_core`、`wink_mcs51_cms8s`、`wink_mcs51_at89`、`wink_mcs51_adc0832` 四目标（统一 `STATIC EXCLUDE_FROM_ALL`，ESP 平台守卫保留；`at89` 仅一行空 register 也编静态库，保持四目标属性对称——`INTERFACE` 不能编 `.cpp`，禁混用）；codegen 按 `wink-app.json mcu` 生成 `mcs51_family_select.h`（board_config 同机制；外仓 generator 未就绪则测试先用检入式 fixture 头，见前置依赖）；bridge `__has_include` 纳入并在首次 reset 前调用其 register 函数——**`wink-micro-app` 用户代码一行不改**（以 `mcs51_health_pot` 零 diff 为验收）；保留 `wink_mcs51_compat` 别名一版。
- [ ] **Step 3（编译开关与下沉）**：STRICT 改 per-target；XDATA_SIZE 下沉板级；board_config 移出静态库 include 路径（中央 `test/CMakeLists.txt` 缝同步；design 02/03 文档同步更新）。
- [ ] **Step 4（wasm 改链库）**：`add_wink_wasm_mcs51_test.cmake` 手写源列表改链分目标库，彻底删除手写列表（含 stage1 Step 0a 的临时补齐）。
- [ ] **Step 5**：新芯片演练：只加 manifest + `chips/<new>/` 空壳即能配置出目标。

### Task S6-2：工具链 manifest 化 `[状态: ⏳ 待开始]`

- [ ] **Step 0**：冻结 manifest schema 骨架（`tools/manifests/chips/schema.json`），字段只含五组，多一个不加：
  ```yaml
  family: cms8s78xx            # key = 文件名 stem = chips 目录名（总纲 §3.1c）；与描述符 name 人读字段区分
  aliases: []                  # 构建选择侧接受的兼容别名（如 at89c52.yaml 内 ["stc89c52"]，收编 SDCC 门禁先例）
  headers:                     # 清洗 pass 与 lint 的头文件事实源
    vendor_include_dir: chips/cms8s78xx/include
    allow_regex: ["REG_CMS8S78XX\\.H", "cms8s_.*\\.h"]
    forbid_in_core_regex: ["cms8s", "0xF0", "ADCLDO", "FUNCCR"]
  memory:                      # SDCC 门禁事实源
    xram_bytes: 1024
    xsfr_window: [0xF000, 0x1000]   # base + size，无窗口填 null
  sdcc_gate:                   # 编译门禁事实源
    mem_limits: { xdata_max: 1024 }
    stddriver_link: ["cms8s78xx_stddriver"]   # 占位名，执行时以仓库实有为准
  ```
- [ ] **Step 1**：manifest schema 定头正则/内存上限/SDCC 规则字段；gate/cleanup/lint 只读 manifest。迁移前后 verdict 一致性验证（仓库根目录，需 SDCC 工具链＋vendor 树，手动）：
  `python wink-micro-os/frameworks/mcs51/tools/mcs51_sdcc_gate.py wink-micro-app/mcs51_health_pot [--sdcc <path>]`；
  清洗自测：`python -m pytest wink-micro-os/test/mcs51/test_mcs51_cleanup.py -q`（单文件自带 `sys.path`，任意目录可跑；未进 ctest，见 §6 CI 评估项）。
- [ ] **Step 2**：删 Python 内 `MEM_LIMITS`、`cms8s` 硬编码正则、`REG_CMS8S78XX.H` 强绑定，加缺 manifest 的 fail-fast。

## 4. 验收

- L0：三 target（host/wasm/app 注入）全绿；新芯片演练通过。
- L4：通用核心 app 不链接 cms8s 目标；全仓 `grep -rn 'MCS51_HAS_ADC0832'` 仅 devices/app 命中；`src/` 目录下 `grep -Ei 'cms8s|0xF0'` 零命中。

## 5. 风险与回滚

- R：app 链接断裂 → 缓解：旧目标别名保留一版；回滚：先 revert 工具链 commit，再 revert CMake commit（顺序写死）。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：`chips/cms8s78xx/src/` 包含全部 7 个专有实现文件（adc/buzzer/gpio/uart/timer/extint/sys）+ `cms8s_register.cpp`；`src/` 彻底零专有 `.cpp`。
- [ ] **用户零改**：`wink-micro-app` 全仓用户代码零 diff（以 `mcs51_health_pot` 为抽查锚点）；旧单体别名可用。
- [ ] **Manifest 闭环**：`tools/manifests/chips/` 中的 yaml 文件为唯一事实源，gate/cleanup 脚本零硬编码；`tools/sdcc_gate/` 选择改由 manifest 驱动。
- [ ] **双轨状态**：host 与 wasm 下 4 个拆分 target 均能独立成功编译，测试全绿；手写 wasm 源列表已删除。
- [ ] **门禁进 CI 评估**：当前手动/ctest-local 的门禁（sdcc_gate、shim_audit freshness、headless 取证、cleanup pytest）逐项给出进 CI 结论（接线或书面豁免），不留"我以为它在跑"的灰色地带。
