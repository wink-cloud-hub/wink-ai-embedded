# wink-micro-os 核心源文件列表（供各 target 共享引用）
# 避免 host/wasm/esp32 各 target 重复硬编码源文件路径
# 使用方式：在各 target CMakeLists.txt 中 include 本文件

# ── Runtime 源文件 ──────────────────────────────────────────────────────────
set(WINK_RUNTIME_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_soft_timer.c
)

# ── Trace 源文件 ──────────────────────────────────────────────────────────
set(WINK_TRACE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/trace/src/wink_trace.c
)

# ── DAL 源文件 ──────────────────────────────────────────────────────────
# Phase 3 分类组织：sensor / actuator / output / input / display
set(WINK_DAL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/sensor/dal_ultrasonic.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/actuator/dal_servo.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/output/dal_led.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/input/dal_button.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/display/dal_ssd1306.c
)

# ── 核心包含目录 ──────────────────────────────────────────────────────────
# Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中。
# 这样保持向后兼容：现有代码的 #include "pal_hal.h" / #include "pal_osal.h" 无需修改。
set(WINK_CORE_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/pal/include
    ${CMAKE_CURRENT_LIST_DIR}/pal/include/osal
    ${CMAKE_CURRENT_LIST_DIR}/pal/include/hal
    ${CMAKE_CURRENT_LIST_DIR}/dal/include
    ${CMAKE_CURRENT_LIST_DIR}/runtime/include
    ${CMAKE_CURRENT_LIST_DIR}/trace/include
)

# ── 聚合所有核心源文件 ──────────────────────────────────────────────────────────
set(WINK_CORE_SOURCES
    ${WINK_RUNTIME_SOURCES}
    ${WINK_TRACE_SOURCES}
    ${WINK_DAL_SOURCES}
)
