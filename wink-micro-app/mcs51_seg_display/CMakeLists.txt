# mcs51_seg_display — production MCS-51 (CMS8S78xx) wasm-sim app:
# 4COM 8SEG dynamic scanning LED driver from official Cmsemicon sample.
#
# Compiled UNMODIFIED ("一行不改") via mcs51_cleanup.py and backed by the
# frameworks/mcs51 SFR/XSFR proxies and cooperative fiber scheduler.

get_filename_component(_MCS51_APP_OS_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os" ABSOLUTE)
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

set(_SRC_FILES main.c demo_led.c demo_timer.c isr.c)
set(_GEN_CPPS "")

foreach(_src IN LISTS _SRC_FILES)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_cpp "${GEN_DIR}/${_name}.cpp")
    set(_c "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    add_custom_command(
        OUTPUT ${_cpp}
        COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP} ${_c} ${_cpp}
        DEPENDS ${_c} ${_MCS51_CLEANUP}
        COMMENT "mcs51_seg_display cleanup: ${_src} -> ${_name}.cpp"
        VERBATIM
    )
    set_source_files_properties(${_cpp} PROPERTIES COMPILE_OPTIONS "-std=c++17")
    list(APPEND _GEN_CPPS ${_cpp})

    if(EMSCRIPTEN)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP} ${_c} ${_cpp}
            RESULT_VARIABLE _rc
        )
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "mcs51_seg_display cleanup failed for ${_src}")
        endif()
    endif()
endforeach()

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES      ${_GEN_CPPS}            PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${GEN_DIR} PARENT_SCOPE)
    set(WINK_APP_MCS51        TRUE                    PARENT_SCOPE)
    return()
endif()

message(STATUS "mcs51_seg_display: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
