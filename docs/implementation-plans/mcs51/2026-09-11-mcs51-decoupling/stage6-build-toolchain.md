# Stage6：构建系统与工具链配置化治理

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S6-BUILD` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-15（CMake 单体/宏污染）、CPL-16（工具链硬编码）、CPL-24（STRICT/XDATA knob） |
| **前置依赖** | stage3（头文件已就位，可分目标） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `wink_mcs51_compat` 拆 `wink_mcs51_core` + `wink_mcs51_cms8s`（+ `_at89`、`_adc0832`），旧单体名保留别名一版；STRICT 走 per-target 定义，不搞 2×2 四库。
- ✅ `WINK_MCS51_XDATA_SIZE` 下沉板级；`mcs51_board_config.h` 限 app 作用域。
- ✅ gate/cleanup/lint 改读 `tools/manifests/chips/*.yaml`，零硬编码正则/内存上限。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `frameworks/mcs51/CMakeLists.txt` | ✏️ | 分目标 + 别名 + per-target STRICT |
| `tools/manifests/chips/` | 🆕 | `classic.yaml`、`cms8s78xx.yaml`、`schema.json` |
| `tools/mcs51_sdcc_gate.py`、`mcs51_cleanup.py`、`tools/lint/` | ✏️ | manifest 驱动 |

## 3. 任务拆分

### Task S6-1：CMake 分目标 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：拆 core/cms8s/at89/adc0832 四目标（`STATIC EXCLUDE_FROM_ALL`，ESP 守卫保留）；`wink-app.json mcu` 注入链路打通。
- [ ] **Step 2**：STRICT 改 per-target；XDATA_SIZE 下沉板级；board_config 移出静态库 include 路径。
- [ ] **Step 3**：新芯片演练：只加 manifest + `chips/<new>/` 空壳即能配置出目标。

### Task S6-2：工具链 manifest 化 `[状态: ⏳ 待开始]`

- [ ] **Step 0**：冻结 manifest schema 骨架（`tools/manifests/chips/schema.json`），字段只含四组，多一个不加：
  ```yaml
  family: cms8s78xx            # 与 McuFamilyDescriptor.id 对应
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
- [ ] **Step 1**：manifest schema 定头正则/内存上限/SDCC 规则字段；gate/cleanup/lint 只读 manifest。
- [ ] **Step 2**：删 Python 内 `MEM_LIMITS`、`cms8s` 硬编码正则、`REG_CMS8S78XX.H` 强绑定，加缺 manifest 的 fail-fast。

## 4. 验收

- L0：三 target（host/wasm/app 注入）全绿；新芯片演练通过。
- L4：通用核心 app 不链接 cms8s 目标；全仓 `grep -rn 'MCS51_HAS_ADC0832'` 仅 devices/app 命中。

## 5. 风险与回滚

- R：app 链接断裂 → 缓解：旧目标别名保留一版；回滚：先 revert 工具链 commit，再 revert CMake commit（顺序写死）。
