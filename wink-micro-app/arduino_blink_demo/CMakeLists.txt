# arduino_blink_demo: Arduino-style Blink + Serial sketch running on
# WinkMicroOS Arduino Compatibility Layer (ADR-0035 validation).
#
# Key point: this is the ONLY place `wink_arduino_compat` is linked.
# Kernel libraries (pal, dal, wink_runtime) do NOT depend on it.
# This validates the "leaf-node linking rule" from ADR-0035 §2.1.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

# ── Arduino sketch (.ino) + host E2E test harness ──
# Compile the .ino sketch as C++
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/arduino_blink_demo.ino PROPERTIES LANGUAGE CXX)
if(MSVC)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/arduino_blink_demo.ino PROPERTIES COMPILE_FLAGS "/TP")
else()
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/arduino_blink_demo.ino PROPERTIES COMPILE_FLAGS "-x c++")
endif()

add_executable(app_arduino_blink_e2e
    ${CMAKE_CURRENT_SOURCE_DIR}/arduino_blink_demo.ino
    ${CMAKE_CURRENT_SOURCE_DIR}/test_arduino_blink_e2e.cpp
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
    ${wink-micro-os_SOURCE_DIR}/frameworks/arduino/include)

add_test(NAME app_arduino_blink_e2e COMMAND app_arduino_blink_e2e)
