# BAL (Business Abstraction Layer) — reusable device enhancements,
# math algorithms, and domain controllers (ADR-0038 / 06-bal-layer).
#
# 介于 App 与 {DAL, runtime} 之间：封装 led blink / button poll / ultrasonic /
# servo / telemetry 等常见组合，让 App 代码保持零阻塞、零手写 task wrapper。
#
# 分层红线（ADR-0023 §8 / ADR-0043）：
#   * bal/include/**/*.h 禁止 #include 任何 pal_*.h（pal_log.h 除外）。
#     文本扫描门禁由 `wink lint --pack layering` 执行（不再在此 CMake 重复 REGEX）。
#   * BAL 不得链接 target 专属 PAL 对象，保持跨 target 单源编译。
#
# 目录布局（include/ 与 src/ 按 domain 镜像）：
#   include/output/   — LED blink 等输出类组件
#   include/input/    — Button events 等输入类组件
#   include/sensor/   — Ultrasonic 等传感器组件
#   include/actuator/ — Servo 等执行器组件
#   include/display/  — OLED dashboard（预留）
#   include/comm/     — Telemetry / 通信组件
#   include/math/     — PID、运动学等纯算法
#   include/control/  — 闭环电机、底盘等控制器
#   src/<domain>/     — 与 include/<domain>/ 对应的 .c 实现

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

# BAL 依赖 runtime（wink_periodic / wink_pt_debug 等）+ DAL（底层器件驱动）+ trace（LOG/FAULT）。
# 注意：不直接依赖 pal —— 通过 DAL/runtime 间接拿 PAL 类型，强化分层红线。
target_link_libraries(wink_bal PUBLIC
    pal
    dal
    wink_runtime
    wink_trace
)

# ── Layout / link-face gates (keep in CMake) ───────────────────
# Text layering REGEX gates (BAL-HDR-NO-PAL / BAL-NAME-* / BAL-MATH-1)
# moved to `wink lint` (ADR-0043). Parity probe: tools/tests/test_lint_parity.py

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
