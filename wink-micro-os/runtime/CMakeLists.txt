# runtime (OS 运行时) —— STATIC 库，回调注入主循环 + 执行器关断注册表（Phase 5）。
add_library(wink_runtime STATIC
    src/wink_runtime.c
    src/wink_actuator_registry.c
    src/wink_soft_timer.c
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
