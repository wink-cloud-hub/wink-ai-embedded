# 06. BAL（业务抽象层）设计规范

> **本文是 BAL 层目录、命名、依赖、实现形态与 CI 门禁的权威活规范（SSOT）。**  
> 决策历史：[ADR-0023](../../decisions/core/0023-bal-business-abstraction-layer.md)（分层建立）、[ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md)（A/B/C 动词）、[ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md)（三域 + 闭环）、[ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)（命名硬切割）、[ADR-0047](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md)（FOC ISR 可调 BAL 快环约束）。  
> 操作动词细则仍见 [coding-conventions.md §3](../07-platform-governance/coding-conventions.md)；本节不重复整表。

---

## 0. 目标与非目标

| | 内容 |
|---|---|
| **目标** | 用固定决策树判定：新组件放哪、叫什么、依赖什么、如何实现；CI 可机械检查。 |
| **非目标** | 不改变 DAL/PAL 契约；不引入器件虚表；不在 BAL 公共头暴露 PAL 类型。 |
| **迁移策略** | 硬切割（ADR-0038）：合入后仓库内**不得**残留旧公开符号 / `_helper` / `_controller` 文件名。 |

---

## 1. 在系统中的位置

```text
App  →  BAL  →  { DAL, runtime }
              ↘ math 不依赖 DAL/runtime
```

- BAL 公共头 **禁止** `#include` 任何 `pal_*.h`（唯一豁免：`pal_log.h`，仅日志宏）。
- BAL `.c` 可为临界区 / 时间戳 / 日志 include PAL；不得把 PAL 类型泄漏到公共头。
- 静态分发（ADR-0004）：POD + 命名 API；器件抽象禁用 `ops`/vtable。

---

## 2. 三域目录（冻结）

```text
wink-micro-os/bal/
├── include/
│   ├── wink_bal_opts.h              # 跨域共享调度选项（唯一根级公共头，除本文件外慎增）
│   ├── input/                       # 物理增强 · 输入
│   ├── output/                      # 物理增强 · 输出
│   ├── sensor/                      # 物理增强 · 传感器
│   ├── actuator/                    # 物理增强 · 执行器（单器件、无跨器件闭环）
│   ├── display/                     # 物理增强 · 显示
│   ├── comm/                        # 物理增强 · 通信/遥测
│   ├── math/                        # 纯算法（无硬件、无调度）
│   └── control/                     # 领域控制（跨器件闭环 / 编排）
└── src/                             # 必须与 include 子目录镜像
    ├── input|output|sensor|actuator|display|comm|math|control/
    └── …
```

### 2.1 域职责与依赖

| 域 | 职责 | 允许依赖 | 禁止 |
|---|---|---|---|
| **物理增强** | 增强**单个**主 DAL 实例的行为（周期 poll、sweep、事件投递等） | 该主 DAL、runtime、`wink_bal_opts` | 跨类 DAL 组成闭环；把「车体速度」等系统目标当主 API |
| **math** | 纯计算 / 有状态算法（PID、滤波、运动学） | `wink_status.h`、标准 C | 任何 `dal_*`、`runtime`、`pal_*`、`wink_bal_opts` |
| **control** | 控制目标 + 反馈环或对多路执行器的编排 | `math`、多个 DAL、runtime、`wink_bal_opts` | 公共头 include `pal_*`（除 `pal_log.h` 豁免策略） |

### 2.2 `actuator/` vs `control/`（硬规则）

| 判定 | 目录 |
|---|---|
| 只增强**一个**执行器 DAL；无传感器反馈环 | `actuator/` |
| 执行器 + 传感器（或编排 ≥2 执行器），且对外是控制目标（速度、位姿、v/ω…） | `control/` |

口诀：**单器件开环/便利增强 → actuator；跟目标、用反馈、跨器件 → control。**

开环电机「只设占空比」的便利包装若出现，进 `actuator/`；闭环电机进 `control/`。

### 2.3 新组件决策树

```text
Q1: 是否依赖任何 DAL 或 Runtime？
    ├─ NO  → math/
    └─ YES → Q2: 是否跨 ≥2 个 DAL，或「执行器+反馈」闭环，或编排多个控制会话？
              ├─ YES → control/
              └─ NO  → 物理增强：按主 DAL 类别选 input|output|sensor|actuator|display|comm
```

---

## 3. 文件与符号命名

### 3.1 文件名规则

| 域 / 角色 | 文件名模式 | 示例 |
|---|---|---|
| math | `wink_<algo>.h` | `wink_pid.h` |
| control | `wink_<capability>.h` | `wink_closed_loop_dc_motor.h`、`wink_chassis.h` |
| 物理 · A 类活动 | `wink_<device>_<activity>.h` | `wink_led_blink.h`、`wink_rc_servo_sweep.h`、`wink_ultrasonic_poll.h` |
| 物理 · B 类事件 | `wink_<device>_<noun>_events.h` | `wink_button_events.h`、`wink_ultrasonic_distance_events.h` |
| 物理 · 具名服务 | `wink_<service>_<qualifier>.h` | `wink_telemetry_default.h` |

**禁止**（公开树内）：

- 文件名后缀 `_helper`、`_controller`
- 公开符号 / 路径中的 `sonar`（统一 `ultrasonic`）
- 文件名 stem 与公共 API 前缀不一致

**强制**：`wink_chassis.h` → `wink_chassis_start`；可从路径 grep 到符号。

### 3.2 API 动词（ADR-0032）

| 类 | 动词 | 典型 |
|---|---|---|
| A 活动 | `start` / `stop`（+ `_ex`） | 周期会话、闭环会话 |
| B 能力 | `enable_*` / `disable_*` | 主交付为事件队列投递 |
| C 动作 | `set` / `get` / `request` / … | 设定值、一次读写 |
| 谓词 | `is_` / `has_` / `can_` | 只读查询 |

Control 组件默认 **A + C**。同一路径禁止两套动词；细节与决策树见 coding-conventions §3。

### 3.3 词表

- 器件名与 DAL 对齐：`button`、`led`、`ultrasonic`、`rc_servo`、`dc_motor`、`encoder`（禁止泛称 `motor` 作 control/DAL 前缀；见 ADR-0048 / ADR-0049 / ADR-0050）。
- 容量宏：`WINK_<CAPABILITY>_MAX`（可由 `WINK_APP_MAX_<DAL>_INSTANCES` 派生）。
- 共享选项：`wink_bal_opts_t`（头文件 `wink_bal_opts.h`）。

### 3.4 Include 形式

```c
#include "math/wink_pid.h"
#include "control/wink_chassis.h"
#include "sensor/wink_ultrasonic_poll.h"
#include "wink_bal_opts.h"
```

CMake：`wink_bal` 的 PUBLIC include **只**添加 `bal/include`，**不得**再把各子目录加入 PUBLIC 路径。

---

## 4. 实现形态（强制）

### 4.1 对所有 Class A / 持有周期会话的组件

1. **Opaque**：公共头不得暴露 `wink_periodic_handle_t` 或 slot 内部字段。
2. **静态 slot 池**：`.c` 内 `static` 数组；key = 主 DAL 指针（底盘：左电机）。
3. **双轨**：`_start` / `_start_ex(const wink_bal_opts_t *opts)`。
4. **配置 / 状态分离**：对外 `*_config_t`（可 `static const`）；运行态藏在 slot。
5. **单位契约**：头文件 doxygen **钉死**单位（例：闭环电机 `counts/s`；底盘 `m/s` + `rad/s`）。
6. **停止即安全**：`_stop` 必须将执行器置于安全输出（如占空比 0 / safe_off）。

### 4.2 Control 额外强制（继承 ADR-0037）

1. **Fail-safe**：反馈超时 → 制动 + `wink_trace_fault`（如 `WINK_FAULT_MOTOR_FEEDBACK_LOSS`）。
2. **实测 dt**：用高精度时间戳差，禁止假定周期绝对准时。
3. **设定值并发**：App 写 / 周期任务读须临界区保护（防 float 撕裂）。
4. **组合回滚**：多路 start 失败须回滚已启动会话（无半开）。

### 4.3 Math 额外强制

1. 可无硬件 mock 的 host 单测。
2. 非法输入返回 `wink_status_t`（推荐），不得把错误与「输出 0」混为同一语义而不文档化。
3. PID：反馈微分（derivative-on-measurement）+ 明确 anti-windup 策略（见 ADR-0037）。

### 4.4 DAL 裁剪与 control 组件（降门槛）

依赖具体 DAL 的 control 实现（`wink_closed_loop_dc_motor`、`wink_chassis`）必须在对应 `WINK_USE_DC_MOTOR` / `WINK_USE_ENCODER` **未启用**时编译为 stub（`MAX==0` 路径，API 返回 `WINK_ERR_UNSUPPORTED`），**禁止**引用已被 `WINK_UNAVAILABLE` 标掉的 `dal_*` 符号。

> **DC safe-off（ADR-0048）**：`dal_dc_motor_safe_off` 绑定 **brake**。单方向脚（`dir_pin_b < 0`）无法短接制动时 DAL 返回 `WINK_ERR_UNSUPPORTED`；closed_loop 关断路径在该错误下回退 `coast`，避免 fail-safe 后仍保持上一拍占空比。双脚 H 桥推荐用于需要真制动的底盘。

### 4.5 Control 最低 host 单测场景表

| 场景 | 最低要求 |
|------|----------|
| 非法参数 / 生命周期 | 已有 |
| 反馈丢失 fail-safe | 已有（虚拟时钟驱动超时；与 `dal_dc_motor_safe_off` / brake 或 coast fallback 绑定一致） |
| 可注入反馈的跟踪 | **新增**（POD encoder count 注入 + `dal_encoder_get_count` 读路径；`dal_dc_motor` 输出收敛） |
| 饱和 / anti-windup（观测积分器状态） | **新增**（可与 math 层 PID 测互补；control 至少一场景，断言积分器停止爬升而非仅输出夹紧） |
| encoder count 溢出/回绕 | **新增**（经 `dal_encoder_get_count` 读路径；速度无 int32 量级跳变） |
| 时间注入（禁墙钟，虚拟时钟驱动 dt） | **新增**（所有依赖 dt 的场景通用要求：`sim_set_mono_time_us` / `sim_advance_mono_time_us`） |

用户只需在 `wink-app.json` 声明器件；无需手写 `-DWINK_USE_*`（[ADR-0039](../../decisions/core/0039-dal-dual-mode-auto-pruning.md)）：

| 条件 | 行为 |
|------|------|
| **有** `wink-app.json` | 仅声明到的 DAL 驱动 `WINK_USE_*=ON`，其余 OFF；由 codegen `app_options.cmake` 写满九宏 |
| **无** JSON（如 Arduino） | 九驱动全部 ON；configure 打 WARNING。正式固件应提供 JSON 以免镜像偏胖 |

共享逻辑在 `wink-micro-os/cmake/wink_dal_drivers.cmake`；ESP32 / Host / Binary SDK / wasm 单 App 同源消费，禁止各 target 再硬编码基线驱动列表。

### 4.6 ISR 可调用的 control / 快环函数约束（ADR-0047）

> **Scope**：仅 **SimpleFOC 本地算法型** 快环。ISR 宿主在 **DAL/target trampoline**，**禁止**在 BAL 公共头暴露 ISR 注册 / `pal_hwtimer` 符号。VESC/ODrive 外部驱动不走本清单。

ADR-0047 **允许**周期控制 ISR 调用 BAL 纯快环函数（Clarke/Park/SVPWM/电流环等），须满足：

| # | 约束 | 说明 |
|---|---|---|
| 1 | **无阻塞** | 禁止 `pal_delay_*`、mutex 阻塞获取、busy-wait、同步 I/O |
| 2 | **无 `pal_log`** | 快环路径禁止日志（含 `pal_log.h` 宏）；诊断改慢环 / 共享 flag |
| 3 | **有限栈** | 禁止大数组 / 深递归；栈预算由 trampoline / Wave C 实现标注 |
| 4 | **仅显式共享状态** | 与慢环（~50Hz 参数环）通信只经文档化的共享缓冲 / 原子字段；禁止隐式全局可变状态 |
| 5 | **数值类型锁定** | 周期控制 ISR **优先定点（Q15/Q31）**；若选 float，必须显式处理 Xtensa 中断 FPU 上下文（禁污染被抢占线程）。此裁决锁定 BAL `control/`（及配套 `math/`）数学 API 数值类型，**禁止** target 分支各行其是 |
| 6 | **无 `pal_*` 于公共头** | 快环公共 API 不得依赖除既有豁免外的 PAL；ISR 注册属 DAL/target + `pal_hwtimer` |

**评审勾选（新增 FOC 快环 BAL API 时）：**

- [ ] 无阻塞 / 无 `pal_log` / 有限栈  
- [ ] 仅触碰显式共享状态（与慢环缓冲契约已文档化）  
- [ ] 数值类型遵守 ADR-0047（Q15/Q31 优先，或 float+FPU 策略已写明）  
- [ ] 公共头无 ISR 注册 / `pal_hwtimer` 符号  

---

## 5. 目标态清单（硬切割后）

| 路径 | 主要 API |
|---|---|
| `wink_bal_opts.h` | `wink_bal_opts_t` |
| `output/wink_led_blink.h` | `wink_led_blink_start` / `_stop` |
| `input/wink_button_events.h` | `wink_button_enable_events` / `_disable_events` |
| `sensor/wink_ultrasonic_poll.h` | `wink_ultrasonic_poll_start` / `_stop` |
| `sensor/wink_ultrasonic_distance_events.h` | `wink_ultrasonic_enable_distance_events` |
| `actuator/wink_rc_servo_sweep.h` | `wink_rc_servo_sweep_start` / `_stop`；`wink_rc_servo_set_angle` |
| `comm/wink_telemetry_default.h` | `wink_telemetry_default_start` / `_stop` |
| `math/wink_pid.h` | `wink_pid_init` / `_update` / `_reset` |
| `math/wink_diff_drive_kinematics.h` | `wink_diff_drive_to_*` |
| `control/wink_closed_loop_dc_motor.h` | `wink_closed_loop_dc_motor_start` / `_set_speed`（仅 `dal_dc_motor`；ADR-0049） |
| `control/wink_chassis.h` | `wink_chassis_start` / `_set_velocity`（当前后端为 DC + encoder） |

完整旧→新映射见 [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)；闭环正名见 [ADR-0049](../../decisions/core/0049-bal-closed-loop-dc-motor-naming.md)。

---

## 6. CI 门禁（合入硬切割后启用）

文本扫描类规则由 **`wink lint --pack layering`**（[ADR-0043](../../decisions/tools/0043-yaml-driven-layer-lint.md)）执行；
链接/布局类仍留在 `bal/CMakeLists.txt`。

| ID | 规则 | 执行面 |
|---|---|---|
| BAL-INC-1 / BAL-HDR-NO-PAL | `bal/include/**/*.h` 不得 `#include` `pal_*.h`（`pal_log.h` 豁免） | `wink lint` |
| BAL-MATH-1 | `bal/include/math/**/*.h` 不得出现 `dal_`、`wink_runtime`、`wink_periodic`、`pal_` | `wink lint` |
| BAL-NAME-1 | `bal/include/**` 不得存在 `*_helper.h`、`*_controller.h` | `wink lint` |
| BAL-NAME-2 | `bal/include/**/*.h` 不得出现标识符 `sonar` | `wink lint` |
| BAL-INC-2 | `wink_bal` PUBLIC include directories 仅含 `…/bal/include` | CMake |
| BAL-SRC-1 | 每个 `include/<domain>/*.h` 的实现 `.c` 位于 `src/<domain>/`（镜像） | CMake |

`wink lint` 失败即 CI fail；CMake 项失败即 `FATAL_ERROR`。

---

## 7. 评审清单（新增/修改 BAL 组件时）

- [ ] 决策树 §2.3 域正确；`actuator` vs `control` 未误放  
- [ ] 文件名符合 §3.1；API 前缀 = 文件 stem  
- [ ] A/B/C 动词正确（§3.2 / coding-conventions §3）  
- [ ] Include 带域前缀；公共头无 PAL 泄漏  
- [ ] Class A：opaque slot + `_start_ex` + 单位文档 + stop 安全  
- [ ] Control：fail-safe / 实测 dt / 设定值临界区（若适用）  
- [ ] Math：无 DAL；单测可纯算  
- [ ] 测试与 CMake 已挂上；无旧名残留  
- [ ] 若 ISR 可调快环：§4.6 清单全部满足（ADR-0047）  

---

## 8. 与其它文档的关系

| 文档 | 关系 |
|---|---|
| ADR-0023 | 分层、slot、双轨、禁 PAL 头——仍有效；**目录树与 helper 命名以本文 + ADR-0038 为准** |
| ADR-0032 | A/B/C 动词仍有效；「不机械改名」已被 ADR-0038 硬切割取代 |
| ADR-0037 | 三域与闭环安全仍有效；文件名 `_controller` 等以 ADR-0038 为准 |
| ADR-0049 | 闭环能力正名 `wink_closed_loop_dc_motor`（对齐 `dal_dc_motor`）；禁止万能 motor 门面 |
| ADR-0047 | FOC 前后台切分；ISR 可调 BAL 快环约束（§4.6）；数值类型 / 两类 ISR / `pal_hwtimer` |
| `03-directory-architecture.md` | 内核骨架；BAL 细则以本文为准 |
| `coding-conventions.md` | §3 动词；§5 指向本文 |

