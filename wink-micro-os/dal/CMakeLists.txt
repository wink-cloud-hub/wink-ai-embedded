# DAL (Device Abstraction Layer) Component CMake
add_library(dal STATIC)

# 暴露包含路径给其他组件使用
# Phase 3：所有分类子目录均在搜索路径中，保持向后兼容
target_include_directories(dal PUBLIC
    include
    include/input
    include/output
    include/actuator
    include/display
    include/sensor
    include/communication
    include/storage
)

# DAL 源码通过 `wink_pt_debug.h` 拿到 WINK_ASSERT_NONBLOCKING()（ADR-0017 层 3）。
# 该头存放在 runtime/include/（2026-07-04 层级修正：从 pal/include/wink_status.h 迁出）。
# 仅需 include 路径，不引入 link 依赖（extern wink_pt_in_context/wink_trace_fault 由
# 各 target 的 OSAL + wink_trace 提供，可执行链接期解析）。
target_include_directories(dal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include
)

# DAL 依赖 PAL
target_link_libraries(dal PUBLIC pal)

# 声明 DAL 层的源文件（Phase 3 分类组织）
target_sources(dal PRIVATE
    # input: 输入器件
    include/input/dal_button.h
    src/input/dal_button.c
    # output: 输出器件
    include/output/dal_led.h
    src/output/dal_led.c
    # actuator: 执行器
    include/actuator/dal_servo.h
    src/actuator/dal_servo.c
    # display: 显示器驱动
    include/display/dal_ssd1306.h
    src/display/dal_ssd1306.c
    # sensor: 传感器
    include/sensor/dal_ultrasonic.h
    src/sensor/dal_ultrasonic.c
    # communication: 通信外设（Phase 3 新增）
    include/communication/dal_gps.h
    src/communication/dal_gps.c
    # storage: 存储外设（Phase 3 新增）
    include/storage/dal_eeprom.h
    src/storage/dal_eeprom.c
)
