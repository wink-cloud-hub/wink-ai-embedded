# Shared DAL dual-mode pruning helpers (ADR-0039).
#
# Consumers (ESP32 IDF component, Host, Binary SDK, wasm single-app):
#   include(${WINK_MICRO_OS_ROOT}/cmake/wink_dal_drivers.cmake)
#   wink_dal_apply_pruning("<json or empty>" "<codegen out dir>")
#   wink_dal_add_enabled_sources(<target>)
#
# Requires WINK_MICRO_OS_ROOT to be an absolute path to wink-micro-os/.
#
# Driver table (SSOT — keep in sync with ADR-0039 / ALL_WINK_USE_OPTIONS):
#   LED        → dal/src/output/dal_led.c
#   BUTTON     → dal/src/input/dal_button.c
#   SERVO      → dal/src/actuator/dal_servo.c
#   SSD1306    → dal/src/display/dal_ssd1306.c (+ font TU)
#   ULTRASONIC → dal/src/sensor/dal_ultrasonic.c
#   GPS        → dal/src/communication/dal_gps.c
#   EEPROM     → dal/src/storage/dal_eeprom.c
#   MOTOR      → dal/src/actuator/dal_motor.c
#   ENCODER    → dal/src/sensor/dal_encoder.c

function(wink_dal_apply_pruning JSON_PATH OUT_DIR)
    if(JSON_PATH STREQUAL "")
        message(WARNING
            "wink_dal: no wink-app.json — enabling ALL DAL drivers (ADR-0039). "
            "Add wink-app.json to prune unused drivers.")
        foreach(_opt IN ITEMS LED BUTTON SERVO SSD1306 ULTRASONIC GPS EEPROM MOTOR ENCODER)
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
                    ${WINK_MICRO_OS_ROOT}/tools/codegen/app_codegen.py
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

    _wink_dal_enable_one(${target} LED        dal/src/output/dal_led.c)
    _wink_dal_enable_one(${target} BUTTON     dal/src/input/dal_button.c)
    _wink_dal_enable_one(${target} SERVO      dal/src/actuator/dal_servo.c)
    _wink_dal_enable_one(${target} SSD1306    dal/src/display/dal_ssd1306.c)
    _wink_dal_enable_one(${target} ULTRASONIC dal/src/sensor/dal_ultrasonic.c)
    _wink_dal_enable_one(${target} GPS        dal/src/communication/dal_gps.c)
    _wink_dal_enable_one(${target} EEPROM     dal/src/storage/dal_eeprom.c)
    _wink_dal_enable_one(${target} MOTOR      dal/src/actuator/dal_motor.c)
    _wink_dal_enable_one(${target} ENCODER    dal/src/sensor/dal_encoder.c)

    # Font TU only when SSD1306 is enabled (matches dal/CMakeLists.txt).
    if(WINK_USE_SSD1306)
        if(NOT DEFINED WINK_SSD1306_FONT)
            set(WINK_SSD1306_FONT "ascii_upper")
        endif()
        if(WINK_SSD1306_FONT STREQUAL "minimal")
            target_sources(${target} PRIVATE
                ${WINK_MICRO_OS_ROOT}/dal/src/display/dal_ssd1306_font_5x7_minimal.c)
            target_compile_definitions(${target} PUBLIC WINK_SSD1306_FONT_MINIMAL=1)
        elseif(WINK_SSD1306_FONT STREQUAL "ascii_upper")
            target_sources(${target} PRIVATE
                ${WINK_MICRO_OS_ROOT}/dal/src/display/dal_ssd1306_font_5x7_ascii_upper.c)
            target_compile_definitions(${target} PUBLIC WINK_SSD1306_FONT_ASCII_UPPER=1)
        else()
            message(FATAL_ERROR
                "WINK_SSD1306_FONT must be 'minimal' or 'ascii_upper', "
                "got '${WINK_SSD1306_FONT}'")
        endif()
    endif()
endfunction()
