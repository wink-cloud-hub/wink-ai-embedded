# PAL (Platform Abstraction Layer) —— 纯契约 INTERFACE 库（无 .c，无符号）。
# 实现全部下沉到 targets/<platform>/（见 03-directory-architecture.md §4/§5）。
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
