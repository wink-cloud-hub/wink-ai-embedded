# mcs51_uart_hello — production MCS-51 (8051) wasm-sim app, UART TX beacon
# (Stage 2 T1/T5 headless proof).
#
# Same framework auto-link contract as mcs51_button_led (ADR-0075 D1/D2): no
# app_codegen.py, no app_callbacks.c / device_tree.c — wink_mcs51_compat
# strongly supplies wink_app_get_callbacks(). The ONLY app source is the
# unmodified Keil .c rewritten to a .cpp by mcs51_cleanup.py; the app opts in
# via WINK_APP_MCS51=TRUE. Wasn-sim only: host/esp32 export no sources (the mcs51
# tree self-skips on ESP_PLATFORM).

get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")
set(_MCS51_HELLO_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/uart_hello.c")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})
set(GEN_HELLO_CPP "${GEN_DIR}/uart_hello.cpp")

# Keil dialect cleanup: uart_hello.c -> uart_hello.cpp (source never edited).
add_custom_command(
    OUTPUT ${GEN_HELLO_CPP}
    COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP}
            ${_MCS51_HELLO_SRC} ${GEN_HELLO_CPP}
    DEPENDS ${_MCS51_HELLO_SRC} ${_MCS51_CLEANUP}
    COMMENT "mcs51_uart_hello cleanup: uart_hello.c -> uart_hello.cpp"
    VERBATIM
)

# REGX52.H uses C++17 inline variables; the root build does not set a global
# CXX standard, so pin C++17 on the cleaned translation unit.
set_source_files_properties(${GEN_HELLO_CPP} PROPERTIES
    COMPILE_OPTIONS "-std=c++17")

if(EMSCRIPTEN)
    # Materialize the cleaned source at configure time (mirrors the
    # avoidance_car emscripten branch) so the first build has it on disk.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP}
                ${_MCS51_HELLO_SRC} ${GEN_HELLO_CPP}
        RESULT_VARIABLE _mcs51_hello_cleanup_rc
    )
    if(NOT _mcs51_hello_cleanup_rc EQUAL 0)
        message(FATAL_ERROR
            "mcs51_uart_hello cleanup failed (rc=${_mcs51_hello_cleanup_rc}); "
            "check ${_MCS51_HELLO_SRC}")
    endif()

    set(WINK_APP_SOURCES     ${GEN_HELLO_CPP} PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${GEN_DIR}      PARENT_SCOPE)
    set(WINK_APP_MCS51       TRUE             PARENT_SCOPE)
    return()
endif()

message(STATUS
    "mcs51_uart_hello: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
