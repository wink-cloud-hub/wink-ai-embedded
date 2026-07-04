# PAL (Platform Abstraction Layer) —— 契约面 + 少量 target 无关的算法 TU。
#
# 三层结构：
#   1. `pal`         INTERFACE ——纯契约面（headers + include dirs, 无 .o）。
#                    存在意义：让所有 downstream `target_link_libraries(... pal)` 保持
#                    向后兼容，且 target 层可"只依赖 header 不拉 .o"。
#   2. `PAL_COMMON_SOURCES` CACHE 变量 —— target 无关的 PAL 翻译单元 SSOT
#                    （目前仅 pal_pwm_router.c）。所有 target 用它替代硬编码路径，
#                    确保新增 target 无关 .c 只需改本文件一处（P1-P3 整改，2026-07-04）。
#   3. `pal_common`  OBJECT ——同一批 .c 的 OBJECT 库形式，供**顶层可执行文件**
#                    （wink_simulator）直接 `$<TARGET_OBJECTS:pal_common>` 引用。
#                    ⚠️ 不能嵌套进 pal_host OBJECT 的 sources —— OBJECT 库不递归
#                    传递其他 OBJECT 的 .o，会导致 pal_host consumer 处未定义符号。
#                    pal_host 通过 ${PAL_COMMON_SOURCES} 直接编译，避免嵌套。

add_library(pal INTERFACE)

# 暴露 PAL 契约面给所有链 pal 的组件。
# Phase 1 目录重组：include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中。
# 这样保持向后兼容：现有代码的 #include "pal_hal.h" / #include "pal_osal.h" 无需修改。
target_include_directories(pal INTERFACE
    include
    include/osal
    include/hal
)

# 自动生成的配置头文件（wink_config.h）路径，所有组件自动继承
target_include_directories(pal INTERFACE ${WINK_CONFIG_DIR})

# ── target-independent PAL 源文件 SSOT ────────────────────────────────
# 新增 target 无关 .c 只在此处追加一行；所有 target CMakeLists 自动生效。
# 用 CACHE INTERNAL 让 add_subdirectory 边界外的（如根 CMakeLists 的 wink_simulator）
# 也能读取。
set(PAL_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pal_pwm_router.c
    CACHE INTERNAL "wink-micro-os: target-independent PAL sources SSOT")

# ── pal_common OBJECT ─────────────────────────────────────────────────
# 便利 alias：允许顶层可执行文件用 $<TARGET_OBJECTS:pal_common> 直接拉入。
# 与 ${PAL_COMMON_SOURCES} 引用同一批 .c，语义等价（同一 TU 只编一次）。
# host target 目前直接消费 ${PAL_COMMON_SOURCES}，未使用本 OBJECT（不能嵌套 OBJECT）。
add_library(pal_common OBJECT ${PAL_COMMON_SOURCES})
target_link_libraries(pal_common PUBLIC pal)
