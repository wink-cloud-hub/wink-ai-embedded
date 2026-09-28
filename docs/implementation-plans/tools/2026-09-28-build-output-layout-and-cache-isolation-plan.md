# 构建输出归位与 CMake 缓存隔离实施计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260928-BUILD-LAYOUT |
| 契约版本 | `WINK_BUILD_LAYOUT_VERSION = 2` |
| 创建日期 | 2026-09-28 |
| 最近更新 | 2026-09-29（完成 CI 流水线迁移、Host 测试路径切换、IDE 编译数据库联动与 clean/ide CLI 命令收尾，全阶段彻底闭环） |
| 状态 | **已全部实施并完成闭环验收** |
| 优先级 | P1：防止错误复用缓存是正确性门禁；载体源码零污染与路径安全为底线要求；分阶段平滑交付 |
| 影响范围 | `wink-ai-embedded` 的 Host、Wasm、ESP-IDF 仿真、ESP32 固件构建入口与载体实现；`wink-ai/packages/wink-tools` 公开命令契约与元数据生成；IDE 语言服务支持；CI、`.gitignore` 与现行操作文档 |
| 关联技术设计 | 无；路径规则、元数据规范、载体无状态改造、兼容性和迁移设计并入本计划 |
| 关联设计规范 | [构建系统与工具链规范](../../zh/design/06-build-toolchain/README.md) |
| 关联决策 | [ADR-0002 双目标编译](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0079 完整 app ID 与默认 Wasm 路径](../../decisions/core/0079-micro-app-nested-app-discovery.md) |
| 现状证据 | [ESP-IDF 仿真基线审计](../../reviews/esp32/2026-09-28-esp-idf-sim-baseline-review.md)、[ESP-IDF 仿真加固计划](../esp32/2026-09-28-esp-idf-simulation-hardening-plan.md) |

---

## 1. 目标与边界

本计划系统性解决多目标、多架构、多 Profile 嵌入式开发中的核心架构与工程痛点：
1. **不同配置误用同一 CMake 缓存**：杜绝交叉编译工具链、SoC 目标、Sanitizer、优化级别混用同一 `CMakeCache.txt` 导致的静默符号污染与 ABI 破坏。
2. **源码树原位生成物污染（In-Tree Mutation）**：根治 ESP32 固件载体中在源码目录内原地生成 `app_sources.cmake` 与 `sdkconfig` 的隐患，实现载体源码目录绝对只读。
3. **临时构建树散落在 `build/` 外**：统一本仓所有构建入口，根目录 `build/` 仅作为纯容器，自身不得包含 `CMakeCache.txt`，所有 binary dir 均为隔离的叶子目录。
4. **IDE 编译数据库与生命周期断联**：建立统一的活动 `compile_commands.json` 隔离与发布机制，配套构建缓存生命周期自动回收（Pruning），避免磁盘空间无节制膨胀与 IDE 语言服务抖动。

**范围边界与演进策略：**
- **渐进交付**：拆分为“Phase 1 核心路径隔离与载体去污染”、“Phase 2 状态机门禁与并发安全”、“Phase 3 IDE 体验与生命周期运维”、“Phase 4 最终清理与验收”，确保基础编译链路闭环后再扩展辅助功能。
- **范围限制**：管理本仓工作区的临时 CMake/ESP-IDF 构建树、测试报告和工具生成的临时包。
- `unisim-assets/` 等由应用显式发布、供运行时消费的资产按现有发布契约处理，不作为可清理缓存。
- 厂商 SDK、`docs/vendors/`、私有 `.internals/` 不迁入 `build/`。
- 历史实施计划与已归档评审是时间点记录，不批量改写。

**安全纪律：**
- **审阅批准后才执行。** 实施前须记录两个仓库的 HEAD 与脏文件，严禁覆盖用户当前工作。
- 任何旧目录删除必须经过 Phase 4 最终清理门禁；获准实施本计划绝不等于允许立即删除现有 `build*`。

---

## 2. 已核对的现状与风险

| 现状类别 | 现状事实与证据 | 后果与风险评级 |
|---|---|---|
| **载体受控文件与源码原位污染** | `wink-firmware-carriers/esp32/sdkconfig` 是 **Git 受控跟踪文件**，包含大量与 `sdkconfig.defaults` 存在差异的配置项；生成脚本 `generate_app_sources.py` 硬编码写死写入载体 `main/app_sources.cmake`，且被载体 CMakeLists 与 CLI `esp32.py` **双重独立调用**。 | **极高（致命）**。若直接物理删除 `sdkconfig` 会使 Git 跟踪异常；若默认配置未回写会导致固件构建破坏；单改载体 CMake 无法阻断 CLI 的独立写入，导致原位污染持续存在。 |
| **根目录构建树散落与嵌套** | 根目录已有 `build/` 加 14 个 `build-*`／`build_*` 目录（含各类 SoC、sanitizer、profile 构建树）。`build/` 根本身是一份 Visual Studio CMake 构建树，其内部子目录又被用作 `test/`、`wasm/` 等独立构建。 | **高**。根工程重建或清理时容易递归冲垮子工程构建树，所有权互相交叉，干扰全局搜索与 IDE 索引。 |
| **超长 App ID 与契约自相矛盾** | 仓库中已存在大量深度应用（如 `vendor/esp_idfv61/gptimer_alarm` 达 31 字符）。若机械推行“>24 字符缩写”规则，将直接篡改 ADR-0079 强绑定的默认 Wasm 路径 `build/wasm/<app-id>`。 | **高**。破坏外部仿真引擎 UniSim 与前端调用契约；固定 75 字符预算在不同用户工作区绝对深度下缺乏动态安全性。 |
| **文件锁与清理生命周期冲突** | 若进程文件锁 `.build.lock` 放在构建叶子内部，当并发执行 `wink clean --prune` 或重建清理时，Windows 下打开的文件句柄会导致 `PermissionError (Access Denied)`；或清理直接连锁删除，导致构建失去保护。 | **高**。并发构建或清理时导致磁盘文件半残破损或抛出不可捕获的 I/O 异常。 |
| **IDE 链接抖动与单文件 Junction 限制** | Windows NTFS Junction **仅限目录，不支持单个文件**；若软链接指向“最近一次成功配置”，在交替构建 Host（MinGW/MSVC）与 ESP32（Xtensa GCC）时，`compile_commands.json` 架构宏反复震荡。 | **中**。单文件 Junction 报错，交替编译导致 VS Code 与 clangd 频繁重新全量索引，头文件报错红线满天飞。 |
| **双仓 CLI 协同与手动 CMake 阻断** | 工具仓（`wink-ai/packages/wink-tools`）与固件仓（`wink-ai-embedded`）为双仓架构；若在 CMakeLists 入口强校验 CLI 版本变量，将直接阻断文档承诺的原生 `cmake -B ...` 命令行构建。 | **高**。开发者脱离 CLI 无法进行底层 CMake 调试；双仓发版时间差会导致旧 CLI 或新载体构建瞬间阻断。 |

---

## 3. 拟采用的目录契约

路径相对于执行任务的工作区根；从 `wink-ai-embedded` 构建时基准目录即为 `wink-ai-embedded/build/`。独立 SDK 使用其自身工作区 `build/`，不反向污染只读 SDK。

```text
build/                                      # 容器：严格禁止存在 CMakeCache.txt
├── ide/                                    # IDE 语言服务专用隔离目录
│   ├── host/compile_commands.json          # 稳定的 Host 编译数据库镜像
│   └── esp32/compile_commands.json         # 稳定的 ESP32 编译数据库镜像
├── compile_commands.json                   # 默认活动编译数据库链接（默认稳定绑定 Host）
├── wasm/<完整-app-id>/                     # 主应用默认 Wasm 配置（严格保持 ADR-0079 契约，零截断）
├── host/<完整-app-id>/                     # 主应用默认 Host 配置
├── variants/<host|wasm>/[<来源键>/]<app-id>/<配置键>/
├── tests/<host|wasm>/<测试套件>/<配置键>/
├── firmware/esp32/[<来源键>/]<app-id>/<SoC>/<配置键>/
└── artifacts/<任务或场景>/<配置键>/          # 报告、日志与临时包，不作为 CMake binary dir
```

### 3.1 路径身份、公开契约与动态长度预算

1. **公开契约路径（Public Contract Paths）**：
   - 默认 Wasm 路径：`build/wasm/<完整-app-id>/`（遵循 ADR-0079，如 `build/wasm/vendor/esp_idfv61/gptimer_alarm`）。
   - **严格禁止对公开契约路径应用任何缩写、前缀截断或短 Hash 压缩**，保证外部 UniSim 引擎、自动化测试和前端环境消费的绝对兼容。
2. **内部变体与固件路径（Internal Variant Paths）**：
   - 包含 `build/variants/...` 与 `build/firmware/esp32/...`。
   - 默认主工作区（`wink-micro-app/`）应用省略 `<来源键>` 段；外部应用使用其规范绝对路径的 6 字符 SHA-256 短摘要（如 `ext-a1b2c3`）。
   - `<配置键>` 格式规范：`<短前缀>-<8~10位短Hex>`（如 `idf-s3-dbg-3f8a9b2c`），完整配置特征保留在旁路元数据中。
3. **动态 Windows 路径深度预算（Dynamic Path Budget）**：
   - 废除固定 75 字符静态限制，改用构建前动态安全探测：
     $$\text{当前工作区绝对路径长度} + \text{叶子相对路径长度} + \text{最深构建对象裕度 (预留 130 字符)} \le 250$$
   - 仅在内部变体路径中，若探测到接近 Windows `MAX_PATH`（260 字符），自动对内部变体路径中的中间 App ID 段进行安全的平铺缩写压缩；若工作区过深无法缓解，且系统未开启 `LongPathsEnabled`，提前给出阻断告警并提供开启指引。

### 3.2 载体源码树零污染与受控文件治理

针对 ESP32 固件载体（`wink-firmware-carriers/esp32/`），实施绝对无状态化与受控文件流转：
1. **生成脚本外部化改造（向后兼容）**：
   - 改造 `wink-ai/packages/wink-tools/tools/esp32/generate_app_sources.py`，新增 `--output`（或 `-o`）参数。
   - 兼容策略：传入 `--output` 时将结果写入指定路径；未传入时降级写回 `<esp32_firmware_dir>/main/app_sources.cmake` 并打印 Deprecation 警告。
   - 路径生成修正：生成文件中引用的源文件路径，改用基于工作区根目录的统一路径或绝对路径，不再依赖基于 `${CMAKE_CURRENT_LIST_DIR}` 的层级上溯，防止挪入 `build/` 后层级失效。
2. **载体与 CLI 双调用闭环适配**：
   - 重构 `wink-firmware-carriers/esp32/main/CMakeLists.txt`：调用生成脚本时显式传入 `--output ${CMAKE_CURRENT_BINARY_DIR}/app_sources.cmake` 并执行 `include`。
   - 重构 `tools/cli/commands/esp32.py`：CLI 在调用 `build.py` 前若显式预生成，同样传参输出到 `${BUILD_DIR}/main/app_sources.cmake`，彻底杜绝 CLI 原地写回源码树。
3. **受控 `sdkconfig` 安全迁移与固化**：
   - **严禁直接从工作区物理删除**：先运行配置审计，提取现有 `sdkconfig` 中相对 `sdkconfig.defaults` 独有的有效选项（如调试等级、串口波特率、任务栈深度等），合并固化到 `sdkconfig.defaults`。
   - `wink esp32` 启动 `idf.py` 时强制注入 `-DSDKCONFIG=${BUILD_DIR}/sdkconfig`。
   - 验证默认生成行为一致后，通过标准的 Git 变更流程将旧的受控 `sdkconfig` 移除或重构为纯只读模板。
4. **载体零污染验证准则**：
   - 构建前后执行 `git status --porcelain wink-firmware-carriers/esp32`，必须满足：**构建前后的输出完全一致，构建过程未新增未跟踪文件（untracked files），未修改受控文件**。

### 3.3 旁路元数据规范与半成品状态机

每个受管理的 CMake 叶子目录必须在配置初期建立 `.wink-build-meta.json`：

```json
{
  "schema_version": 1,
  "created_at": "2026-09-28T20:45:00Z",
  "app_id": "vendor/esp_idfv61/gptimer_alarm",
  "target": "esp32",
  "soc": "esp32s3",
  "profile": "debug",
  "generator": "Ninja",
  "compiler_fingerprint": "xtensa-esp32s3-elf-gcc-14.2.0",
  "config_full_hash": "3f8a9b2c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a",
  "status": "ready"
}
```

* **状态机构建保护与 `--clean` 穿透保证**：
  - 生命周期：`configuring` $\rightarrow$ `building` $\rightarrow$ `ready`（产物完整） $\rightarrow$ `failed`。
  - **复用门禁**：常规构建时，若检测到元数据缺失、哈希不匹配或 `status != "ready"`，阻断构建并提示清理。
  - **核心容错：`--clean` 具有最高优先权**。当用户显式指定 `--clean` 或 `--rebuild` 时，工具在前置阶段直接安全抹除目标叶子目录，**绝不在清理入口前触发状态机校验拦截**，彻底防范损坏目录导致“既不能构建也不能 clean”的自锁现象。

### 3.4 并发安全与锁生命周期协议

1. **锁位置与生命周期分离**：
   - 构建进程与清理进程必须遵循统一的锁互斥协议。
   - 锁机制采用外置句柄或安全锁文件（例如登记在 `build/.locks/<leaf-path-hash>.lock`，或构建前尝试独占持有）。
2. **清理时序防护（Clean-Lock Protocol）**：
   - 执行 `wink clean --prune` 或叶子清理前，清理进程必须先尝试获取该叶子的独占互斥锁。
   - 若锁被持有（说明有进程正在构建），清理进程安全跳过该目录（`Skipped busy target`），严禁强行删除导致构建进程抛出 Windows `Access Denied` 或半残破损。
   - 构建进程退出（无论成功、失败或异常中断）必须在 `finally` 块中确保释放锁句柄。

### 3.5 IDE 语言服务稳态隔离与原生 CMake 放行

1. **多架构隔离与活动编译数据库**：
   - 在 `build/ide/` 下分别维护 `host/compile_commands.json` 与 `esp32/compile_commands.json` 的稳定镜像。
   - 根目录 `build/compile_commands.json` 默认稳定绑定通用 Host 构建数据库；仅当开发者通过显式命令 `wink ide select <host|esp32>` 时才切换根链接。
   - 在 Windows 环境下放弃单文件 Junction，改用支持变更校验的原子文件复制（Atomic Copy-if-changed），当软链接权限受限时平滑回退，杜绝 IDE 报错。
2. **原生 CMake 与调试兼容**：
   - 契约版本 `WINK_BUILD_LAYOUT_VERSION = 2` **仅用于 CLI 自身握手与工具脚本**，严禁在 C/C++ `CMakeLists.txt` 中作为必填强校验条件。
   - 开发者或 IDE CMake Tools 插件在终端直接执行原生 `cmake -B ...` 时，CMakeLists 预设安全缺省放行，保证文档承诺的手动 CMake 与底层断点调试开箱即用。
   - 辅助指令 `wink path <app-id>` 输出当前应用对应构建叶子的绝对路径；在 App 变体目录下自动维护 `.latest` 符号快捷方式便于终端直达。

### 3.6 缓存生命周期管理与磁盘空间回收

- `wink clean --prune [--older-than 7d]`：扫描 `build/variants/` 与 `build/tests/`，在安全获锁的前提下移除超过指定天数未访问的陈旧叶子目录；
- `wink clean --target <host|wasm|esp32>`：按平台维度批量清除衍生配置；
- `wink cache status`：结构化输出 `build/` 容器内各平台、各 App、各测试套件的实际磁盘占用明细。

---

## 4. 旧路径处置映射

| 当前路径类别 | 新建时的归属 | 处置策略 |
|---|---|---|
| 根 `build/` CMake 树 | 不再创建；后续 CMake 树进入相应叶子 | 最后退役。不可直接删除整个 `build/`，其中包含独立目录和有效测试报告。 |
| `build-host`、`build_host` | `build/tests/host/general/<配置键>/` | 对照源目录与生成器重新配置；两者不是可直接合并的缓存。 |
| `build_esp_idf`、`build_esp_hardening_{lite,standard,pro,esp32c3,esp32c6,esp32s3,msvc}` | `build/tests/host/esp-idf/<配置键>/` | 根据 cache 的实际 target、SoC、profile、生成器建键；目录名不能代替 cache 事实。 |
| `build_esp_idf_wasm_ctor` | `build/tests/wasm/esp-idf-runtime-ctor/<配置键>/` | 新目录重配并验证，保留旧测试证据直到新链路全绿。 |
| `build_esp_hardening_asan*`、`build_esp_hardening_invalid` | 仅在具备相应工具链并需要复验时建 `tests/.../<配置键>` | 既有失败配置只登记归档，不机械重建。 |
| `build/test*`、`build/wasm-unisim-smoke` | `build/tests/host|wasm/<套件>/<配置键>/` | 避免新的独立 CMake 根落在旧 `build/test` 内部。 |
| 现有 `build/wasm/<app-id>` | 保留默认路径语义 | 完整 App ID 与配置验证后重建；保持 ADR-0079 绝对兼容。 |
| 覆盖率 `build_cov`、CI `build_stress` 等 | `build/tests/...` 与 `build/artifacts/...` | 脚本中的配置、CTest、覆盖率报告和上传路径必须同步切换。 |
| 载体受控 `sdkconfig` 与 `app_sources.cmake` | 外部化至 `build/firmware/esp32/...` | 先完成配置向 `sdkconfig.defaults` 的合并审计，再通过 Git 受控变更；源码树恢复干净。 |

---

## 5. 分阶段实施任务与门禁

### T0：冻结基线与受控文件审计（只读准备阶段）

- [x] 记录两个仓的 HEAD、工作区修改状态、现有 `CMakeCache.txt` 关键属性、目录大小及测试结果。
- [x] **审计载体受控文件**：对比 `wink-firmware-carriers/esp32/sdkconfig` 与 `sdkconfig.defaults`，形成完整配置差异清单，提取必须保留的配置项。
- [x] 审计 `generate_app_sources.py` 的现有调用方与路径解析依赖。
- [x] 全局冻结在仓库根目录执行 `cmake -B build` 与 `cmake --build build`，防止产生新的嵌套树。
- **退出条件**：基线状态完整归档；`sdkconfig` 差异合并提案审阅通过；路径契约与双仓兼容方案明确。

---

### Phase 1：核心路径隔离与载体去污染（P0：基础编译正确性）

- [x] **T1.1 改造生成脚本 `generate_app_sources.py`**（工具仓）：
  - 增加 `--output` 参数；未传入时降级并告警；
  - 修正生成路径逻辑：使用相对于工作区根目录的绝对或规范化变量，消除 `${CMAKE_CURRENT_LIST_DIR}` 跨目录失效问题；
  - 更新生成脚本对应的单元测试（`test_esp32_generate_app_sources.py`）。
- [x] **T1.2 改造载体 CMakeLists 与 CLI 驱动**（载体仓＋工具仓）：
  - 改造载体 `main/CMakeLists.txt`：显式传参输出至 `${CMAKE_CURRENT_BINARY_DIR}/app_sources.cmake` 并包含；
  - 改造 `tools/cli/commands/esp32.py`：CLI 预生成同样对齐输出至 `${BUILD_DIR}/main/app_sources.cmake`；
  - 改造 `wink esp32` 驱动逻辑：强制注入 `-DSDKCONFIG=${BUILD_DIR}/sdkconfig`；
  - 将 T0 审计得出的必要配置合并固化至 `sdkconfig.defaults`。
- [x] **T1.3 核心构建路径归位与零污染验证**：
  - 默认 Wasm 路径严格保留 `build/wasm/<完整-app-id>`，验证长 App ID 逐字节可用；
  - Host 与 ESP-IDF 仿真测试构建切入 `build/tests/...`；
  - 交替构建两个不同 App（如 `devkitc_smoke` 与长 ID 应用）及不同 SoC；
  - 验证构建前后 `git status --porcelain wink-firmware-carriers/esp32` 保持绝对零新增、零修改。
- **退出条件**：载体原位生成彻底消除；多应用交替构建互不干扰；核心编译与单测全量通过。

---

### Phase 2：状态机门禁与并发安全（P1：构建健壮性与防污染）

- [x] **T2.1 元数据状态机与哈希门禁**（工具仓）：
  - 实现 `.wink-build-meta.json` 状态机记录（`configuring` $\rightarrow$ `building` $\rightarrow$ `ready`）；
  - 实现配置指纹与 Hash 校验；损坏与半成品目录精准拦截并引导清理；
  - **确保 `--clean` / `--rebuild` 具备最高穿透权限**，绕过损坏校验直接安全清空叶子。
- [x] **T2.2 并发锁与清理时序安全**（工具仓）：
  - 实现外置进程文件锁协议（`build/.locks/<hash>.lock`）；
  - 实现清理安全互斥：清理前获取排他锁，锁被占用时跳过忙碌叶子，避免 Windows 句柄冲突与清理破坏。
- [x] **T2.3 异常与冲突测试用例**：
  - 编写单元测试覆盖：中断半成品自愈测试、`--clean` 穿透测试、并发构建锁竞争测试。
- **退出条件**：并发构建安全互斥；损坏或半成品目录无法被错误复用，且通过 `--clean` 可 100% 自愈。

---

### Phase 3：IDE 体验、CI 强化与生命周期运维（P2：开发体验与自动化）

- [x] **T3.1 IDE 编译数据库稳态发布**（工具仓＋本仓）：
  - 在 `build/ide/{host,esp32}/` 发布独立的编译数据库；
  - 根目录 `build/compile_commands.json` 默认稳定绑定 Host；
  - Windows 环境下使用带 Hash 校验的原子更新（Atomic Copy-if-changed），规避单文件 Junction 限制。
- [x] **T3.2 调试辅助与路径解析**（工具仓）：
  - 提供 `wink path <app-id>` 调试命令并在叶子上层建立 `.latest` 快捷链接；
  - 确保原生手动 CMake 命令不受契约版本变量误伤阻断。
- [x] **T3.3 缓存生命周期清理指令**（工具仓）：
  - 实现并测试 `wink clean --prune [--older-than 7d]`、`wink clean --target` 与 `wink cache status`。
- [x] **T3.4 CI 门禁强化与报告归位**（本仓）：
  - 更新 CI Workflow 与根目录 CMakeLists，增加纯容器防嵌套守卫；
  - 在 CI 中加入静态扫描：根目录除容器 `build/` 外严禁新增任何 `build*`。
- [x] **T3.5 文档与规范回写**（本仓）：
  - 更新设计规范核心文件 [docs/zh/design/06-build-toolchain/README.md](file:///D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/zh/design/06-build-toolchain/README.md)，修正旧路径示例；
  - 修正根 README、测试指南与 CLI 帮助文档。
- **退出条件**：IDE 智能提示稳定不报错；CI 门禁全绿；现行公开文档全量替换为新路径契约。

---

### Phase 4：最终清理与受控收网（P3：历史产物彻底清扫，最后执行）

- [x] 确认新链路在 Host、Wasm、ESP-IDF 仿真与物理真机固件全量通过。
- [x] 备份旧目录中有价值的历史日志至 `build/artifacts/historical-evidence/`。
- [x] 正式退役根 `build/` 的历史 VS CMake 树，转换为纯容器。
- [x] **受控文件正式流转**：通过标准 Git 变更安全移除或重构载体下的历史 `sdkconfig`。
- [x] 逐项安全物理移除散落在根目录下的历史 `build-*` 目录。
- [x] **收网 `.gitignore`**：移除历史通配规则 `build-*/` 与 `build_*/`，仅保留 `build/`。
- **退出条件**：根目录除容器 `build/` 外无任何 `build*`；工作区 Git 状态绝对干净；`.gitignore` 漏洞关闭。

---

## 6. 验证矩阵与完成标准

| 检查维度 | 验证方法与最小验收准则 |
|---|---|
| **路径确定性** | 同一配置反复执行命中同一路径；切换 App、SoC、Profile 产生隔离路径；默认工作区路径不含冗余来源键。 |
| **Wasm 契约完整性** | 默认 `build/wasm/<app-id>` 保持完整 App ID（包含 >24 字符的深层应用），正常输出 `wink_simulator.js` 与 `.wasm`；UniSim 引擎无缝消费。 |
| **载体源码零污染** | 交替构建不同 App 与 SoC 后，运行 `git status --porcelain wink-firmware-carriers/esp32`，构建前后比对完全一致，零脏文件产生。 |
| **Windows 路径安全** | 在 Windows 深度工作区执行构建，动态路径深度检测有效预留 130 字符裕度，无 `MAX_PATH` 截断报错。 |
| **缓存与半成品防护** | 损坏或中断目录被自动拒绝复用；执行 `--clean` 能无障碍穿透并成功自愈。 |
| **并发与清理安全** | 并发构建时安全互斥拦截；清理执行时安全跳过正在编译的叶子，无 Windows `Access Denied` 抛错。 |
| **IDE 稳态体验** | `build/compile_commands.json` 稳定可用；交替构建 Host 与 ESP32 时 VS Code 不发生索引架构宏抖动。 |
| **手动构建兼容性** | 在不使用 CLI 的情况下，在终端直接运行 `cmake -B ...` 能直接成功配置与编译，无契约版本报错。 |
| **规范与收敛** | 现行文档与 CI 均使用新路径；`.gitignore` 仅保留 `build/`；运行 `python .github/scripts/check_license_map.py` 验证合规。 |

---

## 7. 风险、回滚与停止条件

| 潜在风险 | 应对与防范对策 |
|---|---|
| **双仓 CLI 协同脱节** | 生成脚本改造保持向后兼容（缺失 `--output` 时降级并告警）；明确双仓合入顺序，先发工具再改载体。 |
| **`sdkconfig` 丢失导致固件异常** | T0 阶段比对提取全部差异项并固化至 `sdkconfig.defaults`，真机/仿真固件验证一致后才通过 Git 办理变更。 |
| **半成品状态机自锁** | 明确 `--clean` 具有最高优先权，前置绕过状态校验直接执行目录重置。 |
| **Windows 锁句柄占用导致清理崩溃** | 锁采用外置安全登记文件；清理进程严格实施 Clean-Lock 协议，遇忙跳过而非强行删除。 |
| **回滚策略** | 严格分阶段（Phase 1~4）推进，每个阶段具备独立的 Git 提交节点；出现工具链阻断时回退当前阶段代码，严禁混用新旧 CMake 缓存。 |

---
