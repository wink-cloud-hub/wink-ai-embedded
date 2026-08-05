if(NOT WIN32)
    message(FATAL_ERROR
        "wink-micro-os host target currently supports Windows only "
        "(uses Win32 Fiber via sim_ctx_win32_fiber.c). "
        "See ADR-0013 §host-platform-matrix; Linux/macOS support requires "
        "adding sim_ctx_posix_ucontext.c (out of scope for this wave).")
endif()

# host 一等 target（OBJECT 库）—— 提供完整 PAL 符号，供 test 与 host 样例链接。
add_library(pal_host OBJECT
    pal_hal_host.c
    pal_hal_adc_host.c
    pal_log_host.c
    ${WINK_OSAL_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    pal_storage_host.c
    # target 无关 PAL 源（P1-P3 整改，2026-07-04）：SSOT 位于 pal/CMakeLists.txt
    # 的 ${PAL_COMMON_SOURCES}，替代过去硬编码 pal/src/pal_pwm_router.c。
    # 直接展开源文件而非 $<TARGET_OBJECTS:pal_common>，因为 OBJECT 库嵌入到另一
    # OBJECT 库的 sources 不会向下游传播 .o（MinGW/GCC 行为）。
    ${PAL_COMMON_SOURCES}
    # ADR-0008 设备树覆写解析器：已迁至 runtime/src/（层级修正），
    # host OBJECT 仍打包供 test_dev_config 等直接链 pal_host 的单测使用。
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_dev_config.c
    # Phase 2：targets/common 的仿真基础设施（物理退化算法库）
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    # Q3 scheduler additions
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
)

target_link_libraries(pal_host PUBLIC pal)

target_include_directories(pal_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_OSAL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include    # Phase 2：wink_sim_physical.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs     # host_test_ctrl.h
    # wink_dev_config.c 现存放于 runtime/src/，编译时需 runtime/include/ 里的
    # wink_dev_config.h（2026-07-04 层级修正）。PUBLIC 传递给 host 测试/样例，
    # 消除它们额外声明 runtime/include 的重复。
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include)
