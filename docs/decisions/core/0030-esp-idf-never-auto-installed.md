# ADR-0030：Wink 工具永不自动安装 ESP-IDF

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-13 |
| 触发 | Toolchain Bootstrap 引入 `wink setup --install <cap>` 概念时，必须显式排除 ESP-IDF；见 [Toolchain Bootstrap Design Spec](../../superpowers/specs/2026-07-13-wink-toolchain-bootstrap-design.md) §15 |
| 影响范围 | `wink-micro-os/tools/toolchain/providers/idf.py`、`wink doctor` / `wink setup --install` 报告文案、`wink-micro-os/tools/preinstall.md` §3、SDK 消费者上手文档 |
| 决策者 | Wink Toolchain 维护者、SDK 分发负责人、ESP32 真机流水线负责人 |

---

## 背景（Context）

Phase A 的 `toolchain/` 门控机制会在 Phase B 引入 `wink setup --install <capability>`，允许把"体积可控、许可清晰、安装位置由 Wink 掌控"的依赖（Jinja2、CMake、WinLibs GCC、emsdk、Node）安装到用户指定的 `tools_home`。设计过程中必须一次性地把 **ESP-IDF 是否列入自动安装名单**这个问题固化下来——如果不明确定性，Phase B 的实施与消费者反馈会不断把它挑起来重议。

ESP-IDF 与上表其他依赖有本质差异：

1. **体积极大**：完整安装含 xtensa-esp-elf、riscv32-esp-elf、openocd、ninja、cmake 内嵌副本、Python venv、内置 esptool 等，本地占用 2 GB+；对元数据连接、CI 磁盘、离线开发都是显著负担。
2. **强绑定 Espressif 官方分发（EIM）**：Windows 上正确的安装/升级/驱动配置由 **Espressif IDF Manager (EIM)** 负责，EIM 维护 PowerShell profile（`C:\Espressif\tools\Microsoft.v*.PowerShell_profile.ps1`）、驱动包（CP210x / CH34x）、许可条款展示。Wink 若自建一套并行安装，几乎必然与 EIM 的注册表/环境变量/驱动状态互相踩踏。
3. **许可与合规**：IDF 内含部分组件（如 Xtensa 相关）由 Espressif 及其上游附许可条款；由第三方脚本"透明代跑"下载可能触发条款争议。让用户在 EIM 里显式点同意，是唯一无争议的路径。
4. **驱动依赖**：真机烧录需要 USB-Serial 驱动（Windows 上）；EIM 会一并处理。Wink 是一个 Python + PS 脚本工具，无权限也不应触碰驱动安装。
5. **多版本共存**：一台开发机常并存 v5.1 / v5.3 / v6.0 多个 IDF；用户会通过 EIM profile 切换。Wink 自动安装第 N 个副本只会加剧混乱。

关联记忆：本仓项目锁定 IDF v6.0.1，且激活必须走 EIM profile + PowerShell + `PYTHONUTF8=1`，不能走 `export.ps1` 也不能从 git-bash 调。这些前提再次印证——IDF 的安装/激活是**用户级、平台特化、状态多**的事务，任何"透明帮你装"都是错的抽象。

---

## 方案比选（Options）

### 方案 A：把 IDF 纳入 Phase B 自动安装（对齐 emsdk）

- **做法**：`wink setup --install idf` 内部封装"下载 EIM 静默安装 / 或克隆 esp-idf + 跑 `install.ps1`"。
- **优点**：口径最"一致"（所有依赖都能自动装）。
- **缺点**：与上述 5 条 IDF 特有约束全面冲突，且会把 EIM 的许可确认/驱动安装/多版本切换责任揽到 Wink 身上。任何一次错误安装都会污染用户已存在的 EIM 环境。
- **判定**：不推荐。是把复杂性揽到最难维护的位置。

### 方案 B：仅提供"检测 + 引导"，`install()` 抛 UnsupportedError；策略永久固定

- **做法**：`providers/idf.py.install()` 无条件抛 `UnsupportedError`；`wink doctor` / 失败报告在提到 IDF 时**必现**一句"ESP-IDF is never auto-installed by Wink"以及 EIM/preinstall.md 引导；`preinstall.md §3` 是唯一权威手册。用户可以用 `wink setup --set idf=<path> --set idf_tools=<path>` 让 Wink 认识非标准位置。
- **优点**：
  - 与 EIM 生态零冲突。
  - 许可、驱动、多版本切换责任仍在 Espressif/EIM，Wink 只做 detect + hint。
  - Design spec §15 已明列为 non-goal，方案 B 是把它固化为**永久策略**（不是 Phase A 限制）。
- **缺点**：Phase B 用户看到"其他依赖都能装、IDF 不行"会问"为什么"——但只要 doctor 报告与 preinstall.md 每次都把理由摆清，这个问题一次解释即可。
- **判定**：**推荐**。

### 方案 C：Phase A 检测 + 引导，Phase B 再评估

- **做法**：Phase A 与方案 B 相同；Phase B 视情况再讨论是否允许 IDF 自动安装。
- **缺点**：把决策拖到 Phase B，会在 Phase A 期间反复被消费者/AI 生成脚本挑起。Design 上"IDF 永不自动安装"是一个**能立即定性**的策略问题，没有必要延后。
- **判定**：不推荐（用永久策略代替"再评估"）。

---

## 决策结论（Decision）

采纳**方案 B：永久策略——Wink 工具链永不自动安装 ESP-IDF**。

规范化的落地约束：

1. `wink-micro-os/tools/toolchain/providers/idf.py`：
   - `detect()`：按 Toolchain Bootstrap Spec §7.2 探测顺序运行（`idf.py --version` on PATH → EIM PowerShell profile → `IDF_PATH` / `IDF_TOOLS_PATH` 环境变量 → 失败）。
   - `install(ctx)`：**无条件**抛出 `UnsupportedError`，附带定型 hint 文案：

     > `ESP-IDF is never auto-installed by Wink. Please install via Espressif IDF Manager (EIM) or see wink-micro-os/tools/preinstall.md §3.`

   - `hint()`：内容必须包含相同的"never auto-installed"表述 + EIM 引导 + `PYTHONUTF8=1` UTF-8 提示（避 Chinese Windows GBK 崩溃）。
2. `wink setup --install idf`（Phase B）：命中该 CLI 时必须复用 `providers/idf.py.install()`，同样抛 `UnsupportedError` 并展示上述文案；不允许绕开或"更友好地静默失败"。
3. `wink doctor` 与 `ensure_for(esp32)` 门控报告在提到 IDF 时**必现**"never auto-installed"文案 + `preinstall.md §3` 引用。可以在此处附加"如已装在非标准位置：`wink setup --set idf=<path> --set idf_tools=<path>`"提示。
4. `preinstall.md §3` 是 IDF 安装/激活的**单一事实来源**。Provider hint 文案、`doctor` 输出、消费者 README、CI 文档若与 §3 分叉，以 §3 为准；下一步 spec 生效前需检查 §3 已同步 v6.x 说明。
5. 关联行为：`build_esp32.ps1` 允许在缺 `IDF_PATH` 时回退去 dot-source EIM profile 或 `$IDF_PATH/export.ps1`（Spec §12.2），但**不允许**从 Wink 侧下载 IDF。

---

## 后果与约束（Consequences & Constraints）

- **对 SDK 消费者**：拿到 Binary/Source SDK 后想构建 ESP32 firmware，第一次必须走 EIM 或手动 IDF 安装（有一次性成本，但完全无 Wink 侧维护责任）。SDK README 与 preinstall.md 应突出这一步。
- **对 CI**：CI 镜像需要预装 IDF（EIM 或 `idf-install.ps1`）；Wink CI job 不能"运行 `wink setup --install idf` 兜底"。这是有意的设计——CI 环境的 IDF 状态由镜像 recipe 显式管理。
- **对 AI 代码生成**：Codegen 不得生成"跑 wink setup --install idf"的模板/脚本；生成 ESP32 上手指南时必须引用 `preinstall.md §3` 与 EIM。
- **对多版本切换**：EIM profile 支持多版本共存，Wink `providers/idf.py` 应先择本仓锁定的 v6.x major（Spec §7.2），并把选中的 profile 路径记入 detect 结果，让用户能看到"当前用的是哪个 profile"。
- **不倒退口子**：本 ADR Accepted 后，任何提议"支持 wink 自动安装 IDF"的变更必须先提新的 ADR 覆盖本条，不可通过 tech-design/implementation-plan 悄悄破坏。
- **例外通道**：如需在受控环境（例如 Wink 官方托管的云端编译服务）预置 IDF，属于**基础设施部署**范畴，可用 Docker/镜像脚本处理；这与"用户开发机上 wink 自动安装"是完全不同的场景，不受本 ADR 约束。

---

## 遵循与后续（Compliance & Follow-up）

- 本 ADR Accepted 后立即回写至 `docs/design/06-build-toolchain/01-toolchain-deployment.md`，作为 ESP-IDF 供给策略的活文档；`preinstall.md §3` 中的策略段落若与本 ADR/06-01 分叉，以本 ADR + 06-01 为准。
- 关联 ADR：与 [ADR-0029](../tools/0029-toolchain-command-front-gating.md) 协同——`ensure_for(esp32)` 门控失败时的报告必须触发本 ADR 定义的文案。
- 关联记忆：`esp-idf-install-state` / `esp-idf-build-from-git-bash` / `esp32-idf-v6-i2c-bus-reset-api` 等 IDF 相关记忆条目继续以 EIM + PowerShell + `PYTHONUTF8=1` 为准，本 ADR 只补齐"永不自动安装"这一条策略。
- Phase B `wink setup --install` 实现要点：其他能力（jinja2/cmake/gcc/emsdk/node）走各自 Provider 的 install 通道；IDF 单独走 `UnsupportedError` 分支且输出定型文案。
- 关联实施计划：`docs/implementation-plans/tools/2026-07-13-toolchain-bootstrap-phase-a-plan.md` 中的 IDF Provider 任务需要显式引用本 ADR 的 hint 文案作为验收条件。

---

*本 ADR 状态变更请在此记录：*
- 2026-07-13：Proposed（Toolchain Bootstrap 策略提议——把"永不自动安装 IDF"作为永久策略而非 Phase A 限制）

