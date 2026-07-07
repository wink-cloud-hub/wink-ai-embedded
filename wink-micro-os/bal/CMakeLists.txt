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

# Stage 1 骨架：空 stub 保证静态库能独立链接（无未解析符号）。
# Stage 2-3 会把 samples/common/ 的 helper 迁移进来，替换此文件。
target_sources(wink_bal PRIVATE
    src/wink_bal_stub.c
)

target_include_directories(wink_bal PUBLIC
    include
    include/output
    include/input
    include/sensor
    include/actuator
    include/display
    include/comm
)

# BAL 依赖 runtime（wink_periodic / wink_pt_debug 等）+ DAL（底层器件驱动）+ trace（LOG/FAULT）。
# 注意：不直接依赖 pal —— 通过 DAL/runtime 间接拿 PAL 类型，强化分层红线。
target_link_libraries(wink_bal PUBLIC
    pal
    dal
    wink_runtime
    wink_trace
)
