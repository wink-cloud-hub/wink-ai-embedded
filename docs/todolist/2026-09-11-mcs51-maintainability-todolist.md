# MCS-51 框架可维护性/可扩展性审查任务清单（简版）

| 元数据项 | 说明 |
| :--- | :--- |
| **文档编号** | MCS51-MAINT-2026-09-11 |
| **创建日期** | 2026-09-11 |
| **所属模块** | `wink-micro-os/frameworks/mcs51/`（src/ + include/ + tools/ + test/） |
| **状态** | **执行中**：M1+M2、M3+M5+M7 均已落地（2026-09-11，工作区未提交），40/40 host 测试全绿；M4+M6 文档化待定 |
| **审计基线** | master @ e0bce2b 之后工作区（含未提交 GAP-12：T2/3/4 参数化、重复向量计数、EXTIF W0C）；三路并行只读审计 + 双 target/内存/ISR 补查 |
| **关联文档** | [仿真/真机缝隙清单](./2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md)（GAP-09/12/22/23 相关）、[后端责任划分](./2026-09-10-mcs51-sim-backend-responsibility-classification.md) |

> 约定：P0 = 加新系列会炸或状态归属错误，必须改；P1 = 应该改；P2 = 卫生项。修复须保持静态分发（ADR-0004，无 vtable/container_of）与双 target 同源。

---

## P0（必须改）

| 编号 | 问题 | 证据 | 修复方向 | 验收 |
| :--- | :--- | :--- | :--- | :--- |
| M1 | 家族二值谓词散落 + 外设表无门控，违反 OCP | `mcs51_context.cpp:25`、`mcs51_uart.cpp:110`、`mcs51_xdata.cpp:58` 三处 `== CMS8S78XX`；`mcs51_peripheral.cpp:36-85` 6 外设无条件注册（含 3 个 `cms8s_*`） | 新增 `mcs51_family.h` 描述符表（xram/xsfr/ckcon/fosc），`src/family/` 每系列一文件；外设表按家族过滤；`mcs51_context.h:20-29` 系列常量搬迁首站 | 新增第 3 家族只需加文件 + 表加行，通用文件零改；at89 下无 cms8s hook |
| M2 | 全局 static 与 ctx 归属混乱 | `s_mcu_family`（`mcs51_context.cpp:15`）进程全局；模型态误放文件 static：`mcs51_adc0832.cpp:44`、`cms8s_sys.cpp:39`、`cms8s_buzzer.cpp:29`、`mcs51_adc.cpp:19-20`、`mcs51_pwm_meter.cpp:20`；reset 覆盖不一致（ADC 注入轨未接入描述符表、pwm 无全局 reset、irq map 不随 context_reset 恢复、uni_bridge 日志只清计数不清内容） | 家族进 `Mcu51Context`（reset 装载）；模型态进 ctx（或 `soc_priv` 按家族分块）；每个全局二选一声明（per-context 随 reset 清 / 进程级诊断不清）并补齐缺失 reset | 双家族同测不串扰；全部 reset 路径有单测覆盖 |
| M3 | C/C++ linkage 混杂 | 6 处 C++ linkage hook 存入 C typedef（`cms8s_sys.cpp:52,66,81`、`mcs51_extint.cpp:22`、`mcs51_uart.cpp:273`、`cms8s_adc.cpp:119`、`cms8s_buzzer.cpp:65`）；`mcs51_proxy.hpp:44-47` 裸 `extern "C"`；`absacc.h:55/78-79` 不对称 | hook 定义统一包 `extern "C"`（或 typedef 侧一致）；代理头 guard 配平 | 更严工具链（-Wreturn-type/严格原型）零警告；host + wasm 构建通过 |

## P1（应该改）

| 编号 | 问题 | 证据 | 修复方向 | 验收 |
| :--- | :--- | :--- | :--- | :--- |
| M4 | 返回值形态：几乎无 `wink_status_t` | 仅 `mcs51_edge_queue.h:18` 一处；其余 `void/uint32_t/bool` + abort/log+counter | 成文“counter-as-API”契约（GAP-10 已在做），新增 fallible API 不开新口 | 契约写入 `mcs51_trap.h` 头注释 |
| M5 | SFR 地址多处重复定义 | `0xB4` 两套前缀（`mcs51_extint.cpp:14` vs `:60`）；PS `0x7F` 种子两处；T34MOD/T2CON 各文件自备 `constexpr` | 单一寄存器地图头（与 `mcs51_shim_audit.py` 同源生成） | 全量 diff 脚本覆盖新增地图头 |
| M6 | shadow 双写通道缺成文规则 | proxy 先直写后调 hook；模型 poll/step 大量直写不经桥（无 microstep 计费） | 规则写入 `mcs51_trap.h` 注释 | 新模型不再误用 |
| M7 | bridge 绕过注册 API | `mcs51_bridge.cpp:59` 直接数组赋值 `sfr_write_hooks[0x87]` | 改走 `mcs51_trap_register_sfr_write` | tripwire/统计一致 |

## P2（卫生项，确认做得好 + 小修）

- 双 target：`__EMSCRIPTEN__` 仅 KEEPALIVE + edge_queue host 桩，范围小（良好）；KEEPALIVE 散 4 文件，可集中宏。
- 内存：无 malloc/string/vector，`memcpy/memset` 限 reset；66KB ctx 有 BSS 注释（良好）。
- ISR 分发（深度栈 4 + RETI 抑制 + 优先级）清晰；`s_duplicate_vector_count` 不清零合理但需文档化。
- 构建三源并存（目录级 `WINK_MCU_*` + 测试级定义 + 运行时全局）随 M1 收敛；cleanup 正则启发式归阶段 4。

---

## 建议执行顺序

1. M1 + M2（约 1 天）：描述符表 + `ctx->family` + 外设表过滤 + 模型态进 ctx。**✅ 已执行（2026-09-11）**：
   - 新增 `mcs51_family.h`（id/掩码/描述符/能力查询）+ `mcs51_family.cpp`（Classic/CMS8S78XX 两行）；CMake 接入双库。
   - `Mcu51Context` 新增 `family` + §8 模型态（adc0832/sysProt/buzzer/cms8sAdc/ADC 注入轨/pwm_meters）；`soc_priv` 单例别名消除（`cms8s_adc.cpp` 改直连 ctx 存储）。
   - `mcs51_peripheral_desc_t` 加 `family_mask`，init/reset/poll/next_event 四处遍历按家族过滤。
   - `mcs51_xdata.cpp`/`mcs51_uart.cpp`/`mcs51_context.cpp`/`cms8s_sys.cpp` 改查描述符；`cms8s_sys` 的 24MHz 改取 `fosc_hz` 单源。
   - 附带迁移（预期内）：7 个 CMS8S/XSFR 测试补 `mcs51_context_set_family(CMS8S78XX)`（此前窗口/hook 无条件存在，测试从不声明家族；库编译无 `WINK_MCU_*`，默认 classic）。`timer_ext_clk` Test 6 的 24MHz 隐含假设一并显式化。
   - 验证：40/40 mcs51 host 全绿、`mcs51_shim_audit.py` 无硬失配；`test_sim_scheduler` 1 项失败系预存 Unity 64 位环境问题、`oled_dashboard` 构建失败系预存缺 sister 文件，均与本次无关。
2. M3 + M5 + M7（约半天）：linkage 包齐、寄存器地图单源、bridge 走 register。**✅ 已执行（2026-09-11）**：
   - M3：9 个 hook 定义加 `extern "C"`（sys×3、extint、uart、adc、buzzer、adc0832×4；pcon 本就在块内）；`mcs51_proxy.hpp`/`absacc.h` guard 配平。教训：匿名命名空间内 `extern "C" static` 被 GCC 拒（linkage-spec 不配 static），用裸 `extern "C"`（内链接来自匿名空间）。
   - M5：新增 `mcs51_sfr_map.h`（EXTIF/T34MOD/T2CON/EIE2/EIF2/CKCON/PS_xx 单源）；timer/uart/adc/extint/context 改别名引用，用点零改；模型私有地址保留本地。
   - M7：bridge PCON 注册改走 `mcs51_trap_register_sfr_write`。
   - 验证：40/40 全绿、shim audit 无硬失配、cleanup 自测 OK。
3. M4 + M6 文档化（随手）：头注释 + GAP 清单回写。

> 流程提醒：按仓库规则，阶段 1/2 执行前建议先确认本清单范围；涉及时钟/中断语义的改动补 ADR。
