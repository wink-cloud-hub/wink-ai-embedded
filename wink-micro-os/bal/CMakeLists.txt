# BAL (Business Abstraction Layer) — reusable device enhancements,
# math algorithms, and domain controllers (ADR-0038 / 06-bal-layer).
#
# Layering rules (ADR-0023 §8 / ADR-0043):
#   * bal/include/**/*.h MUST NOT #include any pal_*.h (except pal_log.h).
#   * BAL MUST NOT link target-specific PAL objects.
#
# Directory layout (include/ and src/ mirrored by domain):
#   include/output/   — Output components like LED blink
#   include/input/    — Input components like Button events
#   include/sensor/   — Sensor components like Ultrasonic
#   include/actuator/ — Actuator components like Servo
#   include/display/  — Display components (OLED dashboard)
#   include/comm/     — Telemetry and communication components
#   include/math/     — Pure math algorithms like PID, kinematics
#   include/control/  — Closed-loop controllers like DC motor, chassis
#   src/<domain>/     — Corresponding .c implementations

add_library(wink_bal STATIC)

set(_BAL_SOURCES
    src/wink_bal_stub.c
    src/comm/wink_telemetry_default.c
)

if(WINK_USE_BUTTON)
    list(APPEND _BAL_SOURCES
        src/input/wink_button_events.c
        src/input/wink_button_events_irq.c
    )
endif()

if(WINK_USE_LED)
    list(APPEND _BAL_SOURCES src/output/wink_led_blink.c)
endif()

if(WINK_USE_ULTRASONIC)
    list(APPEND _BAL_SOURCES
        src/sensor/wink_ultrasonic_poll.c
        src/sensor/wink_ultrasonic_distance_events.c
    )
endif()

if(WINK_USE_RC_SERVO)
    list(APPEND _BAL_SOURCES src/actuator/wink_rc_servo_sweep.c)
endif()

if(WINK_USE_DC_MOTOR OR WINK_USE_ENCODER)
    list(APPEND _BAL_SOURCES
        src/math/wink_pid.c
        src/math/wink_diff_drive_kinematics.c
        src/control/wink_closed_loop_dc_motor.c
        src/control/wink_chassis.c
    )
endif()

target_sources(wink_bal PRIVATE ${_BAL_SOURCES})

target_include_directories(wink_bal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wink_bal PUBLIC
    pal
    dal
    wink_runtime
    wink_trace
)

# ── Layout / link-face gates (keep in CMake) ───────────────────

# BAL-INC-2: PUBLIC include directories must be only bal/include root.
get_target_property(_bal_public_inc wink_bal INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _bal_public_inc)
    message(FATAL_ERROR "BAL-INC-2: wink_bal has no PUBLIC include directories.")
endif()
if(NOT _bal_public_inc STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/include")
    message(FATAL_ERROR
        "BAL-INC-2: wink_bal PUBLIC include must be only "
        "${CMAKE_CURRENT_SOURCE_DIR}/include (got: ${_bal_public_inc}).")
endif()

# BAL-SRC-1: include/<domain>/*.h must mirror src/<domain>/*.c (whitelist display).
set(_BAL_SRC1_WHITELIST_DOMAINS display)
file(GLOB _BAL_DOMAIN_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*/*.h")
foreach(_dh ${_BAL_DOMAIN_HEADERS})
    get_filename_component(_dh_dir "${_dh}" DIRECTORY)
    get_filename_component(_dh_name "${_dh}" NAME)
    file(RELATIVE_PATH _dh_rel "${CMAKE_CURRENT_SOURCE_DIR}/include" "${_dh_dir}")
    if(_dh_rel IN_LIST _BAL_SRC1_WHITELIST_DOMAINS)
        continue()
    endif()
    string(REPLACE ".h" ".c" _dc_name "${_dh_name}")
    set(_dc_path "${CMAKE_CURRENT_SOURCE_DIR}/src/${_dh_rel}/${_dc_name}")
    if(NOT EXISTS "${_dc_path}")
        message(FATAL_ERROR
            "BAL-SRC-1: ${_dh_rel}/${_dh_name} has no mirrored "
            "implementation at src/${_dh_rel}/${_dc_name}")
    endif()
endforeach()
