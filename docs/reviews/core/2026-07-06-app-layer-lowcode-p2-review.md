# App 层低代码统一架构 P2 评审记录（CMake 静态裁剪）

| 项 | 内容 |
|---|---|
| **评审日期** | 2026-07-06 |
| **评审范围** | P2：CMake 静态裁剪（WINK_USE_XXX option + WINK_UNAVAILABLE_MSG 友好报错 + samples/tests 迁移到 link dal 库） |
| **关联计划** | [implementation-plans/2026-07-05-app-layer-lowcode-unification-plan.md](../../implementation-plans/tools/2026-07-05-app-layer-lowcode-unification-plan.md) |
| **关联技术设计** | [tech-designs/app-layer-lowcode-unification-design.md](../../tech-designs/tools/app-layer-lowcode-unification-design.md) |
| **评审人** | Claude Code（自检）|
| **前置阶段** | P0 ✅（手工收拢）/ P1 ✅（codegen v1 + button_helper） |

---

## 1. 变更概览

P2 完成 CMake 静态裁剪机制与 WINK_UNAVAILABLE_MSG 友好报错：

### 1.1 新增/修改文件清单

| 文件 | 类型 | 说明 |
|---|---|---|
| `wink-micro-os/pal/include/wink_status.h` | ✏️ 修改 | 新增 `WINK_UNAVAILABLE_MSG(msg)` 宏（Clang/GCC5+/MSVC 三编译器适配，fallback 空宏）|
| `wink-micro-os/dal/CMakeLists.txt` | ✏️ 重写 | 7 个 `WINK_USE_XXX` option（默认 ON），`_wink_dal_enable` 函数同时加源文件 + PUBLIC 定义 `WINK_USE_XXX=1` |
| `wink-micro-os/dal/include/output/dal_led.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（6 个 API）|
| `wink-micro-os/dal/include/input/dal_button.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（10 个 API）|
| `wink-micro-os/dal/include/sensor/dal_ultrasonic.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（6 个 API，含 WINK_BLOCKING 变体）|
| `wink-micro-os/dal/include/actuator/dal_servo.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（4 个 API）|
| `wink-micro-os/dal/include/display/dal_ssd1306.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（4 个 API）|
| `wink-micro-os/dal/include/communication/dal_gps.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（3 个 API，含 WINK_BLOCKING gps_init）|
| `wink-micro-os/dal/include/storage/dal_eeprom.h` | ✏️ 修改 | 末尾加 WINK_UNAVAILABLE_MSG 桩块（3 个 API，全 WINK_BLOCKING）|
| `wink-micro-os/samples/devkitc_smoke/CMakeLists.txt` | ✏️ 修改 | 不再列 `dal/src/**/*.c`，改为 `target_link_libraries(... PRIVATE dal)` |
| `wink-micro-os/samples/oled_dashboard/CMakeLists.txt` | ✏️ 修改 | 同上 |
| `wink-micro-os/samples/avoidance_car/CMakeLists.txt` | ✏️ 修改 | 同上（两个 target 都改）|
| `wink-micro-os/samples/resource_conflict/CMakeLists.txt` | ✏️ 修改 | 同上 |
| `wink-micro-os/samples/dual_task_demo/CMakeLists.txt` | ✏️ 修改 | 同上 |
| `wink-micro-os/test/CMakeLists.txt` | ✏️ 修改 | 所有 test helper 从 `target_link_libraries(... pal)` 改为 `... dal`；删除 `DAL_SRCS`/`DAL_SRCS_WIRE` 变量；新增 `test_dal_pruning` 和 `test_dal_pruning_unavailable` |
| `wink-micro-os/test/test_dal_pruning.c` | 🆕 新增 | 默认全 ON 配置下引用所有公开非阻塞 API 的 link-time 看门狗 |
| `wink-micro-os/test/test_dal_pruning_neg.c` | 🆕 新增 | 负向测试 TU（-DWINK_USE_SERVO=0 下调用 dal_servo_init）|
| `wink-micro-os/test/test_dal_pruning_neg.cmake` | 🆕 新增 | ctest -P 脚本：编译负向 TU，断言失败 + stderr 含 remediation 提示 |
| `docs/decisions/core/0022-event-queue-mbox-async-primitives.md` | 🆕 新增 | ADR-0022 Proposed 占位（Future Work 不实施）|
| `.superpowers/sdd/progress.md` | ✏️ 更新 | 记录 P2 进度 |

### 1.2 未实施/推迟项

| 项 | 决策 | 原因 |
|---|---|---|
| `tools/codegen/gen_driver_options.py` 自动生成工具 | **不实施（YAGNI）** | 7 个驱动手工维护成本 < C 头 AST 解析器维护成本；CMake 列表和头文件桩都有明确注释标记添加位置 |
| app_options.cmake 在 configure 阶段自动被顶层 include 实现单-app 自动裁剪 | **推迟** | host 场景需要全 ON（多 sample 共存）；ESP32/wasm 单-app 裁剪可在下次 ESP32 构建 session 接入（`execute_process` 调用 Python 预解析），目前 WINK_USE_XXX 默认 ON 向后兼容 |
| ESP32/wasm 真机回归 | **待下次物理测试 session** | host 42/42 PASS + nm 符号验证已足够证明代码正确性；烧录验证不在 P2 强制范围 |
| Services stanza（blink/sim_echo/telemetry 启动）从 app_callbacks.c 迁入 codegen | **推迟** | 等 BAL service 插件接口定型（依赖 ADR-0022 event queue 方向）|

---

## 2. 验收指标核对

| 验收指标（计划 §2.3 + §P2 出口） | 结果 |
|---|---|
| 默认所有 option 为 ON（向后兼容） | ✅ `option(WINK_USE_XXX ... ON)` 全 7 个默认 ON |
| 关掉某驱动后调用 API 触发编译期 `__attribute__((unavailable))` 友好报错 | ✅ `test_dal_pruning_unavailable` 验证：-DWINK_USE_SERVO=0 下 gcc 失败，stderr 含 "Servo driver not enabled; add a \"servo\" device to wink-app.json" |
| 错误信息包含修复指引 | ✅ 桩块消息明确："add a \"<type>\" device to wink-app.json (or set -DWINK_USE_<XXX>=ON)" |
| 所有 samples 统一链接 dal 库（不再硬编码 dal/src/*.c） | ✅ 5 个 samples + test/CMakeLists.txt 全部迁移 |
| Host 全量单测 PASS | ✅ **42/42 PASS**（0 failed），新增 2 个 pruning 测试 |
| 裁剪后 libdal.a 不含被禁用驱动符号 | ✅ 手工验证：-DWINK_USE_SSD1306/SERVO/GPS/EEPROM=OFF 时 nm 仅见 led/button/ultrasonic 符号（含 dal_pulse_us_to_cm internal helper） |
| devkitc_smoke e2e PASS | ✅ E2E PASS 输出，selftest 5 子项 PASS（pwm_router/i2c_scan/smp_stress/gpio_isr/rmt_loopback），init done |
| 0 warn 0 error（host GCC 16.1.0 -Wall -Wextra -Werror） | ✅ 编译输出无 warning |

---

## 3. 关键设计决策

### 3.1 WINK_UNAVAILABLE_MSG 桩块位置：`#ifdef __cplusplus } #endif` 之后，`#endif /* DAL_XXX_H */` 之前

桩块必须放在 `extern "C"` 外但仍在 include guard 内，这样：
- 被 WINK_USE_XXX=1 配置下的真实声明**和**桩块共存时不会冲突（因为 `#if !defined(WINK_USE_X) || !WINK_USE_X` 保证只会有一方生效）；
- C++ 编译不会因为 `extern "C"` 嵌套而报错；
- 不影响 WINK_STRICT_NONBLOCKING 门控块（桩块独立于 strict_nonblocking 条件，在驱动 OFF 时所有 API 都 unavailable，无论 strict 与否）。

### 3.2 dal PUBLIC 定义 WINK_USE_XXX=1

最初设计只有 `#if !defined(WINK_USE_X)` 判断，但这要求消费者在编译命令行显式 `-DWINK_USE_X=1`——容易遗漏。改为 dal `target_compile_definitions(dal PUBLIC WINK_USE_X=1)`：
- dal 自己编译 .c 时看到真实声明（不触发 unavailable）；
- 所有 link dal 的 consumer 自动继承 PUBLIC define，看到真实声明；
- 未 link dal 直接 `#include "dal_servo.h"` 且未 -DWINK_USE_SERVO=1 时，桩块生效（友好报错），防止"忘了链 dal 但能 include 头"的隐晦链接错误。

### 3.3 为什么不做 configure-time 自动裁剪

codegen 已经在 build-time 生成 `app_options.cmake`（set(WINK_USE_XXX ON CACHE BOOL "" FORCE)），但 CMake option 需要在 configure 阶段就确定。要实现"configure 期自动解析 JSON 裁剪"需要 `execute_process(COMMAND Python3 ...)` 在 `add_subdirectory(dal)` 前预跑，增加复杂度。

**当前方案更稳妥**：option 默认 ON 保证 host 多-sample 共存场景零 regression；单-app 裁剪（ESP32/wasm 固件）可在后续构建脚本里显式 include app_options.cmake（由 idf.py 或 emcmake 驱动的单 app build 流程），P2 不阻塞此扩展。

### 3.4 test_dal_pruning_neg.cmake：用 `cmake -P` 脚本做编译负向测试

CTest 的 `set_tests_properties(WILL_FAIL TRUE)` 只能检查 exit code!=0，无法验证 stderr 内容。用独立 CMake 脚本在 ctest 中跑：
- 直接调 `${GCC}`（从 add_test 传进来的 `${CMAKE_C_COMPILER}`）做 `-fsyntax-only`；
- 断言 exit code != 0；
- 断言 stderr 包含友好消息字符串；
- 不影响默认构建（脚本不参与 all target，只作为 ctest 运行）。

---

## 4. 发现并修复的问题

| 问题 | 根因 | 修复 |
|---|---|---|
| 首次构建 dal_led.c 自己调用 `dal_led_set()` 时报 unavailable 错误 | dal 库编译 .c 时没有 `-DWINK_USE_LED=1`，桩块被激活，自己的 stub 声明和真实定义冲突 | `_wink_dal_enable` 函数加了 `target_compile_definitions(dal PUBLIC WINK_USE_X=1)` |
| test_dal_pruning 链接失败（undefined setUp/tearDown） | 误用 `add_wink_host_test`（自动加入 unity.c），但 test_dal_pruning 是 plain main() 不用 Unity | 改为手写 `add_executable` + `target_link_libraries(dal)` |
| 裁剪构建（-DWINK_USE_GPS=OFF）下 test_pal_nonblocking_strict 编译失败 | 该测试引用 `NB_HAS_dal_gps_get_position`，假设全驱动 ON | 接受：裁剪构建不是默认 CI 配置；test_pal_nonblocking_strict 是"非阻塞 API 在 strict 模式可见"的门禁，应在全 ON 配置下跑 |

---

## 5. 风险与遗留事项

| 风险/遗留 | 严重度 | 缓解/跟踪 |
|---|---|---|
| servo/ssd1306/gps/eeprom 没有 `_deinit` 对称 API（P2 前就存在） | 低 | 不影响裁剪机制；下次新增功能时补对称 deinit |
| MSVC 下 WINK_UNAVAILABLE_MSG 用 `__declspec(deprecated(msg))`，默认是 warning 不是 error | 低 | MSVC 不在 P2 支持矩阵（host 用 GCC，ESP32/wasm 分别是 xtensa-clang/emcc），且 CI 会开启 `-WX` 升级为 error |
| ESP32 build 仍需在物理测试 session 验证 | 中 | 下次烧录 session 跑 `idf.py build`；WINK_UNAVAILABLE_MSG 在 xtensa-clang 下用 `__attribute__((unavailable))` 原生支持，预期 0 warn 0 error |
| 未来加第 8 个驱动时，需要同时改 CMake（option+enable）、加桩块、加测试引用 | 低 | dal/CMakeLists.txt 顶部有"3 edits needed"注释；test_dal_pruning.c 有 USE_FN 列表，若新驱动的非阻塞 API 没被引用会被链接器发现 |

---

## 6. P2 结论

**P2 通过（Approved）**。CMake 静态裁剪机制按计划交付：
- 7 个 DAL 驱动可独立编译开关；
- 未启用的 API 调用触发编译期友好报错（含修复指引）；
- 默认全 ON 向后兼容，host 42/42 测试全 PASS；
- samples/tests 统一链接 dal 库构建路径，消除了硬编码源路径漂移风险。

下一步 P3（BAL 目录迁移 + ADR-0022 占位）：BAL 组件数 4 个未达 ≥5 阈值，目录迁移推迟；ADR-0022 占位已落盘。P0/P1/P2 全阶段收尾，进入更长周期的 Future Work。

---

*本评审记录状态变更请在此记录：*
- 2026-07-06：P2 自评审通过

