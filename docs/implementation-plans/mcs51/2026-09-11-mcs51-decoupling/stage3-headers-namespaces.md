# Stage3：头文件体系与命名空间归位

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260911-MCS51-S3-HEADERS` |
| **创建日期** | `2026-09-11` |
| **目标平台** | `host` / `wasm` |
| **计划状态** | 📋 草稿 |
| **优先级** | 🟡 P1 |
| **关联 CPL** | CPL-09（桥裸包含/硬调用）、CPL-13（sfr_map）、CPL-14（公共头）、CPL-21（ADC0832）、CPL-24（前缀落地） |
| **前置依赖** | stage2（context 已纯净） |
| **总纲** | [`./00-README.md`](./00-README.md) |

## 1. 目标

- ✅ `REG_CMS8S78XX.H` 等下沉 `chips/cms8s78xx/include/`；`mcs51_sfr_map.h` 缩水为 Intel 标准，专有部分改 `CMS8S_` 前缀下沉。
- ✅ `wink_mcu.h` 上移出 51 框架，51 内仅留 51 路由；`wink_mcs51_wdt.h` 去 `cms8s_sys_notify` 硬导出。
- ✅ `mcs51_bridge.cpp` 去裸 include，TA 改钩子派发；ADC0832 下沉 `devices/`。
- ✅ 转发 shim 保留一版（告警），下阶段删除；前缀 lint 全绿。

## 2. 变更范围

| 文件 | 变更 | 说明 |
|------|------|------|
| `chips/cms8s78xx/include/` | 🆕 | REG/cms8s 头 + `cms8s_sfr_map.h` + allowlist 下沉 |
| `include/mcs51_sfr_map.h` | ✏️ | 仅标准 SFR（`T2CON` 保留，`CKCON` 地址可提语义下沉） |
| `include/wink_mcu.h` | ✏️/搬迁 | 上移公共层（新建 `wink-micro-os/include/` 并评估构建引用，51 内残留改名 `mcs51_family_route.h`） |
| `include/wink_mcs51_wdt.h` | ✏️ | 去厂商硬导出 |
| `src/mcs51_bridge.cpp` | ✏️ | 去裸 include，去 TA 硬调用（TA hook 首注册保序） |
| `devices/adc0832/` | 🆕 | 状态机下沉：`include/adc0832.h` + `src/mcs51_adc0832.cpp`，trap 接入 |
| `tools/mcs51_shim_audit.py` + freshness 门禁 | ✏️ | `include/REG_CMS8S78XX.H` 硬编码路径随头搬迁同步；allowlist 生成目标改为 chips 路径 |
| 消费侧同步（本阶段遗漏即断链） | ✏️ | 6 个 `wink-micro-app/mcs51_*` 应用头引用、`tools/mcs51_cleanup.py` 重写目标、`tools/sdcc_gate/` 分发头、`test/CMakeLists.txt` facade 测试与 shim 抑制段、unit 测试直引 `cms8s_adc.h`/`ADC0832.H` 处、`test/.../samples/cms8s_adc_test.c` |

## 3. 任务拆分

### Task S3-1：寄存器表与厂商头下沉 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：建 `chips/cms8s78xx/include/` 并搬移 4 头（`REG_CMS8S78XX.H`, `cms8s78xx.h`, `cms8s_adc.h`, `cms8s_buzzer.h`）+ `mcs51_xsfr_allowlist.h`；原路径留一版转发 shim（`#warning`）。
- [ ] **Step 2**：`mcs51_sfr_map.h` 删 `P0EXTIF/T34MOD/EIE2/EIF2/PS_*`，专有寄存器改 `CMS8S_` 前缀下沉到 `chips/cms8s78xx/include/cms8s_sfr_map.h`；`T2CON` 保留。
- [ ] **Step 3**：`wink_mcu.h` 上移至 `wink-micro-os/runtime/include/wink_mcu.h`（不新建顶层 `include/`：host 库 PUBLIC 与 wasm `_WASM_MCS51_INCLUDES` 已含该目录，零新增 `-I`；先枚举 6 app + test + wasm 三条消费链验证，无一例外才搬）；51 内残留瘦身并改名 `mcs51_family_route.h`（禁留同名文件）；同步 §2 消费侧表全部条目。
- [ ] **Step 4**：`mcs51_shim_audit.py` 的 `REG_CMS8S78XX.H` 路径与 allowlist 生成目标同步到 chips 路径（freshness 门禁同改；isr 交叉校验的家族化留 stage5）。验证命令（仓库根目录，需 Python3＋vendor 树）：
  `python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py --check-xsfr-allowlist wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_xsfr_allowlist.h`；
  重生成：`--emit-xsfr-allowlist` 同路径（输出与检入文件 `diff` 为空方为绿）。

### Task S3-2：桥 + WDT + ADC0832 `[状态: ⏳ 待开始]`

- [ ] **Step 1**：bridge 删 `ADC0832.H`/`cms8s_adc.h` 裸包含；`cms8s_sys_notify_sfr_write` 改为 `sfr_write_hooks` 派发（TA hook 首注册，保序约束见总纲 §3.1b-5）。
- [ ] **Step 2**：`wink_mcs51_wdt.h` 去厂商导出；真通用语义保留，否则改名下沉。
- [ ] **Step 3**：新建 `devices/adc0832/include/` 与 `devices/adc0832/src/`，将 `include/ADC0832.H` 与 `src/mcs51_adc0832.cpp` 搬移至对应目录下（规范小写头 `adc0832.h`；大小写敏感纪律：`git mv -f`，Linux CI 校验；原路径留带 `#warning` 的 `ADC0832.H` 转发 shim，下阶段删）；同步更新 wasm 手写源列表中的文件路径 + 追加 devices include 目录（单体库侧 include 目录同加；总纲 §6.1-7 双列表规则）；对外提供 `adc0832_device_attach(ctx, cs, clk, di, do)` 经 trap 挂载（需外挂的板型在自己的 post-init 钩子 `mcs51_framework_set_post_init_hook` 里调用）；`MCS51_HAS_ADC0832` 作用域限器件与 app。
- [ ] **Step 4（板级模拟空间日落决策，日落条款）**：二选一、当期定案并记结论（ADR 或本计划 §6 签署）：(A) 保留 32~63 为永久板级通道空间（device-tree/前端共有契约）；(B) 设备私有 pull（board net id 进 `adc0832_device_attach` 参数，rail 回纯 Pin）。到期不定则按 (B) 执行；无论选何者，本阶段 `devices/` 落位与 trap 接入不变。

## 4. 验收

- L0：全仓 include 链不断（shim 期零 error，告警可接受）。
- L4：公共 `include/` 无 `REG_CMS8S78XX.H`；bridge 编译不依赖芯片/器件头；`grep -rn 'MCS51_HAS_ADC0832' src/mcs51_bridge.cpp` 零命中（全仓仅 `devices/` 与 app 侧命中）；前缀 lint 全绿。

## 5. 风险与回滚

- R-03（include 断裂）：shim 缓解；回滚：先 revert 搬移 commit，shim 独立 commit 可单独回退。

## 6. 阶段自审自我检验清单（Self-Audit Checkpoint）
- [ ] **目录落位**：`chips/cms8s78xx/include/` 与 `devices/adc0832/{include,src}/` 文件物理路径 100% 符合终态目录树。
- [ ] **通用纯净度**：公共 `include/` 零厂商专有头；`src/mcs51_bridge.cpp` 零裸包含 `ADC0832.H`/`cms8s_adc.h`。
- [ ] **转发兼容性**：保留的转发 shim 带有 `#warning` 告警但编译不报错。
- [ ] **日落决策**：板级模拟空间去留（保留 32~63 vs 设备私有 pull）已二选一并归档，后续阶段只执行结论。
- [ ] **双轨状态**：全仓编译通过，`core-tests` 与 `cms8s-tests` 双绿。
