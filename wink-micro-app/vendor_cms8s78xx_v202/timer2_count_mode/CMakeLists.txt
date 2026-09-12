# vendor_cms8s78xx_v202_timer2_count_mode — production MCS-51 (CMS8S78xx) wasm-sim app:
# Official Timer2 CountMode example: T2 external pulse counter via P1.6 (PS_T2),
# 16-bit auto-reload, counting 5 falling edges per overflow, toggling P3.2 in ISR.
# Compiled UNMODIFIED ("一行不改") via transpile_app_keil_c51.py and backed by the
# frameworks/mcs51 SFR/XSFR proxies and the cooperative fiber scheduler.

if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
set(_MCS51_TRANSPILE
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/transpile_app_keil_c51.py")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

set(_SRC_FILES main.c demo_timer.c isr.c)
set(_GEN_CPPS "")

foreach(_src IN LISTS _SRC_FILES)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_cpp "${GEN_DIR}/${_name}.cpp")
    set(_c "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    add_custom_command(
        OUTPUT ${_cpp}
        COMMAND ${Python3_EXECUTABLE} ${_MCS51_TRANSPILE} ${_c} ${_cpp}
        DEPENDS ${_c} ${_MCS51_TRANSPILE}
        COMMENT "vendor_cms8s78xx_v202_timer2_count_mode transpile: ${_src} -> ${_name}.cpp"
        VERBATIM
    )
    set_source_files_properties(${_cpp} PROPERTIES COMPILE_OPTIONS "-std=c++17")
    list(APPEND _GEN_CPPS ${_cpp})

    if(EMSCRIPTEN)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${_MCS51_TRANSPILE} ${_c} ${_cpp}
            RESULT_VARIABLE _rc
        )
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "vendor_cms8s78xx_v202_timer2_count_mode transpile failed for ${_src}")
        endif()
    endif()
endforeach()

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES      ${_GEN_CPPS}            PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${GEN_DIR} PARENT_SCOPE)
    set(WINK_APP_MCS51        TRUE                    PARENT_SCOPE)
    return()
endif()

message(STATUS "vendor_cms8s78xx_v202_timer2_count_mode: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
