# ESP32 Target (ESP-IDF component mode).

if(NOT WINK_MICRO_OS_ROOT)
    set(WINK_MICRO_OS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
endif()
include(${WINK_MICRO_OS_ROOT}/core_sources.cmake)

if(ESP_PLATFORM)
    # Define fallback OSAL variables for the isolated ESP-IDF component registration context
    if(NOT WINK_OSAL_SOURCES)
        set(WINK_OSAL_SOURCES "${WINK_MICRO_OS_ROOT}/osal/freertos_esp32/pal_osal_freertos_esp32.c")
    endif()
    if(NOT WINK_OSAL_INCLUDE_DIRS)
        set(WINK_OSAL_INCLUDE_DIRS "${WINK_MICRO_OS_ROOT}/osal/freertos_esp32")
    endif()

    set(WINK_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${WINK_CONFIG_DIR})

    if(NOT DEFINED ESP_IDF_VERSION)
        if(DEFINED IDF_VERSION_MAJOR)
            set(ESP_IDF_VERSION "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")
        elseif(DEFINED ENV{IDF_VERSION})
            set(ESP_IDF_VERSION $ENV{IDF_VERSION})
        else()
            set(ESP_IDF_VERSION "0.0.0")
        endif()
    endif()

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
            pal_log_esp32.c
            pal_hal_gpio_esp32.c
            pal_hal_i2c_esp32.c
            pal_hal_pwm_esp32.c
            pal_hal_adc_esp32.c
            pal_hal_uart_esp32.c
            pal_rmt_esp32.c
            pal_irq_esp32.c
            ${WINK_OSAL_SOURCES}
            pal_resource_esp32.c
            pal_storage_esp32.c
            ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
            ${WINK_CORE_SOURCES}
        INCLUDE_DIRS
            ${WINK_CORE_INCLUDE_DIRS}
            ${WINK_OSAL_INCLUDE_DIRS}
            ${WINK_CONFIG_DIR}
        REQUIRES
            ${WINK_ESP_DRIVER_COMPONENTS}
            esp_timer
            esp_system
            esp_ringbuf
            nvs_flash
    )
    set(ESP32_PAL_LIB ${COMPONENT_LIB} CACHE INTERNAL "")
    message(STATUS "wink esp32 target registered as IDF component")

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(NOT DEFINED WINK_APP_DIR OR WINK_APP_DIR STREQUAL "")
        if(DEFINED ENV{WINK_APP_DIR} AND NOT "$ENV{WINK_APP_DIR}" STREQUAL "")
            set(WINK_APP_DIR "$ENV{WINK_APP_DIR}")
        else()
            message(FATAL_ERROR
                "WINK_APP_DIR must be set for esp32 wink_config.h codegen "
                "(CMake -DWINK_APP_DIR= or env WINK_APP_DIR).")
        endif()
    endif()
    if(NOT IS_ABSOLUTE "${WINK_APP_DIR}")
        get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE
            BASE_DIR "${WINK_MICRO_OS_ROOT}")
    endif()
    set(WINK_APP_JSON "${WINK_APP_DIR}/wink-app.json")
    if(NOT EXISTS "${WINK_APP_JSON}")
        message(STATUS "WINK_APP_JSON not found: ${WINK_APP_JSON} — Using default configuration.")
        set(WINK_APP_JSON "")
    endif()
    set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)
    include(${WINK_MICRO_OS_ROOT}/cmake/wink_tools.cmake)
    set(_CONFIG_H_DEPS ${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py)
    if(NOT WINK_APP_JSON STREQUAL "")
        list(APPEND _CONFIG_H_DEPS ${WINK_APP_JSON})
    endif()
    add_custom_command(
        OUTPUT ${WINK_CONFIG_H}
        COMMAND ${Python3_EXECUTABLE} ${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py
                --input "${WINK_APP_JSON}"
                --output ${WINK_CONFIG_H}
                --target esp32
        DEPENDS ${_CONFIG_H_DEPS}
        COMMENT "Generating wink_config.h (esp32)"
        VERBATIM
    )
    add_custom_target(wink_generate_config DEPENDS ${WINK_CONFIG_H})
    add_dependencies(${COMPONENT_LIB} wink_generate_config)

    target_compile_options(${COMPONENT_LIB} PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )

    get_filename_component(WINK_MICRO_OS_ROOT "${WINK_MICRO_OS_ROOT}" ABSOLUTE)
    include(${WINK_MICRO_OS_ROOT}/cmake/wink_dal_drivers.cmake)
    wink_dal_apply_pruning("${WINK_APP_JSON}" "${WINK_CONFIG_DIR}")
    wink_dal_add_enabled_sources(${COMPONENT_LIB})
elseif(TARGET_PLATFORM STREQUAL "esp32")
    set(ESP32_PAL_SOURCES
        pal_log_esp32.c
        pal_hal_gpio_esp32.c
        pal_hal_i2c_esp32.c
        pal_hal_pwm_esp32.c
        pal_hal_adc_esp32.c
        pal_hal_uart_esp32.c
        pal_rmt_esp32.c
        pal_irq_esp32.c
        ${WINK_OSAL_SOURCES}
        pal_resource_esp32.c
        pal_storage_esp32.c
        ${WINK_MICRO_OS_ROOT}/pal/src/pal_pwm_router.c
        ${WINK_CORE_SOURCES}
    )
    message(STATUS "wink esp32 target configured (static analysis)")
endif()
