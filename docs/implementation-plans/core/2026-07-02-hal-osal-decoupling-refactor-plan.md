# 2026-07-02 WinkMicroOS HAL × OSAL 解耦与迁移实施计划

> **修订**: 2026-07-19 — 按架构评审收紧范围：本计划交付 **Phase A（物理解耦 + 默认强绑定 + 统一 CMake 导出）**；**不**将「STM32 复用同一份 FreeRTOS OSAL」列入本计划验收。真正跨 MCU 的 portable FreeRTOS 核属 **Phase B**（另开计划）。

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-02 |
| 修订日期 | 2026-07-19 |
| 完成日期 | 2026-07-19 |
| 状态 | **Completed（Phase A 已完成）** |
| 关联 ADR | [ADR-0041](../../decisions/core/0041-hal-osal-directory-orthogonality.md)（Accepted）— HAL/OSAL 目录正交与合法组合矩阵 |
| 回写设计规范（Accepted 后） | [`02-pal-platform-abstraction.md`](../../design/02-wink-micro-os/02-pal-platform-abstraction.md)、[`03-directory-architecture.md`](../../design/02-wink-micro-os/03-directory-architecture.md) |
| 执行入口 | `wink-micro-os/`（CMake + ESP-IDF component） |

**Goal:** 将 OSAL 实现从 `targets/<plat>/` 物理迁出到 `wink-micro-os/osal/`，使 `targets/` 仅承载硬件/仿真 HAL 与芯片私有设施；构建期用 `TARGET_PLATFORM` × `WINK_OSAL_TYPE` 装配，默认组合强绑定、非法组合硬失败。

**Architecture:** PAL 契约仍在 `pal/include/`（`pal_hal.h` / `pal_osal.h` 等）。实现侧拆成两列：`targets/` = 硬件与平台外设适配；`osal/` = 操作系统/运行环境适配。Phase A 只做 1:1 默认绑定（wasm↔wasm、host↔host、esp32↔freertos_esp32）；CMake 以 `osal/CMakeLists.txt` 为 OSAL 源文件 SSOT，禁止各 target 再硬编码 OSAL 路径。

**Tech Stack:** CMake 3.x、ESP-IDF component、Emscripten、Host MinGW + Unity/GTest、现有 `python wink-tools/wink.py test` / `wink.py`。

---

## 0. 范围、非目标与阶段划分

### 0.1 Phase A（本计划必须交付）

1. OSAL 源文件迁入 `osal/<variant>/`，`targets/` 不再包含 `pal_osal_*.c` / `sim_ctx_*.c` / `pal_osal_ringbuf.c`。
2. 引入 `WINK_OSAL_TYPE` + **合法组合矩阵**（非法 `FATAL_ERROR`）。
3. `osal/CMakeLists.txt` 统一导出 `WINK_OSAL_SOURCES` / `WINK_OSAL_INCLUDE_DIRS`；wasm / host / esp32（含 idf 与静态分析双写）只消费这两个变量。
4. 三平台零功能回归（Host 全测 + Wasm 调度烟测 + ESP32 编译；有板则 OSAL 硬件烟测）。
5. ADR-0041 Accepted 后回写目录/PAL 设计规范；全仓路径清扫（脚本、注释、活跃文档中的旧路径）。

### 0.2 Phase B（明确不在本计划验收内）

| 交付 | 说明 |
|------|------|
| `osal/freertos/` 可移植核 | 仅映射标准 FreeRTOS API（task/mutex/sem/delay） |
| `osal/freertos_esp32/` 缩为增量 | tick=`esp_timer`、WDT、RTC_NOINIT boot-count、`xTaskCreatePinnedToCore`、ROM busy-wait 等 |
| STM32 + FreeRTOS 组合 | `TARGET_PLATFORM=stm32` × `WINK_OSAL_TYPE=freertos`（或 `freertos_stm32` 增量） |
| 切断 `osal/wasm` → `targets/wasm` 私有头 | 将 `wasm_bridge.h` / OSAL 所需声明抽到 OSAL 边界头 |

> **动机降级声明（重要）:** 当前 `targets/esp32/pal_osal_esp32.c` **不是**可复用 FreeRTOS 适配（深度绑定 ESP-IDF）。Phase A 迁到 `osal/freertos_esp32/` **不会**消除「给 STM32 再抄一份」；那是 Phase B 的目标。本计划主收益是：**净化 `targets/`、建立正交装配骨架、避免未来 `stm32_bare`/`stm32_freertos` 目录爆炸。**

### 0.3 Non-goals（本计划禁止顺手做）

- 不改 `pal_osal.h` / `pal_hal.h` 公共契约语义（除非发现搬家导致的纯路径/include 修复）。
- 不重构 `pal_irq_*`、`pal_storage_*`、fault/物理退化子系统。
- 不迁移 `wink_sim_scheduler.c` / `wink_sim_physical.c`（Phase A 仍留 `targets/common/`，见归属矩阵）。
- 不实现 STM32 target，不抽取 portable FreeRTOS 核。
- 不扩大 `#ifdef SIMULATION` 范围；不引入运行期 ops/vtable。

---

## 1. 架构背景与痛点

现行四类运行环境（`wasm` / `host` / `esp32` / 预留 `baremetal`）中，OSAL 与 HAL 混在同一 `targets/<plat>/`：

- `targets/esp32/` 含 GPIO/PWM/… 与 `pal_osal_esp32.c`（FreeRTOS + ESP-IDF）。
- `targets/wasm/` 含虚拟外设 HAL 与 `pal_osal_wasm.c` + Asyncify fiber。
- `targets/host/` 含 host HAL 与 Win32 Fiber OSAL。

痛点：

1. **OS 适配无法按 OS 维度复用目录**（即便 Phase A 尚不能跨 MCU 复用代码，目录耦合仍会逼出平台×OS 笛卡尔积目录）。
2. **`targets/` 语义不清**：新人/AI 易把「换芯片」与「换 OS」绑死。
3. **构建配置单一**：缺少显式 `WINK_OSAL_TYPE` 与非法组合门禁。

---

## 2. 归属矩阵（Phase A 冻结）

搬家前以本表为 SSOT。有争议的模块已给出 Phase A 裁定；变更须先改 ADR-0041。

| 模块 / 文件 | Phase A 归属 | 裁定理由 |
|-------------|--------------|----------|
| `pal_os_*`（sleep/tick/mutex/sem/task/critical/…） | **`osal/`** | `pal_osal.h` 契约实现 |
| `sim_ctx_*.c`（fiber / Asyncify 上下文） | **`osal/wasm` 或 `osal/host`** | 调度原语，非外设 |
| `pal_osal_ringbuf.c` | **`osal/common/`** | OSAL 环形缓冲共享实现 |
| `wink_sim_scheduler.c` + `wink_sim_scheduler.h` | **仍留 `targets/common/`** | 被 OSAL 与测试共用；Phase A 只加 include，不搬文件，避免牵动 test 面过大 |
| `wink_sim_physical.c` | **仍留 `targets/common/`** | 物理退化属仿真 HAL 侧算法库 |
| `pal_hal_*` / `pal_rmt_*` / GPIO·I2C·PWM | **`targets/<plat>/`** | 硬件总线 |
| `pal_irq_*` | **`targets/<plat>/`** | 第三列能力，本波不抽独立 `irq/`；文档注明「非 OSAL」 |
| `pal_log_*` | **`targets/<plat>/`** | 平台日志后端 |
| `pal_storage_*` | **`targets/<plat>/`** | 平台存储 |
| `pal_resource_*` / `targets/common/pal_resource.c` | **不变** | 资源表，非 OS |
| WDT / reset reason / abnormal boot-count | **留在对应 `pal_osal_*.c`（即随 OSAL 迁走）** | 契约已在 `pal_osal.h`；实现碰芯片 RTC/ROM 属已知耦合，Phase A 整文件搬迁，Phase B 再拆钩子 |
| `wasm_bridge.h` / `pal_wasm_internal.h` | **仍居 `targets/wasm/`** | Phase A 允许 `osal/wasm` **仅**为这两个头把 `targets/wasm` 加入 include（记为技术债 T-DEP-01） |
| `host_test_ctrl.h` | **仍居 `test/stubs/`** | host OSAL 继续 include；实现若在 `pal_osal_host.c` 则随文件迁到 `osal/host/` |
| `host_wall_clock.h`（若存在于 host target） | **随 host 边界**：头若仅 OSAL WCET 用，可随迁或保留在 `targets/host` + include | 实施时以「谁实现符号」为准，禁止循环依赖 |
| `*_entry.c` / `wasm_entry.c` | **`targets/<plat>/`** | 启动入口属平台，不是 OSAL |

### 2.1 依赖方向红线

```text
App / BAL / DAL / runtime
        │
        ▼
   pal/include  （契约，无 .c）
        │
        ├──────────────► targets/<plat>/   （HAL / irq / storage / log / entry）
        │
        └──────────────► osal/<variant>/   （OSAL 实现）
                              │
                              ├──► pal/include
                              ├──► targets/common/include   （scheduler / physical 头）
                              └──► 【例外 T-DEP-01】targets/wasm 仅 wasm_bridge + pal_wasm_internal
```

**禁止：** `targets/<plat>/*.c`（HAL）`#include` `osal/` 私有实现头。  
**禁止：** `osal/` 依赖 DAL / App。  
**禁止：** 新建 `osal → targets/<plat>` 私有头依赖（T-DEP-01 白名单除外）。

---

## 3. 合法组合矩阵

| `TARGET_PLATFORM` | 默认 `WINK_OSAL_TYPE` | Phase A 允许覆盖？ | 说明 |
|-------------------|----------------------|-------------------|------|
| `wasm` | `wasm` | **否**（仅自身） | Asyncify + 虚拟时钟 |
| `host` | `host` | **否**（仅自身） | Win32 Fiber + 虚拟时间 |
| `esp32` | `freertos_esp32` | **否**（仅自身） | ESP-IDF FreeRTOS-SMP |
| （未来）`stm32` | （未来）`freertos` / `baremetal` | Phase B | 本计划不启用 |
| （任意） | `baremetal` | 仅当平台显式列入矩阵 | Phase A：**可编译进树，但不挂到现有三平台默认组合**；CMake 枚举存在即可 |

非法示例（必须 `message(FATAL_ERROR)`）：

- `-DTARGET_PLATFORM=esp32 -DWINK_OSAL_TYPE=wasm`
- `-DTARGET_PLATFORM=wasm -DWINK_OSAL_TYPE=freertos_esp32`
- `-DTARGET_PLATFORM=host -DWINK_OSAL_TYPE=baremetal`（Phase A）

实现要求：在顶层配置阶段调用 `wink_validate_osal_combo(${TARGET_PLATFORM} ${WINK_OSAL_TYPE})`（可放在 `cmake/wink_osal.cmake`）。

### 3.1 `WINK_OSAL_TYPE` 推导与 CACHE 陷阱

```cmake
# 伪代码 —— 正式实现写入 cmake/wink_osal.cmake
set(TARGET_PLATFORM "wasm" CACHE STRING "Hardware platform: wasm, esp32, host")

# 1) 若用户未显式 -DWINK_OSAL_TYPE=...，则按平台推导
# 2) 若 CACHE 中已有值但与平台非法，则 FATAL_ERROR（不要静默 FORCE 改掉用户意图）
# 3) 文档与 wink.py 提示：换平台请同时清 build 目录或显式传 WINK_OSAL_TYPE

function(wink_resolve_osal_type platform out_var)
  if(DEFINED WINK_OSAL_TYPE_USER_OVERRIDE)  # 或通过 CACHE 属性检测
    ...
  endif()
endfunction()
```

**硬性规则：**

- 禁止「第一次 `set(... CACHE)` 写入后，换 `TARGET_PLATFORM` 却仍沿用旧 OSAL」的静默错绑。
- 推荐策略：每次 configure 根据矩阵校验；非法则失败并打印修复命令（`cmake -UWINK_OSAL_TYPE` 或删 build）。

---

## 4. 目标目录树（Phase A 完成后）

```text
wink-micro-os/
├── cmake/
│   └── wink_osal.cmake                 # 【NEW】推导默认值、合法矩阵、导出变量
├── osal/                               # 【NEW】OS 维度
│   ├── CMakeLists.txt                  # 【NEW】SSOT：按 WINK_OSAL_TYPE 填 WINK_OSAL_*
│   ├── common/
│   │   └── pal_osal_ringbuf.c
│   ├── baremetal/
│   │   └── pal_osal_baremetal.c        # 入树可编；Phase A 不绑三平台
│   ├── freertos_esp32/
│   │   └── pal_osal_freertos_esp32.c   # 自 pal_osal_esp32.c 迁入并重命名
│   ├── wasm/
│   │   ├── pal_osal_wasm.c
│   │   └── sim_ctx_emscripten_fiber.c
│   └── host/
│       ├── pal_osal_host.c
│       └── sim_ctx_win32_fiber.c
│
├── targets/                            # 【REFAC】仅硬件/仿真 HAL 等
│   ├── common/                         # scheduler / physical / resource 仍在此
│   ├── esp32/                          # 删除 pal_osal_esp32.c
│   ├── wasm/                           # 删除 pal_osal_wasm.c、sim_ctx_emscripten_fiber.c
│   ├── host/                           # 删除 pal_osal_host.c、sim_ctx_win32_fiber.c
│   └── （删除空目录 targets/baremetal/）
│
└── pal/include/osal/pal_osal.h         # 契约位置不变
```

### 4.1 命名例外（须写入 ADR + 目录规范）

既有规范倾向 `pal_<domain>_<plat>.c`。OSAL 迁出后 plat 不再等于「芯片目录名」：

| 文件 | 允许理由 |
|------|----------|
| `pal_osal_freertos_esp32.c` | OS 变体 + 芯片特化后缀；目录已是 `osal/freertos_esp32/` |
| `pal_osal_wasm.c` / `pal_osal_host.c` | 与运行环境同名，非 MCU 型号 |

禁止再出现：`targets/esp32/pal_osal_esp32.c`（旧布局）。

---

## 5. CMake 装配设计（本计划核心）

### 5.1 SSOT：`osal/CMakeLists.txt` + `cmake/wink_osal.cmake`

**产出变量（名称冻结）：**

| 变量 | 含义 |
|------|------|
| `WINK_OSAL_TYPE` | 当前 OSAL 变体字符串 |
| `WINK_OSAL_SOURCES` | 该变体全部 `.c` 列表（含 `osal/common` 按需） |
| `WINK_OSAL_INCLUDE_DIRS` | 该变体 PRIVATE/PUBLIC include |

各消费方：

| 消费方 | 用法 |
|--------|------|
| 顶层 `wink_simulator`（wasm） | `add_executable(... ${WINK_OSAL_SOURCES})` + include |
| `targets/host` → `pal_host` OBJECT | **禁止**再写死 `../../osal/host/...`；改为 `${WINK_OSAL_SOURCES}` |
| `targets/esp32` `idf_component_register(SRCS ...)` | 使用 `${WINK_OSAL_SOURCES}` |
| `targets/esp32` 非 IDF 分支 `ESP32_PAL_SOURCES` | **必须与 idf SRCS 中 OSAL 条目对齐**（双写门禁） |

### 5.2 消费者改造要点

#### 顶层 `wink-micro-os/CMakeLists.txt`

- `include(cmake/wink_osal.cmake)` → resolve + validate → `add_subdirectory(osal)`。
- wasm 可执行文件：HAL 仍来自 `targets/wasm` 的 `PAL_WASM_SOURCES`；OSAL 只来自 `WINK_OSAL_SOURCES`。

#### `targets/wasm/CMakeLists.txt`

从 `PAL_WASM_SOURCES` **删除**：

- `pal_osal_wasm.c`
- `sim_ctx_emscripten_fiber.c`
- `../common/src/pal_osal_ringbuf.c`

保留 HAL、fault、devices、`wink_sim_*`、`pal_resource.c` 等。

#### `targets/host/CMakeLists.txt`

`pal_host` OBJECT **只含 HAL/log/storage/common/physical/scheduler**；OSAL 三个文件改为上层或本文件通过 `${WINK_OSAL_SOURCES}` 注入（若 OBJECT 库必须自洽，则 `list(APPEND)` 使用已导出的 `WINK_OSAL_SOURCES`，**禁止**手写绝对相对路径列表）。

#### `targets/esp32/CMakeLists.txt`（双写）

**两处**同时改：

1. `idf_component_register(SRCS ...)`：`pal_osal_esp32.c` → `${WINK_OSAL_SOURCES}`（或展开后的 freertos_esp32 + 所需 common）。
2. `elseif(TARGET_PLATFORM STREQUAL "esp32")` 的 `ESP32_PAL_SOURCES`：同样替换，**禁止只改一边**。

`INCLUDE_DIRS` 追加 `${WINK_OSAL_INCLUDE_DIRS}`。  
`REQUIRES` 保持 `esp_timer` / `esp_system` / `esp_ringbuf` 等（仍由 ESP OSAL 需要）。

### 5.3 反例（禁止合并进 PR）

```cmake
# ❌ Phase 2 旧稿：把 OSAL 路径又写死进 targets/host —— 不是正交装配
add_library(pal_host OBJECT
    ...
    ${CMAKE_CURRENT_SOURCE_DIR}/../../osal/host/pal_osal_host.c
    ...
)
```

---

## 6. 任务拆解（按门禁顺序执行）

> 每个 Phase 结束必须有可验证门禁；未过门禁不得进入下一 Phase。  
> Commit 粒度：按「ADR / 迁移 / CMake / 路径清扫 / 文档回写」分原子提交（英文 message）。

### Phase 0 — 决策与文档骨架（先于任何 `git mv`）

#### Task 0.1: 撰写 ADR-0041

**Files:**

- Create: `docs/decisions/core/0041-hal-osal-directory-orthogonality.md` ✅（2026-07-19，**Accepted**）

**必须写清：**

- Context：`targets/` 内 HAL/OSAL 耦合。
- Decision：目录正交；Phase A 默认强绑定；合法矩阵；归属表；命名例外；T-DEP-01。
- Consequences：STM32 复用延后到 Phase B；设计规范回写清单。
- Alternatives：继续 `targets/stm32_freertos` 目录爆炸；或一次抽 portable FreeRTOS（否决理由：范围过大）。

- [x] ADR 正文完成并标 `Proposed`
- [x] 链接本实施计划
- [x] 用户确认后将 ADR 标为 `Accepted`（门禁 0）

#### Task 0.2: 在计划/ADR 冻结归属矩阵与合法组合（本文 §2–§3）

- [x] 评审确认 WDT/boot-count 随 OSAL 走
- [x] 评审确认 scheduler 暂留 `targets/common`
- [x] 评审确认 T-DEP-01 白名单

**门禁 0：** ✅ 已通过（ADR-0041 Accepted）。

---

### Phase 1 — 物理迁移（不改行为）

#### Task 1.1: 创建目录

- [x] 创建 `wink-micro-os/osal/{common,baremetal,freertos_esp32,wasm,host}/`
- [x] 创建空的 `osal/CMakeLists.txt` 与 `cmake/wink_osal.cmake` 骨架（先能 `add_subdirectory`）

#### Task 1.2: `git mv` 文件并重命名

| 源 | 目标 |
|----|------|
| `targets/baremetal/pal_osal_baremetal.c` | `osal/baremetal/pal_osal_baremetal.c` |
| `targets/esp32/pal_osal_esp32.c` | `osal/freertos_esp32/pal_osal_freertos_esp32.c` |
| `targets/wasm/pal_osal_wasm.c` | `osal/wasm/pal_osal_wasm.c` |
| `targets/wasm/sim_ctx_emscripten_fiber.c` | `osal/wasm/sim_ctx_emscripten_fiber.c` |
| `targets/host/pal_osal_host.c` | `osal/host/pal_osal_host.c` |
| `targets/host/sim_ctx_win32_fiber.c` | `osal/host/sim_ctx_win32_fiber.c` |
| `targets/common/src/pal_osal_ringbuf.c` | `osal/common/pal_osal_ringbuf.c` |

- [x] 删除空目录 `targets/baremetal/`（若已空）
- [x] 更新迁入文件头 `@file` 路径注释
- [x] **本 Task 不改逻辑**；若编译暂时破坏，允许在同一 PR 的 Phase 2 立即修复，但不得夹带行为变更

**门禁 1：** `git status` 显示旧路径无残留 OSAL 实现文件；`rg "targets/.*/pal_osal_.*\\.c"` 在 `wink-micro-os/` 源树为 0（CMake 未改前可能仍引用——Phase 2 消除）。

---

### Phase 2 — CMake 正交装配

#### Task 2.1: 实现 `cmake/wink_osal.cmake`

- [x] 默认推导表（§3）
- [x] `wink_validate_osal_combo`
- [x] 单测式手工验证：轻量烟测工程 `wink-micro-os/cmake/smoke/`（勿对完整 `esp32` 树强行非法组合 configure，易拖入 IDF）

#### Task 2.2: 实现 `osal/CMakeLists.txt` 导出

按 `WINK_OSAL_TYPE` 分支填充 `WINK_OSAL_SOURCES` / `WINK_OSAL_INCLUDE_DIRS`：

- `wasm` → `osal/wasm/*.c` + `osal/common/pal_osal_ringbuf.c`；includes: `osal/wasm`、`osal/common`、`targets/common/include`、**以及 T-DEP-01 的 `targets/wasm`**
- `host` → `osal/host/*.c` + ringbuf；includes: `osal/host`、`osal/common`、`targets/common/include`、`test/stubs`（若 OSAL 仍依赖 `host_test_ctrl.h`）
- `freertos_esp32` → `pal_osal_freertos_esp32.c` +（若 ringbuf 仍链 common 实现则加入；注意 ESP 现用 IDF `esp_ringbuf`——以迁入文件实际链接需求为准，**不要盲目双链**）
- `baremetal` → 仅 baremetal 源（供未来/静态分析）

> **ringbuf 注意：** ESP32 真机 OSAL 可能走 IDF ringbuf API，而 `pal_osal_ringbuf.c` 主要服务 host/wasm。导出列表必须按变体精确包含，禁止「所有变体都链 common ringbuf」导致重复符号。

#### Task 2.3: 改造三处消费者

- [x] 顶层 wasm `wink_simulator`
- [x] `targets/wasm/CMakeLists.txt` 移除 OSAL 条目
- [x] `targets/host/CMakeLists.txt` 改为消费 `WINK_OSAL_*`（消灭硬编码 osal 路径）
- [x] `targets/esp32/CMakeLists.txt` **idf + ESP32_PAL_SOURCES 双写对齐**

#### Task 2.4: 非法组合与默认绑定冒烟（configure-only）

```powershell
# 推荐：轻量矩阵烟测（仅 include wink_osal.cmake）
cmake -S wink-micro-os/cmake/smoke -B build-osal-matrix-bad `
  -DTARGET_PLATFORM=esp32 -DWINK_OSAL_TYPE=wasm
# 期望：FATAL，日志含 Illegal configuration combo + 两端类型

cmake -S wink-micro-os/cmake/smoke -B build-osal-matrix-ok `
  -DTARGET_PLATFORM=host -DWINK_OSAL_TYPE=host
# 期望：成功
```

- [x] 非法组合 FATAL_ERROR（`wink_osal.cmake` + `cmake/smoke`；本地可复跑上列命令）
- [x] 三平台默认组合 configure/build 成功（Host/Wasm/ESP32，见验收记录）

**门禁 2：** ✅ 已通过。

---

### Phase 3 — Include / 依赖边界收敛

#### Task 3.1: 修复迁入后的 include

重点文件：

- `osal/wasm/pal_osal_wasm.c`：`wasm_bridge.h`、`pal_wasm_internal.h`、`wink_sim_scheduler.h`
- `osal/host/pal_osal_host.c`：`host_test_ctrl.h`、`wink_sim_scheduler.h`、`host_wall_clock.h`（若有）
- fiber 头若有 `sim_ctx.h`，确认仍在 `targets/common/include` 或随 fiber 私有

- [x] 无「文件找不到」类错误
- [x] 无新增 `osal → dal` 依赖

#### Task 3.2: 记录 T-DEP-01

在 ADR-0041 Consequences 与 `osal/wasm/CMakeLists` 注释中写明：

```text
T-DEP-01: osal/wasm may include targets/wasm/{wasm_bridge.h,pal_wasm_internal.h} only.
Phase B: extract OSAL-facing decls to osal/wasm/ or pal/include.
```

**门禁 3：** Host / Wasm / ESP32 全量链接无 unresolved symbol。

---

### Phase 4 — 全仓路径清扫

#### Task 4.1: 代码与脚本

```text
rg -n "targets/(esp32|wasm|host|baremetal)/pal_osal|sim_ctx_emscripten_fiber|sim_ctx_win32_fiber|common/src/pal_osal_ringbuf" wink-micro-os docs/design
```

- [x] 修复 `python wink-tools/wink.py test` / CI / `wink.py` / `pack_sdk_*.py` 中硬编码路径
- [x] 修复测试注释中的旧路径（如指向 `targets/host/pal_osal_host.c`）
- [x] 活跃设计规范与本计划互链；**历史 ADR/旧 plan 不强制改写**（可加「路径已迁」注或忽略）

#### Task 4.2: Binary SDK / 源码包

- [x] 若打包脚本枚举 `targets/**/pal_osal*`，改为 `osal/**`
- [x] 本地跑一次 pack dry-run（若仓库有该目标）

**门禁 4：** 上述 rg 在「会参与构建或活文档」的路径上无未解释命中（历史归档除外并在 PR 说明）。

---

### Phase 5 — 验收（零回归）

详见 §8。全部通过后才进入 Phase 6。

### Phase 6 — 设计规范回写

#### Task 6.1: 回写 Layer ①

- [x] `docs/design/02-wink-micro-os/03-directory-architecture.md`：目录树、命名例外、`osal/` 说明、targets 不再含 OSAL
- [x] `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`：实现位置改为 `targets/` + `osal/`；补充组合矩阵链接
- [x] ADR-0041 链到已更新规范章节

#### Task 6.2: 计划收尾

- [x] 本计划状态改为 `Completed` + 完成日期
- [x] 列出遗留技术债：T-DEP-01、Phase B portable FreeRTOS、scheduler 是否迁入 `osal/common`

**门禁 6：** 文档与代码目录树一致；ADR Accepted 且回写完成。

---

## 7. 风险与应对

| ID | 风险 | 影响 | 应对 |
|----|------|------|------|
| R1 | ESP-IDF `build/` 缓存仍指向旧 `.c` | 链接缺符号或链到幽灵对象 | `idf.py fullclean` 后再 build；CI 文档写明 |
| R2 | ESP32 **双写**只改 idf、漏 `ESP32_PAL_SOURCES` | 静态分析/非 IDF 配置漂移 | Phase 2 检查清单强制 diff 两处；PR 模板勾选 |
| R3 | Host/Wasm 漏 `osal` include | 编译失败 | `WINK_OSAL_INCLUDE_DIRS` SSOT；门禁 3 |
| R4 | Wasm JS library / Asyncify 符号漂移 | Worker 起不来 | 保持 `LINK_DEPENDS`；门禁 5b stub + scheduler 烟测 |
| R5 | `WINK_OSAL_TYPE` CACHE 错绑 | 静默错误组合 | 矩阵校验 + 非法 FATAL；换平台建议清 build |
| R6 | ringbuf 重复符号（IDF vs `pal_osal_ringbuf.c`） | ESP 链接失败 | 按变体精确导出源列表 |
| R7 | `osal/wasm` ↔ `targets/wasm` 隐式耦合扩大 | 假解耦 | T-DEP-01 白名单；Code review 拒新增私有头依赖 |
| R8 | SDK/CI 硬编码旧路径 | 打包缺文件 | Phase 4 rg 清扫 |
| R9 | 误删 `targets/baremetal` 时遗漏引用 | 死引用 | rg `baremetal`；确认从未入默认构建 |
| R10 | 验收只做「能编过」 | OSAL 运行时回归漏测 | §8 行为烟测强制项 |
| R11 | 文档 SSOT 未回写 | AI/后人继续往 `targets/` 丢 OSAL | Phase 6 门禁 |

---

## 8. 验收与验证计划

### 8.1 Host（必须）

```powershell
cd wink-micro-os
python wink-tools/wink.py test
```

**标准：** CMake 成功；无新增 warning-as-error 失败；Unity/GTest 与 e2e **100% 通过**（含 scheduler / mutex / fiber 相关用例）。

### 8.2 WebAssembly（必须）

```bash
emcmake cmake -S wink-micro-os -B build-wasm -DTARGET_PLATFORM=wasm
cmake --build build-wasm
node targets/wasm/wink_sim_stub.js
```

**标准：**

1. 产出 `wink_simulator.wasm`；体积相对搬家前基线偏离 **≤ 2%**（PR 中记录搬家前/后字节数；无基线则记录本次绝对值供下次对比）。
2. stub：`onRuntimeInitialized` 正常。
3. **行为烟测（至少一条）：** 现有 sim scheduler e2e 或等价用例（sleep / yield / mutex）在 Wasm 或 Host 同源路径上证明调度未毁——优先跑仓库已有 `test_sim_scheduler_e2e`（Host）+ Wasm stub 时钟推进日志无报错。

### 8.3 ESP32（必须编译；有板则硬件烟测）

```bash
# 按项目习惯：wink.py esp32 <app> 或 idf.py
idf.py fullclean
idf.py build
```

**标准：**

1. 链接通过，产出固件 bin。
2. **有 DevKitC 时**（推荐，文件头历史烟测项）：
   - `pal_os_get_ms` 单调
   - WDT init/feed/reset reason（若 App 覆盖）
   - task 钉核（若 App 覆盖）
   - ringbuf push/pop（若 App 覆盖）  
   最小可接受：`devkitc_smoke` 或当前默认烟测 App 烧录跑通。
3. **无板时：** PR 注明「仅 build 验收」，并创建 follow-up 硬件烟测 checkbox。

### 8.4 矩阵门禁（必须）

- [x] 默认三组合 configure+build 成功
- [x] 至少一组非法组合 configure **失败**且错误信息含平台与 OSAL 类型

### 8.5 文档门禁（必须）

- [x] ADR-0041 Accepted
- [x] `03-directory-architecture.md` / `02-pal-platform-abstraction.md` 已反映 `osal/`
- [x] 本计划勾选完成并标 Completed

---

## 9. 文件变更总表（Phase A 预期）

| 路径 | 动作 |
|------|------|
| `wink-micro-os/osal/**` | 新增（迁入） |
| `wink-micro-os/cmake/wink_osal.cmake` | 新增 |
| `wink-micro-os/osal/CMakeLists.txt` | 新增 |
| `wink-micro-os/CMakeLists.txt` | 修改（include + 消费 OSAL 变量） |
| `wink-micro-os/targets/wasm/CMakeLists.txt` | 修改（移除 OSAL 源） |
| `wink-micro-os/targets/host/CMakeLists.txt` | 修改（消费 `WINK_OSAL_*`） |
| `wink-micro-os/targets/esp32/CMakeLists.txt` | 修改（双写对齐） |
| `wink-micro-os/targets/*/pal_osal_*.c` 等 | 删除（已 mv） |
| `docs/decisions/core/0041-hal-osal-directory-orthogonality.md` | 新增 |
| `docs/design/02-wink-micro-os/02-*.md`、`03-*.md` | 回写 |
| 脚本 / SDK pack / 测试注释 | 按 rg 命中修改 |

---

## 10. Phase B 预告（不实施，仅防范围蔓延）

单独开实施计划，建议标题：`YYYY-MM-DD-osal-portable-freertos-extraction-plan.md`。

预估切片：

1. 从 `pal_osal_freertos_esp32.c` 拆出 `osal/freertos/pal_osal_freertos.c`（标准 API）。
2. ESP 增量文件仅保留：`esp_timer` tick、WDT、reset/boot-count、SMP affinity、ROM delay。
3. 引入 `stm32` HAL target + `WINK_OSAL_TYPE=freertos` 合法化。
4. 消除 T-DEP-01（Wasm bridge 声明上移）。
5. 评估 `wink_sim_scheduler` 是否迁入 `osal/common`。

---

## 11. 执行检查清单（PR 用）

**Phase 0**

- [x] ADR-0041 已写并确认
- [x] 归属矩阵 / 合法组合无异议

**Phase 1–2**

- [x] `git mv` 完成且旧路径无实现文件
- [x] `wink_osal.cmake` + `osal/CMakeLists.txt` 为唯一 OSAL 源 SSOT
- [x] 无 target 内硬编码 `osal/host/...` 源路径列表
- [x] ESP32 idf SRCS 与 `ESP32_PAL_SOURCES` 双写一致
- [x] 非法组合 FATAL_ERROR 已测

**Phase 3–4**

- [x] T-DEP-01 已注释/入 ADR
- [x] rg 清扫完成（活路径）

**Phase 5**

- [x] Host `python wink-tools/wink.py test` 全绿
- [x] Wasm build + stub + 体积记录
- [x] ESP32 build（+ 有板烟测或 follow-up）
- [x] 矩阵门禁通过

**Phase 6**

- [x] 设计规范回写
- [x] 计划标 Completed；Phase B / T-DEP-01 记入遗留

---

## 12. 修订历史

| 日期 | 说明 |
|------|------|
| 2026-07-02 | 初版：目录迁移 + 各 target CMake 片段 |
| 2026-07-19 | 架构评审修订：Phase A/B 拆分；归属矩阵；合法组合；`osal` CMake SSOT；禁止 Host 硬编码；双写门禁；CACHE 陷阱；T-DEP-01；验收补行为烟测；ADR+规范回写；风险表扩容；明确不承诺 STM32 代码复用 |

