# P1 Review: App Layer Lowcode Unification (Codegen + button_helper + devkitc_smoke migration)

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-07-06 |
| **评审范围** | P0+P1 commits 0743d72..HEAD（7 commits） |
| **关联技术设计** | [tech-designs/app-layer-lowcode-unification-design.md](../../tech-designs/tools/app-layer-lowcode-unification-design.md) |
| **关联实施计划** | [implementation-plans/2026-07-05-app-layer-lowcode-unification-plan.md](../../implementation-plans/tools/2026-07-05-app-layer-lowcode-unification-plan.md) |
| **评审方法** | subagent 逐 task 实施 + 逐 task spec+quality 双审 + 最终 host 40/40 测试回归 |

## Commit 清单

| Commit | 描述 |
|---|---|
| `76b6760` | refactor(samples/devkitc_smoke): centralize init/register into device_tree |
| `e3fa116` | refactor(samples/common): classify BAL API error-handling levels |
| `b15984b` | feat(tools/codegen): add app_codegen.py skeleton with Jinja2 templates |
| `53ec460` | feat(tools/codegen): add led/button/ultrasonic driver plugins + golden test |
| `3bf8676` | feat(samples/common): add wink_button_helper BAL auto-poll helper |
| `311c291` | feat(samples/devkitc_smoke): migrate to codegen-driven device tree |
| P1-5 | docs backwrite to Layer ① living spec |

## 交付物核查

| P1 交付项 | 状态 | 备注 |
|---|---|---|
| `tools/codegen/app_codegen.py` | ✅ | 280 行（限制 300 行）；argparse+Jinja2+topo sort+difflib |
| DriverBase 插件体系 | ✅ | base.py 78 行（限制 80 行）；`__init_subclass__` 自动注册 |
| 4 Jinja2 模板 | ✅ | device_tree.h/c, app_support.c, app_options.cmake；trim_blocks 无空行噪音 |
| 3 驱动插件 | ✅ | led.py 35 行、button.py 49 行、ultrasonic.py 37 行（均 <50 行） |
| Golden test | ✅ | `tests/golden_expected/` 4 文件 + `test_golden.py`；1/1 PASS |
| `wink_button_helper` | ✅ | 静态槽 pool（默认 4）；重复 start 返 INVALID_STATE；LOG_D 自记 |
| button_helper 单测 | ✅ | 7 subtests（null/noop/duplicate/press+release/long_press/stop/period） |
| devkitc_smoke CMake 接入 | ✅ | `add_custom_command` + DEPENDS json/templates/drivers + CONFIGURE_DEPENDS |
| `wink-app.json` 迁移 | ✅ | 25 行声明式配置，手删 device_tree.c/h（140 行） |
| Layer ① 文档回写 | ✅ | `03-app-codegen/01-app-business-logic.md` 增 §2.2-2.6（JSON schema、device_tree 生命周期、app_services_start、button_helper 约束、三级错误分级） |
| Tech-design 状态 | ✅ | "Design in Progress" → "Implemented (P1)" |

## 测试结果

| 测试项 | Host |
|---|---|
| 全量 ctest | **40/40 PASS**（286 build steps, 0 warn 0 error） |
| Golden test (`python -m tools.codegen.tests.test_golden`) | **PASS** |
| test_button_helper | **PASS**（7 subtests） |
| app_devkitc_smoke_e2e.exe | 启动达 "init done"，selftest 全 PASS |
| -Werror 下编译 | ✅ 无警告（286/286 build steps） |

## 评审发现

### Critical
- 无

### Important
- 无

### Minor / Follow-up
1. **`pal/include/internal/pal_test_loopback.h` 头文件自洽探针失败**：P1-B2 lint 失败，为基线问题（pre-existing），不属于本次变更范围；需独立修复。
2. **Python 依赖**：host build 用的 Python 环境（ESP-IDF venv 或系统 Python）需手动 `pip install jinja2`；README.md 已说明，但未来 CMake 可加 find_package 校验。
3. **`WINK_ERR_NOT_FOUND = -18`**：为 actuator_unregister 新增错误码，已通过 test_pal_contract 的静态断言（WINK_ERR_HARDWARE == -12 未移位）。
4. **app_options.cmake** 暂未被消费（P2 CMake 静态裁剪时接入）；目前生成但 `include()` 被注释掉。
5. **Services stanza**（blink/sim_echo/telemetry 启动）仍在 app_callbacks.c 手写；等 P2 services 通用处理或后续迭代迁移到 codegen。
6. **Button auto_poll 真实硬件 WCET** 需 ESP32 实测（host 虚拟时钟下 elapse_us 恒为 0）；soft_timer dispatch 已有 5ms WCET watchdog，真机运行时超阈会 trace WARN。
7. **app_loop 已空**：保留为业务扩展点（不强制留空，按 §5.1 设计不删除）。

## ADR 合规

| ADR | 合规性 |
|---|---|
| ADR-0001（负错误码） | ✅ 新增错误码、DAL deinit、helper 均遵循 |
| ADR-0002（双 target 同源） | ✅ DAL deinit/PAL helper 使用既有 PAL 抽象；button_helper 三 target 代码路径一致（soft_timer 同源） |
| ADR-0004（静态分发，无 vtable） | ✅ DriverBase 是 Python 端插件体系（不构成 C vtable）；DAL 结构体保持 POD |
| ADR-0012（契约诚实） | ✅ init/deinit 对称；unregister 与 register 对称；fire-and-forget 显式标记 |
| ADR-0013/0014（协作调度） | ✅ button_helper 在 soft_timer 回调（协作 tick 内）；显式文档标注上下文约束，未引入独立 task/ISR 轮询 |
| ADR-0018（IRQ 三级） | ✅ button_helper 使用 soft_timer 而非 ISR；GPIO ISR 仅 edge_counter |

## 结论

**P1 通过。** Host 端 40/40 测试全绿，代码严格遵循所有 ADR 红线（零 vtable、零 malloc、零隐式框架行为），app_callbacks.c 从 150 行降至 140 行（~90 行实际代码；P0 迁移 init 样板，P1 迁移 device_tree 至生成文件），AI 友好度达成——配置 3 设备、1 执行器、1 auto-poll 仅需 25 行 JSON。

后续 P2（CMake 静态裁剪）/ P3（BAL 目录正规化）按原计划推进；event queue/mbox 仍留待独立 ADR-0019 立项。

