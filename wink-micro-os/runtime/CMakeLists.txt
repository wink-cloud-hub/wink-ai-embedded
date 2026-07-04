# runtime (OS 运行时) —— STATIC 库，回调注入主循环 + 执行器关断注册表（Phase 5）。
add_library(wink_runtime STATIC
    src/wink_runtime.c
    src/wink_actuator_registry.c
    src/wink_soft_timer.c
    # ADR-0008 设备树覆写 blob 解析器 + CRC32：runtime 层通用工具（无硬件依赖），
    # 从 pal/src/ 迁出以修复层级反转（PAL 不应含设备树/CRC 领域概念）。
    src/wink_dev_config.c
)

# Ensure wink_config.h is generated before compiling wink_runtime
add_dependencies(wink_runtime generate_config)

target_include_directories(wink_runtime PUBLIC include)
target_link_libraries(wink_runtime PUBLIC pal wink_trace)   # pal: OSAL; wink_trace: fault 上报

# pal_wasm_internal.h is referenced by wink_runtime.c under SIMULATION for poll-based interrupt dispatch
# Header has proper SIMULATION guards, so always add to path to allow compilation on all platforms
# pal_wasm_internal.h also includes wink_sim_physical.h from targets/common/include
target_include_directories(wink_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include)

# MSVC: disable C4100 (unused parameters) — intentional in HAL contract; matches GCC -Wno-unused-parameter
if(MSVC)
    target_compile_options(wink_runtime PRIVATE /wd4100)
endif()
