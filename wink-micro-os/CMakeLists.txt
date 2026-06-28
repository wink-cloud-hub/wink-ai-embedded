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
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wstack-usage=${WINK_STACK_USAGE_LIMIT})
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
    add_executable(wink_simulator
        targets/wasm/pal_hal_wasm.c
        targets/wasm/pal_osal_wasm.c
        targets/wasm/pal_resource_wasm.c
        targets/wasm/pal_storage_wasm.c
        targets/wasm/wasm_entry.c
        pal/src/pal_pwm_router.c
        pal/src/wink_dev_config.c)
    target_link_libraries(wink_simulator PRIVATE pal dal)
    target_link_options(wink_simulator PRIVATE
        "-s" "ASYNCIFY=1"
        "-s" "ASYNCIFY_IMPORTS=['js_pal_delay_ms','js_pal_delay_us']"
        "-s" "EXPORTED_FUNCTIONS=['_main']"                  # _trigger_wasm_interrupt 已移除（方案 C：Poll 模型消除 Push 导出，见 wasm_entry.c）
        "-s" "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-s" "MODULARIZE=1"
        "-s" "EXPORT_NAME='WasmSandbox'"
        "-s" "STACK_OVERFLOW_CHECK=2"      # dev/debug：栈溢出 abort，避免静默线性内存越界（亦为中断重入早期报警，见 plan Phase 1 Task 1-5）
        "-s" "ASSERTIONS=1"                # dev/debug：带运行时开销/体积代价，生产 profile 调优留作后续
        "-s" "ASYNCIFY_STACK_SIZE=65536")  # 起步值；最终须实测最深 AI 生成调用链（plan Task 1-4 清单第 6 项）
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

    add_subdirectory(test)
endif()
