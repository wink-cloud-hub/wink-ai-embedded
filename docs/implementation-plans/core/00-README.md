# WinkMicroOS 综合评审整改计划 · 系列索引

> 本索引是整改系列的**编排谱**：决定阶段执行顺序、依赖、并行度与文件冲突，使整系列可直接交付 agentic worker 落地。
>
> 评审来源：`docs/reviews/core/2026-06-24-wink-micro-os-integrated-review.md`（综合评分 8.0/10）
> 基线：`.claude/skills/embedded-best-practice`、`.claude/rules/c-code.md`、ADR-0001~0005

---

## 一、系列总览

| Phase | 文件 | 对应 review 发现 | 性质 | 依赖前置 |
|---|---|---|---|---|
| **0** | `00-mechanical-fixes-and-docs.md` | P1-2 / P1-3 / P1-4 / P2-3 | 机械 + 1 项微重构 | 无 |
| **1** | `01-wasm-asyncify-stack-safety.md` | P0-1 | 链接配置 + 跨仓契约 | 无（可与 0 并行） |
| **3** ⚠️ | `03-pal-status-migration.md` | P1-1 | PAL API 签名迁移 | 无 |
| **2** ⚠️ | `02-dal-init-and-resource-collision.md` | P0-3 | DAL init + 资源治理 | **必须 3 先行** |
| **4** | `04-ultrasonic-nonblocking-capture.md` | P0-2 | 硬件捕获 + 非阻塞语义 | 2（需 dal_ultrasonic_init） |
| **5** | `05-runtime-failsafe-wdt.md` | P0-4 | Fail-Safe / WDT / 执行器关断 | 2（需 dal_*_init） |
| **6** | `06-serialization-wasm-esp32-boundaries.md` | P1-5 / P2-4 / P2-5 / P2-6 | 规范化 + 边界防线 | 无（可与多数阶段并行） |

---

## 二、⚠️ 修正后的执行顺序（关键，原编号 2→3 是错的）

```
Phase 0 ──┐
          ├──► Phase 3 ──► Phase 2 ──┬──► Phase 4 ──► Phase 5
Phase 1 ──┘                          │
                                     └──► Phase 6（可早启动，文档/规范为主）
```

**落地序：`0 → 1 → 3 → 2 → 4 → 5 → 6`**（Phase 0/1 可并行；Phase 6 可在 2 完成后任意时点并行收尾）

> 🚨 **横切红线（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：Phase 1 Task 1-5「中断桥 Asyncify 重入约束」是 **Phase 4 中断路径（echo 上升沿）的硬前置**——修好 Phase 1 挂起会激活 wasm sleeping 窗口的中断重入崩溃，Phase 4 的 echo 中断路径不得在 Task 1-5 落地前上线。

### 为什么 Phase 3 必须在 Phase 2 之前（硬约束）

Phase 2 Task 2-3 要求 `pal_gpio_init` / `pal_pwm_init` 在资源冲突时返回 `WINK_ERR_BUSY`、资源表满返回 `WINK_ERR_RESOURCE_EXHAUSTED`。但这两个 API 当前返回 `bool`（`pal_hal_wasm.c:9/34`、host 同构），**bool 无法表达 BUSY**。状态化是 Phase 3 的职责。若按原编号 2→3 执行，Phase 2 要么吞掉冲突错误（丢语义），要么擅自 status 化（与 Phase 3 撞车）。

→ **正确做法**：先做 Phase 3（PAL 全量 status 化），再做 Phase 2（资源占用直接经由新 status 签名返回 `WINK_ERR_BUSY`）。Phase 3 Task 3-1 已含 `pal_gpio_init`/`pal_pwm_init` 的 status 签名，正好为 Phase 2 铺路。

> 此约束同样影响 Phase 2 Task 2-1：`dal_servo_init` 内调用 `pal_pwm_init` 须用 status 版本判定失败 → 依赖 Phase 3。

---

## 三、并行度编排（给 subagent-driven-development）

**可并行启动的波次：**

- **Wave A（互不相交）**：Phase 0（dal/trace/skill doc）‖ Phase 1（wasm target + CMake + docs/04）‖ Phase 6 的文档/规范部分（docs + .claude/rules，不碰 C）
- **Wave B**：Phase 3（PAL 签名迁移，全 targets）——Phase 0/1 落地后
- **Wave C**：Phase 2（DAL init + 资源治理）——Phase 3 落地后
- **Wave D**：Phase 4 ‖ Phase 5——Phase 2 落地后（二者文件基本不相交，详见下表）
- **收尾**：Phase 6 的 grep 门禁 + ESP32 清单（依赖前面 API 稳定）

---

## 四、跨阶段文件冲突矩阵（同文件多阶段改动的串行点）

| 文件 | 涉及 Phase | 串行约束 |
|---|---|---|
| `dal/src/dal_servo.c` | 0（括号+常量）→ 2（加 init，重写 set_angle）→ 5（safe_off） | **严格串行 0→2→5**；Phase 2 重写 set_angle 时须**继承** Phase 0 的 `SERVO_*` 常量与 `{}` 风格 |
| `dal/include/dal_servo.h` | 2（加 init/config）→ 5（safe_off）→ 6（契约补全） | 串行 2→5→6 |
| `dal/src/dal_ultrasonic.c` | 0（括号）→ 2（加 init）→ 4（非阻塞重写） | 串行 0→2→4；Phase 4 重写时继承 Phase 0 `{}` |
| `pal/include/pal_hal.h` | 3（status 签名）→ 4（加 pulse_in）→ 6（契约） | 串行 3→4→6 |
| `pal/include/pal_osal.h` | 3（mutex status）→ 5（WDT/reset reason） | 串行 3→5 |
| `targets/wasm/wasm_bridge.h` | 1（已含 trigger/echo）→ 4（pulse_in 旁路）→ 6（回调索引注释） | 串行 1→4→6 |
| `samples/avoidance_car/device_tree.c` | 2（init 调用）→ 4（非阻塞状态机） | 串行 2→4 |
| `samples/avoidance_car/app_main.c` | 2（init）→ 4（loop 改状态机） | 串行 2→4 |

> 规则：任何 subagent 在认领 Task 前，先查本表；同文件的 Task **禁止并行**，须按箭头串行。

---

## 五、关键跨阶段技术注记（架构师红线）

1. **Phase 1 Asyncify 是两端契约**：C 侧 `ASYNCIFY_IMPORTS` + JS 侧 `handleSleep/handleAsync` 缺一不可。JS 胶水在前端仓——P0-1 的关闭以 Task 1-4 跨仓联调通过为准，**不可仅凭 CMake 改完即关闭**。
2. **死符号 `js_sim_get_ultrasonic_distance` 清理分散三处**：CMake（Phase 1 Task 1-1）、`.claude/skills` 副本（Phase 0 Task 0-3）、`docs/design/04`（Phase 1 Task 1-3）。三处须全部清零才算 SSOT 闭合。
3. **Phase 0 的 dal_servo.c 改动被 Phase 2 部分取代**：Phase 2 重写 `set_angle`（移除其内 `pal_pwm_init`）。Phase 0 提取的 `SERVO_*` 常量须在 Phase 2 新代码中保留；Phase 0 的 `initialized` 缺位由 Phase 2 补。Phase 0 不得声称 servo 已"完成"。
4. **Phase 3 的 ABI 破坏面**：PAL 失败型 API 由 `bool` 改 `wink_status_t` 是签名级破坏，波及所有 targets 与全部 DAL 调用点。Phase 3 必须一次性迁移完毕并通过 `rg` search gate（见 `03` 末尾），不留混合态。
5. **Phase 2/5 的执行器安全假设**：`dal_servo_safe_off` 用 `pal_pwm_set_duty(ch, 0)` 实现失能——对舵机（断电=limp=安全）成立；对**未来 DC 电机 DAL**（0 duty 可能 coast 而非制动）不成立。Actuator Registry 的"各自定义 safe-off"模型已为此预留，Phase 5 须在规范中写明此边界。
6. **Phase 4 host 时序须用虚拟时间**：host 上 `dal_ultrasonic_request_measurement → READY` 的迁移不能靠真实墙钟阻塞，须复用 host 虚拟时间（review 亮点 #5）。`pal_gpio_pulse_in` host 实现依赖 `host_echo_rise_us`/`host_echo_high_us`——Phase 4 落地前**先核验这两个 helper 存在**。
7. **Phase 1 修好挂起会激活中断重入崩溃（review [D1](../../reviews/unisim/2026-06-24-wink-micro-os-phase1-asyncify-deep-dive.md)）**：`wink_runtime_run` 每 tick 的 Asyncify sleeping 窗口期间，JS 调 `_trigger_wasm_interrupt`（模拟 GPIO 中断）会触发 `RuntimeError: invalid Asyncify state`。P0-1 未修时被"卡死"表象掩盖，修通即成活跃 bug——**修 A 激活 B 的次生风险，严重度高于 Phase 1 要修的死符号本身**。Phase 1 Task 1-5 以"中断排队 + tick 边界注入"闭环，是 Phase 4 的硬前置。**P0-1 不得仅凭 CMake 改完即关闭，须含 Task 1-4 联调 + Task 1-5 重入验证。**

---

## 六、出口验证矩阵（每阶段统一 gate）

| Phase | 自验（host） | 额外门禁 |
|---|---|---|
| 0 | `python wink-tools/wink.py test` 8 PASS | simulation.md grep 死符号=0；lint 规则就位 |
| 1 | host 8 PASS | CMake grep 死符号=0；`ASYNCIFY_IMPORTS` 含 js_pal_delay_ms、**不含** js_pal_i2c_transfer（D2）；**跨仓 JS 联调清单 + Task 1-5 中断重入不再可触发（D1）** |
| 3 | `python wink-tools/wink.py test --clean` 全绿 | `rg "bool pal_(gpio_init|pwm_init|...)"` = 0 |
| 2 | `python wink-tools/wink.py test --clean` 全绿 | 资源冲突测试（claim/conflict/exhausted）通过 |
| 4 | 全绿 | 单 tick 超声波路径墙钟 < 10ms（host guard） |
| 5 | 全绿 | fault 路径在 app `on_fault` 前调用 safe-off |
| 6 | 全绿 | `rg "packed|#pragma pack"` dal/runtime = 0；docs 无死符号 |

---

## 七、整改落地状态（2026-06-25 落地）

> **回写位置裁决**：review `2026-06-24-wink-micro-os-integrated-review.md` 自述「归档后按 reviews 约定只读；
> 后续整改应回写至对应 ADR、设计规范和代码」（见其 §九末注），与 `.claude/rules/docs-adr.md` 的 reviews
> 只读约定一致。故**不回写 review §九**（保持评审快照只读）；整改状态记录于本计划（活文档）与 `01~07`
> 活规范/代码（已落地）。原「回写 review §九」指令据此修正。

| 发现 | 落地 Phase | 状态 |
|---|---|---|
| P0-1 Asyncify + 栈门禁 | Phase 1 | **部分完成**：CMake/栈/断言 + 两端契约 + 中断重入约束已交付文档；P0-1 关闭须跨仓 Emscripten 联调（Task 1-4 #4-5 + Task 1-5）通过 |
| P0-2 超声波非阻塞/捕获 | Phase 4 | host 非阻塞完成；真机 RMT 捕获随 P2-6 |
| P0-3 DAL init + 资源冲突 | Phase 2 | host 完成；esp32 资源治理随 P2-6 |
| P0-4 WDT/Fail-Safe | Phase 5 | 软件闭环完成；硬件级默认安全态由板级电路保证（文档化）；clear-lock follow-up |
| P1-1 PAL status 化 | Phase 3 | 完成（search gate 0 残留 bool 签名） |
| P1-2 大括号门禁 | Phase 0 | **部分完成**：手动补齐 ✓；`.clang-tidy` 门禁规则就位，CI 接入待 |
| P1-3 trace thread-safety | Phase 0 | 完成（契约声明落地） |
| P1-4 simulation.md / docs 死符号 | Phase 0/1/4 | 完成（活动规范死符号清零；plan/review/ADR 归档中的死符号为历史记录，保留） |
| P1-5 对齐/序列化规范 | Phase 6 | 规范完成（`c-code.md §4` + 01-dal + 03-sandbox + CI grep 门禁） |
| P2-2 吞错 `(void)status` | Phase 2 | 完成（app_init init 失败 trace，不吞错） |
| P2-3 舵机魔法数 | Phase 0 | 完成（派生常量 `SERVO_PERIOD_MS` 等） |
| P2-4 wasm 回调 index 边界 | Phase 6 | 契约完成（索引安全）；时序安全见 Phase 1 Task 1-5 |
| P2-5 DAL header 契约补全 | Phase 2/4/6 | 完成（servo/ultrasonic 全公共 API 契约字段齐全） |
| P2-6 ESP32 PAL 路线 | Phase 6 | 移植清单完成（02-pal §4.1）；esp32 实现随移植推进 |

**统一出口**：`python wink-tools/wink.py test --clean` → **10/10 PASS**（基线 8 → 10，新增 `test_pal_resource`、
`test_actuator_registry`，servo/ultrasonic/runtime/host_pal 用例扩展）。各 Phase 详见 `00`~`06` 计划文件。

---

*本索引随各 Phase 落地更新状态。执行顺序变更（如 Phase 2/3 互换）为 2026-06-24 评审核验后的架构修正，原 review 编号仅供发现追踪。*

---

## 八、长期架构演进规划 (WinkMicroOS 层 Future Work)

这部分属于“强身健体”的架构演进，不在本次 P0/P1 的紧急修复队列中，但作为核心 OS 层的长远基石，需在未来版本（Phase 7+）中逐步落地：

1. **构建 AI 友好的 OS 契约清单 (Machine-Readable OS Manifest)**
   - **范畴**：OS Build/Tooling 层
   - **目标**：导出系统底层所有 API 的结构化契约（如 YAML/JSON 格式，标注 @blocking, @isr-safe 及资源占用）。让后期的 AI CodeGen 能够阅读理解，而非单纯依赖 C 语言注释，从根源防止生成的代码违背 RTOS 纪律或导致资源冲突。
2. **预留 OTA 与存储分区隔离 (A/B Partition & Storage)**
   - **范畴**：Bootloader & PAL Flash 层
   - **目标**：作为智能设备 OS 的必备能力，需尽早收紧和规范 `.bss` 和 `.data` 段的内存碎片，将硬件状态固化为外存（KV/NVS）的持久化数据，为双库平滑 OTA 留出 Flash 空间和框架接口。

