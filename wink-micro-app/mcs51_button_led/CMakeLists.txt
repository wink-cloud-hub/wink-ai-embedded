# mcs51_button_led — production MCS-51 (8051) wasm-sim app (ADR-0075).
#
# Unlike the esp32-style apps (avoidance_car etc.) this app does NOT run
# app_codegen.py and does NOT provide app_callbacks.c / device_tree.c: the
# mcs51 interception framework (wink_mcs51_compat) strongly supplies
# wink_app_get_callbacks() (init = mcs51_framework_init, loop -> the Keil main
# fiber). The ONLY app source is the unmodified Keil .c rewritten to a .cpp by
# mcs51_cleanup.py. The app opts into the framework auto-link by exporting
# WINK_APP_MCS51=TRUE (see wink-micro-os/CMakeLists.txt, ADR-0075 D1/D2).
#
# This app is wasm-sim only: on host/esp32 it exports no sources (the mcs51
# tree self-skips on ESP_PLATFORM).

get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")
set(_MCS51_KEYLE_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/button_led.c")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})
set(GEN_KEYLE_CPP "${GEN_DIR}/button_led.cpp")

# Keil dialect cleanup: button_led.c -> button_led.cpp (source never edited).
add_custom_command(
    OUTPUT ${GEN_KEYLE_CPP}
    COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP}
            ${_MCS51_KEYLE_SRC} ${GEN_KEYLE_CPP}
    DEPENDS ${_MCS51_KEYLE_SRC} ${_MCS51_CLEANUP}
    COMMENT "mcs51_button_led cleanup: button_led.c -> button_led.cpp"
    VERBATIM
)

# REGX52.H uses C++17 inline variables; the root build does not set a global
# CXX standard, so pin C++17 on the cleaned translation unit.
set_source_files_properties(${GEN_KEYLE_CPP} PROPERTIES
    COMPILE_OPTIONS "-std=c++17")

if(EMSCRIPTEN)
    # Materialize the cleaned source at configure time (mirrors the
    # avoidance_car emscripten branch) so the first build has it on disk.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP}
                ${_MCS51_KEYLE_SRC} ${GEN_KEYLE_CPP}
        RESULT_VARIABLE _mcs51_keyle_cleanup_rc
    )
    if(NOT _mcs51_keyle_cleanup_rc EQUAL 0)
        message(FATAL_ERROR
            "mcs51_button_led cleanup failed (rc=${_mcs51_keyle_cleanup_rc}); "
            "check ${_MCS51_KEYLE_SRC}")
    endif()

    set(WINK_APP_SOURCES     ${GEN_KEYLE_CPP} PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${GEN_DIR}      PARENT_SCOPE)
    set(WINK_APP_MCS51       TRUE             PARENT_SCOPE)
    return()
endif()

message(STATUS
    "mcs51_button_led: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
