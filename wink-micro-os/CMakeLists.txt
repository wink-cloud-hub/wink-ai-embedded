cmake_minimum_required(VERSION 3.15)
project(wink-micro-os C CXX)

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

# ── App 注入根（须在 wink_config.h codegen 之前解析）──────────────────
# Dual-Mode SDK Phase 1: config 只读 App 目录下的 wink-app.json，禁止写死
# monorepo 根 ../wink-app.json（解压 SDK 后该路径不存在）。
#
# 相对路径相对「SDK 的父目录」（workspace 根）解析，以便
#   cmake -S wink-micro-os -DWINK_APP_DIR=wink-micro-app/avoidance_car
# 与 wink.py 传入的绝对路径都能工作。
if(NOT DEFINED WINK_APP_DIR OR WINK_APP_DIR STREQUAL "")
    set(WINK_APP_DIR "wink-micro-app/avoidance_car"
        CACHE PATH "Path to App directory containing wink-app.json")
endif()
get_filename_component(_WINK_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT IS_ABSOLUTE "${WINK_APP_DIR}")
    # Paths starting with "." are relative to the SDK root (e.g. ExternalProject
    # -DWINK_APP_DIR=../wink-micro-app/unisim_smoke). Other relative paths are
    # workspace-relative (e.g. wink-micro-app/avoidance_car).
    if(WINK_APP_DIR MATCHES "^[.]")
        get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE
            BASE_DIR "${_WINK_WORKSPACE_ROOT}")
    endif()
else()
    get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE)
endif()
set(WINK_APP_JSON "${WINK_APP_DIR}/wink-app.json")
if(NOT EXISTS "${WINK_APP_JSON}")
    message(STATUS "WINK_APP_JSON not found at: ${WINK_APP_JSON} — Using default configuration.")
    set(WINK_APP_JSON "")
endif()
message(STATUS "WINK_APP_DIR = ${WINK_APP_DIR}")
message(STATUS "WINK_APP_JSON = ${WINK_APP_JSON}")

# ── SDK Mode Detection (early: config codegen needs binary ceilings) ───
if(NOT DEFINED WINK_SDK_MODE OR WINK_SDK_MODE STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/host")
        set(WINK_SDK_MODE "binary")
    else()
        set(WINK_SDK_MODE "source")
    endif()
endif()
message(STATUS "WINK_SDK_MODE = ${WINK_SDK_MODE}")

if(WINK_SDK_MODE STREQUAL "binary" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/host")
    message(FATAL_ERROR
        "WINK_SDK_MODE=binary but libs/host/ not found in SDK tree. "
        "Use a Binary SDK tarball or switch to --sdk-mode source.")
endif()
if(WINK_SDK_MODE STREQUAL "source" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDK_MANIFEST.txt")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/SDK_MANIFEST.txt" _sdk_manifest_lines)
    foreach(_line ${_sdk_manifest_lines})
        if(_line MATCHES "^mode=binary")
            message(FATAL_ERROR
                "WINK_SDK_MODE=source but SDK_MANIFEST.txt says mode=binary. "
                "This is a Binary SDK; use --sdk-mode binary.")
        endif()
    endforeach()
endif()

if(WINK_SDK_MODE STREQUAL "binary")
    set(_CONFIG_SDK_MODE_ARGS --sdk-mode binary)
else()
    set(_CONFIG_SDK_MODE_ARGS)
endif()

# ── 配置生成层 (Task 0: codegen/config_h.py) ──────────────────────────
# Generate wink_config.h from wink_app.json - SSOT for runtime parameters
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WINK_CONFIG_DIR ${CMAKE_BINARY_DIR}/generated)
set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)

set(_CONFIG_H_DEPS ${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/config_h.py)
if(NOT WINK_APP_JSON STREQUAL "")
    list(APPEND _CONFIG_H_DEPS ${WINK_APP_JSON})
endif()

add_custom_command(
    OUTPUT ${WINK_CONFIG_H}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/config_h.py
            --input "${WINK_APP_JSON}"
            --output ${WINK_CONFIG_H}
            --target ${TARGET_PLATFORM}
            ${_CONFIG_SDK_MODE_ARGS}
    DEPENDS ${_CONFIG_H_DEPS}
    COMMENT "Generating wink_config.h"
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

# ── C++ 子集裁剪编译选项 (ADR-0036) ──────────────────────────────────
# 仅对 C++ 源文件应用异常、RTTI 和静态线程安全初始化锁的禁用选项。
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
elseif(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/GR->) # MSVC 禁用 RTTI
endif()

# 注：GCC 参数化警告 -Wstack-usage= 不支持 -Werror=stack-usage= 精确升级。
# 云端构建服务（Docker worker）在 App 编译阶段使用全局 -Werror 将其升级为硬错误。
# 本地开发环境仅产出 warning，不阻塞迭代（生产部署由 build service 门禁拦截）。

# ── Protothread Footgun Detector (ADR-0007) ─────────────────────────
# Catch the #1 protothread bug AT BUILD TIME, not runtime!
# Detects non-static auto variables inside functions that yield.
add_custom_target(
    check_pt_footguns ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint/check_pt_variables.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "🔍 Scanning for protothread auto variable footguns..."
    VERBATIM
)

# 收集子组件构建
if(WINK_SDK_MODE STREQUAL "binary")
    # BINARY mode: import precompiled libwink_micro_os.a + public headers.
    # Skip add_subdirectory for pal/dal/trace/runtime/bal — the binary import
    # creates INTERFACE aliases that forward to the IMPORTED static library.
    include(${CMAKE_CURRENT_SOURCE_DIR}/targets/host/wink_binary_import.cmake)
else()
    # SOURCE mode: build everything from source (monorepo or Source SDK).
    add_subdirectory(pal)
    add_subdirectory(dal)
    add_subdirectory(trace)
    add_subdirectory(runtime)
    add_subdirectory(bal)     # BAL (Business Abstraction Layer) — ADR-0023
    add_subdirectory(frameworks)

    # Ensure checker runs before wink_runtime compiles (must come AFTER runtime subdir)
    add_dependencies(wink_runtime check_pt_footguns)
endif()

# 根据目标平台配置可执行程序或测试用例 (主要针对 WebAssembly / Wasm 仿真)
if(TARGET_PLATFORM STREQUAL "wasm" AND EMSCRIPTEN)
    if(WINK_SDK_MODE STREQUAL "binary")
        # BINARY+wasm: import precompiled libwink_micro_os.a (contains PAL wasm
        # objects + component libs).  Only App sources need compiling.
        include(${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_binary_import.cmake)
        message(STATUS "WINK_SDK_MODE = binary (Wasm Binary SDK consumer)")

        add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

        if(NOT DEFINED WINK_APP_SOURCES OR "${WINK_APP_SOURCES}" STREQUAL "")
            message(FATAL_ERROR
                "WINK_APP_SOURCES not exported from ${WINK_APP_DIR}. "
                "Its CMakeLists.txt must call set(WINK_APP_SOURCES ... PARENT_SCOPE) "
                "before any EMSCRIPTEN return().")
        endif()

        add_executable(wink_simulator ${WINK_APP_SOURCES})
        target_link_libraries(wink_simulator PRIVATE wink_micro_os)
        target_include_directories(wink_simulator PRIVATE
            ${WINK_APP_DIR}
            ${WINK_APP_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/input
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/output
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/actuator
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/display
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/sensor
            ${WINK_CONFIG_DIR})
    else()
    # SOURCE+wasm: build PAL sources from source (monorepo or Source SDK).
    # 让 targets/wasm/CMakeLists.txt 作为 wasm PAL 源列表的 SSOT。
    # 该子目录通过 PARENT_SCOPE 导出 PAL_WASM_SOURCES / WASM_ENTRY_SOURCE / WASM_EXTRA_INCLUDE_DIRS。
    # ADR-0009 Wave 2：算法库 wink_sim_physical.c 在子目录里追加，本顶层无需关心具体源文件。
    add_subdirectory(targets/wasm)

    # §6.1 约束3：标准 App 注入接口（WINK_APP_DIR 已在文件顶部解析）。
    # App CMakeLists 在 EMSCRIPTEN 分支下只 set(WINK_APP_SOURCES ... PARENT_SCOPE)
    # 即 return()，不定义 host e2e target（那些依赖 pal_host / test_*_e2e.c）。
    add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

    # 兜底契约：hw-only sample 已用 FATAL_ERROR 硬拦截；此处覆盖第三方 App
    # 忘记 set(WINK_APP_SOURCES ... PARENT_SCOPE) 的场景，给出可诊断的错误。
    if(NOT DEFINED WINK_APP_SOURCES OR "${WINK_APP_SOURCES}" STREQUAL "")
        message(FATAL_ERROR
            "WINK_APP_SOURCES not exported from ${WINK_APP_DIR}. "
            "Its CMakeLists.txt must call set(WINK_APP_SOURCES ... PARENT_SCOPE) "
            "before any EMSCRIPTEN return(). See samples/avoidance_car/CMakeLists.txt.")
    endif()

    # ── Wasm export manifest SSOT ─────────────────────────────────────────
    # Read targets/wasm/exported_runtime_functions.json at configure time
    # so the JSON is the single source of truth for link options.
    set(_WASM_EXPORT_JSON "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/exported_runtime_functions.json")
    set(_WASM_EXPORT_CMAKE "${CMAKE_BINARY_DIR}/wasm_export_options.cmake")
    set(_WASM_EXPORT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tools/wasm_export_codegen.py")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${_WASM_EXPORT_SCRIPT}"
                "${_WASM_EXPORT_JSON}" "${_WASM_EXPORT_CMAKE}"
        RESULT_VARIABLE _wasm_json_rc)
    if(NOT _wasm_json_rc EQUAL 0)
        message(FATAL_ERROR "Failed to parse ${_WASM_EXPORT_JSON}")
    endif()
    include(${_WASM_EXPORT_CMAKE})

    add_executable(wink_simulator
        ${PAL_WASM_SOURCES}
        ${WASM_ENTRY_SOURCE}
        ${WINK_APP_SOURCES}                # ← App 注入（Task 1.2）
        # pal_pwm_router.c 现由 pal_common OBJECT 库贡献（P1-P3，2026-07-04）
        $<TARGET_OBJECTS:pal_common>)

    # ADR-0017/0025 Stage 5: sim target defaults to STRICT_NONBLOCKING=1 for
    # app source files. PAL sources are NOT affected (they implement the
    # blocking APIs). Escape hatch: -DWINK_STRICT_NONBLOCKING=0.
    set(WINK_STRICT_NONBLOCKING 1 CACHE STRING "Enable strict nonblocking for app sources")
    if(WINK_STRICT_NONBLOCKING)
        set_source_files_properties(${WINK_APP_SOURCES} PROPERTIES
            COMPILE_DEFINITIONS WINK_STRICT_NONBLOCKING=1)
    endif()

    # ADR-0017 Stage 5 Task 5.3: LIGHT-context blocking assertions → hard fault
    # (assert/abort) in sim debug builds. ASSERTIONS=1 is already enabled below.
    target_compile_definitions(wink_simulator PRIVATE WINK_PT_DEBUG)
    # 注：wink_dev_config.c 已迁至 runtime/src/，随 wink_runtime STATIC 库链入，
    # 无需在此显式列出（旧位置 pal/src/wink_dev_config.c 已删除）。
    # 契约断言：wink_simulator 必须 link wink_runtime（否则 wasm_entry.c 里的
    # wink_app.h 找不到、wink_runtime_run 也 undef）。link 顺序无关紧要，这里
    # 显式列出便于人肉 grep 时定位。
    target_link_libraries(wink_simulator PRIVATE pal dal wink_runtime wink_bal)
    target_include_directories(wink_simulator PRIVATE
        ${WASM_EXTRA_INCLUDE_DIRS}
        # App 本身的 header（如 device_tree.h）
        ${WINK_APP_DIR}
        # App codegen 输出目录（如 oled_dashboard 使用 codegen 生成 device_tree.h）
        ${WINK_APP_INCLUDE_DIRS}
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
        "-sASYNCIFY_IMPORTS=[${WASM_ASYNCIFY_IMPORTS}]"
        # Asyncify 备份栈配置（P1-1 CI 安全水位门控）：
        #   - ASYNCIFY_STACK_SIZE=65536 (64 KiB)：每个 fiber Asyncify 挂起/恢复时
        #     的 unwind/rewind 备份栈容量。过小会静默破坏线性内存；过大浪费。
        #   - EXPORTED_RUNTIME_METHODS 包含 'Asyncify' 让 JS 可调用
        #     Asyncify.getStackMax()，e2e 跑完后断言高水位 < 80% (≈51.2 KiB)。
        #   - CI 阈值与此处 STACK_SIZE 强耦合：修改此值须同步更新 CI 脚本中的
        #     80% 计算常量，否则门控会误报/漏报。
        #   - 64 KiB 是当前 AI 生成调用链深度的起步值；若未来 getStackMax()
        #     持续贴近 80%，应同步放大此值而非放宽 CI 阈值。
        "-sASYNCIFY_STACK_SIZE=${WASM_ASYNCIFY_STACK_SIZE}"
        "-sEXPORTED_FUNCTIONS=[${WASM_EXPORT_FUNCTIONS}]"
        "-sEXPORTED_RUNTIME_METHODS=[${WASM_EXPORT_RUNTIME}]"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=${WASM_EXPORT_NAME}"
        "-sWASM_BIGINT=1"               # ADR-0009 Wave 2：64 位整型 ↔ JS BigInt 原生映射，杜绝 number 隐式失精
        "-sSTACK_OVERFLOW_CHECK=2"      # dev/debug：栈溢出 abort，避免静默线性内存越界（亦为中断重入早期报警，见 plan Phase 1 Task 1-5）
        "-sASSERTIONS=1")               # dev/debug：带运行时开销/体积代价，生产 profile 调优留作后续

    # 触发 wink_sim_js.js 变化后重 link；否则 CMake 不知道 --js-library=<path>
    # 是构建输入，改 stub 后 make 会认为无事发生。
    set_property(TARGET wink_simulator APPEND PROPERTY LINK_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_js.js
        ${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/exported_runtime_functions.json)

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
    endif()  # SOURCE+wasm / BINARY+wasm inner branch
endif()

# ---- host 单元测试（非 wasm target 时启用）----
if(NOT TARGET_PLATFORM STREQUAL "wasm")
    enable_testing()

    if(WINK_SDK_MODE STREQUAL "binary")
        # BINARY mode: pal_host already provided as INTERFACE IMPORTED by
        # wink_binary_import.cmake; skip the OBJECT library in targets/host/.
        message(STATUS "BINARY mode: skipping targets/host/ (pal_host from precompiled .a)")
    else()
        add_subdirectory(targets/host)      # host 一等 target，供 test 链接
    endif()

    # samples/common (ADR-0023 Stage 2): INTERFACE forwarding-shim library.
    # All helpers have migrated to BAL (blink/button/telemetry) or
    # runtime/selftest (sim_echo); only #include-path shims remain. Must be
    # added before the samples that target_link_libraries() against it.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/common)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/common ${CMAKE_BINARY_DIR}/common_build)
    elseif(EXISTS ${WINK_APP_DIR}/../common)
        add_subdirectory(${WINK_APP_DIR}/../common ${CMAKE_BINARY_DIR}/common_build)
    elseif(EXISTS ${WINK_APP_DIR}/common)
        add_subdirectory(${WINK_APP_DIR}/common ${CMAKE_BINARY_DIR}/common_build)
    endif()

    # §6.1 约束3：标准 App 注入接口（WINK_APP_DIR 已在文件顶部解析）。
    if(EXISTS ${WINK_APP_DIR})
        add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)
    else()
        message(FATAL_ERROR "WINK_APP_DIR does not exist: ${WINK_APP_DIR}")
    endif()

    # Extra sample apps — skipped in BINARY mode (they reference source paths
    # like test/ and runtime/src/ that don't exist in the binary SDK tree).
    if(NOT WINK_SDK_BINARY_MODE)
        macro(wink_add_sample_subdirectory name)
            get_filename_component(_abs_sample_dir "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/${name}" ABSOLUTE)
            if(EXISTS "${_abs_sample_dir}")
                if(NOT "${WINK_APP_DIR}" STREQUAL "${_abs_sample_dir}")
                    add_subdirectory("${_abs_sample_dir}" "${CMAKE_BINARY_DIR}/${name}_build")
                endif()
            endif()
        endmacro()

        # oled_dashboard
        wink_add_sample_subdirectory(oled_dashboard)

        # DevKitC 冒烟测试：裸板真机 S1-S8 全链路验证
        wink_add_sample_subdirectory(devkitc_smoke)

        # Track A M1 / Task A-3：资源占用冲突反例样本（host-only 断言样本）
        wink_add_sample_subdirectory(resource_conflict)

        # Cooperative multi-task scheduler demo App
        wink_add_sample_subdirectory(dual_task_demo)

        # Arduino Compat Layer E2E: standard Arduino sketch (Blink + Serial)
        # running on WinkMicroOS via wink_arduino_compat leaf-node linking.
        wink_add_sample_subdirectory(arduino_blink_demo)
    endif()  # NOT WINK_SDK_BINARY_MODE

    # Host unit / e2e tests live under test/. Source SDK tarballs omit this tree
    # (Dual-Mode SDK Phase 1); skip rather than FATAL when packaging for M2.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
        add_subdirectory(test)
    else()
        message(STATUS "Skipping test/: not present in this SDK tree (Source SDK / M2)")
    endif()

    # Wasm node smoke: use emcmake to build the unisim_smoke variant as an
    # ExternalProject, then run it under `node wink_sim_stub.js` from ctest.
    # This gives the host build a single automated wasm-runtime regression test;
    # ESP32 remains a manual burn step. Skipped gracefully when node/emcmake is
    # not on PATH (see wasm_node_smoke.cmake for details).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wasm_node_smoke.cmake")
        include(targets/wasm/wasm_node_smoke.cmake)
    endif()
endif()
