# vendor_cms8s78xx_v202_extint0 — production MCS-51 (CMS8S78xx) wasm-sim app:
# Official EXTINT0 external interrupt: INT0 muxed to P3.0, falling edge
# toggles P3.2 (P32 = ~P32 in the INT0 ISR), from the official Cmsemicon sample.
# Compiled UNMODIFIED ("一行不改") via mcs51_cleanup.py and backed by the
# frameworks/mcs51 SFR/XSFR proxies and cooperative fiber scheduler.

if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

set(_SRC_FILES main.c demo_extint.c isr.c)
set(_GEN_CPPS "")

foreach(_src IN LISTS _SRC_FILES)
    get_filename_component(_name "${_src}" NAME_WE)
    set(_cpp "${GEN_DIR}/${_name}.cpp")
    set(_c "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    add_custom_command(
        OUTPUT ${_cpp}
        COMMAND ${Python3_EXECUTABLE} ${_MCS51_CLEANUP} ${_c} ${_cpp}
        DEPENDS ${_c} ${_MCS51_CLEANUP}
        COMMENT "vendor_cms8s78xx_v202_extint0 cleanup: ${_src} -> ${_name}.cpp"
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
            message(FATAL_ERROR "vendor_cms8s78xx_v202_extint0 cleanup failed for ${_src}")
        endif()
    endif()
endforeach()

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES      ${_GEN_CPPS}            PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${GEN_DIR} PARENT_SCOPE)
    set(WINK_APP_MCS51        TRUE                    PARENT_SCOPE)
    return()
endif()

message(STATUS "vendor_cms8s78xx_v202_extint0: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
