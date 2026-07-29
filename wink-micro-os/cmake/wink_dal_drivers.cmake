# Shared DAL dual-mode pruning helpers (ADR-0039 + ADR-0046 + ADR-0051).
#
# Consumers (ESP32 IDF component, Host, Binary SDK, wasm single-app):
#   include(${WINK_MICRO_OS_ROOT}/cmake/wink_dal_drivers.cmake)
#   wink_dal_apply_pruning("<json or empty>" "<codegen out dir>")
#   wink_dal_add_enabled_sources(<target>)
#
# Requires WINK_MICRO_OS_ROOT to be an absolute path to wink-micro-os/.
# Driver universe SSOT = wink-tools codegen registry + extension-root YAML
# (list_drivers.py). Build truth for extra roots: cache WINK_CODEGEN_PATHS.

include(${WINK_MICRO_OS_ROOT}/cmake/wink_tools.cmake)

# Register CMAKE_CONFIGURE_DEPENDS for builtin plugins, extension YAML,
# referenced templates, plus GLOB_RECURSE watches on OS codegen/drivers|roles.
function(wink_dal_register_codegen_configure_depends)
    if(NOT WINK_MICRO_OS_ROOT)
        message(FATAL_ERROR
            "wink_dal_register_codegen_configure_depends: WINK_MICRO_OS_ROOT is not set")
    endif()
    if(NOT Python3_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endif()

    execute_process(
        COMMAND ${Python3_EXECUTABLE}
                ${WINK_TOOLS_ROOT}/tools/codegen/list_drivers.py
                --depend-files
                "--codegen-paths=${WINK_CODEGEN_PATHS}"
        OUTPUT_VARIABLE _wink_depend_out
        RESULT_VARIABLE _wink_depend_rc
        ERROR_VARIABLE _wink_depend_err
    )
    if(NOT _wink_depend_rc EQUAL 0)
        message(FATAL_ERROR
            "list_drivers.py --depend-files failed (rc=${_wink_depend_rc}): "
            "${_wink_depend_err}")
    endif()
    string(REPLACE "\r\n" "\n" _wink_depend_out "${_wink_depend_out}")
    string(REPLACE "\n" ";" _wink_depend_list "${_wink_depend_out}")
    set(_wink_depend_clean "")
    foreach(_p IN LISTS _wink_depend_list)
        if(NOT _p STREQUAL "")
            list(APPEND _wink_depend_clean "${_p}")
        endif()
    endforeach()
    if(_wink_depend_clean)
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS ${_wink_depend_clean})
    endif()

    # Add/delete under OS extension root must reconfigure even before
    # list_drivers has seen the new file in a prior configure.
    if(IS_DIRECTORY "${WINK_MICRO_OS_ROOT}/codegen/drivers")
        file(GLOB_RECURSE _wink_os_codegen_drivers CONFIGURE_DEPENDS
            "${WINK_MICRO_OS_ROOT}/codegen/drivers/*")
        if(_wink_os_codegen_drivers)
            set_property(DIRECTORY APPEND PROPERTY
                CMAKE_CONFIGURE_DEPENDS ${_wink_os_codegen_drivers})
        endif()
    endif()
    if(IS_DIRECTORY "${WINK_MICRO_OS_ROOT}/codegen/roles")
        file(GLOB_RECURSE _wink_os_codegen_roles CONFIGURE_DEPENDS
            "${WINK_MICRO_OS_ROOT}/codegen/roles/*")
        if(_wink_os_codegen_roles)
            set_property(DIRECTORY APPEND PROPERTY
                CMAKE_CONFIGURE_DEPENDS ${_wink_os_codegen_roles})
        endif()
    endif()
endfunction()

function(wink_dal_load_driver_table MODE)
    if(NOT WINK_MICRO_OS_ROOT)
        message(FATAL_ERROR "wink_dal_load_driver_table: WINK_MICRO_OS_ROOT is not set")
    endif()
    if(NOT Python3_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endif()
    if(NOT MODE STREQUAL "source" AND NOT MODE STREQUAL "defs")
        message(FATAL_ERROR "wink_dal_load_driver_table: MODE must be source|defs")
    endif()
    set(_gen "${CMAKE_BINARY_DIR}/generated_drivers_${MODE}.cmake")
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
                ${WINK_TOOLS_ROOT}/tools/codegen/list_drivers.py
                --cmake --mode=${MODE}
                "--codegen-paths=${WINK_CODEGEN_PATHS}"
        OUTPUT_FILE "${_gen}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "list_drivers.py --mode=${MODE} failed (rc=${_rc}): ${_err}")
    endif()
    include("${_gen}")
    wink_dal_register_codegen_configure_depends()
endfunction()

function(wink_dal_apply_pruning JSON_PATH OUT_DIR)
    if(NOT DEFINED WINK_KNOWN_DRIVERS)
        wink_dal_load_driver_table(source)
    endif()
    if(JSON_PATH STREQUAL "")
        message(WARNING
            "wink_dal: no wink-app.json - enabling ALL DAL drivers (ADR-0039). "
            "Add wink-app.json to prune unused drivers.")
        foreach(_opt IN LISTS WINK_KNOWN_DRIVERS)
            set(WINK_USE_${_opt} ON CACHE BOOL "" FORCE)
        endforeach()
    else()
        if(NOT WINK_MICRO_OS_ROOT)
            message(FATAL_ERROR
                "wink_dal_apply_pruning: WINK_MICRO_OS_ROOT is not set")
        endif()
        if(NOT Python3_EXECUTABLE)
            find_package(Python3 REQUIRED COMPONENTS Interpreter)
        endif()
        execute_process(
            COMMAND ${Python3_EXECUTABLE}
                    ${WINK_TOOLS_ROOT}/tools/codegen/app_codegen.py
                    --config "${JSON_PATH}"
                    --out-dir "${OUT_DIR}"
            RESULT_VARIABLE _wink_dal_codegen_rc
            OUTPUT_VARIABLE _wink_dal_codegen_out
            ERROR_VARIABLE _wink_dal_codegen_err
        )
        if(NOT _wink_dal_codegen_rc EQUAL 0)
            message(FATAL_ERROR
                "wink_dal: app_codegen failed (rc=${_wink_dal_codegen_rc}): "
                "${_wink_dal_codegen_err}")
        endif()
        set(_wink_app_opts "${OUT_DIR}/app_options.cmake")
        if(NOT EXISTS "${_wink_app_opts}")
            message(FATAL_ERROR
                "wink_dal: expected ${_wink_app_opts} after app_codegen")
        endif()
        include("${_wink_app_opts}")
    endif()
endfunction()

# Enable one driver: PRIVATE .c + PUBLIC WINK_USE_<OPT>=1.
function(_wink_dal_enable_one target opt rel_src)
    if(WINK_USE_${opt})
        target_sources(${target} PRIVATE
            ${WINK_MICRO_OS_ROOT}/${rel_src})
        target_compile_definitions(${target} PUBLIC WINK_USE_${opt}=1)
    endif()
endfunction()

function(wink_dal_add_enabled_sources target)
    if(NOT WINK_MICRO_OS_ROOT)
        message(FATAL_ERROR
            "wink_dal_add_enabled_sources: WINK_MICRO_OS_ROOT is not set")
    endif()
    if(NOT IS_ABSOLUTE "${WINK_MICRO_OS_ROOT}")
        message(FATAL_ERROR
            "wink_dal_add_enabled_sources: WINK_MICRO_OS_ROOT must be absolute "
            "(got '${WINK_MICRO_OS_ROOT}')")
    endif()
    if(NOT DEFINED WINK_KNOWN_DRIVERS)
        wink_dal_load_driver_table(source)
    endif()

    foreach(_drv IN LISTS WINK_KNOWN_DRIVERS)
        _wink_dal_enable_one(${target} ${_drv} ${WINK_DAL_${_drv}_REL_SRC})
    endforeach()

    set(WINK_DAL_TARGET ${target})
    wink_dal_apply_extra_cmake()
endfunction()
