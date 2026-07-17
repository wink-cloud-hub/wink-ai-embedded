# arduino_blink_demo: Arduino-style Blink + Serial sketch running on
# WinkMicroOS Arduino Compatibility Layer (ADR-0035 validation).
#
# Key point: this is the ONLY place `wink_arduino_compat` is linked.
# Kernel libraries (pal, dal, wink_runtime) do NOT depend on it.
# This validates the "leaf-node linking rule" from ADR-0035 §2.1.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

# ── Codegen: wink-app.json → device_tree.[hc] + app_options.cmake ──
set(WINK_APP_JSON     ${CMAKE_CURRENT_SOURCE_DIR}/wink-app.json)
if(NOT DEFINED WINK_CODEGEN_ROOT)
    set(WINK_CODEGEN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/tools/codegen)
endif()
set(WINK_CODEGEN_TOOL ${WINK_CODEGEN_ROOT}/app_codegen.py)
set(GEN_DIR           ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(GEN_DEVICE_TREE_H ${GEN_DIR}/device_tree.h)
set(GEN_DEVICE_TREE_C ${GEN_DIR}/device_tree.c)
set(GEN_APP_OPTIONS   ${GEN_DIR}/app_options.cmake)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND
     PROPERTY CMAKE_CONFIGURE_DEPENDS ${WINK_APP_JSON})

file(GLOB WINK_CODEGEN_TEMPLATES ${WINK_CODEGEN_ROOT}/templates/*.j2)
file(GLOB WINK_CODEGEN_DRIVERS   ${WINK_CODEGEN_ROOT}/drivers/*.py)

add_custom_command(
    OUTPUT ${GEN_DEVICE_TREE_H} ${GEN_DEVICE_TREE_C}
           ${GEN_APP_OPTIONS}
    COMMAND ${Python3_EXECUTABLE} ${WINK_CODEGEN_TOOL}
            --config ${WINK_APP_JSON}
            --out-dir ${GEN_DIR}
    DEPENDS ${WINK_APP_JSON} ${WINK_CODEGEN_TOOL}
            ${WINK_CODEGEN_TEMPLATES} ${WINK_CODEGEN_DRIVERS}
    COMMENT "Generating arduino_blink_demo device tree from wink-app.json"
    VERBATIM
)

# ── Arduino sketch (.ino.cpp) + host E2E test harness ──
# The sketch is compiled as C++ (it includes <Arduino.h>).
# The test harness is plain C (it calls setup()/loop() via extern "C").
add_executable(app_arduino_blink_e2e
    ${GEN_DEVICE_TREE_C}
    ${CMAKE_CURRENT_SOURCE_DIR}/arduino_blink_demo.ino.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_arduino_blink_e2e.c
    ${WINK_SAMPLE_RUNTIME_SOURCES}
    $<TARGET_OBJECTS:pal_host>)

wink_sample_apply_common(app_arduino_blink_e2e)
wink_sample_link_common(app_arduino_blink_e2e)

# Link kernel libraries (dal, wink_bal, etc.)
target_link_libraries(app_arduino_blink_e2e PRIVATE dal)

# ── ADR-0035 §2.1: wink_arduino_compat linked as leaf-node dependency ──
# This is the critical architectural validation: only the App binary
# links the C++ Arduino compat library. No kernel lib depends on it.
if(TARGET wink_arduino_compat)
    target_link_libraries(app_arduino_blink_e2e PRIVATE wink_arduino_compat)
endif()

# Arduino compat headers
target_include_directories(app_arduino_blink_e2e PRIVATE
    ${wink-micro-os_SOURCE_DIR}/frameworks/arduino/include
    ${GEN_DIR})

add_test(NAME app_arduino_blink_e2e COMMAND app_arduino_blink_e2e)
