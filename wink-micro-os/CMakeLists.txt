cmake_minimum_required(VERSION 3.15)
project(wink-micro-os C)

# 设置 C 标准为 C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 定义目标编译平台变量，默认为 WASM 仿真环境
set(TARGET_PLATFORM "wasm" CACHE STRING "Target hardware platform: wasm, esp32, stm32")
message(STATUS "Configuring wink-micro-os for platform: ${TARGET_PLATFORM}")

# 通用全局宏定义
add_compile_definitions(PLATFORM_${TARGET_PLATFORM})
if(TARGET_PLATFORM STREQUAL "wasm")
    add_compile_definitions(SIMULATION=1)
endif()

# ── 配置生成层 (Task 0: codegen_config.py) ──────────────────────────
# Generate wink_config.h from wink_app.json - SSOT for runtime parameters
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WINK_CONFIG_DIR ${CMAKE_BINARY_DIR}/generated)
set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)

add_custom_command(
    OUTPUT ${WINK_CONFIG_H}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen_config.py
            --input ${CMAKE_CURRENT_SOURCE_DIR}/../wink-app.json
            --output ${WINK_CONFIG_H}
            --target ${TARGET_PLATFORM}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../wink-app.json
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen_config.py
    COMMENT "Generating wink_config.h from wink_app.json"
    VERBATIM
)

add_custom_target(generate_config DEPENDS ${WINK_CONFIG_H})
message(STATUS "Configuration will be generated at: ${WINK_CONFIG_H}")

# ── 栈深度静态门禁（P-stack：AI Codegen 栈安全）──────────────────
# AI Codegen 可能生成深调用链。使用 -Wstack-usage 在编译期硬性拦截。
# 阈值说明：
#   - ESP32 FreeRTOS task 栈常为 4KB，App 回调需留 50% 余量 → ~1536 字节
#   - Wasm Asyncify unwind 栈 64KB，但统一收紧以保证双 target 可移植
#   - 该值仅限制单个函数的栈帧，不包含调用链累加（GCC 静态分析局限性）
#   - 配合 .clang-tidy misc-no-recursion 禁止递归，可有效约束总栈消耗
set(WINK_STACK_USAGE_LIMIT 1536 CACHE STRING
    "Per-function stack usage hard limit in bytes (-Wstack-usage)")
# Only apply to GCC/Clang (MSVC does not support -Wstack-usage)
# GCC 与 Clang 的等价选项不同：
#   - GCC:   -Wstack-usage=<N>       （支持精确到函数级、可关联到 .su 报告）
#   - Clang: -Wframe-larger-than=<N> （emcc 底层为 clang，只识别此形式）
# 之前统一用 -Wstack-usage 会让 emcc（Clang）报 "unknown warning option"，
# 从而阻断 wasm build。这里按编译器 ID 分派，双 target 皆有等价栈门禁。
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wstack-usage=${WINK_STACK_USAGE_LIMIT})
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wframe-larger-than=${WINK_STACK_USAGE_LIMIT})
elseif(MSVC)
    # Enable UTF-8 source/execution character set for MSVC to prevent C4819 encoding warnings/errors
    add_compile_options(/utf-8)
endif()
# 注：GCC 参数化警告 -Wstack-usage= 不支持 -Werror=stack-usage= 精确升级。
# 云端构建服务（Docker worker）在 App 编译阶段使用全局 -Werror 将其升级为硬错误。
# 本地开发环境仅产出 warning，不阻塞迭代（生产部署由 build service 门禁拦截）。

# ── Protothread Footgun Detector (ADR-0007) ─────────────────────────
# Catch the #1 protothread bug AT BUILD TIME, not runtime!
# Detects non-static auto variables inside functions that yield.
add_custom_target(
    check_pt_footguns ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pt_variables.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "🔍 Scanning for protothread auto variable footguns..."
    VERBATIM
)

# 收集子组件构建
add_subdirectory(pal)
add_subdirectory(dal)
add_subdirectory(trace)
add_subdirectory(runtime)

# Ensure checker runs before wink_runtime compiles (must come AFTER runtime subdir)
add_dependencies(wink_runtime check_pt_footguns)

# 根据目标平台配置可执行程序或测试用例 (主要针对 WebAssembly / Wasm 仿真)
if(TARGET_PLATFORM STREQUAL "wasm" AND EMSCRIPTEN)
    # 让 targets/wasm/CMakeLists.txt 作为 wasm PAL 源列表的 SSOT。
    # 该子目录通过 PARENT_SCOPE 导出 PAL_WASM_SOURCES / WASM_ENTRY_SOURCE / WASM_EXTRA_INCLUDE_DIRS。
    # ADR-0009 Wave 2：算法库 wink_sim_physical.c 在子目录里追加，本顶层无需关心具体源文件。
    add_subdirectory(targets/wasm)

    # §6.1 约束3：标准 App 注入接口。未指定则默认 samples/avoidance_car。
    # App CMakeLists 在 EMSCRIPTEN 分支下只 set(WINK_APP_SOURCES ... PARENT_SCOPE)
    # 即 return()，不定义 host e2e target（那些依赖 pal_host / test_*_e2e.c）。
    if(NOT DEFINED WINK_APP_DIR)
        set(WINK_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples/avoidance_car)
    endif()
    message(STATUS "WINK_APP_DIR = ${WINK_APP_DIR}")
    add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

    # 兜底契约：hw-only sample 已用 FATAL_ERROR 硬拦截；此处覆盖第三方 App
    # 忘记 set(WINK_APP_SOURCES ... PARENT_SCOPE) 的场景，给出可诊断的错误。
    if(NOT DEFINED WINK_APP_SOURCES OR "${WINK_APP_SOURCES}" STREQUAL "")
        message(FATAL_ERROR
            "WINK_APP_SOURCES not exported from ${WINK_APP_DIR}. "
            "Its CMakeLists.txt must call set(WINK_APP_SOURCES ... PARENT_SCOPE) "
            "before any EMSCRIPTEN return(). See samples/avoidance_car/CMakeLists.txt.")
    endif()

    add_executable(wink_simulator
        ${PAL_WASM_SOURCES}
        ${WASM_ENTRY_SOURCE}
        ${WINK_APP_SOURCES}                # ← App 注入（Task 1.2）
        pal/src/pal_pwm_router.c
        pal/src/wink_dev_config.c)
    # 契约断言：wink_simulator 必须 link wink_runtime（否则 wasm_entry.c 里的
    # wink_app.h 找不到、wink_runtime_run 也 undef）。link 顺序无关紧要，这里
    # 显式列出便于人肉 grep 时定位。
    target_link_libraries(wink_simulator PRIVATE pal dal wink_runtime)
    target_include_directories(wink_simulator PRIVATE
        ${WASM_EXTRA_INCLUDE_DIRS}
        # App 本身的 header（如 device_tree.h）
        ${WINK_APP_DIR}
        # DAL 子分类头（供 App 直接 #include "dal_servo.h" 等）
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/sensor
        # 生成的 wink_config.h（App device_tree.c 可能 #include）
        ${WINK_CONFIG_DIR})
    target_link_options(wink_simulator PRIVATE
        # 一元 -sKEY=VALUE 形式：Emscripten 6.x + MinGW Makefiles / Ninja 通用；
        # 若拆成两个 token（"-s" "KEY=VALUE"），MinGW make 会把第二个 token
        # 当成输入文件传给 emcc，报 "No such file or directory"。
        #
        # 允许 C 侧的 extern js_pal_* / js_sim_* 作为 WebAssembly imports 保留在
        # 二进制里、留给 JS 侧提供实现；否则 emcc 会因 undefined symbol 报错。
        "-sERROR_ON_UNDEFINED_SYMBOLS=0"
        # JS-library 桩：编译期把 wasm_bridge.h 里所有 extern js_* 的默认实现
        # 注入到 wink_simulator.js 胶水。宿主（Workbench / node stub）可通过
        # Module.js_* 覆盖，无需重编 wasm。
        "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_js.js"
        "-sASYNCIFY=1"
        "-sASYNCIFY_IMPORTS=['js_pal_os_sleep_ms','js_pal_os_busy_wait_us']"
        "-sEXPORTED_FUNCTIONS=['_main']"              # _trigger_wasm_interrupt 已移除（方案 C：Poll 模型消除 Push 导出，见 wasm_entry.c）
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8']"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=WasmSandbox"
        "-sWASM_BIGINT=1"               # ADR-0009 Wave 2：64 位整型 ↔ JS BigInt 原生映射，杜绝 number 隐式失精
        "-sSTACK_OVERFLOW_CHECK=2"      # dev/debug：栈溢出 abort，避免静默线性内存越界（亦为中断重入早期报警，见 plan Phase 1 Task 1-5）
        "-sASSERTIONS=1"                # dev/debug：带运行时开销/体积代价，生产 profile 调优留作后续
        "-sASYNCIFY_STACK_SIZE=65536")  # 起步值；最终须实测最深 AI 生成调用链（plan Task 1-4 清单第 6 项）

    # 触发 wink_sim_js.js 变化后重 link；否则 CMake 不知道 --js-library=<path>
    # 是构建输入，改 stub 后 make 会认为无事发生。
    set_property(TARGET wink_simulator APPEND PROPERTY LINK_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_js.js)

    # 契约断言：wink_simulator 必须 link wink_runtime。历史上 baca3cf
    # (2026-06-24) 把 wasm_entry.c 改为依赖 wink_runtime_run() 但漏改此处 link
    # 列表，wasm build 因此 broken 7 天没被踩。这条断言让未来的删链回归在
    # configure 阶段就爆掉，而不是留给某个下游踩到"wink_app.h not found"。
    get_target_property(_wink_simulator_link_libs wink_simulator LINK_LIBRARIES)
    if(NOT "wink_runtime" IN_LIST _wink_simulator_link_libs)
        message(FATAL_ERROR
            "wink_simulator must link wink_runtime; wasm_entry.c depends on "
            "wink_runtime_run(). See docs/design/implementation-plans/"
            "2026-07-01-wasm-simulator-target-repair-plan.md §4.2.")
    endif()
endif()

# ---- host 单元测试（非 wasm target 时启用）----
if(NOT TARGET_PLATFORM STREQUAL "wasm")
    enable_testing()
    add_subdirectory(targets/host)      # host 一等 target，供 test 链接

    # §6.1 约束3：标准 App 注入接口。未指定则默认 samples/avoidance_car。
    if(NOT DEFINED WINK_APP_DIR)
        set(WINK_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples/avoidance_car)
    endif()
    message(STATUS "WINK_APP_DIR = ${WINK_APP_DIR}")
    add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

    # Phase 2：OLED Dashboard 示例（额外 e2e target，不改默认 App）
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples/oled_dashboard
                     ${CMAKE_BINARY_DIR}/oled_dashboard_build)

    # DevKitC 冒烟测试：裸板真机 S1-S8 全链路验证
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples/devkitc_smoke
                     ${CMAKE_BINARY_DIR}/devkitc_smoke_build)



    # Track A M1 / Task A-3：资源占用冲突反例样本（host-only 断言样本）
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples/resource_conflict
                     ${CMAKE_BINARY_DIR}/resource_conflict_build)

    # Cooperative multi-task scheduler demo App
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples/dual_task_demo
                     ${CMAKE_BINARY_DIR}/dual_task_demo_build)

    add_subdirectory(test)
endif()
