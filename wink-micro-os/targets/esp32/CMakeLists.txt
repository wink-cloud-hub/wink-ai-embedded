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

    # ESP-IDF v5.x/v6.x 可能未在 CMake 变量中直接暴露 ESP_IDF_VERSION。
    # 我们根据 IDF_VERSION_MAJOR/MINOR/PATCH 或环境中的 IDF_VERSION 构建它。
    if(NOT DEFINED ESP_IDF_VERSION)
        if(DEFINED IDF_VERSION_MAJOR)
            set(ESP_IDF_VERSION "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")
        elseif(DEFINED ENV{IDF_VERSION})
            set(ESP_IDF_VERSION $ENV{IDF_VERSION})
        else()
            set(ESP_IDF_VERSION "0.0.0")
        endif()
    endif()

    # ESP-IDF 环境：注册为 IDF 组件
    #
    # ESP-IDF 5.4 起把 driver 单体拆分为 esp_driver_gpio/i2c/rmt/ledc 等细粒度组件；
    # 5.3 及更早只有 driver 单体，esp_driver_* 组件不存在。故按版本门控 REQUIRES：
    #   >= 5.4：显式列出 esp_driver_* 精确依赖（仍保留 driver 以兼容尚未迁移的 header）。
    #   <  5.4：仅要求 driver 单体。
    # 版本变量 ESP_IDF_VERSION 由 idf.cmake 自动注入（形如 "5.3.1"）。
    if(ESP_IDF_VERSION VERSION_GREATER_EQUAL "5.4")
        set(WINK_ESP_DRIVER_COMPONENTS
            driver
            esp_driver_gpio
            esp_driver_rmt
            esp_driver_ledc
            esp_driver_i2c
        )
    else()
        set(WINK_ESP_DRIVER_COMPONENTS driver)
    endif()

    idf_component_register(
        SRCS
            # ESP32 PAL 实现
            pal_hal_esp32.c
            pal_hal_gpio_esp32.c
            pal_hal_i2c_esp32.c
            pal_hal_pwm_esp32.c
            pal_rmt_esp32.c
            pal_irq_esp32.c
            pal_osal_esp32.c
            pal_resource_esp32.c
            pal_storage_esp32.c
            # pal_pwm_router.c: target-independent PWM timer allocator.
            # P1-P3 (2026-07-04): host/wasm 通过 pal_common OBJECT 库拉入，
            # ESP32 是 IDF 组件（独立 CMake 作用域，无法跨作用域引用 pal_common
            # OBJECT target），故此处继续直接列源文件。改动限于 host/wasm。
            ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
            # wink_dev_config.c 已迁至 runtime/src/（已包含在 WINK_CORE_SOURCES 里，
            # 见 core_sources.cmake WINK_RUNTIME_SOURCES），此处不再显式列出。
            # 核心共享源文件（来自 core_sources.cmake）
            ${WINK_CORE_SOURCES}
        INCLUDE_DIRS
            ${WINK_CORE_INCLUDE_DIRS}
            ${WINK_CONFIG_DIR}    # codegen 生成的 wink_config.h 所在目录
        REQUIRES
            ${WINK_ESP_DRIVER_COMPONENTS}
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
        pal_hal_gpio_esp32.c
        pal_hal_i2c_esp32.c
        pal_hal_pwm_esp32.c
        pal_rmt_esp32.c
        pal_irq_esp32.c
        pal_osal_esp32.c
        pal_resource_esp32.c
        pal_storage_esp32.c
        ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
        # wink_dev_config.c 已迁至 runtime/src/，已在 WINK_CORE_SOURCES 中包含。
        ${WINK_CORE_SOURCES}
    )
    message(STATUS "wink esp32 target configured (static analysis)")
endif()
