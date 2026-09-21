# 实施计划：wink-micro-app 全量微应用生态分组重构与 CMake 深度解耦（ADR-0079 落地 Phase 2）

> 📋 **本文档是基于 ADR-0079（嵌套目录与清单剪枝发现）与架构方案 A，推进 `wink-micro-app/` 存量微应用向“生态模式”分类分组全面迁移的权威实施计划**。

---

## 1. 元数据表

| 字段 | 内容 |
|:---|:---|
| **计划编号** | `PLAN-20260921-MICRO-APP-ECOSYSTEM-REORGANIZATION` |
| **创建日期** | `2026-09-21` |
| **目标平台/SoC** | `wasm` (Emscripten), `host` (GCC/MSVC), `esp32` (ESP-IDF), MCS-51 (SDCC), PDK (SDCC) |
| **工具链/SDK版本** | Python 3.10+, CMake 3.15+, Emscripten, ESP-IDF |
| **计划状态** | 🟡 **Draft（待评审与执行）** |
| **优先级** | 🟡 P1（应用架构体系治理与开发者体验） |
| **计划版本** | `v1.0` |
| **关联技术规范** | [`docs/zh/design/02-wink-micro-os/03-directory-architecture.md`](../../zh/design/02-wink-micro-os/03-directory-architecture.md) |
| **关联 ADR** | [`ADR-0079：wink-micro-app 最多三级嵌套目录与清单边界剪枝发现`](../../decisions/core/0079-micro-app-nested-app-discovery.md) |
| **前置参考计划** | [`docs/implementation-plans/core/2026-09-09-vendor-cms8s78xx-apps-migration-plan.md`](./2026-09-09-vendor-cms8s78xx-apps-migration-plan.md) |
| **计划负责人** | 嵌入式系统架构团队 |

---

## 2. 背景与目标

### 2.1 现状与痛点
在 2026-09-09 实施的 [ADR-0079 阶段一](./2026-09-09-vendor-cms8s78xx-apps-migration-plan.md) 中，工具链（Python `wink-tools`、TS `unisim`、Vue `embedded-frontend`）已经全线具备了 1~3 级受控嵌套发现、清单边界剪枝与全树唯一别名解析能力，并将 33 个原厂外设示例收拢至 `vendor_cms8s78xx_v202/` 组下。

然而，`wink-micro-app/` 根目录下仍平铺了 14 个异构应用与测试夹具：
1. **模式 1 核心标杆被淹没**：WinkMicroOS 原生的 AI-Native / Role-Action 应用（`avoidance_car`、`oled_dashboard`、`dual_task_demo`）直接混杂在外部生态用例中，缺乏主舞台展示效应。
2. **模式 2 外部兼容生态割裂**：
   - 6 个 MCS-51 经典应用继续使用 `mcs51_*` 平铺前缀；
   - 1 个 Arduino 兼容应用名为 `arduino_blink_demo`；
   - 1 个 PDK 汇编应用名为 `pdk_button_led`。
3. **测试基础设施侵入应用区**：`devkitc_smoke`、`unisim_smoke`、`determinism_fixture`、`resource_conflict` 是平台底层 Bring-up 与 CI 确定性回归夹具，不属于终端开发者参考的业务应用。

### 2.2 重构目标
1. **生态与领域正交分层（方案 A+ 落地）**：
   - `native/`：归拢 3 个 Wink 原生 AI-Native (Role-Action / OS 内核同源) 核心应用；
   - `appliances/`：★ 独立建立商业级智能小家电品类（GB 4706 安规 / 热力学闭环），归拢 `health_pot` 并为微波炉、烤箱预留空间；
   - `mcs51/`：归拢 5 个 MCS-51 架构特性与基础外设回归 Demo，去除冗余 `mcs51_` 前缀；
   - `arduino/`：归拢并规范为 `arduino/blink`；
   - `pdk/`：归拢并规范为 `pdk/button_led`；
   - `fixtures/`：剥离 4 个平台冒烟与自检夹具，隔离核心应用区；
   - `vendor_cms8s78xx_v202/`：保持深度 2 原厂回归套件不变。
2. **CMake 深度解耦（终结硬编码相对路径断裂）**：
   - 改造各 App 的 `CMakeLists.txt`，支持深度自适应加载 `sample_common.cmake`；
   - 引用 `wink-tools` 与 `wink-micro-os` 根路径时优先消费 CMake 变量，消除深度变动引发的构建中断。
3. **100% 历史继承与短别名兼容**：
   - 目录迁移全部采用 `git mv`，保留 Git Blame 历史轨迹；
   - 依靠 ADR-0079 的唯一别名机制，开发者在 CLI 执行 `winkcli build wasm --app avoidance_car` 或 `winkcli build wasm --app health_pot` 依然无缝直达，保持无损后向兼容。
4. **安全可回滚一键脚本交付**：提供带有 `--dry-run` 预览与 `--apply` 执行的双模 Python 脚本，支持单步验证与一键回滚。

### 2.3 成功指标（验收出口）

| 指标 | 验收标准 | 验证方法 |
|:---|:---|:---|
| **应用发现完整性** | 全部 15 个移动后的 App 被工具链准确识别到新 Qualified ID（如 `native/avoidance_car`、`appliances/health_pot`、`mcs51/button_led` 等） | Python 发现单测 + `winkcli` 扫描 |
| **短别名解析能力** | 唯一叶子名（如 `--app health_pot`、`--app avoidance_car`）解析正确，同名时能准确定位全 ID | `matchAppRef` / `app_discovery.find_app` 测试 |
| **Host 单元与 E2E 测试** | 根 CMake 与全部 App CMakeLists 配置通过，测试 100% PASS | `ctest -j8`（含 mcs51 transpile、devkitc e2e 等） |
| **WASM 仿真编译抽检** | `avoidance_car`、`health_pot`、`oled_dashboard`、`unisim_smoke` 成功生成 Wasm | `winkcli build wasm --app <id>` |
| **架构与分层门禁** | 零违规，符合 user_surface 与 API 规范 | `winkcli lint --pack layering --pack api` |

---

## 3. 架构设计与映射矩阵

### 3.1 目录组织与 ID 映射表

| 原平铺目录 | 重构后分组目录 | 新 Qualified App ID | 所属模式 / 架构角色 | 典型特征 |
|:---|:---|:---|:---|:---|
| `avoidance_car` | `native/avoidance_car` | `native/avoidance_car` | 模式 1 (AI-Native) | L1 语义级：避障小车 (Role-Action + 距离事件) |
| `oled_dashboard` | `native/oled_dashboard` | `native/oled_dashboard` | 模式 1 (AI-Native) | L1 语义级：OLED 仪表盘 |
| `dual_task_demo` | `native/dual_task_demo` | `native/dual_task_demo` | 模式 1 (AI-Native) | L2 专家级：手写任务编排与 actuator 注册 |
| `mcs51_health_pot` | `appliances/health_pot` | `appliances/health_pot` | 商业小家电 (Appliance) | 商业级养生壶 (GB 4706 安规 / NTC / 继电器 / 段码屏 / 热力学 Plant) |
| `mcs51_analog_threshold` | `mcs51/analog_threshold` | `mcs51/analog_threshold` | 模式 2 (MCS-51) | ADC 阈值检测 Demo |
| `mcs51_button_led` | `mcs51/button_led` | `mcs51/button_led` | 模式 2 (MCS-51) | 基础 GPIO 轮询 Demo |
| `mcs51_button_led_int` | `mcs51/button_led_int` | `mcs51/button_led_int` | 模式 2 (MCS-51) | 外部中断响应 Demo |
| `mcs51_uart_echo` | `mcs51/uart_echo` | `mcs51/uart_echo` | 模式 2 (MCS-51) | 串口中断回显 Demo |
| `mcs51_uart_hello` | `mcs51/uart_hello` | `mcs51/uart_hello` | 模式 2 (MCS-51) | 串口轮询打印 Demo |
| `arduino_blink_demo` | `arduino/blink` | `arduino/blink` | 模式 2 (Arduino) | ArduinoCore 兼容层验证 (.ino) |
| `pdk_button_led` | `pdk/button_led` | `pdk/button_led` | 模式 2 (PDK) | 8 位 OTP 芯片纯汇编 / SDCC 固件 |
| `devkitc_smoke` | `fixtures/devkitc_smoke` | `fixtures/devkitc_smoke` | 系统基础设施 | ESP32 物理硬件板级 Bring-up 冒烟 |
| `unisim_smoke` | `fixtures/unisim_smoke` | `fixtures/unisim_smoke` | 系统基础设施 | Wasm 仿真器基础特性与胶水层冒烟 |
| `determinism_fixture` | `fixtures/determinism_fixture`| `fixtures/determinism_fixture`| 系统基础设施 | 微秒级确定性虚拟时间回归夹具 |
| `resource_conflict` | `fixtures/resource_conflict` | `fixtures/resource_conflict` | 系统基础设施 | 硬件引脚冲突与负向治理测试 (C 源码夹具) |

*(注：`vendor_cms8s78xx_v202/` 保持原状，`common/` 保持在 `wink-micro-app/common` 作为根级公共库。)*

---

### 3.2 CMakeLists.txt 适配规范

当 App 从深度 1（`wink-micro-app/<app>`）下沉至深度 2（`wink-micro-app/<group>/<app>`）后，需统一应用以下适配范式：

#### 范式 1：`sample_common.cmake` 路径自适应引入
```cmake
# 支持自适应向上寻找 sample_common.cmake（兼容平铺与分组嵌套）
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake")
endif()
```

#### 范式 2：`WINK_CODEGEN_ROOT` 根路径自适应推导
```cmake
if(NOT DEFINED WINK_CODEGEN_ROOT)
    if(DEFINED WINK_TOOLS_ROOT)
        set(WINK_CODEGEN_ROOT "${WINK_TOOLS_ROOT}/tools/codegen")
    else()
        get_filename_component(WINK_CODEGEN_ROOT
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-tools/tools/codegen" ABSOLUTE)
    endif()
endif()
```

#### 范式 3：MCS-51 统一 OS 根推导
```cmake
if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
```

#### 范式 4：PDK 包含路径推导
```cmake
get_filename_component(PDK_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/vendors/PDK/pdk-includes" ABSOLUTE)
```

---

### 3.3 外部关键引用与门禁规则同步

1. **`wink-micro-os/CMakeLists.txt`**：
   - 默认应用路径更新：`set(WINK_APP_DIR "wink-micro-app/native/avoidance_car" ...)`；
   - `wink_add_sample_subdirectory` 更新为分组子目录并做构建目录名称斜杠归一化：
     ```cmake
     macro(wink_add_sample_subdirectory name)
         get_filename_component(_abs_sample_dir "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/${name}" ABSOLUTE)
         if(EXISTS "${_abs_sample_dir}")
             if(NOT "${WINK_APP_DIR}" STREQUAL "${_abs_sample_dir}")
                 string(REPLACE "/" "_" _build_tag "${name}")
                 add_subdirectory("${_abs_sample_dir}" "${CMAKE_BINARY_DIR}/${_build_tag}_build")
             endif()
         endif()
     endmacro()

     wink_add_sample_subdirectory(native/oled_dashboard)
     wink_add_sample_subdirectory(fixtures/devkitc_smoke)
     wink_add_sample_subdirectory(fixtures/resource_conflict)
     wink_add_sample_subdirectory(native/dual_task_demo)
     wink_add_sample_subdirectory(arduino/blink)
     ```
2. **`wink-micro-os/test/CMakeLists.txt`**：
   - 函数 `mcs51_transpile_app` 增强变量名与输出文件名安全转换：
     ```cmake
     string(REPLACE "/" "_" _app_tag "${app_dir}")
     set(_cpp ${_MCS51_GEN_DIR}/${_app_tag}_${app_src}.cpp)
     set(_MCS51_${_app_tag}_${app_src}_CPP ${_cpp} PARENT_SCOPE)
     ```
   - 调用点更新为：`mcs51_transpile_app(appliances/health_pot health_pot)`。
3. **`wink-tools/tools/lint/rules/user_surface.yaml`**：
   - 放行白名单更新：
     - `wink-micro-app/fixtures/resource_conflict/**`
     - `wink-micro-app/native/dual_task_demo/**`
4. **CI 流程文件**：
   - `.github/workflows/clang-tidy.yml`、`nightly.yml`、`pr.yml` 中 `-DWINK_APP_DIR=wink-micro-app/unisim_smoke` 更新为 `wink-micro-app/fixtures/unisim_smoke`。
   - `wink-micro-os/targets/wasm/wasm_node_smoke.cmake` 中的 `unisim_smoke` 路径同步。

---

## 4. 任务拆分与执行步骤

### Phase 1：一键迁移脚本交付与预演 (Task 1)
- [ ] **Task 1.1**：在 `scripts/migrate_micro_app_ecosystem.py` 中编写自动化迁移脚本。
- [ ] **Task 1.2**：执行 `python scripts/migrate_micro_app_ecosystem.py --dry-run`，审查移动路径与待修改文件清单。

### Phase 2：执行搬迁与 CMake/配置打补丁 (Task 2)
- [ ] **Task 2.1**：执行 `python scripts/migrate_micro_app_ecosystem.py --apply`。
  - 通过 `git mv` 原子搬迁 14 个应用目录；
  - 自动打补丁更新 14 个应用内的 `CMakeLists.txt`；
  - 自动更新 `wink-app.json` 内的 `app_name` 为叶子短名；
  - 自动打补丁更新 `wink-micro-os/CMakeLists.txt`、`test/CMakeLists.txt`、`user_surface.yaml` 与 CI 工作流配置。

### Phase 3：构建、测试与门禁验证 (Task 3)
- [ ] **Task 3.1**：验证 App 发现机制：
  ```bash
  python -c "from tools.app_discovery import discover_apps; from pathlib import Path; print([a.id for a in discover_apps(Path('wink-micro-app'))])"
  ```
- [ ] **Task 3.2**：Host 单元与 E2E 测试回归：
  ```powershell
  cmake -B build-host -S wink-micro-os -DTARGET_PLATFORM=host
  cmake --build build-host --config Debug
  ctest --test-dir build-host --output-on-failure
  ```
- [ ] **Task 3.3**：抽检各生态典型应用的 WASM 编译：
  - `native/avoidance_car`
  - `native/oled_dashboard`
  - `mcs51/health_pot`
  - `arduino/blink`
  - `fixtures/unisim_smoke`
- [ ] **Task 3.4**：架构门禁与许可检查：
  ```powershell
  python .github/scripts/check_license_map.py
  winkcli lint --pack layering --pack api
  ```

### Phase 4：文档回写与提交归档 (Task 4)
- [ ] **Task 4.1**：更新 `wink-micro-app/README.md` 与 `docs/zh/design/02-wink-micro-os/03-directory-architecture.md`。
- [ ] **Task 4.2**：按模块进行原子 Git Commit：
  - `refactor(apps): reorganize micro apps into ecosystem groups (ADR-0079)`
  - `build(cmake): decouple root paths for nested micro apps`
  - `docs: update app directory layout and user guides`

---

## 5. 风险评估与回滚策略

### 5.1 风险评估
1. **历史缓存干扰风险**：CMake 缓存（如 `build-host/CMakeCache.txt`）可能记录了旧的绝对路径导致配置报错。
   - **应对措施**：迁移完成后建议清空旧的构建目录，执行干净配置。
2. **同名应用歧义风险**：如果未来不同分组引入了相同短名（如 `native/button_led` 与 `mcs51/button_led`），短名别名会触发 ADR-0079 规定的歧义报警。
   - **应对措施**：目前全树所有叶子名（`avoidance_car`, `oled_dashboard`, `button_led`, `health_pot`, `blink` 等）完全唯一；若产生歧义，工具链会清晰列出候选 ID 要求使用全 ID。

### 5.2 回滚策略
本重构全程走 `git mv` 与干净的代码补丁，未引入破坏性删除。在未 push 前如需 100% 撤销：
```bash
git restore --staged .
git checkout .
git clean -fd
```
即可逐字节恢复至平铺初始状态。
