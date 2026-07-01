# ESP32 真机端口（ESP-IDF 组件模式）。
# host/wasm 构建不包含本目录。

# 引用核心源文件列表（DRY，避免重复硬编码）
if(NOT WINK_MICRO_OS_ROOT)
    set(WINK_MICRO_OS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
endif()
include(${WINK_MICRO_OS_ROOT}/core_sources.cmake)

# IDF 组件上下文判定：ESP_PLATFORM 由 ESP-IDF 在组件处理时定义，是"当前正作为 IDF 组件被构建"
# 的可靠信号。原门控 if(TARGET_PLATFORM STREQUAL "esp32") 依赖调用方在根 CMakeLists 设置
# TARGET_PLATFORM，但 IDF 组件在独立作用域内处理，未必继承该 cache 变量，导致组件静默不注册
# （现象：组件不出现在 Components 列表、app_main 找不到 wink 头文件）。改用 ESP_PLATFORM 判定。
if(ESP_PLATFORM)
    # wink_config.h 由 codegen 生成（add_custom_command 见 register 之后）。
    # 此处仅定义其输出目录：INCLUDE_DIRS 要引用，且 set 在 requirements 脚本通下可脚本化。
    set(WINK_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    # idf_component_register 校验 INCLUDE_DIRS 必须是已存在目录（component.cmake:320），
    # 而 generated/ 要等构建期 codegen 才创建。此处预创建空目录以满足校验，头文件稍后由 codegen 填入。
    file(MAKE_DIRECTORY ${WINK_CONFIG_DIR})

    # ESP-IDF 环境：注册为 IDF 组件
    idf_component_register(
        SRCS
            # ESP32 PAL 实现
            pal_hal_esp32.c
            pal_hal_esp32_gpio.c
            pal_irq_esp32.c
            pal_hal_esp32_rmt.c
            pal_hal_ultrasonic.c
            pal_osal_esp32.c
            pal_resource_esp32.c
            pal_storage_esp32.c
            ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
            ${WINK_MICRO_OS_ROOT}/pal/src/wink_dev_config.c
            # PLAN-20260701-PAL-TARGET-P1-MAINT Task 1：共享中断责任链算法层（target-private）
            ${WINK_MICRO_OS_ROOT}/targets/common/src/pal_shared_chain.c
            # 核心共享源文件（来自 core_sources.cmake）
            ${WINK_CORE_SOURCES}
        INCLUDE_DIRS
            ${WINK_CORE_INCLUDE_DIRS}
            # target-private：pal_shared_chain.h **不进入** WINK_CORE_INCLUDE_DIRS，
            # 只对 targets/esp32/ 下的实现 TU 可见，避免泄漏到 DAL / runtime。
            ${WINK_MICRO_OS_ROOT}/targets/common/include
            ${WINK_CONFIG_DIR}    # codegen 生成的 wink_config.h 所在目录
        REQUIRES
            # ESP-IDF v6.x: driver was split into fine-grained components
            # v5.x uses 'driver' monolith; v6.x requires esp_driver_*
            driver
            esp_driver_gpio
            esp_driver_rmt
            esp_driver_ledc
            esp_driver_i2c
            esp_timer
            esp_system
            # freertos/ringbuf.h + xRingbuffer* API 属于 esp_ringbuf 组件（不在 freertos 组件内），
            # pal_osal_esp32.c 的 pal_ringbuf 跨核通信需要。host/wasm 构建不含本目录，仅 ESP-IDF 需要。
            esp_ringbuf
            # ADR-0008：pal_storage_esp32.c 的 NVS 存取需要 nvs_flash 组件
            nvs_flash
    )
    # 输出组件库目标
    set(ESP32_PAL_LIB ${COMPONENT_LIB} CACHE INTERNAL "")
    message(STATUS "wink esp32 target registered as IDF component")

    # ── wink_config.h codegen（必须在 idf_component_register 之后）──────────────
    # ESP-IDF 收集组件依赖时，会用 cmake -P 脚本模式 include 本文件以抽取
    # SRCS/INCLUDE_DIRS/REQUIRES；register 前的代码会在脚本通执行（仅 set 等可脚本化
    # 命令），register 后的代码只在真实配置阶段执行，add_custom_command 才可用。
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)
    add_custom_command(
        OUTPUT ${WINK_CONFIG_H}
        COMMAND ${Python3_EXECUTABLE} ${WINK_MICRO_OS_ROOT}/tools/codegen_config.py
                --input ${WINK_MICRO_OS_ROOT}/../wink-app.json
                --output ${WINK_CONFIG_H}
                --target esp32
        DEPENDS ${WINK_MICRO_OS_ROOT}/../wink-app.json
                ${WINK_MICRO_OS_ROOT}/tools/codegen_config.py
        COMMENT "Generating wink_config.h from wink_app.json (esp32)"
        VERBATIM
    )
    add_custom_target(wink_generate_config DEPENDS ${WINK_CONFIG_H})
    add_dependencies(${COMPONENT_LIB} wink_generate_config)

    # Windows GCC 默认按 GBK 解析源文件，中文 UTF-8 注释会乱码报错
    # 强制指定源文件编码为 UTF-8，解决中文注释编译错误
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
elseif(TARGET_PLATFORM STREQUAL "esp32")
    # 非 ESP-IDF 环境：仅声明源文件（供静态分析）
    set(ESP32_PAL_SOURCES
        pal_hal_esp32.c
        pal_hal_esp32_gpio.c
        pal_irq_esp32.c
        pal_hal_esp32_rmt.c
        pal_hal_ultrasonic.c
        pal_osal_esp32.c
        pal_resource_esp32.c
        pal_storage_esp32.c
        ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
        ${WINK_MICRO_OS_ROOT}/pal/src/wink_dev_config.c
        ${WINK_MICRO_OS_ROOT}/targets/common/src/pal_shared_chain.c
        ${WINK_CORE_SOURCES}
    )
    message(STATUS "wink esp32 target configured (static analysis)")
endif()
