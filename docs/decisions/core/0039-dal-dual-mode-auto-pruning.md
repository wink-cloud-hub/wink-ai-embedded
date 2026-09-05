# ADR-0039：DAL 双模式自动裁剪（JSON SSOT / 无 JSON 全开）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-18 |
| 触发 | `WINK_USE_*` 机制已齐，但 ESP32 硬编码基线 7 开、仅 MOTOR/ENCODER opt-in；Host/Binary SDK 行为分叉，长期维护与降门槛目标冲突 |
| 影响范围 | codegen `app_options.cmake`；`dal/CMakeLists.txt`；`core_sources.cmake`；`targets/esp32`；Binary SDK；Host 单 App 绑定 JSON 时的裁剪路径；活规范 [06-bal-layer.md §4.4](../../zh/design/02-wink-micro-os/06-bal-layer.md) |
| 决策者 | 项目 Owner |
| 关联 ADR | 低代码裁剪背景见 app-layer 设计与 P2 评审；BAL stub 契约见 [ADR-0037](0037-bal-domain-partition-and-closed-loop-motor.md) / [06-bal-layer.md](../../zh/design/02-wink-micro-os/06-bal-layer.md) |
| 关联活规范（SSOT） | [06-bal-layer.md §4.4](../../zh/design/02-wink-micro-os/06-bal-layer.md)；codegen 管线见 [03-ai-dsl-and-codegen-pipeline.md](../../zh/design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md) |
| 关联设计 | [tech-designs/2026-07-18-dal-dual-mode-auto-pruning.md](../../zh/tech-designs/core/2026-07-18-dal-dual-mode-auto-pruning.md) |
| 关联计划 | [implementation-plans/2026-07-18-dal-dual-mode-auto-pruning-plan.md](../../implementation-plans/core/2026-07-18-dal-dual-mode-auto-pruning-plan.md) |

---

## 背景（Context）

1. DAL 九驱动均已具备 `WINK_USE_*` + `WINK_UNAVAILABLE` 机制，但**自动应用不对称**：
   - ESP32：基线 LED/BUTTON/SERVO/SSD1306/ULTRASONIC/GPS/EEPROM **写死 ON**；仅 MOTOR/ENCODER 经 `app_options` opt-in。
   - Codegen：只对 JSON 出现的类型 `FORCE ON`，不写 OFF；且无 `motor`/`encoder` 插件。
   - Host：默认全 ON；`app_options` 生成但通常不驱动共享 `dal`。
   - Binary SDK：`foreach` 仅 7 项，缺 MOTOR/ENCODER。
2. 目标：长期维护单一心智模型——**有 JSON 只声明才编入；用户无需手写 `-DWINK_USE_*`**；同时不打断无 JSON 的 Arduino / Host 多 sample。

---

## 方案比选（Options）

| 方案 | 结论 |
|------|------|
| A. 仅固件（ESP32/wasm）按 JSON 裁；Host 永不裁 | ❌ 仍留特例；Binary SDK 易再分叉 |
| B. 无 JSON 时全 OFF / 或强制必须有 JSON | ❌ 打断 Arduino 与 Host 共享 `dal` |
| C. 各 target 各自改，不抽共享模块 | ❌ 重复今天的分叉债务 |
| **D. 共享 CMake 模块 + 双模式（有 JSON 按声明；无 JSON 九驱动全 ON）** | ✅ **采纳** |

---

## 决策结论（Decision）

1. **双模式契约**
   - **有** `wink-app.json`：仅声明到的驱动 `WINK_USE_*=ON`，其余 `OFF`。JSON 为驱动编入的 SSOT；用户无需 `-DWINK_USE_*`。
   - **无** JSON：九驱动 **全部 ON**；configure 期打 `WARNING`（提示镜像可能偏胖）。
2. **驱动全集（维护形态见 [ADR-0046](0046-dal-driver-registry-ssot.md)）**  
   运行时裁剪仍消费 `WINK_USE_*`；驱动**全集枚举**以 `tools/codegen/drivers/` registry 为 SSOT，不再在本 ADR 冻结九名列表。增删驱动改插件 + DAL 源，由 `list_drivers.py` 派生 CMake。
3. **实现形态**：共享 `wink-micro-os/cmake/wink_dal_drivers.cmake`（驱动表 + `apply_pruning` + `add_enabled_sources`）；ESP32 / Host `dal` / Binary SDK /（wasm 单 App 源码构建）统一消费。禁止再在 `targets/esp32` 硬编码基线 `WINK_USE_*=1`。
4. **Codegen**：`app_options.cmake` **显式写满**九宏的 ON/OFF；补齐 `motor`/`encoder` driver 插件（或等价 registry，优先独立 plugin）。
5. **Host**：未绑定单 App JSON → 走「无 JSON」= 全 ON（多 sample 共享 `dal`）。绑定 `WINK_APP_JSON` 时可裁。
6. **BAL control**：继续随 `WINK_USE_MOTOR`/`ENCODER` stub（既有契约）；不另开宏。
7. **行为变更（刻意）**：无 JSON 的 ESP32/Arduino 路径，从「基线 7 开 + motor/encoder 关」变为「九个全开」。BAL control 真实现可被链入（未调用则仍可 gc）；与「无 JSON 全开」一致，接受该跳变。
8. **明确不做（本 ADR）**：强制每个 App 必须有 JSON；按 Flash 字节数做强制门禁（仍依赖 `--gc-sections`）；BAL 层 source selection。

---

## 后果（Consequences）

| 正面 | 负面 / 缓解 |
|------|-------------|
| 单一规则；消灭 MOTOR 特例 | 漏写 JSON → 全开；缓解：WARNING |
| 固件/Host/Binary SDK 同源逻辑 | 需一次改 `core_sources` + esp32 + codegen golden |
| 降门槛：只维护 JSON | 无 JSON 镜像变大；正式 App 应带 JSON |

---

## 回写要求

Accepted 后必须更新：

- [x] [06-bal-layer.md §4.4](../../zh/design/02-wink-micro-os/06-bal-layer.md)（无 JSON 行为）— 2026-07-18 已回写
- [x] codegen `app_options` 写满九宏 ON/OFF（不再「只 FORCE ON」）— 实施完成 2026-07-18

