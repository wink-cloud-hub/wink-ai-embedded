# ADR-0041：HAL / OSAL 目录正交与合法组合矩阵

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-19 |
| 触发 | `targets/<plat>/` 同时承载 HAL 与 OSAL，阻碍多 MCU / 多 OS 演进；实施计划架构评审要求先冻结边界再搬家 |
| 影响范围 | `wink-micro-os/targets/` 布局；新建 `wink-micro-os/osal/`；顶层与 ESP-IDF CMake 装配；活规范 [02-pal-platform-abstraction.md](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md)、[03-directory-architecture.md](../../zh/design/02-wink-micro-os/03-directory-architecture.md) |
| 决策者 | 项目 Owner（已确认 Accepted） |
| 关联 ADR | [ADR-0002](../unisim/0002-dual-target-compilation.md)（双 target 同源）；[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（静态分发，本决策不引入运行期 ops）；[ADR-0013](../unisim/0013-sim-cooperative-scheduler.md)（仿真协作调度，OSAL/fiber 归属） |
| 关联活规范（SSOT，Accepted 后回写） | [02-pal-platform-abstraction.md](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md)；[03-directory-architecture.md](../../zh/design/02-wink-micro-os/03-directory-architecture.md) |
| 关联计划 | [implementation-plans/2026-07-02-hal-osal-decoupling-refactor-plan.md](../../implementation-plans/core/2026-07-02-hal-osal-decoupling-refactor-plan.md) |

---

## 背景（Context）

1. PAL 在契约层早已分为 **HAL**（`pal_hal.h`）与 **OSAL**（`pal_osal.h`），但实现仍全部落在 `targets/<plat>/`：
   - `targets/esp32/pal_osal_esp32.c` 与 GPIO/PWM/I2C 等同目录；
   - `targets/wasm|host` 中 OSAL、fiber（`sim_ctx_*`）与虚拟外设 HAL 混放。
2. 后果：
   - 「换芯片」与「换 OS」被目录绑死，易演化出 `stm32_bare` / `stm32_freertos` 式目录爆炸；
   - 新人/AI 生成代码时缺乏清晰落点；
   - 构建缺少显式 `WINK_OSAL_TYPE` 与非法组合门禁。
3. 同时必须诚实面对：当前 `pal_osal_esp32.c` **深度绑定 ESP-IDF**（`esp_timer`、`esp_task_wdt`、`RTC_NOINIT`、`xTaskCreatePinnedToCore`、`esp_rom_delay_us` 等），**不是**可直接给 STM32 复用的「纯 FreeRTOS 适配」。仅搬家**不会**自动获得跨 MCU OSAL 复用。

因此需要一次**有边界的架构决策**：先正交目录与装配骨架，再另开波次抽取 portable FreeRTOS 核。

---

## 方案比选（Options）

| 方案 | 做法 | 优点 | 缺点 | 结论 |
|------|------|------|------|------|
| A. 维持现状 | OSAL 继续留在 `targets/<plat>/` | 零迁移成本 | 目录笛卡尔积；语义混乱 | ❌ |
| B. 一次抽到 portable `osal/freertos/` + 各芯片增量 | 搬家同时拆 ESP-IDF 特化 | 直接兑现 STM32 复用 | 范围大、回归面大、阻塞当前净化 | ❌ 本期 |
| C. 按「平台×OS」复制目录（如 `targets/esp32_freertos`） | 不改抽象 | 短期直观 | 重复实现；与 DRY / 正交目标相反 | ❌ |
| **D. 目录正交 + Phase A 默认强绑定** | `targets/`=HAL 等；`osal/`=OS 变体；CMake `TARGET_PLATFORM × WINK_OSAL_TYPE`；Phase A 仅 1:1 默认组合；portable FreeRTOS 属 Phase B | 净化布局、建立骨架、风险可控 | Phase A 尚不能跨 MCU 复用 OSAL 代码 | ✅ **采纳** |

---

## 决策结论（Decision）

### D1. 两列实现布局

| 列 | 目录 | 职责 |
|----|------|------|
| 硬件 / 平台外设 | `wink-micro-os/targets/<plat>/` | HAL、irq、storage、log、entry、芯片私有扩展；**不再**放置 `pal_osal_*.c` / `sim_ctx_*.c` |
| 操作系统 / 运行环境 | `wink-micro-os/osal/<variant>/` | `pal_os_*` 实现、仿真 fiber 上下文、OSAL 共享 ringbuf（按变体需要） |

契约仍居 `pal/include/`（INTERFACE，无 `.c`）。本决策**不**改变 `pal_osal.h` / `pal_hal.h` 语义。

### D2. 分阶段交付（强制）

| 阶段 | 交付 | 明确不交付 |
|------|------|------------|
| **Phase A**（本 ADR + 关联实施计划） | 物理迁入 `osal/`；`WINK_OSAL_TYPE`；合法组合矩阵；`osal/CMakeLists.txt` + `cmake/wink_osal.cmake` 为 OSAL 源 SSOT；三平台零回归 | STM32 target；portable FreeRTOS 核；跨 MCU 复用同一份 OSAL `.c` |
| **Phase B**（另开 ADR/计划） | `osal/freertos/` 可移植核 + `osal/freertos_esp32/` 缩为增量；合法化 `stm32 × freertos`；消除下文 T-DEP-01 | — |

**对外表述约束：** 不得将 Phase A 描述为「已实现 STM32 与 ESP32 共用 FreeRTOS OSAL」。Phase A 主收益是**目录净化与正交装配骨架**。

### D3. Phase A 归属冻结

| 模块 | 归属 |
|------|------|
| `pal_os_*` / WDT / reset reason / abnormal boot-count | `osal/`（随现有 `pal_osal_*.c` 整文件迁移；WDT 等与芯片 RTC/ROM 的耦合 Phase A 接受，Phase B 再拆钩子） |
| `sim_ctx_*.c` | `osal/wasm` 或 `osal/host` |
| `pal_osal_ringbuf.c` | `osal/common/`（仅链入需要该实现的变体，避免与 ESP-IDF ringbuf 重复符号） |
| `wink_sim_scheduler.*` / `wink_sim_physical.*` | **仍留** `targets/common/`（Phase A 不搬） |
| `pal_hal_*` / `pal_rmt_*` / `pal_irq_*` / `pal_log_*` / `pal_storage_*` / `*_entry.c` | `targets/<plat>/` |
| `wasm_bridge.h` / `pal_wasm_internal.h` | **仍居** `targets/wasm/` |

`pal_irq_*` 视为「既非纯总线 HAL、亦非 OS 服务」的平台能力，**本 ADR 不**单独抽 `irq/` 目录；文档须注明其不属于 OSAL。

### D4. 构建维度与合法组合（Phase A）

- 维度：`TARGET_PLATFORM`（硬件）× `WINK_OSAL_TYPE`（OS/运行环境）。
- Phase A **默认强绑定**，且**不允许**覆盖为其它变体：

| `TARGET_PLATFORM` | 默认且唯一允许的 `WINK_OSAL_TYPE` |
|-------------------|----------------------------------|
| `wasm` | `wasm` |
| `host` | `host` |
| `esp32` | `freertos_esp32` |

- `osal/baremetal/` 可入树，但 **不**挂到上述三平台默认组合。
- 非法组合必须在 configure 期 `message(FATAL_ERROR)`，禁止静默改绑。
- `WINK_OSAL_TYPE` 的 CMake CACHE 行为须防「换平台后仍沿用旧 OSAL」；非法则失败并提示清理 cache / 显式传参（细节见实施计划）。

### D5. CMake SSOT

- `cmake/wink_osal.cmake`：推导默认值、校验合法组合。
- `osal/CMakeLists.txt`：按 `WINK_OSAL_TYPE` 导出 **`WINK_OSAL_SOURCES`** / **`WINK_OSAL_INCLUDE_DIRS`**。
- 消费方（顶层 wasm 可执行文件、`targets/host` 的 `pal_host`、`targets/esp32` 的 `idf_component_register` **以及**非 IDF 分支 `ESP32_PAL_SOURCES`）**只消费上述变量**。
- **禁止**在 `targets/*/CMakeLists.txt` 手写死 `../../osal/host/pal_osal_host.c` 一类路径列表冒充装配。
- ESP32 **双写路径必须对齐**（idf SRCS 与 `ESP32_PAL_SOURCES` 中的 OSAL 条目一致）。

### D6. 命名例外

既有「文件名平台 tag ≈ 目录名」规范对 OSAL 变体开例外：

- 允许：`osal/freertos_esp32/pal_osal_freertos_esp32.c`、`osal/wasm/pal_osal_wasm.c`、`osal/host/pal_osal_host.c`。
- 禁止：恢复 `targets/<plat>/pal_osal_*.c` 旧布局。

### D7. 依赖方向与技术债 T-DEP-01

```text
DAL/runtime → pal/include → targets/<plat>/  （HAL 等）
                          → osal/<variant>/ （OSAL）
```

- **禁止** HAL（`targets/`）依赖 `osal/` 私有实现头。
- **禁止** `osal/` 依赖 DAL / App。
- **T-DEP-01（Phase A 例外）：** `osal/wasm` **仅允许**为 `wasm_bridge.h`、`pal_wasm_internal.h` 将 `targets/wasm` 加入 include。禁止借此扩大对其它 `targets/wasm` 私有头的依赖。Phase B 须将 OSAL 所需声明抽到 OSAL 边界头或 `pal/include` 后消灭该例外。

### D8. 静态分发不变

本决策只调整**实现文件落点与 CMake 装配**，不引入 `ops`/vtable/运行期器件多态；继续遵守 ADR-0004。

---

## 后果与约束（Consequences & Constraints）

| 正面 | 负面 / 缓解 |
|------|-------------|
| `targets/` 语义回到「硬件/平台端口」 | 一次迁移 + 三平台回归；按实施计划门禁执行 |
| 为 STM32×FreeRTOS / baremetal 预留目录与矩阵位 | Phase A **不能**复用 ESP OSAL 代码；须另开 Phase B，避免期望通胀 |
| 非法组合硬失败，减少错绑 | CACHE 换平台需文档/工具提示 |
| fiber/OSAL 与 HAL 目录分离，更清晰 | T-DEP-01 短期保留 wasm 反向 include；评审盯白名单 |
| ESP32 双写强制对齐 | 漏改一侧会导致静态分析配置漂移；PR 检查清单勾选 |

**迁移约束：**

- Phase A **不**改公共 PAL API 语义；**不**搬 `wink_sim_scheduler` / `wink_sim_physical`；**不**重构 irq/storage/fault。
- 历史 ADR / 旧实施计划中的 `targets/*/pal_osal_*.c` 路径不强制全文改写；活规范与构建脚本必须更新。

---

## 遵循与后续（Compliance & Follow-up）

### Accepted 后必须回写（Layer ①）

- [x] [03-directory-architecture.md](../../zh/design/02-wink-micro-os/03-directory-architecture.md)：目录树、`osal/`、命名例外、targets 不再含 OSAL — 2026-07-19
- [x] [02-pal-platform-abstraction.md](../../zh/design/02-wink-micro-os/02-pal-platform-abstraction.md)：实现位置改为 `targets/` + `osal/`；链接组合矩阵 — 2026-07-19
- [x] 本 ADR 状态改为 Accepted，并在此勾选回写完成 — 2026-07-19

### 实施

- Phase A 已按 [2026-07-02-hal-osal-decoupling-refactor-plan.md](../../implementation-plans/core/2026-07-02-hal-osal-decoupling-refactor-plan.md) 执行完毕（见该计划 Completed）。
- Phase B（portable FreeRTOS / STM32 / 消灭 T-DEP-01）**另开** ADR 与实施计划，不在本 ADR 验收范围内。

### 合规检查（给 Review / CI）

- `wink-micro-os/targets/**` 下不应再出现 `pal_osal_*.c` 或 `sim_ctx_*.c`。
- OSAL 源仅通过 `WINK_OSAL_SOURCES` 进入各 target 链接。
- Configure 非法 `TARGET_PLATFORM`×`WINK_OSAL_TYPE` 必须失败。

---

*本 ADR 状态变更请在此记录：*

- 2026-07-19：Proposed（目录正交 + Phase A 强绑定；STM32 代码复用延后 Phase B；关联实施计划同步修订）
- 2026-07-19：Accepted（由项目 Owner 确认通过，正式进入实施阶段）
