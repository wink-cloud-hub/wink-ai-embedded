# samples/common — shared helpers for all sample apps (default telemetry,
# LED blink, ultrasonic echo simulator).  Built as an OBJECT library so that
# each sample that links it pulls in only the .o files it actually references
# (--gc-sections drops unused helpers at link time).
#
# This directory is add_subdirectory()d from the top-level CMakeLists.txt
# (host test branch) because its OBJECT target must exist before samples
# reference it.  ESP32 builds bypass this file and pick up common/src/*.c
# directly via generate_app_sources.ps1.

include_guard(GLOBAL)

# ADR-0023 Stage 2 migration: wink_blink_helper now lives in BAL (bal/src/wink_blink_helper.c).
# It is linked in via the wink_bal static library; samples/common no longer compiles
# its own copy.  Old include path "wink_blink_helper.h" still resolves through
# samples/common/include/wink_blink_helper.h forwarding shim → bal/include/output/wink_blink_helper.h.
set(WINK_SAMPLE_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wink_button_helper.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wink_default_telemetry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wink_sim_ultrasonic_echo.c
    CACHE INTERNAL "wink-micro-os samples/common source list")

set(WINK_SAMPLE_COMMON_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    CACHE INTERNAL "wink-micro-os samples/common include dir")

# OBJECT library: consumers link via $<TARGET_OBJECTS:wink_sample_common>.
add_library(wink_sample_common OBJECT ${WINK_SAMPLE_COMMON_SOURCES})

# Public include dirs: consumers get samples/common/include automatically.
target_include_directories(wink_sample_common PUBLIC
    ${WINK_SAMPLE_COMMON_INCLUDE_DIR})

# Compile options match samples (GCC -Wall -Wextra -Werror, MSVC /W4 /WX).
if(MSVC)
    target_compile_options(wink_sample_common PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(wink_sample_common PRIVATE
        -Wall -Wextra -Werror
        -Wno-unused-parameter
        -Wno-missing-field-initializers)
endif()

# Link the core static libraries so we inherit their PUBLIC include dirs
# (pal INTERFACE-exports WINK_CONFIG_DIR, runtime exports runtime/, dal dal/,
#  trace trace/, BAL includes from wink_bal) and compile options.
# OBJECT libraries need PRIVATE linkage.
if(TARGET pal AND TARGET dal AND TARGET wink_runtime AND TARGET wink_trace AND TARGET wink_bal)
    target_link_libraries(wink_sample_common PRIVATE pal dal wink_runtime wink_trace wink_bal)
else()
    # Fallback for builds that don't define core targets (e.g. static-analysis
    # invocations): just add include paths manually.
    target_include_directories(wink_sample_common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/communication
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bal/include/comm
    )
    if(DEFINED WINK_CONFIG_DIR)
        target_include_directories(wink_sample_common PRIVATE ${WINK_CONFIG_DIR})
    endif()
endif()

# Runtime/selftest internal headers are NOT exposed to common — common is a
# consumer of public APIs only.
