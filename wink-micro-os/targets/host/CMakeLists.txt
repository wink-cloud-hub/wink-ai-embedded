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
    pal_osal_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_osal_ringbuf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    pal_storage_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/src/pal_pwm_router.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/src/wink_dev_config.c
    # Phase 2：targets/common 的仿真基础设施（物理退化算法库）
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    # Q3 scheduler additions
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
    sim_ctx_win32_fiber.c
)

target_link_libraries(pal_host PUBLIC pal)

target_include_directories(pal_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include    # Phase 2：wink_sim_physical.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs)    # host_test_ctrl.h
