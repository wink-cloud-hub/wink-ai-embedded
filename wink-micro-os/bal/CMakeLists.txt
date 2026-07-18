# BAL (Business Abstraction Layer) — reusable device enhancements,
# math algorithms, and domain controllers (ADR-0038 / 06-bal-layer).
#
# 介于 App 与 {DAL, runtime} 之间：封装 led blink / button poll / ultrasonic /
# servo / telemetry 等常见组合，让 App 代码保持零阻塞、零手写 task wrapper。
#
# 分层红线（ADR-0023 §8）：
#   * bal/include/**/*.h 禁止 #include 任何 pal_*.h（pal_log.h 除外，它只暴露 LOG 宏，
#     不引入 OSAL/HAL 类型）。CI grep 卡口强制。
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

target_sources(wink_bal PRIVATE
    src/wink_bal_stub.c
    src/output/wink_led_blink.c
    src/input/wink_button_events.c
    src/input/wink_button_events_irq.c
    src/sensor/wink_ultrasonic_poll.c
    src/sensor/wink_ultrasonic_distance_events.c
    src/actuator/wink_servo_sweep.c
    src/comm/wink_telemetry_default.c
    src/math/wink_pid.c
    src/math/wink_diff_drive_kinematics.c
    src/control/wink_closed_loop_motor.c
    src/control/wink_chassis.c
)

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

# ── Layering red-line CI grep guard (ADR-0023 §8) ──────────────
# bal/include/**/*.h MUST NOT #include any pal_*.h (pal_log.h is
# exempt — it exposes only LOG macros, no OSAL/HAL types).
# Scan all BAL public headers at configure time; FATAL_ERROR if
# a forbidden include is found.
file(GLOB_RECURSE _BAL_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*/*.h)
set(_BAL_FORBIDDEN_INCLUDE_REGEX "^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"]pal_.*\.h[>\"]")
foreach(_h ${_BAL_PUBLIC_HEADERS})
    file(STRINGS "${_h}" _matches REGEX "${_BAL_FORBIDDEN_INCLUDE_REGEX}")
    foreach(_m ${_matches})
        # pal_log.h is the single exempted header (per ADR-0023 §8).
        if(NOT _m MATCHES "pal_log\\.h")
            message(FATAL_ERROR
                "BAL layering violation: ${_h} includes a pal_ header (${_m}). "
                "ADR-0023 §8 red-line: bal/include/**/*.h must not include pal_*.h "
                "(pal_log.h only). Move pal_ usage to the .c file.")
        endif()
    endforeach()
endforeach()
