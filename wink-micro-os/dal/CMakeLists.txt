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
