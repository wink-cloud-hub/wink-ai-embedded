# mcs51_button_led_int — production MCS-51 (8051) wasm-sim app, interrupt
# variant (Stage 2 T3 headless proof).
#
# Same framework auto-link contract as mcs51_button_led (ADR-0075 D1/D2): the
# app does NOT run app_codegen.py and supplies no app_callbacks.c /
# device_tree.c — the mcs51 interception framework strongly supplies
# wink_app_get_callbacks(). The ONLY app source is the unmodified Keil .c
# rewritten to a .cpp by mcs51_cleanup.py; the app opts into the framework via
# WINK_APP_MCS51=TRUE. Wasn-sim only: host/esp32 export no sources (the mcs51
# tree self-skips on ESP_PLATFORM).

get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")
set(_MCS51_KEYLE_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/button_led_int.c")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})
set(GEN_KEYLE_CPP "${GEN_DIR}/button_led_int.cpp")

# Keil dialect cleanup: button_led_int.c -> button_led_int.cpp (source never edited).
add_custom_command(
    OUTPUT ${GEN_KEYLE_CPP}
    COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP}
            ${_MCS51_KEYLE_SRC} ${GEN_KEYLE_CPP}
    DEPENDS ${_MCS51_KEYLE_SRC} ${_MCS51_CLEANUP}
    COMMENT "mcs51_button_led_int cleanup: button_led_int.c -> button_led_int.cpp"
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
            "mcs51_button_led_int cleanup failed (rc=${_mcs51_keyle_cleanup_rc}); "
            "check ${_MCS51_KEYLE_SRC}")
    endif()

    set(WINK_APP_SOURCES     ${GEN_KEYLE_CPP} PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${GEN_DIR}      PARENT_SCOPE)
    set(WINK_APP_MCS51       TRUE             PARENT_SCOPE)
    return()
endif()

message(STATUS
    "mcs51_button_led_int: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
