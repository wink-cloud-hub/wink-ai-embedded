# BAL (Business Abstraction Layer) —— 高层器件 helper 静态库
#
# 介于 App 与 {DAL, runtime} 之间：封装 led blink / button poll / sonar / servo /
# telemetry 等常见组合，让 App 代码保持零阻塞、零手写 task wrapper。
#
# 分层红线（ADR-0023 §8）：
#   * bal/include/**/*.h 禁止 #include 任何 pal_*.h（pal_log.h 除外，它只暴露 LOG 宏，
#     不引入 OSAL/HAL 类型）。CI grep 卡口强制。
#   * BAL 不得链接 target 专属 PAL 对象，保持跨 target 单源编译。
#
# 目录布局（Stage 2-3 迁移填充；Stage 1 仅建骨架 + stub.c 保证库能编过）：
#   include/output/   —— LED blink 等输出类 helper
#   include/input/    —— Button poll 等输入类 helper
#   include/sensor/   —— Ultrasonic 等传感器 helper
#   include/actuator/ —— Servo 等执行器 helper
#   include/display/  —— OLED dashboard helper
#   include/comm/     —— Telemetry / 通信 helper
#   src/              —— 对应 .c 实现

add_library(wink_bal STATIC)

# Stage 1 骨架 → Stage 2 迁移：helper 实现文件按需加入。
# stub.c 保留为空 TU 占位，保证未来暂时无 helper 时库也能链接；
# 当所有 helper 都迁移完成后可删除。
target_sources(wink_bal PRIVATE
    src/wink_bal_stub.c
    src/wink_blink_helper.c
    src/wink_button_events.c
    src/wink_button_events_irq.c
    src/wink_telemetry_helper.c
    src/wink_sonar_helper.c
    src/wink_ultrasonic_distance_events.c
    src/wink_servo_helper.c
    src/wink_pid.c
    src/wink_closed_loop_motor.c
)

target_include_directories(wink_bal PUBLIC
    include
    include/output
    include/input
    include/sensor
    include/actuator
    include/display
    include/comm
    include/math
    include/control
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
