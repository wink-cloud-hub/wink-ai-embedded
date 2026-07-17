cmake_minimum_required(VERSION 3.15)

# C++ Standard definition
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure the search path for the ArduinoCore-API dependency
set(ARDUINO_CORE_API_DIR "${CMAKE_CURRENT_LIST_DIR}/../../third_party/ArduinoCore-API" CACHE PATH "Path to official ArduinoCore-API repository")

if(NOT EXISTS "${ARDUINO_CORE_API_DIR}/api/ArduinoAPI.h")
    message(WARNING "ArduinoCore-API not found at: ${ARDUINO_CORE_API_DIR}. Skipping target wink_arduino_compat.")
    return()
endif()

if(ESP_PLATFORM)
    # ESP-IDF component mode
    idf_component_register(
        SRCS
            src/Common.cpp
            src/WinkHardwareSerial.cpp
            src/ArduinoStubs.cpp
            ${ARDUINO_CORE_API_DIR}/api/Print.cpp
            ${ARDUINO_CORE_API_DIR}/api/Stream.cpp
            ${ARDUINO_CORE_API_DIR}/api/String.cpp
            ${ARDUINO_CORE_API_DIR}/api/IPAddress.cpp
        INCLUDE_DIRS
            include
            ${ARDUINO_CORE_API_DIR}
        REQUIRES
            esp32
    )
    # Ensure source files are compiled as UTF-8
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
else()
    # Host/wasm standalone target mode
    add_library(wink_arduino_compat STATIC
        src/Common.cpp
        src/WinkHardwareSerial.cpp
        src/ArduinoStubs.cpp
        ${ARDUINO_CORE_API_DIR}/api/Print.cpp
        ${ARDUINO_CORE_API_DIR}/api/Stream.cpp
        ${ARDUINO_CORE_API_DIR}/api/String.cpp
        ${ARDUINO_CORE_API_DIR}/api/IPAddress.cpp
    )

    target_include_directories(wink_arduino_compat PUBLIC
        include
        ${ARDUINO_CORE_API_DIR}
    )

    # Link against basic Wink OSAL, HAL, and runtime libraries
    target_link_libraries(wink_arduino_compat PUBLIC
        pal
        dal
        wink_runtime
    )

    message(STATUS "Configured wink_arduino_compat target with ArduinoCore-API at: ${ARDUINO_CORE_API_DIR}")
endif()
