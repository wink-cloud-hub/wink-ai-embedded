# Wasm 仿真端口构建配置（仅在 EMSCRIPTEN 环境生效）。
# 调用方（顶层 CMake）决定是否链接成 wink_simulator 可执行，本文件只声明源与依赖。
#
# ADR-0009 Wave 2：将 `wink_sim_physical.c` 物理退化算法库纳入 wasm 端构建。
# 该文件位于 targets/common/src/ 下，是 target 无关算法库（host 端 Wave 1 已验证），
# wasm 端只需追加到源列表即可零修改复用——验证了「target 无关算法库」的设计。
#
# 零编译污染（ADR-0009 §4.3）：esp32/baremetal target 不引用此 CMakeLists.txt，
# 因此 wink_sim_physical.c 不会进入它们的编译产物。
set(PAL_WASM_SOURCES
    # HAL 层（含中断桥）
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_hal_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_log_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_irq_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_storage_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_adc.c
    # Wasm 特有扩展：物理退化 + fault 子系统 + Wave3 stub + 虚拟器件仿真
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault_domain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_sim_registry.c
    # Target 无关共享源（同源复用，见 targets/common/src/）
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
)

if(WINK_USE_RC_SERVO)
    list(APPEND PAL_WASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_dev_servo.c)
endif()

if(WINK_USE_ULTRASONIC)
    list(APPEND PAL_WASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_dev_ultrasonic.c)
endif()

set(PAL_WASM_SOURCES ${PAL_WASM_SOURCES} PARENT_SCOPE)

set(WASM_ENTRY_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/wasm_entry.c
    PARENT_SCOPE)

# 算法库 include 目录（wink_sim_physical.h）也需要传到顶层
set(WASM_EXTRA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    PARENT_SCOPE)

# 不在此 add_library/add_executable，避免在非 emcc 环境构建失败。
