# PAL (Platform Abstraction Layer) —— 纯契约 INTERFACE 库（无 .c，无符号）。
# 实现全部下沉到 targets/<platform>/（见 03-directory-architecture.md §4/§5）。
add_library(pal INTERFACE)

# 暴露 PAL 契约面（pal.h / pal_hal.h / pal_osal.h / wink_status.h）给所有链 pal 的组件。
target_include_directories(pal INTERFACE include)

# 自动生成的配置头文件（wink_config.h）路径，所有组件自动继承
target_include_directories(pal INTERFACE ${WINK_CONFIG_DIR})
