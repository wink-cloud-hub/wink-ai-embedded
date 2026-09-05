# ADR-0046：DAL 驱动全集 SSOT = codegen `drivers/` registry

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-27 |
| 触发 | 新增外设需同步 4–7 处驱动列表；与 [ADR-0039](0039-dal-dual-mode-auto-pruning.md)「一处驱动表」目标仍有偏差；见 [tech-design](../../zh/tech-designs/tools/2026-07-27-peripheral-onboarding-optimization-design.md) |
| 影响范围 | `tools/codegen/drivers/`；`list_drivers.py`；`dal/CMakeLists.txt`；`cmake/wink_dal_drivers.cmake`；Binary SDK；`app_codegen.py`；`wink.py new-dal`；`wink lint --pack drivers` |
| 决策者 | 项目 Owner |
| 关联 ADR | [ADR-0039](0039-dal-dual-mode-auto-pruning.md)（双模裁剪契约不变；本 ADR 只改驱动全集的维护形态）；ADR-0001/0002/0004/0034/0043 |
| 关联设计 | [tech-designs/2026-07-27-peripheral-onboarding-optimization-design.md](../../zh/tech-designs/tools/2026-07-27-peripheral-onboarding-optimization-design.md) |
| 关联计划 | [implementation-plans/2026-07-27-peripheral-onboarding-optimization-plan.md](../../implementation-plans/tools/2026-07-27-peripheral-onboarding-optimization-plan.md) |
| 关联活规范（SSOT） | [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)；[03-ai-dsl-and-codegen-pipeline.md](../../zh/design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md)；[`adding-peripheral.md`](../../../wink-micro-os/docs/dal-development-guide/adding-peripheral.md)（旧路径 stub：[`adding-peripheral-guide.md`](../../../wink-micro-os/docs/adding-peripheral-guide.md)） |

---

## 背景（Context）

1. ADR-0039 已统一「有 JSON 按声明 / 无 JSON 全开」双模裁剪，但驱动**全集枚举**仍硬编码在：
   - `dal/CMakeLists.txt`（`option` + `_wink_dal_enable`）
   - `cmake/wink_dal_drivers.cmake`（全开列表 + `add_enabled_sources`）
   - `tools/binary_sdk_cmake/CMakeLists.txt`（defs foreach）
   - `app_codegen.py` 的 `ALL_WINK_USE_OPTIONS`
2. Python 侧 `drivers/*.py` **已自动注册**；CMake 无法复用，漏改导致 target 间「能编/不能编」分叉。
3. Binary SDK **只打 compile definitions、不编附加 `.c`**，与源码构建的多 TU（如 SSD1306 字体）需求不同，不能共用含 `target_sources` 的单一片段。

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 继续文档约束多处手改 | ❌ 漏改风险不降 |
| B. 独立 `dal_drivers.json` | ❌ 与插件双写 |
| C. CMake `GLOB` `.c` | ❌ 丢元数据与子选项 |
| **D. `drivers/` registry 为唯一 SSOT；`list_drivers.py` 生成数据型 CMake；`--mode=source\|defs` 分流** | ✅ **采纳** |

## 决策结论（Decision）

1. **驱动全集 SSOT** = `wink-micro-os/tools/codegen/drivers/*.py`（`DriverBase` registry）。增删标准驱动**不再**改本 ADR 正文枚举列表，也不再手改上述四处硬编码。
2. **`DriverBase` 元数据**：必填 `category`（`DriverCategory` 枚举）；可选 `source_stem`；多 TU 用 `extra_cmake_defs` + `extra_cmake_sources`（禁止单一无分流的 `extra_cmake`）。
3. **`list_drivers.py`**：
   - `--cmake --mode=source`：数据表 + defs + sources extras（Host / `wink_dal_drivers`）
   - `--cmake --mode=defs`：数据表 + 仅 defs extras（Binary SDK）
   - `--json` / `--check`；`--check` 挂 `wink lint --pack drivers`
4. **生成物只含数据**：`WINK_KNOWN_DRIVERS`、每驱动 CATEGORY/STEM/REL_SRC、`option()`、按 mode 的 extra 块。**禁止**生成器调用 `_wink_dal_enable*`；各入口 `foreach` 自调既有 helper。
5. **双模裁剪契约不变**（ADR-0039）：有 JSON 按声明；无 JSON 全开 + WARNING。全开列表改为迭代 `WINK_KNOWN_DRIVERS`。
6. **脚手架**：`wink.py new-dal` 生成 DAL `.h/.c` + 插件骨架；不改 CMake 列表；不生成 `extra_cmake_*`；不碰 BAL。
7. **明确不做**：跨仓强制 CI；强制 Role/smoke app；独立 JSON manifest；本 ADR 不扩展仿真观测面。

## 后果与约束（Consequences）

| 正面 | 负面 / 缓解 |
|------|-------------|
| 标准外设新增 ≈ 3 文件 + unisim | configure 依赖 Python（与 codegen 一致） |
| SSD1306 等不再 Host/共享双写 | 插件作者须正确拆 defs/sources |
| Binary 不会误 `target_sources` | 实施需行为等价 golden，非字节相同 |

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须：

- [x] 回写 [01-dal-device-abstraction.md](../../zh/design/02-wink-micro-os/01-dal-device-abstraction.md)（驱动发现 / 新增路径）— 2026-07-27
- [x] 回写 [03-ai-dsl-and-codegen-pipeline.md](../../zh/design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md)（registry SSOT）— 2026-07-27
- [x] 更新 ADR-0039 交叉引用（驱动全集维护形态指向本 ADR）— 2026-07-27
- [x] 落地 `adding-peripheral-guide.md`（见实施计划 P2）— 2026-07-27；2026-07-28 迁入 `dal-development-guide/adding-peripheral.md`

---

*本 ADR 状态变更请在此记录：*
- 2026-07-27：Proposed（配合 peripheral-onboarding 技术设计与实施计划）
- 2026-07-27：Accepted（P0–P2 落地：list_drivers / CMake 迁移 / new-dal / lint pack / 指南与活规范回写）
- 2026-07-29：**SSOT 路径由 [ADR-0051](../tools/0051-scannable-codegen-extension-roots.md) 演进** — registry / `list_drivers` / 双模裁剪机制保留；可写描述默认迁至可扫描扩展根（`wink-micro-os/codegen/` 等）；迁移期可双读 `drivers/*.py`，退出标准见 ADR-0051 tech-design。

