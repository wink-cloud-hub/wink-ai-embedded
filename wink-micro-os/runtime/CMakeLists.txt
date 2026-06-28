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

# MSVC: disable C4100 (unused parameters) — intentional in HAL contract; matches GCC -Wno-unused-parameter
if(MSVC)
    target_compile_options(wink_runtime PRIVATE /wd4100)
endif()
